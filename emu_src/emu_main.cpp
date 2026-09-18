/* emu_main.cpp - tempest_emu: the emulator seam (DESIGN.md).
 *
 * The emulator TWIN of c_src\app_loop.c.  app_loop.c is the game seam of the
 * C port: it implements hw.h for the translated modules and the tempest_app_*
 * hooks for a backend.  This file implements the same tempest_app_* hooks for
 * the same, unchanged Windows backend (c_src\platform\windows\plat_win.c) -
 * but what runs behind them is the real program ROM on AAE's cpu_6502 core
 * (cpu\, vendored; the ONLY piece taken from AAE), inside the board model of
 * c_src\tests\refrun.c, built from the C port's own chip models, compiled in
 * place from ..\c_src: c012294.c (POKEY), er2055.c (EAROM), mathbox.c (the
 * microcode PROMs), avg.c (the display-list walker), state.c (the machine
 * state g: RAM, vector RAM, colour RAM - so win_probe.c and the backend's log
 * readouts work unchanged).  The ROM images come from emu_roms.c.
 *
 * Order of authority (DESIGN.md): the disassembly (disasm\tempest_defines.asm,
 * tempest_program_rom.asm - Atari's names, used here through state_defs.h's
 * generated A_* / K_* constants) and c_src (refrun.c, app_loop.c) first; MAME
 * 0.286 to settle doubts; the AAE driver only as a cross-reference.
 *
 * What lives here:
 *  (a) the memory map, as AAE-style MemoryReadByte / MemoryWriteByte tables
 *      (the core's interface), refrun.c's mem_read / mem_write address for
 *      address: RAM $0000-$07FF, COLPORT $0800-$080F, IN1 $0C00, INOP0 $0D00,
 *      INOP1 $0E00, vector RAM $2000-$2FFF, vector ROM $3000-$3FFF, OUT0 $4000,
 *      VGSTART $4800, WTCHDG/INTACK $5000, VGSTOP $5800, EADAL $6000-$603F,
 *      EACTL / MSTAT $6040, EAIN $6050, MYLOW $6060, MYHIGH $6070, MBSTAR
 *      $6080-$609F, POKEY $60C0-$60CF, POKEY2 $60D0-$60DF, OUTANK $60E0,
 *      program ROM $9000-$DFFF and its $E000-$FFFF mirror of $C000-$DFFF (the
 *      vectors).  Anything else reads $00 / is dropped, and is counted;
 *  (b) machine time: ONE timeline in CPU bus cycles at 1.512 MHz, counted by
 *      the core's cycle callback, so every handler knows the exact cycle of
 *      its access.  Both POKEYs are caught up to that cycle before every
 *      register access and at every tick end (POKEY clock = CPU clock, 1:1):
 *      RANDOM deltas are cycle-true, which the ROM's protection checks need
 *      (LAE20 / LAE23: two RANDOM reads 4 cycles apart must satisfy
 *      hi(r1) == lo(r2) or QT5 $011F goes non-zero; INISOU's SKCTL = 0 then
 *      RANDOM = $FF check sets QT4 $0720) - both cells are logged at exit;
 *  (c) the IRQ: the 3 kHz / 12 counter, every 6144 cycles (246.09 Hz).  The
 *      line is asserted at the grid cycle and held until the ROM's write to
 *      $5000 - Atari's defines give that address two names, WTCHDG and INTACK
 *      ("the write also acknowledges the IRQ"; MAME's wdclr_w clears the line
 *      there too).  refrun.c holds the line "pending until taken" instead; the
 *      two agree whenever the handler runs (SOFTOK LD717 STA WTCHDG is its
 *      first act) and differ by at most one IRQ at RESET's CLI (LD9A4), whose
 *      delay loop strobes $5000 with the IRQ masked;
 *  (d) the hardware watchdog: refrun.c's model - no $5000 write for more than
 *      1,134,000 cycles (0.75 s) pulls RESET (the CPU only: RAM, the POKEYs,
 *      the Mathbox, the EAROM, OUT0 and the IRQ clock keep their state).  Only
 *      the self test's WDGTST spin (LDAF7 BNE WDGTST) should ever starve it;
 *  (e) the AVG: VG HALT (IN1 b6) is refrun.c's model (a list that loops back
 *      to VECRAM never halts; one that reaches HALT reads halted 4000 cycles
 *      after its VGSTART; VGSTOP halts at once), and the picture is
 *      app_loop.c's vg_picture rule: every traversal of the looping list is
 *      one refresh, lasting the list's counted draw time (avg.c), never less
 *      than 4 IRQs (TP_VGW_CYCLES) or with no floor (TP_VGW_FREE);
 *  (f) inputs onto the ports exactly as app_loop.c encodes them (IN1 active
 *      low, the spinner counter on POKEY 1's ALLPOT b0-b3 moved at most +-7
 *      per IRQ, buttons and option bits on the pot lines, DIPs), audio as
 *      app_loop.c makes it (two chips, 20 Hz DC filter, one saturated mono
 *      44100 Hz stream pushed per IRQ tick), the EAROM image policy, fps_lock;
 *  (g) tempest_app_init / tempest_app_step / tempest_app_exit and the other
 *      hooks plat_win.c calls (extern "C").
 *
 * STEP GRANULARITY.  plat_win.c counts tempest_app_step calls as passes:
 * --shot PASS, --hold NAME FROM TO, --spin, --autoplay's script and --pass-log
 * are all in passes, with one plat_input_poll per pass.  So a step here is one
 * pass of whichever loop the CPU is in, detected by PC as refrun.c detects its
 * frames: MAINLN's loop head LC7AD (LDA #0 / STA FRTIMR, after the FRTIMR >= 9
 * wait) or the self test's main diag loop LDA8D (LDY #4).  tempest_app_init
 * runs RESET to the first loop head on a synthetic clock (unpaced, audio
 * dropped), as app_loop.c's boot_body does, and so does every later RESET
 * arrival (watchdog bite, DSPSYS's JMP RESET).  A step that meets no loop
 * head for 2 s of machine time returns anyway, so the window stays alive.
 * The loop heads are found by their byte signatures, so the rev 1 / rev 2
 * sets work if their code moved.
 *
 * Differences from refrun.c that make RAM at pass N NOT byte-identical to
 * tests\ref (DESIGN.md: not a goal): real POKEY RANDOM (c012294.c) instead of
 * a bare poly-17; the core's true bus-cycle counts (page crossings included)
 * instead of refrun's base-cycle table; INTACK (c) above.
 *
 * Command line: plat_win.c's parse_args rejects what it does not know, and
 * c_src is not edited.  The options that exist only here - --roms FILE and
 * --ram-dump PASS FILE - are taken out of __argv by a static initializer
 * before WinMain runs (see cli_strip below).
 */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#pragma warning(push, 3)
#include "cpu/cpu_6502.h"          /* the vendored AAE core (C++) */
#pragma warning(pop)

/* chip models first (their structs have register-named fields), as app_loop.c */
#include "c012294.h"               /* has its own extern "C" guard */
extern "C" {
#include "er2055.h"
#include "mathbox.h"
#include "avg.h"
#include "progrom.h"               /* extern const: the storage is emu_roms.c's */
#include "vecrom.h"
#include "platform/tempest_platform.h"
#include "platform/windows/ini.h"
#include "platform/windows/log.h"
#include "state.h"                 /* g + state_defs.h (A_*, K_*); last: it defines many short macros */
}
#include "emu_roms.h"

