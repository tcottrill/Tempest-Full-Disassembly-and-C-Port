/* refrun.c - Tempest reference harness: the real rev-3 ROM on ref6502.
 *
 * The oracle the C port is diffed against (PLAN.md, "Verification").  Runs the
 * program ROM ($9000-$DFFF) and vector ROM ($3000-$3FFF), as generated into
 * progrom.c / vecrom.c, on the shared NMOS core
 * C:\Source2026\shared\ref6502 (never a new core), inside the Tempest memory
 * map of the AAE driver tempest.cpp (TempestRead/TempestWrite), and dumps
 * RAM $0000-$07FF + vector RAM $2000-$2FFF at every mainline pass.
 *
 *   tests\refrun.exe [--frames N] [--outdir DIR (tests\ref)] [--capture-every K]
 *                       [--snap-pc HEX] [--vg-draw-cycles N] [--no-dumps]
 *                       [--mbtest] [--script FILE]
 *                       [--diag-pc HEX (DA8D; 0 = off)]
 *                       [--watchdog-cycles N | --watchdog-irqs N]
 *                       [--earom-in FILE] [--earom-out FILE]
 *   tests\lockstep.exe  the same options, plus [--trace-out FILE] (M6: the
 *                       replay trace tests\gate.exe runs the C modules on)
 *
 * Hardware model (the CONTRACT - the port's probe seam must reproduce it):
 *  - Clock 1.512 MHz.  Cycles = standard NMOS base-cycle table, +1 for a taken
 *    branch, no page-cross penalty, 7 per interrupt entry (as the Space Duel
 *    oracle).  ref6502 counts no cycles; the table below is only a clock.
 *  - IRQ: 3 kHz / 12 = every 6144 cycles (246.09 Hz, hardware exact - AAE's
 *    240 Hz is a presentation choice).  Line-hold: pending until taken, taken
 *    at an instruction boundary when I is clear; CLI/SEI/PLP poll with the I
 *    flag from BEFORE the instruction (MAME om6502 rule, Space Duel oracle
 *    section 7).  NMI and IRQ vectors are both $D704 (asserted at start), so
 *    the interrupt is delivered with cpu_nmi(): identical push/vector effect.
 *  - IN1 $0C00: b0-b3 coins/slam idle HIGH (active low), b4 self-test off (1),
 *    b5 diag step idle (1), b6 VG HALT, b7 3 kHz clock = (cycle >> 8) & 1
 *    (256-cycle half period; exactly 12 periods per IRQ).
 *  - VG HALT: derived from the display list (see vg_halted()): a list that
 *    loops back to $2000 never halts; one that reaches HALT reads halted
 *    --vg-draw-cycles (default 4000) cycles after its VGSTART ($4800); a
 *    VGSTOP ($5800) write halts at once.
 *  - $0D00/$0E00 option switches = 0x00 (driver defaults: 1C/1C, English,
 *    20000 bonus, 3 lives, 1-credit minimum).
 *  - POKEY 1 ALLPOT $60C8 = 0x00 (spinner 0, upright); POKEY 2 ALLPO2 $60D8 =
 *    0x00 (no buttons, medium, rating 1-9).  Other POKEY reads = 0x00.
 *  - RANDOM $60CA / RANDO2 $60DA: per-chip free-running 17-bit XNOR LFSR,
 *    one right shift per CPU cycle, value = ~(low 8 bits); held at state 0
 *    (reads $FF) while SKCTL bits 0-1 are 0, restarting on release.  One shift
 *    per cycle is what makes $AE1F's pair (reads 4 cycles apart) satisfy
 *    hi(r1) == lo(r2) - the protection result QT5 ($011F) must stay 0.
 *  - Mathbox: mathbox.c, the real microcode PROMs, completes inside the write.
 *  - EAROM: er2055.c (verbatim shared chip model), blank chip reading $FF;
 *    --earom-in FILE loads a 64-byte image instead, --earom-out FILE writes
 *    the chip's 64 bytes at exit (M9).
 *  - Watchdog $5000 (M9): counted, and a hardware watchdog bites when no
 *    $5000 write happened for more than --watchdog-cycles N cycles (default
 *    1,134,000 = 0.75 s; --watchdog-irqs N = N x 6144).  Source of the
 *    default: AAE's watchdog (aae cpu_control.cpp: a 4 Hz timer restarted by
 *    every kick, machine reset on its third expiry = 750 ms).  MAME's
 *    watchdog_timer_device with no time configured uses 3 s; the board's
 *    counter length is not in this repo's sources.  Only the self test's
 *    WDGTST spin ($DAF7) should ever bite (every other kick gap is <= 2 IRQ
 *    periods), so any value > ~13,000 cycles takes the same ROM path, but the
 *    value sets the RANDOM / IRQ phase after the reboot: it is recorded in
 *    ref_index.json ("watchdog_cycles") and must not change silently.
 *    A bite = the RESET line: cpu_reset (I set, S=$FD, PC from $FFFC), AVG
 *    stopped; RAM, POKEYs (LFSRs, SKCTL), Mathbox, EAROM, OUT0/OUTANK, the
 *    IRQ clock and a pending IRQ all keep their state (RESET clears RAM
 *    itself).  A bite anywhere but $DAF7 fails the run.
 *
 * Built with /DLOCKSTEP and tests/lockstep.c (tests\lockstep.exe) the same run
 * also verifies the translated C modules call by call - see lockstep.c.
 * (M9 B3: every RESET arrival - power-on, JMP RESET, watchdog bite, script
 * reset - is passed to lockstep with its cause, lk_note_reset(), right before
 * the boundary at $D93F; lockstep.c "RESET events".)
 *
 * Frame = one loop-head pass.  Two loop heads (M9): --snap-pc (default $C7AD,
 * MAINLN: the "LDA #0 / STA FRTIMR" after the FRTIMR >= 9 wait, i.e. after
 * the previous pass's DISPLAY built its list; kind M) and --diag-pc (default
 * $DA8D, the self test's main diag loop "LDY #4", reached only with the TEST
 * switch closed at RESET; IRQs are masked there; kind D).  Both count as
 * frames (--frames counts both), apply the script, and are dumped.  A run
 * that never reaches $DA8D numbers its passes exactly as before M9.
 * Captured: frames 1-16, then every K-th (default 32).  Writes
 * DIR/frame_NNNN.ram (2048) .vram (4096) and .col (16, colour RAM
 * $0800-$080F), DIR/frame_sched.txt (IRQ count at EVERY pass - the schedule a
 * probe replays; "frame irq_count cycle" for kind M, with a fourth column "D"
 * for diag passes), DIR/ref_index.json (frames carry "loop": "M" / "D") and
 * DIR/coverage_pcs.txt.
 *
 * Scripted inputs (--script FILE, M2/M7/M9) - lines "PASS PORT [VALUE]",
 * '#' comments, lines in non-decreasing PASS order:
 *  - from the start of pass PASS on (before its first instruction), PORT
 *    reads VALUE (hex 0x.. or decimal).  PASS 0 = at power-on, before
 *    cpu_reset (e.g. "0 test 1": TEST switch closed at RESET).
 *  - byte ports: in1 (bits 0-5; b6/b7 synthesized), inop0, inop1, allpot,
 *    allpo2.  A whole-byte in1 line sets the test/diag bits too.
 *  - bit ports, VALUE 1 = active (pressed / closed / on), 0 = released:
 *      IN1 ($0C00, active low):  coinr b0, coinc b1, coinl b2, slam b3,
 *                                test b4 (self-test switch), diag b5
 *      ALLPO2 ($60D8, active high): zap b3, fire b4, start1 b5, start2 b6
 *      ALLPOT ($60C8): cocktail b4 (1 = cocktail cabinet)
 *  - spin N: from pass PASS on the ALLPOT spinner nibble steps by N (signed
 *    byte) at the start of every pass (of either kind; not at PASS 0).
 *  - pseudo ports without VALUE: "PASS reset" pulses the RESET line at the
 *    start of pass PASS (after its dump, after that pass's other lines): the
 *    same effect as a watchdog bite (a power cycle without clearing RAM);
 *    "PASS end" stops the run there.
 *
 * Exit 0 = the run reached N passes with the checks in the summary passing.
 */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir((p), 0777)
