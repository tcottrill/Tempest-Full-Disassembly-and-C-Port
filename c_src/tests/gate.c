/* gate.c - Gates 1 / N (M6): the translated program alone, replayed on a
 * trace of the reference run, byte-identical at every loop pass.
 *
 *   tests\lockstep.exe --frames 5000 --no-dumps --trace-out tests\gate\attract.trc
 *   tests\gate.exe tests\gate\attract.trc [--gate-n 600] [--passes K]
 *                  [--selftest-mutate PASS:ADDR] [--selftest-trace PASS[:ADDR]] [--ref DIR]
 *
 * What runs: state.c, the C modules (ALEXEC ALHAR2 ALDIS2 ALSOUN ALWELG ALSCO2
 * ALCOIN ALLANG ALEARO ALVGUT ALTES2) and the generated data
 * (progrom/vecrom/mbprom/allang_data) with the native Mathbox (mathbox.c).  No
 * ROM code, no ref6502, no refrun/lockstep.  The modules are compiled with
 * /DLOCKSTEP only so that their CK() checkpoints call lk_ck() below (game.h);
 * nothing else in them depends on it.
 *
 * Why a trace: the oracle's seam is not reproducible without CPU cycle timing
 * (RANDOM is a per-cycle LFSR, IN1 b7 a 3 kHz clock of the cycle count and b6
 * the VG HALT model, and IRQs land mid-pass).  So the hardware seam here
 * CONSUMES the trace tests\lockstep.exe --trace-out writes (format: lockstep.c,
 * "M6 Gates 1/N", version 2 since M9 B3), in order:
 *  - a C read returns the recorded value after checking the address (IN1,
 *    INOP0/1, ALLPOT/ALLPO2, RANDOM/RANDO2, EAROM data, the IRQ's TSX value);
 *    'r' records hold runs of identical reads (IN1 busy waits); Mathbox reads
 *    come from the native Mathbox and must equal the record;
 *  - a C write must match the recorded address and value (strobes $4800 $5000
 *    $5800 $60CB $60DB: address only); Mathbox writes also drive mathbox.c;
 *  - CK(pc) must meet the recorded checkpoint visit;
 *  - IRQ markers fire C irq() where lockstep's placement fires it: kind WAIT
 *    only inside hw_wait_frame (MAINLN's FRTIMR >= 9 wait), START in RESET's
 *    CLI (hw_cli), the others (anchored: OTHER in translated code) as soon as
 *    every event before them has been consumed and C is not inside irq();
 *  - RESET arrivals ('X', M9 B3): the trace starts with the power-on 'X'; C
 *    hw_reset() (DSPSYS's JMP RESET) and hw_watchdog_hang() (WDGTST) must meet
 *    an 'X' of cause JMP RESET / watchdog, a script reset's 'X' follows the
 *    'P' of its loop head directly.  At every 'X' the C state is compared with
 *    the ROM image at the arrival, then the gate longjmps back to its loop,
 *    which runs reset() again (altes2.c: to MAINLN's first wait or the diag
 *    loop head) and then mainln_pass() or diag_pass() by g.cpu_loop;
 *  - any mismatch is a hard failure naming the pass, the event number and
 *    both sides.
 * The C state is NEVER loaded from the trace: it boots from zero and runs on
 * its own.  At every 'P' record ($C7AD or $DA8D, = tests\ref\frame_NNNN) and
 * every 'X' it is compared with the ROM image: RAM $0000-$07FF except the
 * stack bytes the ROM has pushed ($0100+lowest SP since the last RESET
 * arrival+1 .. $01FF, the zero-page march's S values excluded - lockstep's
 * exemption, accumulated because the C state is not resynced), colour RAM,
 * vector RAM, the D flag, the IRQ count and (at 'P') the loop kind.  Stray
 * cpu_wr() writes also fail.  X/Y at the loop head (g.loop_x/y) are compared
 * and reported, not gated.
 *
 * What still comes from the oracle (design compromise): every hardware INPUT
 * value (switches, spinner, RANDOM, EAROM data, the IRQ's stack pointer), the
 * IRQ positions in the event order and the RESET arrivals.
 * M9 B3: the boot is always altes2.c's reset() (the M6 gate_boot() and GETOP3
 * stand-ins are deleted; --native-altes2 is accepted and does nothing).
 *
 * Gate 1 = pass 1 (RESET .. frame 1) identical.  Gate N = passes 1..N
 * identical (N = 600 unless --gate-n).  The run continues to the end of the
 * trace (or --passes K) and stops at the first difference.
 * --selftest-mutate PASS:ADDR flips bit 0 of RAM ADDR (hex) before pass PASS
 * runs; --selftest-trace PASS[:ADDR] inverts every value read from ADDR (hex,
 * default $60CA RANDOM) recorded inside pass PASS.  A mutation that reaches
 * the state must make the gate fail at that pass.
 * --ref DIR (e.g. tests\ref) also checks the trace's ROM images against the
 * refrun dumps DIR\frame_NNNN.ram/.vram/.col that exist (attract traces).
 * Exit 0 = every gate that applies passed.
 */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <setjmp.h>