/* ------------------------------------------------------------------ */
/* constants (app_loop.c's, same names)                                */
/* ------------------------------------------------------------------ */

#define TP_CPU_HZ        1512000.0              /* 12.096 MHz / 8: CPU = POKEY clock */
#define TP_POKEY_HZ      1512000u
#define TP_IRQ_CYCLES    6144u                  /* 3 kHz / 12 = 246.09375 Hz          */
#define TP_IRQ_HZ        (TP_CPU_HZ / TP_IRQ_CYCLES)
#define TP_AUDIO_RATE    44100
#define TP_STALL_MS      100.0                  /* catch-up clamp (real clock)        */
#define TP_VG_DRAW_CYC   4000u                  /* refrun --vg-draw-cycles default    */
#define TP_IN1_IDLE      0x3Fu                  /* b0-b5 idle high (active low)        */
#define TP_WD_CYCLES     1134000u               /* hardware watchdog: refrun --watchdog-cycles default */
#define TP_DIAG_PRESENT_CYC 25200u              /* diag loop: present at most 60 frames per machine second */
#define TP_PIC_MIN_CYC   1024u                  /* host guard: no list is this short  */
#define TP_PIC_MIN_IRQS  4u
#define TP_PIC_RETRY_CYC 512u                   /* a walk that met a half-written list: look again */
#define TP_PIC_RETRIES   8

#define EMU_STEP_CAP_CYC 3024000u               /* a step with no loop head: give up after 2 s of machine time */
#define EMU_BOOT_CAP_CYC 90720000u              /* tempest_app_init: 60 s of machine time */

/* ------------------------------------------------------------------ */
/* command line: what plat_win.c's parse_args must not see             */
/* ------------------------------------------------------------------ */

static char          cli_roms[1024];            /* --roms FILE */
static unsigned long cli_dump_pass;             /* --ram-dump PASS FILE: g.ram + g.vram after step PASS (0 = after init) */
static char          cli_dump_path[1024];
static int           cli_dump_on;
static int           cli_quiet;                 /* a run with no visible window: no message boxes either */

/* Runs with the C++ static initializers, i.e. after the CRT has built
 * __argc / __argv and before WinMain: removes --roms FILE and --ram-dump PASS
 * FILE so that plat_win.c's parse_args (which answers an unknown option with
 * its usage box) never meets them.  c_src stays unedited. */
static struct cli_strip {
    cli_strip()
    {
        int n = __argc, o = 1;
        char **v = __argv;
        if (!v) return;
        for (int i = 1; i < n; i++) {
            if (!strcmp(v[i], "--roms") && i + 1 < n) {
                snprintf(cli_roms, sizeof cli_roms, "%s", v[++i]);
                continue;
            }
            if (!strcmp(v[i], "--ram-dump") && i + 2 < n) {
                cli_dump_pass = strtoul(v[i + 1], NULL, 0);
                snprintf(cli_dump_path, sizeof cli_dump_path, "%s", v[i + 2]);
                cli_dump_on = 1;
                i += 2;
                continue;
            }
            if (!strcmp(v[i], "--hidden") || !strcmp(v[i], "--shot") || !strcmp(v[i], "--pass-log")) cli_quiet = 1;
            v[o++] = v[i];
        }
        v[o] = NULL;
        __argc = o;
    }
} cli_strip_once;

/* ------------------------------------------------------------------ */
/* machine state owned by the seam                                     */
/* ------------------------------------------------------------------ */

static cpu_6502   *cpu;
static uint8_t     cpu_mem[0x10000];            /* the core's MEM fallback; never reached (every address is in the tables) */

static plat_inputs cur_in;                      /* sampled once per pass (and at a RESET) */
static ad_pokey    pokey[2];                    /* [0] POKEY $60C0, [1] POKEY2 $60D0 */
static ad_er2055   earom;
static mathbox     mb;
static uint8_t     out0_latch, outank_latch;

/* the cycle timeline */
static uint64_t mach_cyc;                       /* bus cycles since power-on (cycle_tick) */
static uint64_t pokey_cyc;                      /* both POKEYs are clocked to here */
static uint64_t next_irq;                       /* the grid cycle the IRQ line is asserted at */
static uint64_t next_tick;                      /* the same grid, for the per-IRQ housekeeping */
static int      irq_line;                       /* asserted, waiting for INTACK */
static unsigned long n_irq_raised, n_irq_lost;

/* loop heads and vectors, found in the loaded ROM */
static uint16_t pc_mainln = 0xC7AD, pc_diag = 0xDA8D;
static uint16_t vec_reset, vec_irq;

/* spinner: the 4-bit counter on POKEY 1's ALLPOT b0-b3 */
static int      spin_pos, spin_pending;

/* AVG run state (refrun.c's model, see vg_halted) */
static int      vg_running;
static uint64_t vg_start_cyc, vg_busy_until;
static int      vg_ends_in_halt;
static int      vg_walk_valid;                  /* walked since the last vector-RAM write: no HALT */

/* the picture (app_loop.c vg_picture) */
static int      vg_pic_on;
static uint64_t vg_pic_cyc;
static int      vg_pic_blank;
static int      vg_pic_retries;
static int      vg_window = TP_VGW_CYCLES;
static unsigned long n_pictures;
static uint64_t pic_cyc_sum;
static unsigned long pic_irqs_hist[2][10];
static double   pic_draw_ms[2];
static tempest_pic_row pic_rows[TP_PIC_ROWS];
static int      n_pic_rows;

/* RESET arrivals, watchdogs */
enum { RST_POWERON, RST_JMP, RST_WATCHDOG };
static int      reset_cause = RST_POWERON;      /* cause of the next arrival at the RESET vector */
static int      booting;                        /* RESET arrival .. first loop head: synthetic clock */
static uint64_t reset_arrival_cyc, boot_synthetic_cyc;
static uint64_t last_kick_cyc, max_kick_gap_cyc;
static uint64_t wd_timeout_cyc = TP_WD_CYCLES;
static unsigned long n_wdog, n_wd_bites, n_wd_bites_bad, n_jmp_resets, n_boots, n_diag_boots, n_soft_trips;
static uint16_t wd_bad_pc;
static int      in_soft_trip;

/* the diag loop */
static unsigned long n_diag_passes, n_diag_frames;
static uint64_t diag_present_next;
static uint8_t  diag_cells[0x11];               /* MBCOND $78 .. CHKSMS+11 $88 as of the last diag pass */

/* clocks (app_loop.c's anchor + stall clamp) */
static int      clk_valid;
static double   clk_base_ms;
static uint64_t clk_base_cyc;
static double   mach_scale = 1.0;               /* fps_lock: > 1 = underclocked */
static uint32_t pokey_audio_clock_hz = TP_POKEY_HZ;
static double   idle_remain_ms;

/* the pass, measured (what tests\passcost.exe measures on the oracle) */
static int      in_irq;                         /* between the IRQ's entry and its RTI */
static unsigned long pass_io_r, pass_io_w;      /* hardware accesses outside the IRQ */
static uint64_t pass_work, pass_handler, pass_wait;
static unsigned long lp_io_r, lp_io_w;
static uint32_t lp_ops;
static double   lp_work;
static unsigned long steps_done;                /* tempest_app_step calls (plat_win.c's pass number) */

/* statistics */
static unsigned long n_vg_walks, n_walk_bad, n_vggo, n_vgstop;
static unsigned long n_unmapped_r, n_unmapped_w, n_step_caps;
static uint16_t first_unmapped_r[8], first_unmapped_w[8];