#endif

#include "ref6502.h"
#include "er2055.h"
#include "mathbox.h"
#include "progrom.h"
#include "vecrom.h"

/* ------------------------------------------------------------------ */
/* machine                                                             */
/* ------------------------------------------------------------------ */

#define IRQ_CYCLES 6144u
#define POLY17_LEN 131071u

static uint8_t ram[0x800];
static uint8_t vram[0x1000];
static uint8_t colram[16];
static uint8_t out0, outank;
static uint8_t pokey_reg[2][16];

static uint64_t cyc;                 /* cycle count at the current instruction */
static uint64_t vg_busy_until;
static unsigned vg_draw_cycles = 4000;

static mathbox mb;
static ad_er2055  earom;

static uint8_t  poly17[POLY17_LEN];  /* low 8 bits of each LFSR state */
static int      rng_held[2];
static uint64_t rng_base[2];

/* statistics */
static unsigned long n_irq_raised, n_irq_taken, n_irq_lost;
static unsigned long n_vggo, n_vgstop, n_wdog, n_vram_writes, n_color_writes;
static unsigned long n_random[2], n_pokey_w, n_earom_w, n_earom_c, n_earom_r;
static unsigned long n_unmapped_r, n_unmapped_w, n_brk;
static unsigned long last_kick_irq, max_kick_gap;
/* M9: hardware watchdog (cycle based) and RESET arrivals */
#define WD_DEFAULT_CYCLES 1134000u         /* 0.75 s, AAE (see header) */
static uint64_t wd_cycles = WD_DEFAULT_CYCLES;
static uint64_t last_kick_cyc, max_kick_gap_cyc;
static unsigned long n_wd_bites, n_wd_bites_bad;
static uint16_t wd_bad_pc;
enum { RST_POWERON, RST_JMP, RST_WATCHDOG, RST_SCRIPT, RST_KINDS };
static unsigned long n_reset_arr[RST_KINDS];
static int reset_cause = RST_POWERON;     /* cause of the next $D93F arrival */
static unsigned n_pass_m, n_pass_d;
static uint16_t cur_pc;                    /* PC of the instruction being executed */
/* SIGANA ($DB22) starts all 32 Mathbox mapping-PROM entries for a signature
 * analyser; some of those microcode entries do not reach STALL within
 * mathbox.c's step cap.  On the board the microcode simply keeps running
 * until the next start, and nothing reads the result, so these runaways are
 * genuine behaviour: counted apart and excluded from the pass criterion. */
static unsigned long n_mb_runaway_siga;
static uint32_t mb_runaway_siga_offs;
/* self-test result cells as of the last diag pass ($78-$88) */
static uint8_t diag_cells[0x11];
static unsigned diag_cells_frame;
static uint16_t first_unmapped_r[8], first_unmapped_w[8];
static uint8_t  pc_hit[0x10000 / 8];

#ifdef PASSCOST
/* tests\passcost.exe (M8, build_all.bat): the same run as an observer that
 * prints, per MAINLN pass ($C7AD to $C7AD), the 6502 cycles spent in the
 * mainline outside the frame wait (work), in the IRQ handler, and in the
 * wait loop ($C7A7/$C7A9/$C7AB), with the pass's hardware accesses outside
 * the IRQ and the display list it left - the data app_loop.c's pass-cost
 * model is fitted from.  The hardware model is not changed. */
static int      pc_in_irq;
static uint64_t pc_work, pc_handler, pc_wait;
static unsigned long pc_io_r, pc_io_w, pc_irq_prev;
static uint8_t  pc_qstate, pc_qdstate;
#endif

enum { AVG_HALT, AVG_LOOP, AVG_BAD, AVG_CAP };
static int avg_walk(unsigned *vectors, unsigned *bright, unsigned *ops);
static unsigned long n_vg_walks;

/* VG HALT (IN1 b6).  The AVG halts only on a HALT instruction; Tempest's
 * master lists ($3DB4 / $3DC8) end in JMPL VECRAM, so a running list usually
 * loops forever and HALT stays 0 - the IRQ's restart path is then idle.
 * Model: after VGSTART the list from $2000 is walked (lazily, at IN1 reads,
 * over the vector RAM as it is at that read); if it reaches HALT the AVG reads
 * halted vg_draw_cycles after the VGSTART, if it loops it keeps running and is
 * re-walked at the next read.  VGSTOP halts at once. */
static int      vg_running;
static uint64_t vg_start_cyc;
static int      vg_ends_in_halt;

static int vg_halted(void)
{
    if (!vg_running) return 1;
    if (!vg_ends_in_halt) {
        unsigned v, b, o;
        n_vg_walks++;
        if (avg_walk(&v, &b, &o) != AVG_HALT) return 0;
        vg_ends_in_halt = 1;
        vg_busy_until = vg_start_cyc + vg_draw_cycles;
    }
    return cyc >= vg_busy_until;
}