#include "state.h"
#include "hw.h"
#include "game.h"
#include "mathbox.h"

#define TR_IMG 0x1810u               /* RAM 0..7FF, colour 800..80F, vector RAM 810.. */

/* altes2.c, compiled with SYSTEM / DSPSYS renamed (build_all.bat) */
xy6502 altes2_system_(uint8_t x, uint8_t y);
xy6502 altes2_dspsys(uint8_t a, uint8_t x, uint8_t y);

static uint8_t *T;                   /* the trace */
static size_t   T_len, tp;           /* cursor */
static unsigned r_used;              /* reads consumed from the 'r' record at tp */
static unsigned long ev_no;          /* events consumed */
static unsigned cur_pass = 1;        /* the pass being run: it ends at frame cur_pass */
static int      in_irq;
static mathbox  mb;
static uint8_t  R[TR_IMG];           /* the ROM image at the last 'P' / 'X' */
static uint8_t  rom_dflag, rom_min_s = 0xFF, rom_x, rom_y, rom_loop;
static uint32_t rom_frame, rom_irqs, trace_wd_cycles;

static jmp_buf  fail_jmp, reset_jmp;
static int      reset_armed;
static char     fail_msg[512];

static unsigned long n_system, n_dspsys;
static unsigned long n_fire[4], n_fire_unproven, n_mb_reads, n_loopxy_differ;
static unsigned long n_pass_kind[2], n_reset_cause[4], n_read_runs;
static const char *const cause_name[4] = { "power-on", "JMP RESET", "watchdog", "script" };

/* --selftest-trace: the trace offset of the byte to perturb (0 = none) */
static unsigned selftest_trace_pass;
static uint16_t selftest_trace_addr = 0x60CA;
static int      selftest_trace_done;

static void fail(const char *fmt, ...)
{
    va_list ap;
    int n = snprintf(fail_msg, sizeof fail_msg, "pass %u, event #%lu (trace offset %zu): ", cur_pass, ev_no, tp);
    va_start(ap, fmt);
    vsnprintf(fail_msg + n, sizeof fail_msg - (size_t)n, fmt, ap);
    va_end(ap);
    longjmp(fail_jmp, 1);
}

static uint8_t tb(size_t o)
{
    if (o >= T_len) fail("trace ends inside a record");
    return T[o];
}
static uint16_t tw(size_t o) { return (uint16_t)(tb(o) | (tb(o + 1) << 8)); }
static uint32_t tl(size_t o) { return (uint32_t)tw(o) | ((uint32_t)tw(o + 2) << 16); }