/* ------------------------------------------------------------------ */
/* time passing                                                        */
/* ------------------------------------------------------------------ */

/* Catch both chips up to the bus cycle the CPU is in (the idea of AAE's
 * c012294_interface.cpp; the chips are c_src's). */
static void pokey_sync(void)
{
    uint64_t n = mach_cyc - pokey_cyc;
    if (n == 0) return;
    ad_pokey_advance(&pokey[0], (uint32_t)n);
    ad_pokey_advance(&pokey[1], (uint32_t)n);
    pokey_cyc = mach_cyc;
}

/* The core's cycle callback: one call per bus cycle, before the access.  The
 * 3 kHz / 12 counter asserts the IRQ line on its grid cycle; the line stays
 * low until INTACK (wr_wtchdg).  A grid point that finds it still asserted is
 * a lost IRQ (refrun's n_irq_lost): RESET's masked delay loop aside, none. */
static bool cycle_tick(void *)
{
    if (++mach_cyc == next_irq) {
        if (irq_line && !booting && g.cpu_loop == LOOP_MAINLN) n_irq_lost++;   /* masked loops (RESET, diag) aside */
        irq_line = 1;
        n_irq_raised++;
        cpu->set_irq_line(true);
        next_irq += TP_IRQ_CYCLES;
    }
    return true;                                /* the bus is always the CPU's */
}

static void io_read(void)  { if (!in_irq) pass_io_r++; }
static void io_write(void) { if (!in_irq) pass_io_w++; }

/* ------------------------------------------------------------------ */
/* inputs                                                              */
/* ------------------------------------------------------------------ */

/* IN1 b6 - VG HALT, tests\refrun.c's model as app_loop.c has it: after VGSTART
 * the list from VECRAM is walked (lazily, at the read, over vector RAM as it
 * is now); a list that loops (the master lists' JMPL VECRAM) keeps running and
 * reads 0; one that reaches HALT reads halted TP_VG_DRAW_CYC cycles after its
 * VGSTART; VGSTOP halts at once.  A walk that found no HALT is reused until
 * the ROM next writes vector RAM. */
static int vg_halted(void)
{
    if (!vg_running) return 1;
    if (!vg_ends_in_halt) {
        avg_result r;
        if (vg_walk_valid) return 0;
        r = avg_run_frame(NULL);
        n_vg_walks++;
        if (r.stop != AVG_STOP_HALT) { vg_walk_valid = 1; return 0; }
        vg_ends_in_halt = 1;
        vg_busy_until = vg_start_cyc + TP_VG_DRAW_CYC;
    }
    return mach_cyc >= vg_busy_until;
}

/* IN1 $0C00 (app_loop.c hw_in1): b0 right coin, b1 centre coin, b2 left coin,
 * b3 slam, b4 the self-test (TEST) switch, b5 the diagnostic step switch - all
 * active LOW; b6 VG HALT; b7 the 3 kHz clock = (cycle >> 8) & 1 (256-cycle
 * half period, 12 periods per IRQ - refrun.c's).  The diag loop's busy waits
 * (LDA91 BIT IN1 / BPL, LDA96 BIT IN1 / BMI) run on b7. */
static UINT8 rd_in1(UINT32, MemoryReadByte *)
{
    uint8_t v = (uint8_t)TP_IN1_IDLE;
    if (cur_in.coin_r) v = (uint8_t)(v & ~K_MCOINR);
    if (cur_in.coin_c) v = (uint8_t)(v & ~K_MCOINC);
    if (cur_in.coin_l) v = (uint8_t)(v & ~K_MCOINL);
    if (cur_in.slam)   v = (uint8_t)(v & ~K_S_LMBIT);
    if (cur_in.test)   v = (uint8_t)(v & ~K_MTEST);
    if (cur_in.diag)   v = (uint8_t)(v & ~K_MDITES);
    if (vg_halted())          v |= K_MHALT;
    if ((mach_cyc >> 8) & 1u) v |= K_M3KHTI;
    io_read();
    return v;
}

static UINT8 rd_inop0(UINT32, MemoryReadByte *) { io_read(); return plat_dsw_n13(); }   /* INOP0 $0D00: N13 */
static UINT8 rd_inop1(UINT32, MemoryReadByte *) { io_read(); return plat_dsw_l12(); }   /* INOP1 $0E00: L12 */

/* The pot lines (app_loop.c pot_pins).  Every ROM read of ALLPOT / ALLPO2
 * follows a POTGO strobe of the same chip (the IRQ's SWITCHES block: LD71A STA
 * POTGO / LD71D LDA ALLPOT, LD73B STA POTGO2 / LD73E LDY ALLPO2; GETOP3), so
 * c012294.c answers mid-scan with the pins' comparator mask, set at the POTGO
 * write:
 *   POKEY 1: b0-b3 the spinner counter, b4 cocktail (K_COCKTA), b5 option;
 *   POKEY 2: b0-b2 option bits (K_MOPT13), b3 zap, b4 fire, b5 start 1,
 *            b6 start 2 (active high). */
static uint8_t pot_pins(int chip)
{
    if (chip == 0)
        return (uint8_t)((plat_dsw_pokey1() & 0xF0u) | ((unsigned)spin_pos & 0x0Fu));
    return (uint8_t)((plat_dsw_pokey2() & K_MOPT13) |
                     (cur_in.zap    ? K_MSUZA  : 0u) |
                     (cur_in.fire   ? K_MFIRE  : 0u) |
                     (cur_in.start1 ? K_MSTRT1 : 0u) |
                     (cur_in.start2 ? K_MSTRT2 : 0u));
}

/* POKEY $60C0-$60CF / POKEY2 $60D0-$60DF: the chip on the machine timeline. */
static UINT8 rd_pokey(UINT32 reg, MemoryReadByte *h)
{
    int chip = h->lowAddr == A_POKEY2;
    pokey_sync();
    io_read();
    return ad_pokey_read(&pokey[chip], (uint8_t)(reg & 0x0Fu));
}

static void wr_pokey(UINT32 reg, UINT8 v, MemoryWriteByte *h)
{
    int chip = h->lowAddr == A_POKEY2;
    pokey_sync();
    reg &= 0x0Fu;
    if (reg == W_POTGO) ad_pokey_set_allpot(&pokey[chip], pot_pins(chip));
    ad_pokey_write(&pokey[chip], (uint8_t)reg, v);
    io_write();
}

/* ------------------------------------------------------------------ */
/* Mathbox, EAROM, outputs                                             */
/* ------------------------------------------------------------------ */

/* MBSTAR $6080-$609F: mathbox.c runs the microcode to its STALL inside the
 * write, so MSTAT D7 (busy) reads 0.  SIGANA ($DB22) starts all 32 entries
 * for a signature analyser and some never reach STALL within mathbox.c's step
 * cap - genuine behaviour (refrun.c), visible as mb.runaway in the exit log. */
static void  wr_mbstar(UINT32 off, UINT8 v, MemoryWriteByte *) { mb_write(&mb, (uint8_t)(off & 0x1F), v); io_write(); }
static UINT8 rd_mstat(UINT32, MemoryReadByte *)  { io_read(); return mb_status(&mb); }   /* MSTAT  $6040 */
static UINT8 rd_mylow(UINT32, MemoryReadByte *)  { io_read(); return mb_ylow(&mb); }     /* MYLOW  $6060 */
static UINT8 rd_myhigh(UINT32, MemoryReadByte *) { io_read(); return mb_yhigh(&mb); }    /* MYHIGH $6070 */