/* ---- scripted inputs (--script FILE; syntax in the header) --------------
 * Byte ports replace the whole port value; bit ports set or clear one bit
 * (IN1 bits active low, ALLPOT/ALLPO2 bits active high); spin (M4) steps the
 * spinner nibble of ALLPOT at the start of every pass; reset / end (M9) are
 * events. */
static uint8_t in_in1 = 0x3F, in_inop0 = 0x00, in_inop1 = 0x00, in_allpot = 0x00, in_allpo2 = 0x00;
static uint8_t in_spin = 0x00;
#define MAX_SCRIPT 4096
enum { SC_BYTE, SC_BIT_LOW, SC_BIT_HIGH, SC_RESET, SC_END };
static struct { unsigned pass; int kind; uint8_t *port; uint8_t mask; uint8_t val; } script[MAX_SCRIPT];
static int script_n, script_pos;
static const char *script_path;

static const struct { const char *name; int kind; uint8_t *port; uint8_t mask; } script_ports[] = {
    { "in1", SC_BYTE, &in_in1, 0 },       { "inop0", SC_BYTE, &in_inop0, 0 },
    { "inop1", SC_BYTE, &in_inop1, 0 },   { "allpot", SC_BYTE, &in_allpot, 0 },
    { "allpo2", SC_BYTE, &in_allpo2, 0 }, { "spin", SC_BYTE, &in_spin, 0 },
    { "coinr", SC_BIT_LOW, &in_in1, 0x01 }, { "coinc", SC_BIT_LOW, &in_in1, 0x02 },
    { "coinl", SC_BIT_LOW, &in_in1, 0x04 }, { "slam", SC_BIT_LOW, &in_in1, 0x08 },
    { "test", SC_BIT_LOW, &in_in1, 0x10 },  { "diag", SC_BIT_LOW, &in_in1, 0x20 },
    { "zap", SC_BIT_HIGH, &in_allpo2, 0x08 },    { "fire", SC_BIT_HIGH, &in_allpo2, 0x10 },
    { "start1", SC_BIT_HIGH, &in_allpo2, 0x20 }, { "start2", SC_BIT_HIGH, &in_allpo2, 0x40 },
    { "cocktail", SC_BIT_HIGH, &in_allpot, 0x10 },
    { "reset", SC_RESET, NULL, 0 },         { "end", SC_END, NULL, 0 },
};

static int load_script(const char *path)
{
    char line[256], port[32];
    unsigned pass, last_pass = 0;
    int val;
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "REFRUN: cannot open script %s\n", path); return 1; }
    while (fgets(line, sizeof line, f)) {
        char *h = strchr(line, '#');
        if (h) *h = 0;
        int nf = sscanf(line, "%u %31s %i", &pass, port, &val);
        if (nf < 2) continue;
        int k = -1;
        for (size_t i = 0; i < sizeof script_ports / sizeof script_ports[0]; i++)
            if (!strcmp(port, script_ports[i].name)) { k = (int)i; break; }
        int is_event = k >= 0 && (script_ports[k].kind == SC_RESET || script_ports[k].kind == SC_END);
        if (k < 0 || script_n >= MAX_SCRIPT || (is_event ? nf != 2 : nf != 3) || pass < last_pass ||
            (is_event && pass == 0) || (script_ports[k].kind >= SC_BIT_LOW && !is_event && val != 0 && val != 1)) {
            fprintf(stderr, "REFRUN: bad script line: %s", line);
            fclose(f);
            return 1;
        }
        last_pass = pass;
        script[script_n].pass = pass;
        script[script_n].kind = script_ports[k].kind;
        script[script_n].port = script_ports[k].port;
        script[script_n].mask = script_ports[k].mask;
        script[script_n].val = is_event ? 0 : (uint8_t)val;
        script_n++;
    }
    fclose(f);
    return 0;
}

/* Applies the lines up to `pass` and steps the spinner (not at pass 0, the
 * power-on call).  Returns SC_RESET / SC_END if such a line was applied
 * (end wins), else 0. */
static int apply_script(unsigned pass)
{
    int ev = 0;
    while (script_pos < script_n && script[script_pos].pass <= pass) {
        uint8_t *p = script[script_pos].port, m = script[script_pos].mask, v = script[script_pos].val;
        switch (script[script_pos].kind) {
        case SC_BYTE:     *p = v; break;
        case SC_BIT_LOW:  *p = (uint8_t)(v ? (*p & ~m) : (*p | m)); break;
        case SC_BIT_HIGH: *p = (uint8_t)(v ? (*p | m) : (*p & ~m)); break;
        case SC_RESET:    if (ev != SC_END) ev = SC_RESET; break;
        default:          ev = SC_END; break;
        }
        script_pos++;
    }
    if (pass && in_spin)
        in_allpot = (uint8_t)((in_allpot & 0xF0) | ((in_allpot + in_spin) & 0x0F));
    return ev;
}

static uint8_t in1_byte(void)
{
    uint8_t v = (uint8_t)(in_in1 & 0x3F);          /* coins, slam, test, diag (idle $3F) */
    if (vg_halted()) v |= 0x40;                   /* VG HALT = done */
    if ((cyc >> 8) & 1u)      v |= 0x80;          /* 3 kHz */
    return v;
}

static uint8_t rng_read(int chip)
{
    n_random[chip]++;
    if (rng_held[chip]) return 0xFF;
    return (uint8_t)~poly17[(cyc - rng_base[chip]) % POLY17_LEN];
}

static void note_unmapped(uint16_t *list, unsigned long *n, uint16_t a)
{
    if (*n < 8) list[*n] = a;
    (*n)++;
}

static uint8_t peek(uint16_t a)                    /* no side effects */
{
    if (a < 0x0800) return ram[a];
    if (a >= 0x2000 && a < 0x3000) return vram[a - 0x2000];
    if (a >= 0x3000 && a < 0x4000) return vecrom[a - 0x3000];
    if (a >= 0x9000 && a < 0xE000) return progrom[a - 0x9000];
    if (a >= 0xE000) return progrom[a - 0x2000 - 0x9000];   /* $E000-$FFFF mirror */
    return 0;
}

#ifdef LOCKSTEP
/* tests/lockstep.c: call-by-call verification of the translated modules. */
void lk_init(const uint8_t *ram, const uint8_t *vram, const uint8_t *colram);
void lk_io(uint32_t addr, uint8_t val, int write);
void lk_irq(const cpu *c);
void lk_boundary(const cpu *c, uint8_t last_op);
void lk_mem(uint16_t addr, int write);
int  lk_report(void);
int  lk_trace_open(const char *path, const char *script, uint64_t watchdog_cycles);
void lk_note_reset(int cause);                /* M9 B3: the next boundary ($D93F) is a RESET arrival */
#endif