static const char *kind_name(uint8_t k)
{
    return k == 0 ? "START" : k == 1 ? "WAIT" : k == 2 ? "SEG" : k == 3 ? "OTHER" : "unplaced";
}

/* the event at the cursor, in words */
static const char *ev_text(void)
{
    static char s[96];
    if (tp >= T_len) return "nothing (end of trace)";
    switch (T[tp]) {
    case 'R': snprintf(s, sizeof s, "read $%04X = $%02X", tw(tp + 1), tb(tp + 3)); break;
    case 'r': snprintf(s, sizeof s, "read $%04X = $%02X (%u of %u identical)", tw(tp + 1), tb(tp + 3), r_used + 1, tw(tp + 4)); break;
    case 'W': snprintf(s, sizeof s, "wrote $%02X to $%04X", tb(tp + 3), tw(tp + 1)); break;
    case 'S': snprintf(s, sizeof s, "TSX in the IRQ (SP $%02X)", tb(tp + 1)); break;
    case 'C': snprintf(s, sizeof s, "visited checkpoint $%04X", tw(tp + 1)); break;
    case 'I': snprintf(s, sizeof s, "took an IRQ (%s) at $%04X", kind_name(tb(tp + 1)), tw(tp + 3)); break;
    case 'P': snprintf(s, sizeof s, "reached a loop head, %s (frame %u)", tb(tp + 9) ? "$DA8D" : "$C7AD", (unsigned)tl(tp + 1)); break;
    case 'X': snprintf(s, sizeof s, "arrived at RESET (%s)", cause_name[tb(tp + 1) & 3]); break;
    case 'E': snprintf(s, sizeof s, "ended the run"); break;
    default:  snprintf(s, sizeof s, "unknown record '%c'", T[tp]); break;
    }
    return s;
}

/* ------------------------------------------------------------------ */
/* IRQ placement                                                       */
/* ------------------------------------------------------------------ */
static void fire(void)
{
    uint8_t kind = tb(tp + 1), flags = tb(tp + 2);
    if (kind == 2 || kind == 3) { if (!(flags & 1)) n_fire_unproven++; }
    n_fire[kind < 4 ? kind : 3]++;
    tp += 5; ev_no++;
    in_irq = 1;
    irq();
    in_irq = 0;
}

/* anchored IRQs whose position has been reached */
static void try_fire(void)
{
    if (in_irq) return;
    while (tp < T_len && T[tp] == 'I' && tb(tp + 1) != 0 && tb(tp + 1) != 1)
        fire();
}

/* ------------------------------------------------------------------ */
/* the hardware seam, consuming the trace                              */
/* ------------------------------------------------------------------ */
static int is_strobe(uint32_t a)
{
    return a == 0x4800 || a == 0x5000 || a == 0x5800 || a == 0x60CB || a == 0x60DB;
}

/* one recorded read of address a ('R', or the next of an 'r' run) */
static uint8_t take_read(uint16_t a, const char *what)
{
    uint8_t v;
    if (tp >= T_len || (T[tp] != 'R' && T[tp] != 'r') || tw(tp + 1) != a)
        fail("C read %s$%04X, ROM %s", what, a, ev_text());
    v = tb(tp + 3);
    if (T[tp] == 'R') {
        tp += 4;
    } else if (++r_used >= tw(tp + 4)) {
        r_used = 0;
        tp += 6;
        n_read_runs++;
    }
    ev_no++;
    return v;
}

static uint8_t rd(uint16_t a)
{
    int in_pass = selftest_trace_pass && cur_pass == selftest_trace_pass && a == selftest_trace_addr;
    uint8_t v = take_read(a, ""), orig = v;
    if (in_pass) {
        v ^= 0xFF;
        if (!selftest_trace_done++)
            printf("GATE: selftest: every read of $%04X in pass %u inverted (first: event #%lu, $%02X -> $%02X)\n",
                   a, cur_pass, ev_no, orig, v);
    }
    try_fire();
    return v;
}