/* EAROM: the ER2055 behind EADAL $6000-$603F / EACTL $6040 / EAIN $6050.  The
 * image is the NVRAM blob; app_loop.c's policy: EAUPD deselects the chip
 * (EACTL = 0) on every call, which is the flush point once an erase / write
 * changed a cell. */
static void wr_eadal(UINT32 off, UINT8 v, MemoryWriteByte *)
{
    ad_er2055_set_addr_data(&earom, (uint8_t)(off & 0x3F), v);
    io_write();
}

static void wr_eactl(UINT32, UINT8 v, MemoryWriteByte *)
{
    ad_er2055_control(&earom, v);
    if (v == 0x00 && earom.dirty) {
        if (plat_nvram_write(earom.rom, sizeof earom.rom) == 0)
            earom.dirty = false;
    }
    io_write();
}

static UINT8 rd_eain(UINT32, MemoryReadByte *) { io_read(); return ad_er2055_data(&earom); }

/* A READ of the write-only EADAL / COLPORT ranges: not the ROM's doing but the
 * NMOS 6502's - an indexed store (LDE7F / LDEAB / LDEB8 STA EADAL,X, LDA7D STA
 * COLPORT,X) spends its fourth cycle reading the target address before it
 * writes it, and the vendored core runs that bus cycle.  Nothing answers on
 * the board; $00 here, counted apart from the genuinely unmapped reads. */
static unsigned long n_dummy_reads;
static UINT8 rd_write_only(UINT32, MemoryReadByte *) { n_dummy_reads++; return 0x00; }

/* OUT0 $4000: b0-b2 coin counters (ignored), b3 / b4 video invert X / Y (to
 * the renderer as plat_video_begin's flip).  OUTANK $60E0: start LEDs, flip. */
static void wr_out0(UINT32, UINT8 v, MemoryWriteByte *)   { out0_latch = v;   io_write(); }
static void wr_outank(UINT32, UINT8 v, MemoryWriteByte *) { outank_latch = v; io_write(); }

/* Vector RAM $2000-$2FFF: g.vram; a write invalidates the VG HALT walk. */
static void wr_vecram(UINT32 off, UINT8 v, MemoryWriteByte *) { g.vram[off] = v; vg_walk_valid = 0; }

/* VGSTART $4800 (strobe): the AVG starts at VECRAM; the first traversal's
 * picture is due one idle PROM tick later (app_loop.c hw_vgstart). */
static void wr_vgstart(UINT32, UINT8, MemoryWriteByte *)
{
    n_vggo++;
    vg_running = 1; vg_start_cyc = mach_cyc; vg_ends_in_halt = 0; vg_walk_valid = 0;
    vg_pic_on = 1; vg_pic_cyc = mach_cyc + AVG_VGGO_LEADIN / AVG_CYC_PER_CPU; vg_pic_retries = 0;
    io_write();
}

/* VGSTOP $5800 (strobe): VG reset - halted at once. */
static void wr_vgstop(UINT32, UINT8, MemoryWriteByte *)
{
    n_vgstop++;
    vg_running = 0; vg_walk_valid = 0; vg_pic_on = 0;
    io_write();
}

/* WTCHDG = INTACK $5000 (strobe): kicks the hardware watchdog and releases
 * the IRQ line.  The IRQ does it first thing when its software watchdog is
 * happy (SOFTOK LD717); RESET's delay loop and the diag loop do it too. */
static void wr_wtchdg(UINT32, UINT8, MemoryWriteByte *)
{
    n_wdog++;
    if (mach_cyc - last_kick_cyc > max_kick_gap_cyc) max_kick_gap_cyc = mach_cyc - last_kick_cyc;
    last_kick_cyc = mach_cyc;
    if (irq_line) { irq_line = 0; cpu->set_irq_line(false); }
    io_write();
}

/* Everything else: reads $00, writes dropped (refrun.c), counted. */
static UINT8 rd_unmapped(UINT32 a, MemoryReadByte *)
{
    if (n_unmapped_r < 8) first_unmapped_r[n_unmapped_r] = (uint16_t)a;
    n_unmapped_r++;
    return 0x00;
}

static void wr_unmapped(UINT32 a, UINT8, MemoryWriteByte *)
{
    if (n_unmapped_w < 8) first_unmapped_w[n_unmapped_w] = (uint16_t)a;
    n_unmapped_w++;
}

/* ------------------------------------------------------------------ */
/* the memory map                                                      */
/* ------------------------------------------------------------------ */
/* AAE-style tables: {low, high, handler, user}; a NULL handler = plain memory
 * at `user`; handlers get the offset from `low`.  First match wins, so the
 * program ROM (every opcode fetch) comes first and the catch-all last. */

#define ROM_PTR(p) (const_cast<uint8_t *>(p))   /* read-only table entries over the extern const images */
#define MAP_END    { (UINT32)-1, (UINT32)-1, NULL, NULL }

static MemoryReadByte emu_read[] = {
    { 0x9000,    0xDFFF,          NULL,        ROM_PTR(progrom) },            /* program ROM */
    { 0xE000,    0xFFFF,          NULL,        ROM_PTR(progrom) + 0x3000 },   /* mirror of $C000-$DFFF: the vectors */
    { 0x0000,    0x07FF,          NULL,        g.ram },
    { A_VECRAM,  0x2FFF,          NULL,        g.vram },
    { A_VectorRom, 0x3FFF,        NULL,        ROM_PTR(vecrom) },
    { A_IN1,     A_IN1,           rd_in1,      NULL },
    { A_INOP0,   A_INOP0,         rd_inop0,    NULL },
    { A_INOP1,   A_INOP1,         rd_inop1,    NULL },
    { A_MSTAT,   A_MSTAT,         rd_mstat,    NULL },
    { A_EAIN,    A_EAIN,          rd_eain,     NULL },
    { A_MYLOW,   A_MYLOW,         rd_mylow,    NULL },
    { A_MYHIGH,  A_MYHIGH,        rd_myhigh,   NULL },
    { A_POKEY,   A_POKEY + 0x0F,  rd_pokey,    NULL },
    { A_POKEY2,  A_POKEY2 + 0x0F, rd_pokey,    NULL },
    { A_EADAL,   A_EADAL + 0x3F,  rd_write_only, NULL },
    { A_COLPORT, A_COLPORT + 15,  rd_write_only, NULL },
    { 0x0000,    0xFFFF,          rd_unmapped, NULL },
    MAP_END
};

static MemoryWriteByte emu_write[] = {
    { 0x0000,    0x07FF,          NULL,        g.ram },
    { A_COLPORT, A_COLPORT + 15,  NULL,        g.colram },                    /* colour RAM, write-only on the board */
    { A_VECRAM,  0x2FFF,          wr_vecram,   NULL },
    { A_OUT0,    A_OUT0,          wr_out0,     NULL },
    { A_VGSTART, A_VGSTART,       wr_vgstart,  NULL },
    { A_WTCHDG,  A_WTCHDG,        wr_wtchdg,   NULL },
    { A_VGSTOP,  A_VGSTOP,        wr_vgstop,   NULL },
    { A_EADAL,   A_EADAL + 0x3F,  wr_eadal,    NULL },
    { A_EACTL,   A_EACTL,         wr_eactl,    NULL },
    { A_MBSTAR,  A_MBSTAR + 0x1F, wr_mbstar,   NULL },
    { A_POKEY,   A_POKEY + 0x0F,  wr_pokey,    NULL },
    { A_POKEY2,  A_POKEY2 + 0x0F, wr_pokey,    NULL },
    { A_OUTANK,  A_OUTANK,        wr_outank,   NULL },
    { 0x0000,    0xFFFF,          wr_unmapped, NULL },
    MAP_END
};