static uint8_t mem_read_io(uint16_t a);

static uint8_t mem_read(uint16_t a)
{
    if (a < 0x0800 || (a >= 0x2000 && a < 0x4000) || a >= 0x9000) {
#ifdef LOCKSTEP
        if (a < 0x3000) lk_mem(a, 0);             /* RAM / vector RAM access sets (M5 IRQ placement) */
#endif
        return peek(a);
    }
    uint8_t v = mem_read_io(a);
#ifdef LOCKSTEP
    lk_io(a, v, 0);
#endif
#ifdef PASSCOST
    if (!pc_in_irq) pc_io_r++;
#endif
    return v;
}

static uint8_t mem_read_io(uint16_t a)
{
    if (a >= 0x60C0 && a < 0x60E0) {
        int chip = (a >= 0x60D0);
        unsigned r = a & 0x0F;
        if (r == 0x8) return chip ? in_allpo2 : in_allpot;   /* ALLPOT / ALLPO2 */
        if (r == 0xA) return rng_read(chip);       /* RANDOM / RANDO2 */
        return 0x00;
    }
    switch (a) {
    case 0x0C00: return in1_byte();
    case 0x0D00: return in_inop0;                  /* INOP0 */
    case 0x0E00: return in_inop1;                  /* INOP1 */
    case 0x6040: return mb_status(&mb);
    case 0x6050: n_earom_r++; return ad_er2055_data(&earom);
    case 0x6060: return mb_ylow(&mb);
    case 0x6070: return mb_yhigh(&mb);
    default: break;
    }
    note_unmapped(first_unmapped_r, &n_unmapped_r, a);
    return 0x00;
}

static void mem_write(uint16_t a, uint8_t v)
{
#ifdef LOCKSTEP
    if (a < 0x0810 || (a >= 0x2000 && a < 0x3000)) lk_mem(a, 1);
#endif
    if (a < 0x0800) { ram[a] = v; return; }
    if (a >= 0x0800 && a < 0x0810) { colram[a - 0x0800] = v; n_color_writes++; return; }
    if (a >= 0x2000 && a < 0x3000) { vram[a - 0x2000] = v; n_vram_writes++; return; }
#ifdef LOCKSTEP
    lk_io(a, v, 1);
#endif
#ifdef PASSCOST
    if (!pc_in_irq) pc_io_w++;
#endif
    if (a >= 0x6000 && a < 0x6040) { ad_er2055_set_addr_data(&earom, (uint8_t)(a & 0x3F), v); n_earom_w++; return; }
    if (a >= 0x6080 && a < 0x60A0) {
        uint32_t r0 = mb.runaway;
        mb_write(&mb, (uint8_t)(a - 0x6080), v);
        if (mb.runaway != r0 && cur_pc >= 0xDB22 && cur_pc < 0xDB5A) {   /* SIGANA's start scan */
            n_mb_runaway_siga++;
            mb_runaway_siga_offs |= 1u << (a - 0x6080);
        }
        return;
    }
    if (a >= 0x60C0 && a < 0x60E0) {
        int chip = (a >= 0x60D0);
        unsigned r = a & 0x0F;
        pokey_reg[chip][r] = v;
        n_pokey_w++;
        if (r == 0xF) {                            /* SKCTL: bits 0-1 = 0 -> reset */
            if ((v & 3) == 0) rng_held[chip] = 1;
            else if (rng_held[chip]) { rng_held[chip] = 0; rng_base[chip] = cyc; }
        }
        return;
    }
    switch (a) {
    case 0x4000: out0 = v; return;
    case 0x4800: n_vggo++; vg_running = 1; vg_start_cyc = cyc; vg_ends_in_halt = 0; return;
    case 0x5000:
        n_wdog++;
        if (n_irq_taken - last_kick_irq > max_kick_gap) max_kick_gap = n_irq_taken - last_kick_irq;
        last_kick_irq = n_irq_taken;
        if (cyc - last_kick_cyc > max_kick_gap_cyc) max_kick_gap_cyc = cyc - last_kick_cyc;
        last_kick_cyc = cyc;
        return;
    case 0x5800: n_vgstop++; vg_running = 0; return;
    case 0x6040: ad_er2055_control(&earom, v); n_earom_c++; return;
    case 0x60E0: outank = v; return;
    default: break;
    }
    note_unmapped(first_unmapped_w, &n_unmapped_w, a);
}

/* NMOS base cycles per opcode (0 = undocumented; ref6502 exits on those). */
static const uint8_t base_cycles[256] = {
/*        0 1 2 3 4 5 6 7 8 9 A B C D E F */
/* 0 */   7,6,0,0,0,3,5,0,3,2,2,0,0,4,6,0,
/* 1 */   2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,
/* 2 */   6,6,0,0,3,3,5,0,4,2,2,0,4,4,6,0,
/* 3 */   2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,
/* 4 */   6,6,0,0,0,3,5,0,3,2,2,0,3,4,6,0,
/* 5 */   2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,
/* 6 */   6,6,0,0,0,3,5,0,4,2,2,0,5,4,6,0,
/* 7 */   2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,
/* 8 */   0,6,0,0,3,3,3,0,2,0,2,0,4,4,4,0,
/* 9 */   2,6,0,0,4,4,4,0,2,5,2,0,0,5,0,0,
/* A */   2,6,2,0,3,3,3,0,2,2,2,0,4,4,4,0,
/* B */   2,5,0,0,4,4,4,0,2,4,2,0,4,4,4,0,
/* C */   2,6,0,0,3,3,5,0,2,2,2,0,4,4,6,0,
/* D */   2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,
/* E */   2,6,0,0,3,3,5,0,2,2,2,0,4,4,6,0,
/* F */   2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,
};

static int is_branch(uint8_t op) { return (op & 0x1F) == 0x10; }

static void build_poly17(void)
{
    uint32_t s = 0;
    for (uint32_t i = 0; i < POLY17_LEN; i++) {
        poly17[i] = (uint8_t)(s & 0xFF);
        uint32_t fb = (~(s ^ (s >> 5))) & 1u;          /* x^17 + x^12 + 1, XNOR */
        s = (s >> 1) | (fb << 16);
    }
    if (s != 0) { fprintf(stderr, "REFRUN: poly17 period is not %u\n", POLY17_LEN); exit(3); }
}

/* ------------------------------------------------------------------ */
/* AVG list walk (summary only: how much the display list draws)       */
/* ------------------------------------------------------------------ */