static void wr(uint16_t a, uint8_t v)
{
    if (tp >= T_len || T[tp] != 'W' || tw(tp + 1) != a || (!is_strobe(a) && tb(tp + 3) != v))
        fail("C wrote $%02X to $%04X, ROM %s", v, a, ev_text());
    tp += 4; ev_no++;
    try_fire();
}

void lk_ck(uint16_t pc)
{
    if (tp >= T_len || T[tp] != 'C' || tw(tp + 1) != pc)
        fail("C visited checkpoint $%04X, ROM %s", pc, ev_text());
    tp += 3; ev_no++;
    try_fire();
}

uint8_t hw_in1(void)                { return rd(0x0C00); }
uint8_t hw_inop0(void)              { return rd(0x0D00); }
uint8_t hw_inop1(void)              { return rd(0x0E00); }
uint8_t hw_allpot(int chip)         { return rd(chip ? 0x60D8 : 0x60C8); }
uint8_t hw_random(int chip)         { return rd(chip ? 0x60DA : 0x60CA); }
void    hw_pokey_write(int chip, uint8_t reg, uint8_t val) { wr((uint16_t)((chip ? 0x60D0u : 0x60C0u) + (reg & 0x0Fu)), val); }
void    hw_earom_write(uint8_t offset, uint8_t val) { wr((uint16_t)(0x6000u + (offset & 0x3Fu)), val); }
void    hw_earom_ctl(uint8_t val)   { wr(0x6040, val); }
uint8_t hw_earom_read(void)         { return rd(0x6050); }
void    hw_out0(uint8_t v)          { wr(0x4000, v); }
void    hw_outank(uint8_t v)        { wr(0x60E0, v); }
void    hw_color(uint8_t idx, uint8_t v) { g.colram[idx & 15] = v; }
void    hw_vgstart(void)            { wr(0x4800, 0); }
void    hw_vgstop(void)             { wr(0x5800, 0); }
void    hw_watchdog(void)           { wr(0x5000, 0); }
void    hw_soft_watchdog(void)      { fail("C tripped the software watchdog (FRTIMR $%02X)", FRTIMR); }

/* Mathbox: the native model, its reads checked against the ROM's */
void hw_mb_write(uint8_t offset, uint8_t val)
{
    wr((uint16_t)(0x6080u + (offset & 0x1Fu)), val);
    mb_write(&mb, (uint8_t)(offset & 0x1F), val);
}

static uint8_t mb_check(uint16_t a, uint8_t native)
{
    uint8_t rom = take_read(a, "Mathbox ");
    if (rom != native) fail("native Mathbox $%04X = $%02X, ROM read $%02X", a, native, rom);
    n_mb_reads++;
    try_fire();
    return native;
}
uint8_t hw_mb_status(void) { return mb_check(0x6040, mb_status(&mb)); }
uint8_t hw_mb_ylow(void)   { return mb_check(0x6060, mb_ylow(&mb)); }
uint8_t hw_mb_yhigh(void)  { return mb_check(0x6070, mb_yhigh(&mb)); }

/* the IRQ's TSX ($D70A) */
uint8_t hw_sp(void)
{
    uint8_t v;
    if (tp >= T_len || T[tp] != 'S') fail("C's IRQ did TSX, ROM %s", ev_text());
    v = tb(tp + 1);
    tp += 2; ev_no++;
    try_fire();
    return v;
}

/* MAINLN's frame wait: the IRQs the ROM took in its wait loop */
void hw_wait_frame(void)
{
    while (FRTIMR < 9) {
        if (tp >= T_len || T[tp] != 'I' || tb(tp + 1) != 1)
            fail("C waits for an IRQ (FRTIMR $%02X), ROM %s", FRTIMR, ev_text());
        fire();
    }
    /* an IRQ taken at $C7A9/$C7AB after LDA FRTIMR already read >= 9 */
    while (tp < T_len && T[tp] == 'I' && tb(tp + 1) == 1) fire();
}