/* ------------------------------------------------------------------ */
/* POKEY audio: one IRQ tick of rendered sound (app_loop.c)            */
/* ------------------------------------------------------------------ */

static int      audio_live;
static int      audio_resync;                   /* set at the end of a boot (boot_done) */
static uint64_t audio_tick_phase;
static unsigned long audio_underrun_frames;
static int16_t  audio_buf[2][512];

static void pokey_init(void)
{
    for (int i = 0; i < 2; i++) {
        ad_pokey_init(&pokey[i], pokey_audio_clock_hz, TP_AUDIO_RATE);
        ad_pokey_set_measured_audio(&pokey[i], 20.0, 1.0);
        ad_pokey_set_allpot(&pokey[i], pot_pins(i));
    }
}

static void audio_clear(void)
{
    ad_pokey_audio_clear(&pokey[0]);
    ad_pokey_audio_clear(&pokey[1]);
    audio_tick_phase = 0;
}

static void render_push_audio_tick(void)
{
    const int cap = (int)(sizeof audio_buf[0] / sizeof audio_buf[0][0]);
    int n, i;
    if (audio_resync) {                         /* the first tick end after a boot: live audio starts on a whole tick */
        audio_clear();
        audio_resync = 0;
        return;
    }
    audio_tick_phase += (uint64_t)TP_AUDIO_RATE * TP_IRQ_CYCLES;
    n = (int)(audio_tick_phase / pokey_audio_clock_hz);
    audio_tick_phase %= pokey_audio_clock_hz;
    if (n > cap) n = cap;
    if (n <= 0) return;
    for (i = 0; i < 2; i++) {
        int got = ad_pokey_audio_read(&pokey[i], audio_buf[i], n);
        if (got < n) {
            if (!booting) audio_underrun_frames += (unsigned long)(n - got);
            memset(audio_buf[i] + got, 0, (size_t)(n - got) * sizeof audio_buf[i][0]);
        }
    }
    /* drained either way; a boot on the synthetic clock pushes nothing live */
    if (!audio_live || booting) return;
    for (i = 0; i < n; i++) {
        int v = (int)audio_buf[0][i] + (int)audio_buf[1][i];
        if (v > 32767) v = 32767; else if (v < -32768) v = -32768;
        audio_buf[0][i] = (int16_t)v;
    }
    plat_audio_push(audio_buf[0], n);
}

/* ------------------------------------------------------------------ */
/* pacing                                                              */
/* ------------------------------------------------------------------ */

/* Give up the CPU until `remain` ms have passed (app_loop.c machine_idle):
 * the backend's plat_sleep_ms pumps the window and presents; under 2 ms the
 * caller spins on the clock. */
static void machine_idle(double remain)
{
    idle_remain_ms = remain;
    if (remain > 2.0) plat_sleep_ms(1);
}

/* Block until the wall clock reaches machine cycle `ev` (the CPU has already
 * run up to it): app_loop.c machine_tick's anchor, stall clamp and fps_lock
 * scale.  A boot runs on the synthetic clock: no wait. */
static void wait_until_cyc(uint64_t ev)
{
    if (booting) return;
    for (;;) {
        double now = plat_now_ms(), due;
        if (!clk_valid) {                       /* (re)anchor: the machine is at `ev` now */
            clk_base_ms = now;
            clk_base_cyc = ev;
            clk_valid = 1;
        }
        due = clk_base_ms + ((double)ev - (double)clk_base_cyc) * 1000.0 / TP_CPU_HZ * mach_scale;
        if (now - due > TP_STALL_MS) {          /* debugger / dragged window: do not dump time */
            clk_base_ms = now;
            clk_base_cyc = ev;
            due = now;
        }
        if (now >= due) return;
        machine_idle(due - now);
    }
}

/* The per-IRQ housekeeping, at the first instruction boundary after a grid
 * cycle (the line itself was asserted on the exact cycle, cycle_tick): move
 * the spinner counter, clock the chips to here, push one tick of audio. */
static void machine_tick(void)
{
    if (g.cpu_loop == LOOP_DIAG && !booting) {
        /* no IRQ reads a delta in the diag loop: TIMEST reads ALLPOT whole */
        spin_pos = (spin_pos + spin_pending) & 0x0F;
        spin_pending = 0;
    } else {
        /* the IRQ adds the sign-extended nibble difference to TBHD (LD728-LD737),
         * so the counter may move at most 7 between IRQs; the rest is carried */
        int step = spin_pending > 7 ? 7 : spin_pending < -7 ? -7 : spin_pending;
        spin_pending -= step;
        spin_pos = (spin_pos + step) & 0x0F;
    }
    next_tick += TP_IRQ_CYCLES;
    pokey_sync();
    render_push_audio_tick();
}

/* ------------------------------------------------------------------ */
/* the picture (app_loop.c vg_picture, see its long comment)           */
/* ------------------------------------------------------------------ */

static void line_seg(void *, const avg_seg *s)
{
    if (s->intensity == 0) return;              /* a dark move: the beam is blanked */
    plat_video_line((float)AVG_Q15_TO_F(s->x0), (float)AVG_Q15_TO_F(s->y0),
                    (float)AVG_Q15_TO_F(s->x1), (float)AVG_Q15_TO_F(s->y1),
                    s->rgb, (int)s->intensity);
}

static void present_list(void)
{
    avg_cfg cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.seg = line_seg;
    plat_video_begin((uint8_t)(out0_latch & (K_MVINVX | K_MVINVY)));
    avg_run_frame(&cfg);
    plat_video_present();
}

/* One traversal of the list the AVG runs from VECRAM = one refresh.  The
 * walk is instantaneous where the AVG takes the list's whole draw time, so -
 * unlike app_loop.c, whose C pass runs in no time - it can catch MAINLN's
 * DISPLAY half way through a JMPL patch.  A walk that ends neither in the
 * master list's JMPL VECRAM (LOOP) nor in HALT is looked at again 512 cycles
 * later, a few times, before it is taken for a broken list. */