/* One pass of the list from $2000: AVG_HALT, AVG_LOOP (a JMPL back to $2000
 * at stack depth 0 - the normal Tempest frame), AVG_BAD (PC outside
 * $2000-$3FFF, stack over/underflow) or AVG_CAP.  Counts are for that pass. */
static int avg_walk(unsigned *vectors, unsigned *bright, unsigned *ops)
{
    uint16_t pc = 0x2000, stack[8];
    int sp = 0;
    *vectors = *bright = *ops = 0;
    while (*ops < 100000u) {
        if (pc < 0x2000 || pc >= 0x4000) return AVG_BAD;
        uint16_t w = (uint16_t)(peek(pc) | (peek((uint16_t)(pc + 1)) << 8));
        unsigned top = w >> 13;
        (*ops)++;
        switch (top) {
        case 0: { uint16_t w2 = (uint16_t)(peek((uint16_t)(pc + 2)) | (peek((uint16_t)(pc + 3)) << 8));
                  (*vectors)++; if (w2 >> 13) (*bright)++; pc = (uint16_t)(pc + 4); break; }
        case 1: return AVG_HALT;
        case 2: (*vectors)++; if ((w >> 5) & 7) (*bright)++; pc = (uint16_t)(pc + 2); break;
        case 5: if (sp >= 8) return AVG_BAD;
                stack[sp++] = (uint16_t)(pc + 2);
                pc = (uint16_t)(0x2000 + ((w & 0x1FFF) << 1)); break;
        case 6: if (sp == 0) return AVG_BAD;
                pc = stack[--sp]; break;
        case 7: pc = (uint16_t)(0x2000 + ((w & 0x1FFF) << 1));
                if (pc == 0x2000 && sp == 0 && *ops > 1) return AVG_LOOP;
                break;
        default: pc = (uint16_t)(pc + 2); break;
        }
    }
    return AVG_CAP;
}

static const char *avg_end_name(int e)
{
    return e == AVG_HALT ? "HALT" : e == AVG_LOOP ? "LOOP" : e == AVG_BAD ? "BAD" : "CAP";
}

/* ------------------------------------------------------------------ */
/* dumps                                                               */
/* ------------------------------------------------------------------ */

/* JSON string body for a path (backslashes / quotes escaped); buf of n bytes */
static const char *json_esc(const char *s, char *buf, size_t n)
{
    size_t o = 0;
    for (; s && *s && o + 3 < n; s++) {
        if (*s == '\\' || *s == '"') buf[o++] = '\\';
        buf[o++] = *s;
    }
    buf[o] = 0;
    return buf;
}

static const char *outdir = "tests\\ref";
static int dumps = 1;

static void dump_file(const char *kind, unsigned frame, const uint8_t *buf, size_t len)
{
    char path[600];
    FILE *f;
    snprintf(path, sizeof path, "%s/frame_%04u.%s", outdir, frame, kind);
    f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "REFRUN: cannot write %s\n", path); exit(4); }
    fwrite(buf, 1, len, f);
    fclose(f);
}

/* ------------------------------------------------------------------ */
/* Mathbox self-check against MBUDOC's formulas                        */
/* ------------------------------------------------------------------ */

static int16_t mb_load_and_run(int16_t a, int16_t b, int16_t e, int16_t f, int16_t x, int16_t y, uint8_t start)
{
    const int16_t v[6] = { a, b, e, f, x, y };
    for (int i = 0; i < 6; i++) {
        mb_write(&mb, (uint8_t)(2 * i), (uint8_t)(v[i] & 0xFF));
        if (i < 5) mb_write(&mb, (uint8_t)(2 * i + 1), (uint8_t)((uint16_t)v[i] >> 8));
    }
    mb_write(&mb, start, (uint8_t)((uint16_t)y >> 8));
    return (int16_t)(mb_ylow(&mb) | (mb_yhigh(&mb) << 8));
}