/* RESET's CLI ($D9A4): the IRQ pending since its SEI - the trace's START
 * marker(s) - then any anchored IRQ now due. */
void hw_cli(void)
{
    while (tp < T_len && T[tp] == 'I' && tb(tp + 1) == 0) fire();
    try_fire();
}

/* ------------------------------------------------------------------ */
/* ALTES2 entry points                                                 */
/* ------------------------------------------------------------------ */
xy6502 system_(uint8_t x, uint8_t y)            { n_system++; return altes2_system_(x, y); }
xy6502 dspsys(uint8_t a, uint8_t x, uint8_t y)  { n_dspsys++; return altes2_dspsys(a, x, y); }

/* ------------------------------------------------------------------ */
/* state compare                                                       */
/* ------------------------------------------------------------------ */
static char diff_first[160];         /* first difference of the failing pass */
static const char *ref_dir;          /* --ref DIR */
static unsigned long n_ref_checked;

/* --ref: the trace's image of this frame against refrun's dump files */
static void check_ref(void)
{
    static const struct { const char *ext; unsigned off, len; } part[3] = {
        { "ram", 0, 0x800 }, { "col", 0x800, 16 }, { "vram", 0x810, 0x1000 } };
    uint8_t buf[0x1000];
    for (int k = 0; k < 3; k++) {
        char p[600];
        FILE *rf;
        size_t n;
        snprintf(p, sizeof p, "%s/frame_%04u.%s", ref_dir, cur_pass, part[k].ext);
        rf = fopen(p, "rb");
        if (!rf) return;                            /* not captured */
        n = fread(buf, 1, part[k].len, rf);
        fclose(rf);
        if (n != part[k].len || memcmp(buf, R + part[k].off, part[k].len) != 0)
            fail("the trace's ROM image differs from %s", p);
    }
    n_ref_checked++;
}

/* image runs at o into R; returns the offset after them */
static size_t read_runs(size_t o)
{
    uint16_t nruns = tw(o);
    o += 2;
    for (unsigned i = 0; i < nruns; i++) {
        uint16_t off = tw(o), len = tw(o + 2);
        if ((unsigned)off + len > TR_IMG || o + 4 + len > T_len) fail("bad image run");
        memcpy(R + off, T + o + 4, len);
        o += 4u + len;
    }
    return o;
}

/* C state against R, the D flag and the IRQ count; returns the number of
 * differing cells / facts and prints them */
static unsigned compare_state(const char *where, int loop)
{
    unsigned nbad = 0, lo, hi;
    char diffs[400];
    size_t dl = 0;

    diffs[0] = 0; diff_first[0] = 0;
    lo = 0x100u + rom_min_s + 1u;
    hi = 0x1FFu;
    for (unsigned a = 0; a < TR_IMG; a++) {
        uint8_t c = a < 0x800 ? g.ram[a] : a < 0x810 ? g.colram[a - 0x800] : g.vram[a - 0x810];
        unsigned cpu = a < 0x810 ? a : a - 0x810 + 0x2000;
        if (a >= lo && a <= hi) continue;
        if (c != R[a]) {
            if (nbad == 0) snprintf(diff_first, sizeof diff_first, "$%04X C=%02X ROM=%02X", cpu, c, R[a]);
            if (nbad < 8) dl += (size_t)snprintf(diffs + dl, sizeof diffs - dl, " $%04X C=%02X ROM=%02X", cpu, c, R[a]);
            nbad++;
        }
    }
    if (g.dflag != rom_dflag) {
        if (!nbad) snprintf(diff_first, sizeof diff_first, "D flag C=%u ROM=%u", g.dflag, rom_dflag);
        nbad++;
    }
    if (g.irq_count != rom_irqs) {
        if (!nbad) snprintf(diff_first, sizeof diff_first, "IRQ count C=%u ROM=%u", (unsigned)g.irq_count, (unsigned)rom_irqs);
        nbad++;
    }
    if (loop >= 0 && g.cpu_loop != loop) {
        if (!nbad) snprintf(diff_first, sizeof diff_first, "loop C=%s ROM=%s", g.cpu_loop ? "DIAG" : "MAINLN", loop ? "DIAG" : "MAINLN");
        nbad++;
    }
    if (g.stray_writes) {
        if (!nbad) snprintf(diff_first, sizeof diff_first, "%u stray cpu_wr writes", (unsigned)g.stray_writes);
        nbad++;
    }
    if (nbad)
        printf("GATE FAIL pass %u (%s): %u difference(s)%s%s (IRQs C %u ROM %u, D C %u ROM %u, stack exempt $%04X-$01FF)\n",
               cur_pass, where, nbad, diffs, g.stray_writes ? "; stray writes" : "",
               (unsigned)g.irq_count, (unsigned)rom_irqs, g.dflag, rom_dflag, lo);
    return nbad;
}