static void vg_picture(void)
{
    avg_result r = avg_run_frame(NULL);
    uint32_t d = (r.cycles + AVG_CYC_PER_CPU - 1u) / AVG_CYC_PER_CPU;     /* the list's draw time, CPU cycles */
    uint32_t floor_cyc = vg_window == TP_VGW_FREE ? TP_PIC_MIN_CYC : TP_PIC_MIN_IRQS * TP_IRQ_CYCLES;
    if (r.stop != AVG_STOP_LOOP && r.stop != AVG_STOP_HALT && vg_pic_retries < TP_PIC_RETRIES) {
        vg_pic_retries++;
        vg_pic_cyc += TP_PIC_RETRY_CYC;
        return;
    }
    vg_pic_retries = 0;
    if (r.stop == AVG_STOP_LOOP) {
        int game = (QSTATUS & K_MATRACT) != 0;
        double ms = avg_cycles_ms(r.cycles);
        uint32_t n = (d + TP_IRQ_CYCLES - 1u) / TP_IRQ_CYCLES;          /* in IRQs, for the statistics */
        if (n < TP_PIC_MIN_IRQS) n = TP_PIC_MIN_IRQS;
        pic_irqs_hist[game][n > 9u ? 9u : n]++;
        pic_draw_ms[game] += ms;
        if (game) {                             /* per (QSTATE, wave), for the backend's exit log */
            int k;
            for (k = 0; k < n_pic_rows; k++)
                if (pic_rows[k].qstate == QSTATE && pic_rows[k].wave == CURWAV) break;
            if (k == n_pic_rows && n_pic_rows < TP_PIC_ROWS) {
                memset(&pic_rows[k], 0, sizeof pic_rows[k]);
                pic_rows[k].qstate = QSTATE;
                pic_rows[k].wave = CURWAV;
                n_pic_rows++;
            }
            if (k < n_pic_rows) {
                pic_rows[k].pictures++;
                pic_rows[k].draw_ms += ms;
                if (ms > pic_rows[k].draw_max) pic_rows[k].draw_max = ms;
                if (pic_rows[k].draw_min == 0.0 || ms < pic_rows[k].draw_min) pic_rows[k].draw_min = ms;
            }
        }
    }
    if (d < floor_cyc) d = floor_cyc;
    if (r.stop == AVG_STOP_LOOP) {
        n_pictures++;
        pic_cyc_sum += d;
        vg_pic_cyc += d;
    } else if (r.stop == AVG_STOP_HALT) {
        /* Drawn once, then dark.  app_loop.c switches the picture off here
         * until the next VGSTART, and gets one: its C pass has rebuilt the list
         * before the picture that follows the 9th IRQ's VGSTART is served.
         * Here the ROM patches VECRAM's JMPL from the SWHALT list ($3DCC) to the
         * looping master list BETWEEN IRQs, and refrun's VG HALT model then
         * reads "running" with no further VGSTART (vg_halted walks the list as
         * it is now) - so the halted list is looked at again at the rate the
         * IRQ would restart it (LD7C9 BIT IN1 / BVC, every 6144 cycles; a lit
         * one at the 4-IRQ picture floor). */
        vg_pic_cyc += r.nlit ? TP_PIC_MIN_IRQS * TP_IRQ_CYCLES : TP_IRQ_CYCLES;
    } else {
        n_walk_bad++;
        vg_pic_cyc += d > 4u * TP_IRQ_CYCLES ? d : 4u * TP_IRQ_CYCLES;
    }
    if (r.nlit == 0 && vg_pic_blank) return;    /* a blank screen is presented once */
    vg_pic_blank = r.nlit == 0;
    present_list();
}

/* The diag loop's frame (app_loop.c diag_present): the list the pass started
 * (ending in VGHALT's HALT) is presented at the loop head, at most 60 times
 * per machine second; the board redraws it every pass, ~90-140 Hz. */
static void diag_present(void)
{
    if (mach_cyc < diag_present_next) return;
    present_list();
    n_diag_frames++;
    diag_present_next += TP_DIAG_PRESENT_CYC;
    if (diag_present_next <= mach_cyc) diag_present_next = mach_cyc + TP_DIAG_PRESENT_CYC;
}

/* ------------------------------------------------------------------ */
/* running the CPU                                                     */
/* ------------------------------------------------------------------ */

/* The RESET line (power-on, watchdog bite): the CPU only - refrun.c: "RAM,
 * POKEYs, Mathbox, EAROM, OUT0/OUTANK, the IRQ clock and a pending IRQ all
 * keep their state (RESET clears RAM itself)"; the AVG is stopped.  RESET
 * reads the TEST switch (LD983) as it is at that moment, so the inputs are
 * sampled again first, as app_loop.c's run_guarded does. */
static void pull_reset(int cause)
{
    plat_input_poll(&cur_in);
    spin_pending += cur_in.spin_delta;
    cpu->reset6502();                           /* I set, PC from $FFFC */
    vg_running = 0; vg_walk_valid = 0; vg_pic_on = 0;
    last_kick_cyc = mach_cyc;
    in_irq = 0;
    in_soft_trip = 0;
    reset_cause = cause;
}

/* The end of a boot: live audio and the clock start from here. */
static void boot_done(void)
{
    if (g.cpu_loop == LOOP_DIAG) n_diag_boots++;
    boot_synthetic_cyc += mach_cyc - reset_arrival_cyc;
    audio_resync = 1;                           /* the boot's samples are dropped at the next tick end */
    diag_present_next = mach_cyc;
    if (vg_pic_cyc < mach_cyc) vg_pic_cyc = mach_cyc;   /* no pictures owed from the boot */
    clk_valid = 0;
    booting = 0;
}

/* Run to the next loop head (LC7AD or LDA8D), at most cap_cyc cycles.
 * Returns 1 at a loop head, 0 at the cap. */
static int run_to_loop_head(uint64_t cap_cyc)
{
    const uint64_t start = mach_cyc;
    int first = 1;
    for (;;) {
        uint16_t pc0, pc1;
        uint8_t op0, s0;
        uint64_t c0;

        /* events the CPU's time has reached, in cycle order, each at its own
         * wall-clock time (app_loop.c machine_tick) */
        for (;;) {
            int pic = vg_pic_on && !booting && g.cpu_loop == LOOP_MAINLN && vg_pic_cyc <= mach_cyc;
            int tick = next_tick <= mach_cyc;
            if (!pic && !tick) break;
            if (pic && (!tick || vg_pic_cyc <= next_tick)) { wait_until_cyc(vg_pic_cyc); vg_picture(); }
            else { wait_until_cyc(next_tick); machine_tick(); }
        }

        pc0 = cpu->get_pc();
        if (pc0 == vec_reset) {                 /* RESET arrival, any cause */
            n_boots++;
            if (reset_cause == RST_JMP) {       /* DSPSYS's LD861 JMP RESET inside running code */
                n_jmp_resets++;
                plat_input_poll(&cur_in);
                spin_pending += cur_in.spin_delta;
            }
            if (!booting) { booting = 1; }
            reset_arrival_cyc = mach_cyc;
            in_irq = 0;
            reset_cause = RST_JMP;              /* until the RESET line is next pulled */
        }
        if (!first && (pc0 == pc_mainln || pc0 == pc_diag)) {
            if (pc0 == pc_mainln) {
                g.cpu_loop = LOOP_MAINLN;
                g.pass_count++;
            } else {
                g.cpu_loop = LOOP_DIAG;
                n_diag_passes++;
                memcpy(diag_cells, g.ram + K_MBCOND, sizeof diag_cells);
            }
            if (booting) boot_done();
            if (g.cpu_loop == LOOP_DIAG) diag_present();
            return 1;
        }
        first = 0;

        op0 = cpu_rd(pc0);
        s0 = cpu->m6502_get_reg(cpu_6502::M6502_S);
        c0 = mach_cyc;
        cpu->step6502();
        pc1 = cpu->get_pc();

        /* the pass, measured as tests\passcost.exe measures the oracle */
        if (in_irq) {
            pass_handler += mach_cyc - c0;
            if (op0 == 0x40) in_irq = 0;        /* RTI: only the IRQ returns with it */
        } else if (pc1 == vec_irq && op0 != 0x00 && mach_cyc - c0 == 7 &&
                   (uint8_t)(s0 - cpu->m6502_get_reg(cpu_6502::M6502_S)) == 3) {
            in_irq = 1;                         /* that step was the IRQ's entry, not an instruction */
            g.irq_count++;
            pass_handler += 7;
        } else if (pc0 >= (uint16_t)(pc_mainln - 6) && pc0 < pc_mainln) {
            pass_wait += mach_cyc - c0;         /* LC7A7 LDA FRTIMR / CMP #9 / BCC */
        } else {
            pass_work += mach_cyc - c0;
        }
        if (op0 == 0x00 && !in_soft_trip) {     /* LD713 BRK: the IRQ's software watchdog tripped */
            in_soft_trip = 1;
            n_soft_trips++;
        }

        /* the hardware watchdog bites: the RESET line */
        if (mach_cyc - last_kick_cyc > wd_timeout_cyc) {
            n_wd_bites++;
            if (!(cpu_rd(pc1) == 0xD0 && cpu_rd((uint16_t)(pc1 + 1)) == 0xFE)) {   /* not LDAF7 BNE WDGTST */
                if (!n_wd_bites_bad) wd_bad_pc = pc1;
                n_wd_bites_bad++;
            }
            pull_reset(RST_WATCHDOG);
        }
        if (mach_cyc - start > cap_cyc) return 0;
    }
}