static int mbtest(void)
{
    static const int16_t cases[][6] = {
        { 0x4000, 0x0000, 0, 0, 0x0100, 0 },      { 0x2000, 0x1000, 0x10, 0x20, 0x300, 0x200 },
        { -0x3000, 0x1800, 5, -7, -0x120, 0x345 }, { 0x7FFF, -0x7FFF, 0, 0, 0x1000, 0x1000 },
        { 0x1234, 0x0567, 0x89, -0x33, 0x0ABC, -0x0DEF },
    };
    int bad = 0;
    mb_reset(&mb);
    printf("MBTEST  YHSM ($0B): (x-e)*a - (y-f)*b    SYM ($12): (x-e)*b + (y-f)*a\n");
    for (size_t k = 0; k < sizeof cases / sizeof cases[0]; k++) {
        const int16_t *c = cases[k];
        int16_t got = mb_load_and_run(c[0], c[1], c[2], c[3], c[4], c[5], 0x0B);
        int16_t got2 = (int16_t)(mb_write(&mb, 0x12, 0), mb_ylow(&mb) | (mb_yhigh(&mb) << 8));
        long long xp = (long long)(c[4] - c[2]) * c[0] - (long long)(c[5] - c[3]) * c[1];
        long long yp = (long long)(c[4] - c[2]) * c[1] + (long long)(c[5] - c[3]) * c[0];
        int ok = got == (int16_t)(xp >> 16) && got2 == (int16_t)(yp >> 16);
        printf("  a=%6d b=%6d e=%4d f=%4d x=%6d y=%6d  X'=%6d (expect %6lld)  Y'=%6d (expect %6lld)  %s\n",
               c[0], c[1], c[2], c[3], c[4], c[5], got, xp >> 16, got2, yp >> 16, ok ? "ok" : "MISMATCH");
        if (!ok) bad++;
    }
    printf("MBTEST: %s - microcode result = formula >> 16 (steps %lu, runaway %lu)\n",
           bad ? "FAIL" : "PASS", (unsigned long)mb.steps, (unsigned long)mb.runaway);
    return bad ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

static void mark_pc(uint16_t pc) { pc_hit[pc >> 3] |= (uint8_t)(1u << (pc & 7)); }

#ifdef PASSCOST
static void pc_step(uint16_t pc, uint8_t op, uint64_t dc)
{
    if (pc_in_irq) {
        pc_handler += dc;
        if (op == 0x40) pc_in_irq = 0;             /* RTI: only the IRQ returns with it */
    } else if (pc == 0xC7A7 || pc == 0xC7A9 || pc == 0xC7AB) {
        pc_wait += dc;
    } else {
        pc_work += dc;
    }
}

/* at $C7AD: the pass that ends at `frame` (frame 1 = the boot pass) */
static void pc_pass(unsigned frame, unsigned long irqs)
{
    unsigned vec, bri, ops;
    int end = avg_walk(&vec, &bri, &ops);
    if (frame == 1)
        printf("PASSCOST frame irqs work handler wait io_r io_w qstate qdstate avg_ops vectors end\n");
    else
        printf("PASSCOST %u %lu %llu %llu %llu %lu %lu %u %u %u %u %s\n", frame, irqs - pc_irq_prev,
               (unsigned long long)pc_work, (unsigned long long)pc_handler, (unsigned long long)pc_wait,
               pc_io_r, pc_io_w, pc_qstate, pc_qdstate, ops, vec, avg_end_name(end));
    pc_irq_prev = irqs;
    pc_work = pc_handler = pc_wait = 0;
    pc_io_r = pc_io_w = 0;
    pc_qstate = ram[0x00];
    pc_qdstate = ram[0x01];
}
#endif

int main(int argc, char **argv)
{
    unsigned frames_target = 600, capture_every = 32;
    uint16_t snap_pc = 0xC7AD, diag_pc = 0xDA8D;
    int do_mbtest = 0;
    const char *earom_in = NULL, *earom_out = NULL;
#ifdef LOCKSTEP
    const char *trace_path = NULL;
#endif
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames_target = (unsigned)strtoul(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--outdir") && i + 1 < argc) outdir = argv[++i];
        else if (!strcmp(argv[i], "--capture-every") && i + 1 < argc) capture_every = (unsigned)strtoul(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--snap-pc") && i + 1 < argc) snap_pc = (uint16_t)strtoul(argv[++i], NULL, 16);
        else if (!strcmp(argv[i], "--vg-draw-cycles") && i + 1 < argc) vg_draw_cycles = (unsigned)strtoul(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--no-dumps")) dumps = 0;
        else if (!strcmp(argv[i], "--mbtest")) do_mbtest = 1;
        else if (!strcmp(argv[i], "--script") && i + 1 < argc) script_path = argv[++i];
        else if (!strcmp(argv[i], "--diag-pc") && i + 1 < argc) diag_pc = (uint16_t)strtoul(argv[++i], NULL, 16);
        else if (!strcmp(argv[i], "--watchdog-cycles") && i + 1 < argc) wd_cycles = strtoull(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--watchdog-irqs") && i + 1 < argc) wd_cycles = strtoull(argv[++i], NULL, 0) * IRQ_CYCLES;
        else if (!strcmp(argv[i], "--earom-in") && i + 1 < argc) earom_in = argv[++i];
        else if (!strcmp(argv[i], "--earom-out") && i + 1 < argc) earom_out = argv[++i];
#ifdef LOCKSTEP
        else if (!strcmp(argv[i], "--trace-out") && i + 1 < argc) trace_path = argv[++i];
#endif
        else { fprintf(stderr, "usage: see the header of tests/refrun.c\n"); return 2; }
    }
    if (capture_every == 0) capture_every = 1;
#ifdef LOCKSTEP
    /* lockstep.c (pass probe, RESET regions, trace 'P' records) and gate.exe
     * know the loop heads as the constants $C7AD / $DA8D, not these options */
    if (snap_pc != 0xC7AD || diag_pc != 0xDA8D) {
        fprintf(stderr, "LOCKSTEP: --snap-pc %04X / --diag-pc %04X not supported: lockstep's pass probe, RESET regions "
                        "and trace use the fixed loop heads C7AD (MAINLN) and DA8D (diag loop); use tests\\refrun.exe for "
                        "other capture points\n", snap_pc, diag_pc);
        return 2;
    }
#endif
    if (script_path && load_script(script_path)) return 2;
#ifdef LOCKSTEP
    if (trace_path && lk_trace_open(trace_path, script_path, wd_cycles)) return 4;
#endif

    if (peek(0xFFFA) != peek(0xFFFE) || peek(0xFFFB) != peek(0xFFFF)) {
        fprintf(stderr, "REFRUN: NMI and IRQ vectors differ; cpu_nmi cannot stand in for IRQ\n");
        return 3;
    }
    build_poly17();
    mb_reset(&mb);
    if (do_mbtest) return mbtest();
    ad_er2055_init(&earom);
    memset(earom.rom, 0xFF, sizeof earom.rom);    /* blank part reads $FF (PLAN.md decision 4) */
    if (earom_in) {
        FILE *ef = fopen(earom_in, "rb");
        size_t got = ef ? fread(earom.rom, 1, sizeof earom.rom, ef) : 0;
        int extra = ef ? fgetc(ef) : EOF;
        if (ef) fclose(ef);
        if (got != sizeof earom.rom || extra != EOF) {
            fprintf(stderr, "REFRUN: --earom-in %s must be a %u-byte image\n", earom_in, (unsigned)sizeof earom.rom);
            return 2;
        }
    }
    if (wd_cycles < 2u * IRQ_CYCLES) { fprintf(stderr, "REFRUN: watchdog timeout below 2 IRQ periods\n"); return 2; }

    FILE *sched = NULL, *idx = NULL;
    if (dumps) {
        char path[600], js1[600], js2[600];
        MKDIR(outdir);
        snprintf(path, sizeof path, "%s/frame_sched.txt", outdir);
        sched = fopen(path, "w");
        snprintf(path, sizeof path, "%s/ref_index.json", outdir);
        idx = fopen(path, "w");
        if (!sched || !idx) { fprintf(stderr, "REFRUN: cannot write into %s\n", outdir); return 4; }
        fprintf(sched, "# frame irq_count cycle  (irq_count = IRQs serviced before this MAINLN pass)\n");
        fprintf(idx, "{\n  \"rom\": \"tempest rev 3 (tempest3/tempest)\",\n  \"cpu\": \"ref6502\",\n"
                     "  \"model\": {\"irq_cycles\": %u, \"irq_poll\": \"CLI/SEI/PLP use I before\", "
                     "\"clock3k_half\": 256, \"vg_draw_cycles\": %u, \"in1_idle\": \"0x3F\", "
                     "\"inop0\": \"0x00\", \"inop1\": \"0x00\", \"allpot\": \"0x00\", \"allpo2\": \"0x00\", "
                     "\"random\": \"per-chip poly17 XNOR, 1 shift/cycle, ~low8\", \"earom\": \"%s%s\", "
                     "\"mathbox\": \"microcode PROMs 136002-126..132\", \"snap_pc\": \"0x%04X\", \"script\": \"%s\", "
                     "\"diag_pc\": \"0x%04X\", \"watchdog_cycles\": %llu, "
                     "\"watchdog_source\": \"AAE cpu_control.cpp: 4 Hz timer, reset on 3rd expiry (0.75 s)\"},\n  \"frames\": [\n",
                IRQ_CYCLES, vg_draw_cycles, earom_in ? "er2055 image " : "er2055 blank $FF",
                json_esc(earom_in, js1, sizeof js1), snap_pc, json_esc(script_path, js2, sizeof js2), diag_pc,
                (unsigned long long)wd_cycles);
    }

    cpu c;
    memset(&c, 0, sizeof c);
    c.read = mem_read;
    c.write = mem_write;
    apply_script(0);                               /* PASS 0 lines: inputs held at power-on */
    cpu_reset(&c);
#ifdef LOCKSTEP
    lk_init(ram, vram, colram);
#endif

    uint64_t next_irq = IRQ_CYCLES;
    int irq_pending = 0, poll_i = 1;
    uint8_t last_op = 0;
    unsigned frame = 0, captured = 0;
    unsigned long passes_irq_prev = 0, max_irqs_per_pass = 0;
    uint64_t cycle_cap = (uint64_t)(frames_target + 200) * 20u * IRQ_CYCLES;   /* generous */
    const char *stop = "frames reached";

    for (;;) {
        if (irq_pending && !poll_i) {
#ifdef LOCKSTEP
            lk_irq(&c);
#endif
            cpu_nmi(&c);                           /* IRQ: same handler, see header */
            cyc += 7;
#ifdef PASSCOST
            pc_in_irq = 1; pc_handler += 7;
#endif
            irq_pending = 0;
            n_irq_taken++;
            last_op = 0;
        }
#ifdef LOCKSTEP
        if (c.pc == 0xD93F) lk_note_reset(reset_cause);   /* M9 B3: RESET arrival (any cause) */
        lk_boundary(&c, last_op);
#endif
        uint16_t pc = c.pc;
        if (pc == 0xD93F) {                        /* RESET arrival (any cause) */
            n_reset_arr[reset_cause]++;
            if (n_reset_arr[RST_POWERON] + n_reset_arr[RST_JMP] + n_reset_arr[RST_WATCHDOG] + n_reset_arr[RST_SCRIPT] > 1)
                cycle_cap += wd_cycles + 8000000u; /* power-on test + watchdog spin: outside the cap */
            reset_cause = RST_JMP;                 /* until the next cpu_reset */
        }
        int is_m = pc == snap_pc, is_d = diag_pc && pc == diag_pc && !is_m;
        if (is_m || is_d) {
#ifdef PASSCOST
            if (is_m) pc_pass(frame + 1, n_irq_taken);
#endif
            frame++;
            if (is_m) n_pass_m++;
            else { n_pass_d++; memcpy(diag_cells, ram + 0x78, sizeof diag_cells); diag_cells_frame = frame; }
            int ev = apply_script(frame);
            unsigned long d = n_irq_taken - passes_irq_prev;
            if (frame > 1 && d > max_irqs_per_pass) max_irqs_per_pass = d;
            passes_irq_prev = n_irq_taken;
            if (sched) fprintf(sched, is_d ? "%u %lu %llu D\n" : "%u %lu %llu\n", frame, n_irq_taken, (unsigned long long)cyc);
            if (dumps && (frame <= 16 || frame % capture_every == 0)) {
                unsigned vec, bri, ops;
                int end = avg_walk(&vec, &bri, &ops);
                dump_file("ram", frame, ram, sizeof ram);
                dump_file("vram", frame, vram, sizeof vram);
                dump_file("col", frame, colram, sizeof colram);
                fprintf(idx, "%s    {\"frame\": %u, \"loop\": \"%s\", \"irq_count\": %lu, \"cycle\": %llu, \"vggo\": %lu, "
                             "\"wdog\": %lu, \"avg_ops\": %u, \"vectors\": %u, \"bright\": %u, \"list_end\": \"%s\"}",
                        captured ? ",\n" : "", frame, is_d ? "D" : "M", n_irq_taken, (unsigned long long)cyc, n_vggo,
                        n_wdog, ops, vec, bri, avg_end_name(end));
                captured++;
            }
            if (frame >= frames_target) break;
            if (ev == SC_END) { stop = "script end"; break; }
            if (ev == SC_RESET) {                  /* RESET line pulsed before this pass's first instruction */
                cpu_reset(&c);
                vg_running = 0;
                last_kick_cyc = cyc;
                poll_i = 1;
                last_op = 0;
                reset_cause = RST_SCRIPT;
                continue;
            }
        }
        mark_pc(pc);
        uint8_t op = peek(pc);
        uint8_t p_before = c.p;
        if (op == 0x00) n_brk++;
        cur_pc = pc;
#ifdef PASSCOST
        uint64_t pc_c0 = cyc;
#endif
        cpu_step(&c);
        last_op = op;
        cyc += base_cycles[op] + (is_branch(op) && c.pc != (uint16_t)(pc + 2) ? 1u : 0u);
#ifdef PASSCOST
        pc_step(pc, op, cyc - pc_c0);
#endif
        poll_i = ((op == 0x58 || op == 0x78 || op == 0x28) ? p_before : c.p) & CPU_I;
        while (cyc >= next_irq) {
            if (irq_pending) n_irq_lost++;
            irq_pending = 1;
            n_irq_raised++;
            next_irq += IRQ_CYCLES;
        }
        if (cyc - last_kick_cyc > wd_cycles) {     /* hardware watchdog bites: RESET line */
            n_wd_bites++;
            if (c.pc != 0xDAF7) { if (!n_wd_bites_bad) wd_bad_pc = c.pc; n_wd_bites_bad++; }
            cpu_reset(&c);
            vg_running = 0;
            last_kick_cyc = cyc;
            poll_i = 1;
            last_op = 0;
            reset_cause = RST_WATCHDOG;
        }
        if (cyc > cycle_cap) { stop = "CYCLE CAP (stuck?)"; break; }
    }

    if (earom_out) {
        FILE *ef = fopen(earom_out, "wb");
        if (!ef || fwrite(earom.rom, 1, sizeof earom.rom, ef) != sizeof earom.rom) {
            fprintf(stderr, "REFRUN: cannot write --earom-out %s\n", earom_out);
            if (ef) fclose(ef);
            return 4;
        }
        fclose(ef);
    }
    if (idx) { fprintf(idx, "\n  ]\n}\n"); fclose(idx); }
    if (sched) fclose(sched);
    if (dumps) {
        char path[600];
        snprintf(path, sizeof path, "%s/coverage_pcs.txt", outdir);
        FILE *cov = fopen(path, "w");
        if (cov) {
            for (unsigned a = 0x9000; a < 0xE000; a++)
                if (pc_hit[a >> 3] & (1u << (a & 7))) fprintf(cov, "%04X\n", a);
            fclose(cov);
        }
    }

    unsigned long distinct = 0;
    for (unsigned a = 0; a < 0x10000; a++) if (pc_hit[a >> 3] & (1u << (a & 7))) distinct++;
    unsigned vec, bri, ops;
    int end = avg_walk(&vec, &bri, &ops);
    unsigned long vram_nonzero = 0;
    for (size_t i = 0; i < sizeof vram; i++) vram_nonzero += vram[i] != 0;

    printf("REFRUN: stop = %s\n", stop);
    printf("  passes %u (target %u), cycles %llu (%.2f s), IRQs raised %lu taken %lu lost %lu, max IRQs/pass %lu\n",
           frame, frames_target, (unsigned long long)cyc, (double)cyc / 1512000.0,
           n_irq_raised, n_irq_taken, n_irq_lost, max_irqs_per_pass);
    printf("  watchdog kicks %lu (max IRQs between kicks %lu), BRK executed %lu\n", n_wdog, max_kick_gap, n_brk);
    printf("  VGSTART %lu, VGSTOP %lu (list walks for HALT %lu), vector-RAM writes %lu (%lu non-zero bytes), colour writes %lu\n",
           n_vggo, n_vgstop, n_vg_walks, n_vram_writes, vram_nonzero, n_color_writes);
    printf("  display list from $2000: %u AVG ops, %u vectors (%u beam-on), ends in %s; $2000 word $%04X\n",
           ops, vec, bri, avg_end_name(end), (unsigned)(vram[0] | (vram[1] << 8)));
    printf("  RANDOM reads %lu/%lu, POKEY writes %lu, Mathbox starts %lu (%lu microsteps, runaway %lu, xor-sign %lu)\n",
           n_random[0], n_random[1], n_pokey_w, (unsigned long)mb.starts, (unsigned long)mb.steps,
           (unsigned long)mb.runaway, (unsigned long)mb.xor_sign);
    printf("  EAROM writes %lu, control %lu, reads %lu; unmapped reads %lu, writes %lu\n",
           n_earom_w, n_earom_c, n_earom_r, n_unmapped_r, n_unmapped_w);
    for (unsigned long i = 0; i < n_unmapped_r && i < 8; i++) printf("    unmapped read  $%04X\n", first_unmapped_r[i]);
    for (unsigned long i = 0; i < n_unmapped_w && i < 8; i++) printf("    unmapped write $%04X\n", first_unmapped_w[i]);
    printf("  RAM: QSTATE $%02X QDSTATE $%02X QSTATUS $%02X QFRAME $%02X FRTIMR $%02X QT5($011F) $%02X QT4($0720) $%02X CRDT $%02X SP $%02X PC $%04X\n",
           ram[0x00], ram[0x01], ram[0x05], ram[0x03], ram[0x53], ram[0x11F], ram[0x720], ram[0x06], c.s, c.pc);
    printf("  distinct PCs %lu; captured %u frames into %s\n", distinct, captured, dumps ? outdir : "(no dumps)");
    /* M9 lines: only for runs that use the self test / RESET / EAROM files, so
     * the summaries of the pre-M9 runs stay byte-identical */
    unsigned long resets_after = n_reset_arr[RST_JMP] + n_reset_arr[RST_WATCHDOG] + n_reset_arr[RST_SCRIPT];
    int m9 = n_pass_d || resets_after || n_wd_bites || earom_in || earom_out;
    if (m9) {
        printf("  loop passes: MAINLN ($%04X) %u, DIAG ($%04X) %u\n", snap_pc, n_pass_m, diag_pc, n_pass_d);
        printf("  RESET arrivals: power-on %lu, JMP RESET %lu, watchdog %lu, script %lu\n",
               n_reset_arr[RST_POWERON], n_reset_arr[RST_JMP], n_reset_arr[RST_WATCHDOG], n_reset_arr[RST_SCRIPT]);
        printf("  hardware watchdog: timeout %llu cycles, bites %lu (not at $DAF7: %lu", (unsigned long long)wd_cycles,
               n_wd_bites, n_wd_bites_bad);
        if (n_wd_bites_bad) printf(", first at $%04X", wd_bad_pc);
        printf("), max cycles between kicks %llu\n", (unsigned long long)max_kick_gap_cyc);
        if (n_pass_d) {
            printf("  self test (RAM at the last diag pass, frame %u): CHKSMS", diag_cells_frame);
            for (int i = 0; i < 12; i++) printf(" %02X", diag_cells[5 + i]);
            printf("  MBCOND %02X RAMCND %02X PK1CND %02X PK2CND %02X EARCND %02X\n",
                   diag_cells[0], diag_cells[1], diag_cells[2], diag_cells[3], diag_cells[4]);
        }
        printf("  Mathbox runaways in SIGANA's start scan %lu (entries mask $%08lX; excluded from the verdict)\n",
               n_mb_runaway_siga, (unsigned long)mb_runaway_siga_offs);
        printf("  EAROM cells at exit: EABAD %02X EAFLG %02X EAREQU %02X\n", ram[0x1C9], ram[0x1CA], ram[0x1C7]);
        printf("  EAROM image:");
        for (int i = 0; i < 64; i++) printf("%s%02X", i % 16 ? " " : "\n    ", earom.rom[i]);
        printf("\n");
        if (earom_in)  printf("  EAROM loaded from %s\n", earom_in);
        if (earom_out) printf("  EAROM written to %s\n", earom_out);
    }

    int ok = frame >= frames_target && n_brk == 0 && n_vggo > 0 && n_vram_writes > 0 &&
             n_wdog > 0 && max_kick_gap <= 2 && ram[0x11F] == 0 && mb.runaway == n_mb_runaway_siga &&
             (end == AVG_HALT || end == AVG_LOOP) && vec > 0 && n_wd_bites_bad == 0;
    if (!strcmp(stop, "script end"))
        ok = n_brk == 0 && ram[0x11F] == 0 && mb.runaway == n_mb_runaway_siga && n_wd_bites_bad == 0;
    /* a run that went through the diag loop: its self test must have reported all good */
    if (n_pass_d) {
        for (size_t i = 0; i < sizeof diag_cells; i++) if (diag_cells[i]) ok = 0;
    }
#ifdef LOCKSTEP
    if (lk_report() != 0) ok = 0;
#endif
    printf("REFRUN: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