/* consume the 'P' record; returns the number of differing cells / facts */
static unsigned compare_pass(void)
{
    char where[48];
    if (tp >= T_len || T[tp] != 'P') fail("C finished the pass, ROM %s", ev_text());
    rom_frame = tl(tp + 1);
    rom_irqs = tl(tp + 5);
    rom_loop = tb(tp + 9);
    rom_dflag = tb(tp + 10); rom_min_s = tb(tp + 11); rom_x = tb(tp + 12); rom_y = tb(tp + 13);
    if (rom_frame != cur_pass) fail("trace frame %u where pass %u ends", (unsigned)rom_frame, cur_pass);
    tp = read_runs(tp + 14); ev_no++;
    if (ref_dir) check_ref();
    n_pass_kind[rom_loop ? 1 : 0]++;
    if (!rom_loop && (g.loop_x != rom_x || g.loop_y != rom_y)) n_loopxy_differ++;
    snprintf(where, sizeof where, "frame_%04u, %s", cur_pass, rom_loop ? "DIAG $DA8D" : "MAINLN $C7AD");
    return compare_state(where, rom_loop);
}

/* consume an 'X' record of cause want (-1 any) and compare the state at the
 * RESET arrival; the next 'P' is that of pass cur_pass */
static void consume_reset(int want)
{
    uint8_t cause;
    char where[48];
    try_fire();
    if (tp >= T_len || T[tp] != 'X')
        fail("C %s, ROM %s", want == 1 ? "did JMP RESET (hw_reset)" : want == 2 ? "spun in WDGTST (hw_watchdog_hang)" : "expected a RESET",
             ev_text());
    cause = tb(tp + 1);
    if (want >= 0 && cause != want) fail("C arrived at RESET by %s, ROM by %s", cause_name[want & 3], cause_name[cause & 3]);
    rom_irqs = tl(tp + 2);
    rom_dflag = tb(tp + 6); rom_min_s = tb(tp + 7);
    tp = read_runs(tp + 8); ev_no++;
    n_reset_cause[cause & 3]++;
    snprintf(where, sizeof where, "RESET arrival, %s", cause_name[cause & 3]);
    if (compare_state(where, -1)) fail("state differs at the RESET arrival (%s): %s", cause_name[cause & 3], diff_first);
}

void hw_reset(void)
{
    consume_reset(1);
    if (!reset_armed) fail("hw_reset outside the gate loop");
    longjmp(reset_jmp, 1);
}