/* ------------------------------------------------------------------ */
/* set-up                                                              */
/* ------------------------------------------------------------------ */

static void earom_load(void)
{
    ad_er2055_init(&earom);
    ad_er2055_control(&earom, 0);
    if (plat_nvram_read(earom.rom, sizeof earom.rom) != 0)
        memset(earom.rom, 0xFF, sizeof earom.rom);      /* a blank part reads $FF */
    earom.dirty = false;
}

/* A loop head by its bytes: at its rev-3 address if the ROM has them there,
 * else wherever they occur exactly once in $9000-$DFFF; 0 = not found. */
static uint16_t find_code(uint16_t rev3_addr, const uint8_t *sig, size_t n, unsigned head_off)
{
    unsigned hits = 0, at = 0;
    if (memcmp(progrom + (rev3_addr - 0x9000), sig, n) == 0) return (uint16_t)(rev3_addr + head_off);
    for (unsigned a = 0; a + n <= 0x5000; a++)
        if (memcmp(progrom + a, sig, n) == 0) { hits++; at = a; }
    return hits == 1 ? (uint16_t)(0x9000 + at + head_off) : 0;
}

static void find_loop_heads(void)
{
    /* MAINLN: LC7A7 LDA FRTIMR / CMP #9 / BCC LC7A7 / LC7AD LDA #0 / STA FRTIMR */
    static const uint8_t sig_mainln[] = { 0xA5, A_FRTIMR, 0xC9, 0x09, 0x90, 0xFA, 0xA9, 0x00, 0x85, A_FRTIMR };
    /* main diag loop: LDA8D LDY #4 / LDX #$14 / BIT IN1 / BPL / BIT IN1 / BMI */
    static const uint8_t sig_diag[] = { 0xA0, 0x04, 0xA2, 0x14, 0x2C, 0x00, 0x0C, 0x10, 0xFB, 0x2C, 0x00, 0x0C, 0x30, 0xFB };
    pc_mainln = find_code(0xC7A7, sig_mainln, sizeof sig_mainln, 6);
    pc_diag   = find_code(0xDA8D, sig_diag, sizeof sig_diag, 0);
    vec_reset = (uint16_t)(cpu_rd(0xFFFC) | (cpu_rd(0xFFFD) << 8));
    vec_irq   = (uint16_t)(cpu_rd(0xFFFE) | (cpu_rd(0xFFFF) << 8));
    LOG_INFO("loop heads: MAINLN $%04X, diag loop $%04X; RESET $%04X, IRQ $%04X%s", pc_mainln, pc_diag, vec_reset, vec_irq,
             (pc_mainln == 0xC7AD && pc_diag == 0xDA8D) ? " (rev 3's)" : "");
    if (!pc_mainln) LOG_ERROR("MAINLN's loop head was not found in this ROM set: steps will run on the 2 s cap");
    if (!pc_diag)   LOG_WARN("the self test's diag loop head was not found in this ROM set");
}

static void ram_dump(void)
{
    FILE *f = fopen(cli_dump_path, "wb");
    if (!f) { LOG_ERROR("--ram-dump: cannot write %s", cli_dump_path); return; }
    fwrite(g.ram, 1, sizeof g.ram, f);
    fwrite(g.vram, 1, sizeof g.vram, f);
    fclose(f);
    LOG_INFO("--ram-dump: RAM $0000-$07FF + vector RAM $2000-$2FFF after step %lu (loop head %lu of the run) -> %s",
             steps_done, steps_done + 1, cli_dump_path);
    cli_dump_on = 0;
}

/* ------------------------------------------------------------------ */
/* the app hooks (platform\tempest_platform.h)                         */
/* ------------------------------------------------------------------ */

extern "C" {

int tempest_app_picture_rows(const tempest_pic_row **rows) { *rows = pic_rows; return n_pic_rows; }

void tempest_app_set_vg_window(int mode)
{
    vg_window = mode == TP_VGW_FREE ? TP_VGW_FREE : TP_VGW_CYCLES;
}

/* irq_hz <= 0 = the board's own 246.09 Hz.  Underclocks the whole board
 * (app_loop.c): the cycle -> ms conversion is scaled and the POKEYs' audio
 * clock divided, as if the crystal were slower. */
void tempest_app_set_fps_lock(double irq_hz)
{
    mach_scale = irq_hz > 0.0 ? TP_IRQ_HZ / irq_hz : 1.0;
    pokey_audio_clock_hz = (uint32_t)(TP_POKEY_HZ / mach_scale + 0.5);
    if (pokey_audio_clock_hz == 0) pokey_audio_clock_hz = 1;
    clk_valid = 0;
}

double tempest_app_idle_ms(void) { return idle_remain_ms; }

void tempest_app_set_watchdog_cycles(double cycles)
{
    wd_timeout_cyc = cycles >= 2.0 * TP_IRQ_CYCLES ? (uint64_t)cycles : TP_WD_CYCLES;
}

/* The last pass, MEASURED (the C port reports its pass-cost model here): its
 * hardware accesses outside the IRQ, the AVG ops of the list it left, and the
 * mainline's own cycles outside the frame wait and the IRQ handler. */
void tempest_app_pass_info(unsigned long *io_r, unsigned long *io_w, uint32_t *ops, double *work)
{
    *io_r = lp_io_r; *io_w = lp_io_w; *ops = lp_ops; *work = lp_work;
}

void tempest_app_get_stats(tempest_app_stats *s)
{
    s->machine_cycles = mach_cyc;
    s->diag = g.cpu_loop == LOOP_DIAG;
    s->diag_passes = n_diag_passes;
    s->pictures = n_pictures;
    s->picture_cycles = pic_cyc_sum;
    memcpy(s->picture_irqs, pic_irqs_hist, sizeof s->picture_irqs);
    s->picture_draw_ms[0] = pic_draw_ms[0]; s->picture_draw_ms[1] = pic_draw_ms[1];
    s->boots = n_boots;
    s->selftest_boots = n_diag_boots;
    s->watchdog_bites = n_wd_bites;
    s->jmp_resets = n_jmp_resets;
    s->soft_watchdog_trips = n_soft_trips;
}

/* Power on: the ROMs, the chips, the EAROM image, then RESET up to the first
 * loop head on the synthetic clock.  A missing zip or ROM is fatal: logged,
 * shown (unless the run is windowless) and the process exits 3. */
void tempest_app_init(void)
{
    char *ini_roms = get_config_string("main", "roms", "");
    const char *roms = cli_roms[0] ? cli_roms : (ini_roms ? ini_roms : "");
    set_config_string("main", "roms", ini_roms ? ini_roms : "");     /* the key shows up in the ini, as the backend's do */
    LOG_INFO("tempest_emu: the real ROM on AAE's cpu_6502 core; ROM zip: %s",
             roms[0] ? roms : "(default: roms\\tempest.zip beside the exe, then ..\\roms\\tempest.zip)");
    if (emu_roms_load_default(roms, cli_quiet) != 0) {
        free(ini_roms);
        plat_shutdown();
        exit(3);
    }
    free(ini_roms);

    memset(&g, 0, sizeof g);
    memset(&cur_in, 0, sizeof cur_in);
    mach_cyc = pokey_cyc = 0;
    last_kick_cyc = 0;
    next_irq = next_tick = TP_IRQ_CYCLES;
    irq_line = 0;
    spin_pos = 0; spin_pending = 0;
    vg_running = 0; vg_ends_in_halt = 0; vg_walk_valid = 0;
    vg_pic_on = 0; vg_pic_cyc = 0; vg_pic_blank = 0;
    out0_latch = 0; outank_latch = 0;
    clk_valid = 0;
    find_loop_heads();
    mb_reset(&mb);
    pokey_init();
    earom_load();
    audio_live = plat_audio_open(TP_AUDIO_RATE) == 0;

    /* addrmask $FFFF, CPU 0, NMOS.  mame_memory_handling: an address outside
     * the tables would read 0 instead of cpu_mem (none is: see the catch-alls). */
    cpu = new cpu_6502(cpu_mem, emu_read, emu_write, 0xFFFF, 0, CPU_NMOS_6502);
    cpu->mame_memory_handling(true);
    cpu->set_cycle_callback(cycle_tick, NULL);

    booting = 1;
    pull_reset(RST_POWERON);                    /* samples the inputs: TEST as it is at power-on */
    cur_in.spin_delta = 0; spin_pending = 0;
    if (!run_to_loop_head(EMU_BOOT_CAP_CYC)) {
        LOG_ERROR("power-on: no loop head within %u cycles (PC $%04X) - continuing on the step cap",
                  (unsigned)EMU_BOOT_CAP_CYC, cpu->get_pc());
        boot_done();
    }
    LOG_INFO("power-on: %s loop head after %llu cycles (%.2f s of machine time, unpaced), %lu IRQs serviced",
             g.cpu_loop == LOOP_DIAG ? "diag" : "MAINLN", (unsigned long long)mach_cyc, (double)mach_cyc / TP_CPU_HZ,
             (unsigned long)g.irq_count);
    if (cli_dump_on && cli_dump_pass == 0) ram_dump();
}

/* One pass of whichever loop the CPU is in, loop head to loop head. */
double tempest_app_step(double now_ms)
{
    (void)now_ms;                               /* the seam reads the clock itself */
    plat_input_poll(&cur_in);
    spin_pending += cur_in.spin_delta;
    pass_io_r = pass_io_w = 0;
    pass_work = pass_handler = pass_wait = 0;
    if (!run_to_loop_head(EMU_STEP_CAP_CYC)) {
        if (n_step_caps++ == 0)
            LOG_WARN("step %lu: no loop head for 2 s of machine time (PC $%04X); further ones are only counted",
                     steps_done + 1, cpu->get_pc());
    }
    steps_done++;
    lp_io_r = pass_io_r; lp_io_w = pass_io_w; lp_work = (double)pass_work;
    lp_ops = g.cpu_loop == LOOP_MAINLN ? avg_run_frame(NULL).ops : 0u;
    if (cli_dump_on && steps_done == cli_dump_pass) ram_dump();
    return 0.0;
}

void tempest_app_exit(void)
{
    if (earom.dirty && plat_nvram_write(earom.rom, sizeof earom.rom) == 0)
        earom.dirty = false;
    if (audio_live) plat_audio_close();
    audio_live = 0;

    LOG_INFO("emu: ROM set '%s'; %llu cycles (%.2f s; %.2f s of it in boots on the synthetic clock); IRQs raised %lu, "
             "serviced %lu, lost %lu", emu_roms_set_name(), (unsigned long long)mach_cyc, (double)mach_cyc / TP_CPU_HZ,
             (double)boot_synthetic_cyc / TP_CPU_HZ, n_irq_raised, (unsigned long)g.irq_count, n_irq_lost);
    LOG_INFO("emu: watchdog kicks %lu, max %llu cycles between kicks; bites %lu (not at WDGTST: %lu, first at $%04X); "
             "RESET arrivals %lu (self test %lu), JMP RESET %lu, soft-watchdog trips %lu, step caps %lu",
             n_wdog, (unsigned long long)max_kick_gap_cyc, n_wd_bites, n_wd_bites_bad, wd_bad_pc,
             n_boots, n_diag_boots, n_jmp_resets, n_soft_trips, n_step_caps);
    LOG_INFO("emu: protection cells QT5 ($%04X) = $%02X, QT4 ($%04X) = $%02X (both 0 = the POKEY RANDOM checks pass); "
             "Mathbox starts %lu, runaway %lu; VGSTART %lu, VGSTOP %lu, HALT walks %lu, broken-list pictures %lu, "
             "diag frames %lu", A_QT5, g.ram[A_QT5], A_QT4, g.ram[A_QT4], (unsigned long)mb.starts,
             (unsigned long)mb.runaway, n_vggo, n_vgstop, n_vg_walks, n_walk_bad, n_diag_frames);
    LOG_INFO("emu: audio underrun frames %lu, POKEY queue overruns %llu / %llu; unmapped reads %lu, writes %lu "
             "(+ %lu indexed-store dummy reads of EADAL / COLPORT)",
             audio_underrun_frames, (unsigned long long)ad_pokey_audio_overruns(&pokey[0]),
             (unsigned long long)ad_pokey_audio_overruns(&pokey[1]), n_unmapped_r, n_unmapped_w, n_dummy_reads);
    for (unsigned long i = 0; i < n_unmapped_r && i < 8; i++) LOG_INFO("emu:   unmapped read  $%04X", first_unmapped_r[i]);
    for (unsigned long i = 0; i < n_unmapped_w && i < 8; i++) LOG_INFO("emu:   unmapped write $%04X", first_unmapped_w[i]);
    if (n_diag_passes) {
        /* refrun.c's self-test line: every cell 0 = no bad RAM / ROM / POKEY / Mathbox / EAROM */
        char s[64];
        int bad = 0, o = 0;
        for (int i = 0; i < K_NROMS; i++) o += snprintf(s + o, sizeof s - (size_t)o, " %02X", diag_cells[K_CHKSMS - K_MBCOND + i]);
        for (size_t i = 0; i < sizeof diag_cells; i++) bad |= diag_cells[i];
        LOG_INFO("emu: self test (RAM at the last of %lu diag passes): CHKSMS%s  MBCOND %02X RAMCND %02X PK1CND %02X "
                 "PK2CND %02X EARCND %02X -> %s", n_diag_passes, s, diag_cells[0], diag_cells[1], diag_cells[2],
                 diag_cells[3], diag_cells[4], bad ? "SOMETHING REPORTED BAD" : "all good");
    }
    delete cpu;
    cpu = NULL;
}

} /* extern "C" */