void hw_watchdog_hang(void)
{
    consume_reset(2);
    if (!reset_armed) fail("hw_watchdog_hang outside the gate loop");
    longjmp(reset_jmp, 1);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */
static unsigned passes_ok, first_bad, trace_passes;   /* static: they survive the longjmps */
static unsigned max_passes, mut_pass, mut_addr;
static unsigned long n_boots;

/* from a RESET arrival (power-on 'X' consumed) to the end of the trace */
static void gate_run(void)
{
    (void)setjmp(reset_jmp);                    /* every RESET arrival lands here */
    reset_armed = 1;
    n_boots++;
    reset();                                    /* to MAINLN's first wait or the diag loop head */
    try_fire();
    for (;;) {
        if (compare_pass()) { first_bad = cur_pass; break; }
        passes_ok = cur_pass;
        if ((max_passes && cur_pass >= max_passes) || tp >= T_len || T[tp] == 'E') break;
        cur_pass++;
        if (T[tp] == 'X') {                     /* script reset at this loop head */
            consume_reset(3);
            longjmp(reset_jmp, 1);
        }
        if (mut_pass && cur_pass == mut_pass) {
            printf("GATE: selftest: RAM $%04X flipped $%02X -> $%02X before pass %u\n",
                   mut_addr, g.ram[mut_addr], g.ram[mut_addr] ^ 0x01, cur_pass);
            g.ram[mut_addr] ^= 0x01;
        }
        try_fire();
        if (g.cpu_loop == LOOP_DIAG) diag_pass();
        else mainln_pass();
        try_fire();
    }
    reset_armed = 0;
}

int main(int argc, char **argv)
{
    const char *path = NULL;
    unsigned gate_n = 600;
    char script[260] = "";
    FILE *f;
    long sz;
    int ok = 1;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--gate-n") && i + 1 < argc) gate_n = (unsigned)strtoul(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--passes") && i + 1 < argc) max_passes = (unsigned)strtoul(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--selftest-mutate") && i + 1 < argc) {
            char *colon;
            mut_pass = (unsigned)strtoul(argv[++i], &colon, 0);
            if (*colon != ':' || mut_pass < 2) { fprintf(stderr, "GATE: --selftest-mutate PASS:ADDR (PASS >= 2, ADDR hex)\n"); return 2; }
            mut_addr = (unsigned)strtoul(colon + 1, NULL, 16) & 0x7FF;
        }
        else if (!strcmp(argv[i], "--selftest-trace") && i + 1 < argc) {
            char *colon;
            selftest_trace_pass = (unsigned)strtoul(argv[++i], &colon, 0);
            if (*colon == ':') selftest_trace_addr = (uint16_t)strtoul(colon + 1, NULL, 16);
        }
        else if (!strcmp(argv[i], "--ref") && i + 1 < argc) ref_dir = argv[++i];
        else if (!strcmp(argv[i], "--native-altes2")) { /* M9 B3: always native */ }
        else if (argv[i][0] != '-' && !path) path = argv[i];
        else { fprintf(stderr, "usage: see the header of tests/gate.c\n"); return 2; }
    }
    if (!path) { fprintf(stderr, "usage: tests\\gate.exe TRACE [--gate-n N] [--passes K] [--selftest-mutate PASS:ADDR] [--selftest-trace PASS[:ADDR]] [--ref DIR]\n"); return 2; }

    f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "GATE: cannot open %s\n", path); return 2; }
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    T = (uint8_t *)malloc(sz > 0 ? (size_t)sz : 1u);
    if (!T || sz < 18 || fread(T, 1, (size_t)sz, f) != (size_t)sz) { fprintf(stderr, "GATE: cannot read %s\n", path); return 2; }
    fclose(f);
    T_len = (size_t)sz;
    if (memcmp(T, "TEMPTRC1", 8) != 0 || tl(8) != 2) { fprintf(stderr, "GATE: %s is not a version-2 trace (regenerate it with lockstep --trace-out)\n", path); return 2; }
    trace_wd_cycles = tl(12);
    {
        unsigned n = tw(16);
        if (18u + n > T_len) { fprintf(stderr, "GATE: bad trace header\n"); return 2; }
        memcpy(script, T + 18, n < sizeof script - 1 ? n : sizeof script - 1);
        script[n < sizeof script - 1 ? n : sizeof script - 1] = 0;
        tp = 18u + n;
    }
    if (T_len >= 9 && T[T_len - 9] == 'E') trace_passes = tl(T_len - 8);
    printf("GATE: trace %s (%s), %u passes, %zu bytes, watchdog model %u cycles\n", path, script[0] ? script : "attract",
           trace_passes, T_len, (unsigned)trace_wd_cycles);

    mb_reset(&mb);
    if (setjmp(fail_jmp) == 0) {
        memset(&g, 0, sizeof g);                /* power-on: colour RAM 0 like refrun's */
        consume_reset(0);
        gate_run();
    } else {
        reset_armed = 0;
        first_bad = cur_pass;
        snprintf(diff_first, sizeof diff_first, "I/O: %s", fail_msg);
        printf("GATE FAIL %s\n", fail_msg);
    }

    /* the gates */
    if (first_bad == 1) { printf("GATE 1: FAIL (pass 1, %s)\n", diff_first); ok = 0; }
    else if (passes_ok >= 1) printf("GATE 1: PASS\n");
    else { printf("GATE 1: FAIL (not reached)\n"); ok = 0; }
    if (first_bad && first_bad <= gate_n) {
        printf("GATE N (%u): FAIL (first differing pass %u, %s)\n", gate_n, first_bad, diff_first);
        ok = 0;
    } else if (passes_ok >= gate_n) printf("GATE N (%u): PASS\n", gate_n);
    else { printf("GATE N (%u): FAIL (trace ends after %u passes)\n", gate_n, passes_ok); ok = 0; }
    if (first_bad) printf("GATE RUN: FAIL at pass %u (%u passes identical before it)\n", first_bad, passes_ok);
    else printf("GATE RUN: PASS - %u passes byte-identical (of %u in the trace)\n", passes_ok, trace_passes);
    if (!first_bad && (max_passes == 0 || max_passes > trace_passes) && passes_ok != trace_passes) {
        printf("GATE RUN: FAIL - the trace has %u passes, %u compared\n", trace_passes, passes_ok);
        ok = 0;
    }
    printf("GATE: passes MAINLN %lu, DIAG %lu; RESET arrivals compared: power-on %lu, JMP RESET %lu, watchdog %lu, script %lu; "
           "boots through reset() %lu\n",
           n_pass_kind[0], n_pass_kind[1], n_reset_cause[0], n_reset_cause[1], n_reset_cause[2], n_reset_cause[3], n_boots);
    printf("GATE: IRQs %u (START %lu, WAIT %lu, SEG %lu, anchored %lu; %lu without the anchor proof), "
           "Mathbox reads checked %lu, events consumed %lu (read runs %lu), loop X/Y differed at %lu pass(es), stack exempt $%04X-$01FF\n",
           (unsigned)g.irq_count, n_fire[0], n_fire[1], n_fire[2], n_fire[3], n_fire_unproven,
           n_mb_reads, ev_no, n_read_runs, n_loopxy_differ, 0x100u + rom_min_s + 1u);
    printf("GATE: ALTES2 calls: SYSTEM %lu, DSPSYS %lu\n", n_system, n_dspsys);
    if (ref_dir) printf("GATE: trace images equal to %s dumps at %lu frame(s)\n", ref_dir, n_ref_checked);
    if (ref_dir && n_ref_checked == 0) { printf("GATE: FAIL - no %s dump matched a traced frame\n", ref_dir); ok = 0; }
    if (!script[0] && (n_system || n_dspsys)) {
        printf("GATE: FAIL - attract reached SYSTEM/DSPSYS\n");
        ok = 0;
    }
    if (selftest_trace_pass) printf("GATE: selftest: %d read(s) of $%04X inverted in pass %u\n", selftest_trace_done, selftest_trace_addr, selftest_trace_pass);
    if (first_bad) ok = 0;
    printf("GATE: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
