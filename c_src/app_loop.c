/* app_loop.c - Tempest C port: the game seam and the application loop (M8).
 *
 * Linked ONLY into the game binaries (M8 part 2's tempest_win.exe) and the
 * headless ones (tests\tempest_selftest.exe, tests\skeleton.exe).  The
 * verification harnesses have their own hw.h seams (tests\lockstep.c replays
 * the ROM's I/O, tests\gate.c a recorded trace); never link them with this.
 *
 * What lives here (modelled on the Space Duel port's app_loop.c):
 *  (a) every hw.h function over the platform contract
 *      (platform/tempest_platform.h): IN1 / INOP0 / INOP1 bit encodings, two
 *      real POKEYs (c012294.c: sound, RANDOM, the ALLPOT/ALLPO2 pot lines that
 *      carry the spinner, buttons and option bits), the Mathbox (mathbox.c),
 *      the ER2055 EAROM (er2055.c) and its NVRAM image, colour RAM, OUT0 /
 *      OUTANK, the AVG run/HALT state, the watchdog, the software watchdog;
 *  (b) machine time: ONE thread, a cycle timeline at 1.512 MHz; the IRQ
 *      ($D704, 6144 cycles = 246.09 Hz, PLAN.md decision 3) is serviced only
 *      inside MAINLN's frame wait (hw_wait_frame) and at RESET's CLI, when
 *      the clock (real, or synthetic for boot and the self-test) says it is
 *      due; per IRQ the POKEYs are clocked and one tick of audio is pushed;
 *  (c) the frame boundary: at every frame wait the display list DISPLAY just
 *      built is walked with avg.c and handed to plat_video_*;
 *  (c2) M9 B5, the self test: hw_reset / hw_watchdog_hang (CPU restarts),
 *      the TEST / diag switches, the diag loop's own time (no IRQ: machine
 *      time from IN1's 3 kHz busy waits + a per-screen work charge, audio and
 *      frames from machine time) - NOTES_m9.md "B5";
 *  (d) tempest_app_init / tempest_app_step / tempest_app_exit;
 *  (e) TEMPEST_SELFTEST_MAIN: the headless self-test (tests\tempest_selftest.exe)
 *      and TEMPEST_SKELETON_MAIN: tests\skeleton.exe.
 *
 * See NOTES_m8.md for the design, the timing model and the known gaps.
 */
#include <math.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* chip models first (their structs have register-named fields) */
#include "c012294.h"
#include "er2055.h"

#include "state.h"
#include "hw.h"
#include "game.h"
#include "mathbox.h"
#include "avg.h"
#include "platform/tempest_platform.h"

/* ------------------------------------------------------------------ */
/* constants                                                           */
/* ------------------------------------------------------------------ */

#define TP_CPU_HZ        1512000.0              /* 12.096 MHz / 8: CPU = POKEY clock */
#define TP_POKEY_HZ      1512000u
#define TP_IRQ_CYCLES    6144u                  /* 3 kHz / 12 = 246.09375 Hz          */
#define TP_IRQ_HZ        (TP_CPU_HZ / TP_IRQ_CYCLES)
#define TP_AUDIO_RATE    44100
#define TP_STALL_MS      100.0                  /* catch-up clamp (real clock)        */
#define TP_EAROM_SIZE    64u
#define TP_VG_DRAW_CYC   4000u                  /* refrun --vg-draw-cycles default    */
#define TP_IN1_IDLE      0x3Fu                  /* b0-b5 idle high (active low)        */
#define TP_IN1_SLAM      K_S_LMBIT              /* IN1 b3 slam                         */
#define TP_SP_IN_IRQ     0xF9u                  /* MAINLN's SP $FF - 3 (IRQ) - 3 (PHA) */
#define TP_WD_CYCLES     1134000u               /* hardware watchdog: refrun --watchdog-cycles default */
#define TP_DIAG_PRESENT_CYC 25200u              /* diag loop: present at most 60 frames per machine second */
#define TP_SFTEST_CYC    4588221u               /* RESET arrival -> diag loop head, oracle (selftest_boot frame 1) */

/* Seam access costs, in CPU (= POKEY) cycles, charged AFTER the access.
 * The translated code has no cycle annotations, so every hardware access
 * stands in for the instruction that does it plus a little of what follows:
 *  - a read costs 4 (LDA abs).  RANDOM spacing is what the ROM's protection
 *    checks: LDRDSP's $AE1F pair (LAE20 LDA RANDOM / LAE23 LDY RANDOM, and
 *    LAE30/LAE33 on POKEY 2) needs exactly 4 clocks between the two reads of
 *    a chip, or QT5 goes non-zero;
 *  - a write costs 8 (STA abs,X + the loop step of the ROM's register loops).
 *    INISOU writes SKCTL = 0 to both chips and reads RANDOM 14 / 10 cycles
 *    later; the chip needs 9 clocks (8 zero shifts + the SKCTL clock lag) to
 *    read the held $FF, else QT4 is set.  With 8/write + 4/read the gaps are
 *    16 / 12;
 *  - the watchdog strobe costs 13: RESET's delay loop (STA WTCHDG 4, DEC abs
 *    6, BNE 3) is 65536 of them, ~0.56 s of machine time with IRQs masked.
 * tests\tempest_selftest.exe's POKEY probe proves QT4 = QT5 = 0 over many
 * start phases with these costs, and that other costs break them. */
static uint32_t cost_read  = 4u;
static uint32_t cost_write = 8u;
static uint32_t cost_wdog  = 13u;

/* ------------------------------------------------------------------ */
/* machine state owned by the seam                                     */
/* ------------------------------------------------------------------ */

static plat_inputs cur_in;         /* sampled once per MAINLN pass */
static ad_pokey    pokey[2];       /* [0] POKEY 1 $60C0, [1] POKEY 2 $60D0 */
static ad_er2055   earom;
static mathbox     mb;
static uint8_t     out0_latch, outank_latch;

/* the cycle timeline: both POKEYs are clocked exactly to mach_cyc */
static uint64_t mach_cyc;
static uint64_t next_irq;          /* the cycle the next IRQ is due (a 6144 grid) */
static int      in_irq;

/* spinner: the 4-bit counter on POKEY 1's ALLPOT b0-b3 */
static int      spin_pos;
static int      spin_pending;

/* AVG run state (refrun.c's model, see vg_halted) */
static int      vg_running;
static uint64_t vg_start_cyc, vg_busy_until;
static int      vg_ends_in_halt;
static int      vg_cache_valid;    /* inside one IRQ: the list already walked, no HALT */

/* The picture (vg_picture): MAINLN's lists loop, so the AVG redraws the list
 * back to back and every traversal is one refresh of the monitor. */
static int      vg_pic_on;         /* a VGSTARTed list is being redrawn (MAINLN only) */
static uint64_t vg_pic_cyc;        /* the cycle its next traversal starts */
static int      vg_pic_blank;      /* the last picture presented had nothing lit */
static unsigned long n_pictures;   /* traversals of looping lists */
static uint64_t pic_cyc_sum;       /* their draw time, CPU cycles */
static uint32_t pic_cyc_min, pic_cyc_max;
static unsigned long pic_irqs_hist[2][10];  /* [in a game][IRQs per picture, 9 = 9+] */
static double   pic_draw_ms[2];

/* software watchdog; CPU restarts (M9 B5) */
static jmp_buf  wd_jmp;
static int      wd_armed;
static unsigned long wd_trips;
static uint64_t last_kick_cyc;             /* the last $5000 strobe (hardware watchdog model) */
static uint64_t wd_timeout_cyc = TP_WD_CYCLES;
static unsigned long n_wd_bites, n_jmp_resets, n_diag_boots;

/* the self test's diagnostic loop (M9 B5): no IRQ runs there */
static unsigned long n_diag_passes;
static uint64_t diag_timest_cyc;           /* this pass's TIMEST start (its VGSTOP) */
static uint8_t  diag_vgstart_qstate;       /* QSTATE when this pass started the AVG */
static uint64_t audio_next_cyc;            /* diag: the next machine-time audio tick */
static uint64_t diag_present_next;         /* diag: the next machine-time frame */
static unsigned long n_diag_frames, n_walk_halt;
static uint64_t reset_arrival_cyc;
static uint64_t last_boot_cyc, boot_synthetic_cyc;   /* RESET arrival -> first loop head, on the synthetic clock */

/* clocks */
static int      fast_clock;        /* 1 = synthetic 'now' (boot, headless) */
static int      synthetic_only;    /* headless builds: never the wall clock */
static double   fast_ms;
static int      clk_valid;
static double   clk_base_ms;
static uint64_t clk_base_cyc;

/* fps lock (M8 part 2, the Gravitar port's gr_app_set_fps_lock): underclock the
 * whole board.  mach_scale multiplies the cycle -> ms conversion of the IRQ
 * grid, and the POKEYs' audio clock is divided by it, exactly as if the
 * crystal were slower: game speed, timers, sound tempo and pitch slow
 * together.  Host policy only - the self-test never calls the setter. */
static double   mach_scale = 1.0;
static uint32_t pokey_audio_clock_hz = 1512000u;
static double   idle_remain_ms;    /* the wait machine_idle is giving up (tempest_app_idle_ms) */

/* statistics (read by the self-test) */
static unsigned long n_wdog, n_vg_walks, n_frames, n_walk_bad, n_walk_swhalt, n_boots;
static avg_stop last_stop;
static uint32_t last_nlit;
static unsigned long n_lit_total, n_lit_window;   /* lit segments; inside x +-290, y +-285 */
static int      bbox_valid;
static double   bbox_minx, bbox_miny, bbox_maxx, bbox_maxy;

/* ------------------------------------------------------------------ */
/* time passing                                                        */
/* ------------------------------------------------------------------ */

static void spend(uint32_t n)
{
    if (n == 0) return;
    mach_cyc += n;
    ad_pokey_advance(&pokey[0], n);
    ad_pokey_advance(&pokey[1], n);
}

/* Every hardware access: its cost, and (outside the IRQ) the pass's access
 * counts the pass-cost model reads; inside the IRQ the cycles it charges. */
static unsigned long pass_io_r, pass_io_w;
static uint64_t irq_seam_cyc;               /* cycles charged inside IRQs (stats) */

static void io_read(void)
{
    if (in_irq) irq_seam_cyc += cost_read; else pass_io_r++;
    spend(cost_read);
}

static void io_write(void)
{
    if (in_irq) irq_seam_cyc += cost_write; else pass_io_w++;
    spend(cost_write);
}

static void io_write_wdog(void)
{
    if (in_irq) irq_seam_cyc += cost_wdog; else pass_io_w++;
    spend(cost_wdog);
}

static double clock_ms(void) { return fast_clock ? fast_ms : plat_now_ms(); }

/* ------------------------------------------------------------------ */
/* inputs                                                              */
/* ------------------------------------------------------------------ */

/* IN1 b6 - VG HALT, exactly tests\refrun.c's model: after VGSTART the list
 * from $2000 is walked (lazily, at the read, over vector RAM as it is now);
 * a list that loops (Tempest's JMPL VECRAM) keeps running and reads 0; one
 * that reaches HALT reads halted TP_VG_DRAW_CYC cycles after its VGSTART;
 * VGSTOP halts at once.  Inside one IRQ nothing writes vector RAM between
 * its two IN1 reads, so a non-HALT walk is reused there. */
static int vg_halted(void)
{
    if (!vg_running) return 1;
    if (!vg_ends_in_halt) {
        avg_result r;
        if (in_irq && vg_cache_valid) return 0;
        r = avg_run_frame(NULL);
        n_vg_walks++;
        if (r.stop != AVG_STOP_HALT) {
            if (in_irq) vg_cache_valid = 1;
            return 0;
        }
        vg_ends_in_halt = 1;
        vg_busy_until = vg_start_cyc + TP_VG_DRAW_CYC;
    }
    return mach_cyc >= vg_busy_until;
}

/* IN1 $0C00: b0 right coin, b1 centre coin, b2 left coin, b3 slam, b4 the
 * self-test (TEST) switch, b5 the diagnostic step switch - all active LOW;
 * b6 VG HALT; b7 the 3 kHz clock = (cycle >> 8) & 1 (256-cycle half period,
 * 12 periods per IRQ - refrun.c's).  The switches are the inputs sampled at
 * the start of the pass (and before a RESET), so RESET ($D983), NONSTA, the
 * diag loop ($DAC0 / $DAF0) and SYSTEM see TEST exactly as the ROM reads it.
 * In the diag loop the b7 busy waits are what moves machine time: every read
 * costs cost_read cycles, so each wait ends on the clock edge it waits for
 * (~2,700 reads per pass here, ~1,550 on the 6502's 7-cycle BIT/branch). */
uint8_t hw_in1(void)
{
    uint8_t v = (uint8_t)TP_IN1_IDLE;
    if (cur_in.coin_r) v = (uint8_t)(v & ~K_MCOINR);
    if (cur_in.coin_c) v = (uint8_t)(v & ~K_MCOINC);
    if (cur_in.coin_l) v = (uint8_t)(v & ~K_MCOINL);
    if (cur_in.slam)   v = (uint8_t)(v & ~TP_IN1_SLAM);
    if (cur_in.test)   v = (uint8_t)(v & ~K_MTEST);
    if (cur_in.diag)   v = (uint8_t)(v & ~K_MDITES);
    if (vg_halted())             v |= K_MHALT;
    if ((mach_cyc >> 8) & 1u)    v |= K_M3KHTI;
    io_read();
    return v;
}

uint8_t hw_inop0(void) { uint8_t v = plat_dsw_n13(); io_read(); return v; }
uint8_t hw_inop1(void) { uint8_t v = plat_dsw_l12(); io_read(); return v; }

/* ALLPOT / ALLPO2.  Every ROM read follows a POTGO strobe of the same chip
 * (the IRQ's SWITCHES block, GETOP3), so c012294.c answers mid-scan with the
 * pins' comparator mask, which the POTGO write below sets:
 *   POKEY 1: b0-b3 the spinner counter, b4 cocktail, b5 option (DIPs);
 *   POKEY 2: b0-b2 option bits (DIPs), b3 zap, b4 fire, b5 start 1,
 *            b6 start 2 (buttons active high, as refrun's scenarios).
 * The IRQ reads ALLPOT ^ $0F and adds the sign-extended nibble difference
 * to TBHD, so the counter may move at most 7 between IRQs: spin counts are
 * fed at most +-7 per IRQ, the rest carried (irq_run). */
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

uint8_t hw_allpot(int chip)
{
    uint8_t v = ad_pokey_read(&pokey[chip & 1], R_ALLPOT);
    io_read();
    return v;
}

/* RANDOM / RANDO2: the chip's own shift chain on the machine timeline. */
uint8_t hw_random(int chip)
{
    uint8_t v = ad_pokey_read(&pokey[chip & 1], R_RANDOM);
    io_read();
    return v;
}

void hw_pokey_write(int chip, uint8_t reg, uint8_t val)
{
    ad_pokey *p = &pokey[chip & 1];
    reg = (uint8_t)(reg & 0x0Fu);
    if (reg == W_POTGO) ad_pokey_set_allpot(p, pot_pins(chip & 1));
    ad_pokey_write(p, reg, val);
    io_write();
}

/* ------------------------------------------------------------------ */
/* Mathbox, EAROM, outputs                                             */
/* ------------------------------------------------------------------ */

void    hw_mb_write(uint8_t offset, uint8_t val) { mb_write(&mb, (uint8_t)(offset & 0x1F), val); io_write(); }
uint8_t hw_mb_status(void) { uint8_t v = mb_status(&mb); io_read(); return v; }
uint8_t hw_mb_ylow(void)   { uint8_t v = mb_ylow(&mb);   io_read(); return v; }
uint8_t hw_mb_yhigh(void)  { uint8_t v = mb_yhigh(&mb);  io_read(); return v; }

/* EAROM: the ER2055 behind EADAL $6000-$603F / EACTL $6040 / EAIN $6050.
 * The image is the NVRAM blob; a blank part reads $FF (PLAN.md decision 4).
 * EAUPD deselects the chip (EACTL = 0) on every call, which is the flush
 * point once an erase/write changed a cell. */
void hw_earom_write(uint8_t offset, uint8_t val)
{
    ad_er2055_set_addr_data(&earom, (uint8_t)(offset & 0x3F), val);
    io_write();
}

void hw_earom_ctl(uint8_t val)
{
    ad_er2055_control(&earom, val);
    if (val == 0x00 && earom.dirty) {
        if (plat_nvram_write(earom.rom, sizeof earom.rom) == 0)
            earom.dirty = false;
    }
    io_write();
}

uint8_t hw_earom_read(void) { uint8_t v = ad_er2055_data(&earom); io_read(); return v; }

/* OUT0 $4000: b0-b2 coin counters, b3/b4 video invert X/Y (to the renderer
 * as plat_video_begin's flip).  OUTANK $60E0: start LEDs, flip. */
void hw_out0(uint8_t v)   { out0_latch = v;   io_write(); }
void hw_outank(uint8_t v) { outank_latch = v; io_write(); }

void hw_color(uint8_t idx, uint8_t v) { g.colram[idx & 15] = v; }

void hw_vgstart(void)
{
    vg_running = 1; vg_start_cyc = mach_cyc; vg_ends_in_halt = 0; vg_cache_valid = 0;
    if (g.cpu_loop == LOOP_DIAG) diag_vgstart_qstate = QSTATE;     /* the screen SSTATE just drew */
    else { vg_pic_on = 1; vg_pic_cyc = mach_cyc + AVG_VGGO_LEADIN / AVG_CYC_PER_CPU; }
    io_write();
}

void hw_vgstop(void)
{
    vg_running = 0; vg_cache_valid = 0; vg_pic_on = 0;
    if (g.cpu_loop == LOOP_DIAG) diag_timest_cyc = mach_cyc;       /* $DAA9 TIMEST: the wait is over */
    io_write();
}

/* $5000 strobe.  The hardware watchdog (M9 B5) is refrun's model: it bites
 * when no strobe came for wd_timeout_cyc cycles.  Only the diag loop's WDGTST
 * spin can starve it (hw_watchdog_hang). */
void hw_watchdog(void) { n_wdog++; last_kick_cyc = mach_cyc; io_write_wdog(); }

uint8_t hw_sp(void) { return TP_SP_IN_IRQ; }

/* $D713 BRK / $D714 JMP RESET: back to the app's setjmp, which re-boots. */
void hw_soft_watchdog(void)
{
    wd_trips++;
    if (wd_armed) longjmp(wd_jmp, 1);
}

/* M9 B5: hw.h's CPU restarts.  Both abandon the C call stack to run_guarded,
 * which re-samples the inputs (the RESET reads the switches as they are now)
 * and re-enters reset() - the 6502 at its RESET vector.  RAM, vector RAM,
 * colour RAM, the POKEYs, the Mathbox and the EAROM keep their state. */
static void restart_cpu(void)
{
    if (wd_armed) longjmp(wd_jmp, 2);
}

/* LD861 JMP RESET (DSPSYS option 0/1, TEST closed -> the power-on self test) */
void hw_reset(void)
{
    n_jmp_resets++;
    restart_cpu();
}

static void diag_advance_to(uint64_t target);

/* LDAF7 WDGTST: the CPU spins without a strobe until the hardware watchdog
 * pulls RESET: machine time advances to the bite (audio and the clock keep
 * running), then the reboot. */
void hw_watchdog_hang(void)
{
    diag_advance_to(last_kick_cyc + wd_timeout_cyc + 1u);
    n_wd_bites++;
    restart_cpu();
}

/* ------------------------------------------------------------------ */
/* POKEY audio: one IRQ tick of rendered sound                         */
/* ------------------------------------------------------------------ */

static int      audio_live;
static uint64_t audio_tick_phase;
static unsigned long audio_underrun_frames;
static unsigned long audio_ticks;           /* since the last clear */
static uint64_t audio_frames;               /* rendered since the last clear */
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
    audio_ticks = 0;
    audio_frames = 0;
}

static void render_push_audio_tick(void)
{
    int n, i;
    audio_tick_phase += (uint64_t)TP_AUDIO_RATE * TP_IRQ_CYCLES;
    n = (int)(audio_tick_phase / pokey_audio_clock_hz);
    audio_tick_phase %= pokey_audio_clock_hz;
    if (n > (int)(sizeof audio_buf[0] / sizeof audio_buf[0][0]))
        n = (int)(sizeof audio_buf[0] / sizeof audio_buf[0][0]);
    if (n <= 0) return;
    for (i = 0; i < 2; i++) {
        int got = ad_pokey_audio_read(&pokey[i], audio_buf[i], n);
        if (got < n) {
            audio_underrun_frames += (unsigned long)(n - got);
            memset(audio_buf[i] + got, 0, (size_t)(n - got) * sizeof audio_buf[i][0]);
        }
    }
    audio_ticks++;
    audio_frames += (uint64_t)n;
    /* drained either way; a boot on the synthetic clock pushes nothing live */
    if (!audio_live || (fast_clock && !synthetic_only)) return;
    for (i = 0; i < n; i++) {
        int v = (int)audio_buf[0][i] + (int)audio_buf[1][i];
        if (v > 32767) v = 32767; else if (v < -32768) v = -32768;
        audio_buf[0][i] = (int16_t)v;
    }
    plat_audio_push(audio_buf[0], n);
}

/* ------------------------------------------------------------------ */
/* the IRQ                                                             */
/* ------------------------------------------------------------------ */

/* Take the IRQ now (the timeline is already at or past its due cycle). */
static void irq_run(void)
{
    int step = spin_pending > 7 ? 7 : spin_pending < -7 ? -7 : spin_pending;
    spin_pending -= step;
    spin_pos = (spin_pos + step) & 0x0F;
    next_irq += TP_IRQ_CYCLES;
    in_irq = 1;
    vg_cache_valid = 0;
    irq();                                  /* $D704; may longjmp (software watchdog) */
    in_irq = 0;
    render_push_audio_tick();
}

/* The IRQ due at next_irq: clock the chips up to it (unless the CPU's own
 * work already passed it) and take it. */
static void irq_service(void)
{
    if (mach_cyc < next_irq) spend((uint32_t)(next_irq - mach_cyc));
    irq_run();
}

/* LD9A4 CLI.  RESET ran masked; the line has been pending since the first
 * grid point the delay loop passed.  One IRQ is taken now, the periods lost
 * while masked are dropped (the grid itself keeps running). */
void hw_cli(void)
{
    if (mach_cyc >= next_irq) {
        next_irq += (mach_cyc - next_irq) / TP_IRQ_CYCLES * TP_IRQ_CYCLES;
        irq_run();
    }
}

/* Give up the CPU until `remain` ms have passed (synthetic: just move 'now';
 * the 1e-6 bias keeps float round-off from leaving a wait a hair short). */
static void machine_idle(double remain)
{
    if (fast_clock) { fast_ms += (remain > 0.0 ? remain : 0.0) + 1e-6; return; }
    idle_remain_ms = remain;
    if (remain > 2.0) plat_sleep_ms(1);
}

static void vg_picture(void);

/* Service the next IRQ when the clock says it is due - and, on the way, every
 * picture the AVG starts before it (vg_picture), each at its own due time. */
static void machine_tick(void)
{
    for (;;) {
        double now = clock_ms(), due;
        int pic;
        uint64_t ev;
        if (!clk_valid) {                   /* (re)anchor the clock: the CPU is at mach_cyc now */
            clk_base_ms = now;
            clk_base_cyc = mach_cyc;
            clk_valid = 1;
            if (vg_pic_cyc < mach_cyc) vg_pic_cyc = mach_cyc;   /* no pictures owed from before */
        }
        pic = vg_pic_on && g.cpu_loop == LOOP_MAINLN && vg_pic_cyc <= next_irq;
        ev = pic ? vg_pic_cyc : next_irq;
        due = clk_base_ms + ((double)ev - (double)clk_base_cyc) * 1000.0 / TP_CPU_HZ * mach_scale;
        if (now - due > TP_STALL_MS) {      /* debugger / dragged window: do not dump IRQs */
            clk_base_ms = now;
            clk_base_cyc = ev;
            due = now;
        }
        if (now >= due) {
            if (pic) { vg_picture(); continue; }
            irq_service();
            return;
        }
        machine_idle(due - now);
    }
}

/* ------------------------------------------------------------------ */
/* the frame boundary                                                  */
/* ------------------------------------------------------------------ */

/* the pass boundary's statistics */
static void count_seg(void *ctx, const avg_seg *s)
{
    (void)ctx;
    if (s->intensity == 0) return;          /* a dark move: the beam is blanked */
    last_nlit++;
    n_lit_total++;
    if (s->x0 >= -290 * 32768 && s->x0 <= 290 * 32768 && s->x1 >= -290 * 32768 && s->x1 <= 290 * 32768 &&
        s->y0 >= -285 * 32768 && s->y0 <= 285 * 32768 && s->y1 >= -285 * 32768 && s->y1 <= 285 * 32768)
        n_lit_window++;
}

/* to the renderer */
static void line_seg(void *ctx, const avg_seg *s)
{
    (void)ctx;
    if (s->intensity == 0) return;
    plat_video_line((float)AVG_Q15_TO_F(s->x0), (float)AVG_Q15_TO_F(s->y0),
                    (float)AVG_Q15_TO_F(s->x1), (float)AVG_Q15_TO_F(s->y1),
                    s->rgb, (int)s->intensity);
}

static void emit_seg(void *ctx, const avg_seg *s)
{
    count_seg(ctx, s);
    line_seg(ctx, s);
}

/* ---- the picture -----------------------------------------------------------
 * MAINLN's pass rate is NOT the monitor's refresh rate.  The master lists end
 * in JMPL VECRAM, so once VGSTARTed the AVG draws the list again and again
 * with nothing in between, and the IRQ's VGSTOP / VGSTART ($D7C9) only fires
 * when it finds the AVG halted (the SWHALT list).  One traversal = one
 * refresh, and it lasts what the list costs the AVG (avg.h TIMING): the
 * state machine's ticks plus every vector's timer.  On the oracle's dumps
 * that is 16.4 ms on average in play (61 Hz), 24-25 ms in the heaviest
 * scenes of fuseball_pulsar (40 Hz), against a game pass every 36.6 ms or
 * more (FRTIMR >= 9): the same list is redrawn two or three times per pass.
 *
 * So the picture is an event of its own on the machine timeline: at
 * vg_pic_cyc the list is walked as vector RAM stands, presented, and the
 * next traversal is due its draw time later.  machine_tick serves these
 * between the IRQs, each at its own wall-clock time.  (The pass's C code
 * runs in no time at the loop head, so a picture inside the pass's charged
 * CPU time already shows the list the pass built - earlier than the board
 * by at most that CPU time, never later.)
 *
 * A list that HALTs is drawn once and stays dark until the IRQ restarts it;
 * a blank one is presented once, not at the IRQ rate.  Headless builds keep
 * frame_boundary's per-pass frame (the self-test's instrument) and only
 * count the pictures.
 *
 * THE PICTURE PERIOD.  Counted, not approximated: avg_walk adds up the AVG's
 * own cycles for the list - the state PROM's ticks per instruction plus every
 * vector's and CNTR's timer, at the 12.096 MHz master clock (avg.h TIMING;
 * tools\avg_prom_sim.py runs MAME's state machine on the real PROM and gets
 * the same number on every list).  cycles / 8 = CPU cycles = the picture's
 * time on this timeline.
 *   TP_VGW_CYCLES (default) that time, but never less than 4 IRQs: 61.52 Hz is
 *                 the most there is, and a list that costs more than 16.25 ms
 *                 takes what it costs.
 *   TP_VGW_FREE   that time with no floor (the ROM's looping list taken
 *                 literally: a near-empty screen redraws at 130 Hz). */
#define TP_PIC_MIN_CYC  1024u       /* host guard: no list is this short (the emptiest dump: 4.7 ms) */
#define TP_PIC_MIN_IRQS 4u

static int    vg_window = TP_VGW_CYCLES;
static tempest_pic_row pic_rows[TP_PIC_ROWS];
static int    n_pic_rows;

int tempest_app_picture_rows(const tempest_pic_row **rows) { *rows = pic_rows; return n_pic_rows; }

void tempest_app_set_vg_window(int mode)
{
    vg_window = mode == TP_VGW_FREE ? TP_VGW_FREE : TP_VGW_CYCLES;
}

static void vg_picture(void)
{
    avg_cfg cfg;
    avg_result r = avg_run_frame(NULL);
    uint32_t d = (r.cycles + AVG_CYC_PER_CPU - 1u) / AVG_CYC_PER_CPU;     /* the list's draw time, CPU cycles */
    uint32_t floor_cyc = vg_window == TP_VGW_FREE ? TP_PIC_MIN_CYC : TP_PIC_MIN_IRQS * TP_IRQ_CYCLES;
    if (r.stop == AVG_STOP_LOOP) {
        int game = (QSTATUS & K_MATRACT) != 0;
        double ms = avg_cycles_ms(r.cycles);
        uint32_t n = (d + TP_IRQ_CYCLES - 1u) / TP_IRQ_CYCLES;          /* in IRQs, for the statistics */
        if (n < TP_PIC_MIN_IRQS) n = TP_PIC_MIN_IRQS;
        pic_irqs_hist[game][n > 9u ? 9u : n]++;
        pic_draw_ms[game] += ms;
        if (game) {                         /* per (QSTATE, wave), for the exit log */
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
        if (pic_cyc_min == 0 || d < pic_cyc_min) pic_cyc_min = d;
        if (d > pic_cyc_max) pic_cyc_max = d;
        vg_pic_cyc += d;
    } else if (r.stop == AVG_STOP_HALT) {
        vg_pic_on = 0;                      /* until the next VGSTART */
    } else {
        vg_pic_cyc += d > 4u * TP_IRQ_CYCLES ? d : 4u * TP_IRQ_CYCLES;   /* a broken list (frame_boundary reports it) */
    }
    if (synthetic_only) return;
    if (r.nlit == 0 && vg_pic_blank) return;
    vg_pic_blank = r.nlit == 0;
    memset(&cfg, 0, sizeof cfg);
    cfg.seg = line_seg;
    plat_video_begin((uint8_t)(out0_latch & (K_MVINVX | K_MVINVY)));
    avg_run_frame(&cfg);
    plat_video_present();
}

/* DISPLAY has just finished the list the AVG runs from $2000: the pass
 * boundary.  The walk gives the pass-cost model its AVG ops and the
 * statistics their numbers; headless builds also take it as the frame. */
static uint32_t frame_boundary(void)
{
    avg_cfg cfg;
    avg_result r;
    memset(&cfg, 0, sizeof cfg);
    cfg.seg = emit_seg;
    last_nlit = 0;
    if (synthetic_only || g.cpu_loop != LOOP_MAINLN) {
        plat_video_begin((uint8_t)(out0_latch & (K_MVINVX | K_MVINVY)));
        r = avg_run_frame(&cfg);
        plat_video_present();
    } else {
        cfg.seg = count_seg;                /* live: the pictures are vg_picture's */
        r = avg_run_frame(&cfg);
    }
    n_frames++;
    last_stop = r.stop;
    /* LOOP = the master list's JMPL VECRAM.  HALT at $3DCC is the ROM's own
     * SWHALT master list (a blanked screen, e.g. between attract screens);
     * anything else (budget, bad address, RTSL) is a broken list. */
    if (r.stop == AVG_STOP_HALT && r.stop_pc == 0x3DCC) n_walk_swhalt++;
    else if (r.stop == AVG_STOP_HALT && g.cpu_loop == LOOP_DIAG) n_walk_halt++;  /* the diag loop's VGHALT list */
    else if (r.stop != AVG_STOP_LOOP) {
        if (n_walk_bad < 8)
            fprintf(stderr, "frame %lu (pass %u): AVG walk ended %s at $%04X after %u ops\n",
                    n_frames, (unsigned)g.pass_count, avg_stop_name(r.stop), (unsigned)r.stop_pc, (unsigned)r.ops);
        n_walk_bad++;
    }
    if (r.have_bbox) {
        double x0 = AVG_Q15_TO_F(r.minx), y0 = AVG_Q15_TO_F(r.miny);
        double x1 = AVG_Q15_TO_F(r.maxx), y1 = AVG_Q15_TO_F(r.maxy);
        if (!bbox_valid || x0 < bbox_minx) bbox_minx = x0;
        if (!bbox_valid || y0 < bbox_miny) bbox_miny = y0;
        if (!bbox_valid || x1 > bbox_maxx) bbox_maxx = x1;
        if (!bbox_valid || y1 > bbox_maxy) bbox_maxy = y1;
        bbox_valid = 1;
    }
    return r.ops;
}

/* ---- the pass-cost model: the 6502's own time ----------------------------
 * A translated pass costs microseconds, but on the board MAINLN's work
 * (EXSTAT, NONSTA, DISPLAY and the Mathbox traffic) is real CPU time, and a
 * heavy pass runs past the 9-IRQ frame gate: the oracle takes 10-12 IRQs on
 * 13% of attract passes (9.214 IRQs per pass over the oracle's 5000).
 * Without a model every native pass would take exactly 9 (~2.4% fast in
 * attract, ~1% in play).
 *
 * THE COSTS ARE MEASURED.  tests\passcost.exe (tests\refrun.c /DPASSCOST, the
 * oracle unchanged) prints per pass the mainline's cycles outside the frame
 * wait and the IRQ handler ("work"), the handler's cycles, the pass's
 * hardware reads / writes outside the IRQ and the AVG ops of the list it
 * left.  Least squares per (QSTATE, QDSTATE) at the pass start, work = a +
 * b_r * reads + b_w * writes + b_ops * ops, over attract 5000 + coin_start,
 * high_score, superzapper, fuseball_pulsar 3000 passes each (16,995 passes;
 * states with >= 30 passes, the rest take the global fit).  The native
 * access counts are the same events (lockstep verifies the I/O sequence).
 * M8 part 2 refit: + two_player 3000 (19,994 passes).
 *
 * At the frame wait the work is charged as machine time: W cycles of
 * mainline need W * 6144 / (6144 - h') cycles of the grid, h' = the
 * handler's cycles per IRQ not already charged by the native IRQ's own
 * accesses (TP_IRQ_SEAM_CYC), less what this pass's accesses already
 * charged.  IRQs whose grid point that passes are serviced in the wait
 * (pass_charge, hw_wait_frame).  Fractional cycles carry (pass_owed).
 *
 * SPREAD AND CALIBRATION (M8 part 2).  IRQs per pass is a convex function
 * of the work (nothing below the 9-IRQ gate counts), so a fit that is right
 * on average under-charges: its prediction lacks the pass-to-pass scatter
 * that pushes some real passes over a gate.  So the state's residual sd
 * (resid_sd) times a fixed 16-step sequence of standard-normal quantiles
 * (tp_z16, bit-reversed order: deterministic, zero mean) is added to the
 * fitted work, and tools\fit_passcost.py then calibrates `a` per state
 * (bisection) until replaying every recorded pass through this timeline
 * gives the oracle's IRQs per pass for that state.  Replay per state vs the
 * oracle, before -> after: PLAY 9.060 -> 9.086 (oracle 9.086), NEWV2
 * 10.694 -> 10.710 (10.710), DROP 9.662 -> 9.599 (9.601), $1E 9.375 ->
 * 9.344 (9.375, 32 passes); every PAUSE / GETINI / REQRAT / BOOM state is
 * 9.000 in the oracle and in the model.  Per file: attract 9.242 (9.214),
 * coin_start 9.114 (9.114), high_score 9.124 (9.103), superzapper 9.068
 * (9.090), fuseball_pulsar 9.034 (9.095), two_player 9.028 (9.018). */
typedef struct {
    uint8_t qstate, qdstate;
    double  a, b_r, b_w, b_ops;     /* work cycles */
    double  handler;                /* ROM IRQ handler cycles per IRQ */
    double  resid_sd;               /* the fit's residual sd, cycles */
} pass_fit;

static const pass_fit pass_fits[] = {
    /* QSTATE QDSTATE      a      b_r      b_w   b_ops  handler resid_sd  (tests\passcost.exe + fit_passcost.py, 2026-09-16) */
    { 0x04, 0x00,  18320.6,  325.48, -187.32,  11.27,  892.7,  2943.0 },  /* PLAY, CDPLAY    10652 passes */
    { 0x0A, 0x00,   7254.0,  211.98,   47.65,  15.62,  808.7,  1040.6 },  /* PAUSE, CDPLAY     935 */
    { 0x0A, 0x0A,  -4633.2, 1622.91,   30.98,  15.26,  815.9,   171.7 },  /* PAUSE, CDHITB   3378 */
    { 0x0A, 0x0C, -23188.3,    0.00,   39.45,  47.00,  801.6,     1.7 },  /* PAUSE, $0C        40 */
    { 0x0A, 0x0E,    468.3,    0.00,   39.28,  14.64,  802.3,   123.1 },  /* PAUSE, $0E       320 */
    { 0x0A, 0x10,   6292.1,    0.00,   34.68,   5.47,  807.4,    73.7 },  /* PAUSE, CDPRST    126 */
    { 0x0A, 0x12,   -856.9,    0.00,   36.50,  23.63,  816.2,   104.0 },  /* PAUSE, CDBOXP    190 */
    { 0x0A, 0x14,   1523.2,    0.00,   44.99,   3.74,  816.0,   174.6 },  /* PAUSE, CDLOGP   1788 */
    { 0x0A, 0x16,  -2014.3,    0.00,   44.00,  16.28,  815.7,   165.0 },  /* PAUSE, $16        40 */
    { 0x12, 0x06,  23859.5,    0.00,   72.98,   0.23,  801.6,    28.7 },  /* GETINI, CDGETI   117 */
    { 0x16, 0x08,  32369.1,    0.00,   51.83,   5.89,  802.6,   582.2 },  /* REQRAT, CDREQRA  926 */
    { 0x18, 0x00,  26485.3,  297.42, -244.70,  19.46,  831.2,   371.7 },  /* NEWV2, CDPLAY    631 */
    { 0x1E, 0x00,  18684.8,  637.12, -530.56,  10.67,  815.3,  1659.4 },  /* $1E, CDPLAY       32 */
    { 0x20, 0x00,  14622.5,  188.20,  -93.85,  13.01,  898.4,  2221.6 },  /* DROP, CDPLAY     509 */
    { 0x24, 0x04,   -672.2,   22.56,  196.08,  -0.59,  834.1,   319.7 },  /* BOOM, CDBOOM     180 */
};
static const pass_fit pass_fit_global = { 0, 0, 10057.7, 129.40, -2.40, 11.16, 858.3, 6774.5 };

/* standard-normal quantiles at (i + 0.5) / 16, bit-reversed order */
static const double tp_z16[16] = {
    -1.8627, 0.0784, -0.5791, 0.7764, -1.0100, 0.4023, -0.2372, 1.3180,
    -1.3180, 0.2372, -0.4023, 1.0100, -0.7764, 0.5791, -0.0784, 1.8627
};
static unsigned tp_z_index;
#define TP_IRQ_SEAM_CYC  61.0       /* native IRQ access charge per IRQ (measured by the self-test) */
#define TP_PASS_WORK_MAX 120000.0   /* clamp: a fit extrapolated far outside its data */

static int      pass_cost_on = 1;
static int      pass_armed;             /* a MAINLN pass (not the boot) is running */
static uint8_t  pass_qstate, pass_qdstate;
static uint64_t pass_start_cyc;
static double   pass_owed;              /* fractional cycles carried */

static void pass_begin(void)
{
    pass_armed = 1;
    pass_qstate = QSTATE;
    pass_qdstate = QDSTATE;
    pass_start_cyc = mach_cyc;
    pass_io_r = pass_io_w = 0;
}

/* the last charged pass, for a backend's pass log (tempest_app_pass_info) */
static unsigned long lp_io_r, lp_io_w;
static uint32_t lp_ops;
static double   lp_work;

void tempest_app_pass_info(unsigned long *io_r, unsigned long *io_w, uint32_t *ops, double *work)
{
    *io_r = lp_io_r; *io_w = lp_io_w; *ops = lp_ops; *work = lp_work;
}

static void pass_charge(uint32_t ops)
{
    const pass_fit *f = &pass_fit_global;
    double w, h, x;
    size_t i;
    if (!pass_armed) return;
    pass_armed = 0;
    lp_io_r = pass_io_r; lp_io_w = pass_io_w; lp_ops = ops; lp_work = 0.0;
    if (!pass_cost_on) return;
    for (i = 0; i < sizeof pass_fits / sizeof pass_fits[0]; i++)
        if (pass_fits[i].qstate == pass_qstate && pass_fits[i].qdstate == pass_qdstate) { f = &pass_fits[i]; break; }
    w = f->a + f->b_r * (double)pass_io_r + f->b_w * (double)pass_io_w + f->b_ops * (double)ops
        + f->resid_sd * tp_z16[tp_z_index++ & 15u];
    lp_work = w;
    if (w < 0.0) w = 0.0;
    if (w > TP_PASS_WORK_MAX) w = TP_PASS_WORK_MAX;
    h = f->handler - TP_IRQ_SEAM_CYC;
    if (h < 0.0) h = 0.0;
    x = w * TP_IRQ_CYCLES / (TP_IRQ_CYCLES - h) - (double)(mach_cyc - pass_start_cyc) + pass_owed;
    if (x <= 0.0) { pass_owed = 0.0; return; }
    pass_owed = x - (double)(uint32_t)x;
    {
        /* in IRQ-period chunks, servicing the IRQs the CPU time passes (on the
         * board they interrupt the pass): the chips' audio queues are drained
         * once per IRQ and would overflow on one long advance */
        uint32_t left = (uint32_t)x;
        while (left > 0) {
            uint32_t step = left < TP_IRQ_CYCLES ? left : TP_IRQ_CYCLES;
            spend(step);
            left -= step;
            while (next_irq <= mach_cyc) machine_tick();
        }
    }
}

/* ---- the diag loop's time (M9 B5) -----------------------------------------
 * The self test runs with the IRQ masked, so nothing above (the IRQ grid,
 * per-IRQ audio, MAINLN's frame wait) happens there.  Instead:
 *  - machine time is what the diag loop's own busy waits on IN1 b7 spend
 *    (hw_in1's read cost), plus the pass's CPU work (diag_charge);
 *  - audio: one tick (the IRQ's 6144 cycles of POKEY samples) whenever
 *    machine time passes the next tick point (diag_audio);
 *  - the frame: the list the pass started ($2000, ending in VGHALT's HALT) is
 *    presented at the loop head, at most TP_DIAG_PRESENT_CYC apart (60 Hz of
 *    machine time; the board redraws it every pass, ~90-140 Hz);
 *  - pacing: at the loop head the thread idles until the clock reaches
 *    mach_cyc (machine_wait_cyc), with the same anchor, stall clamp and
 *    fps_lock scale as the IRQ grid. */
static void machine_wait_cyc(uint64_t cyc)
{
    for (;;) {
        double now = clock_ms(), due;
        if (!clk_valid) {
            clk_base_ms = now;
            clk_base_cyc = cyc;
            clk_valid = 1;
        }
        due = clk_base_ms + ((double)cyc - (double)clk_base_cyc) * 1000.0 / TP_CPU_HZ * mach_scale;
        if (now - due > TP_STALL_MS) {
            clk_base_ms = now;
            clk_base_cyc = cyc;
            due = now;
        }
        if (now >= due) return;
        machine_idle(due - now);
    }
}

static void diag_audio(void)
{
    while (mach_cyc >= audio_next_cyc) {
        render_push_audio_tick();
        audio_next_cyc += TP_IRQ_CYCLES;
    }
}

/* Machine time moves on with the CPU doing nothing the seam sees (the WDGTST
 * spin, a pass's work): in IRQ-period chunks, audio and the clock following. */
static void diag_advance_to(uint64_t target)
{
    while (mach_cyc < target) {
        uint64_t left = target - mach_cyc;
        spend(left < TP_IRQ_CYCLES ? (uint32_t)left : TP_IRQ_CYCLES);
        diag_audio();
        machine_wait_cyc(mach_cyc);
    }
}

/* ---- the diag pass's CPU work -------------------------------------------
 * A diag pass is: the wait for 21 falling edges of the 3 kHz clock (+ the
 * VG HALT test), then TIMEST's work W, then the loop head.  The next pass's
 * wait ends on the edge 21 periods after the first edge following W, so a
 * pass lasts 512 x (21 + floor(W / 512)) cycles - quantised.  The oracle's
 * (tests\refrun.exe, selftest_boot passes 1-1359, QSTATE at the pass start):
 *   BADEAR 0: 279 of 281 in bucket 21 (10,752), ROMREP 2: 298 of 318 in 33
 *   (16,896, 19 in 32), SHATCH 4 / SHYSTER 6 / SINTEN 8 / SCHEKR 10: 21,
 *   SIGANA 12: 22 (11,264); mean 12,205 cycles = 123.9 passes/s.
 * The translated TIMEST charges only its accesses, so the pass is topped up
 * to the middle of the oracle's bucket: work = max(0, W - native work since
 * TIMEST's VGSTOP), W by the screen the pass drew (QSTATE at VGSTART). */
static uint32_t diag_work_cycles(uint8_t qstate)
{
    switch (qstate) {
    case 0x02: return 6400u;                /* ROMREP: bucket 33 */
    case 0x0C: return 768u;                 /* SIGANA: bucket 22 */
    default:   return 256u;                 /* BADEAR, SHATCH, SHYSTER, SINTEN, SCHEKR: 21 */
    }
}

static void diag_charge(void)
{
    uint64_t w = diag_work_cycles(diag_vgstart_qstate);
    uint64_t done = mach_cyc - diag_timest_cyc;
    if (done < w) diag_advance_to(mach_cyc + (w - done));
}

static void diag_present(void)
{
    if (mach_cyc < diag_present_next) return;
    frame_boundary();
    n_diag_frames++;
    diag_present_next += TP_DIAG_PRESENT_CYC;
    if (diag_present_next <= mach_cyc) diag_present_next = mach_cyc + TP_DIAG_PRESENT_CYC;
}

/* One pass of the diag loop, loop head to loop head.  The spinner counter
 * moves freely (no IRQ reads a delta here: TIMEST reads ALLPOT whole). */
static void diag_body(void)
{
    spin_pos = (spin_pos + spin_pending) & 0x0F;
    spin_pending = 0;
    diag_timest_cyc = mach_cyc;
    diag_vgstart_qstate = QSTATE;
    diag_pass();                            /* altes2.c $DA8D; TEST open: hw_watchdog_hang */
    n_diag_passes++;
    diag_charge();
    diag_audio();
    diag_present();
    machine_wait_cyc(mach_cyc);
}

/* MAINLN's wait ($C7A7-$C7AB LDA FRTIMR / CMP #9 / BCC): present the frame,
 * charge the pass's CPU time, then service IRQs as the clock makes them due
 * until FRTIMR >= 9 - and any whose grid point the CPU's time has already
 * passed (on the board they interrupted the pass). */
void hw_wait_frame(void)
{
    pass_charge(frame_boundary());
    while (FRTIMR < 9 || next_irq <= mach_cyc)
        machine_tick();
}

/* ------------------------------------------------------------------ */
/* EAROM image                                                         */
/* ------------------------------------------------------------------ */

static void earom_load(void)
{
    ad_er2055_init(&earom);
    ad_er2055_control(&earom, 0);
    if (plat_nvram_read(earom.rom, sizeof earom.rom) != 0)
        memset(earom.rom, 0xFF, sizeof earom.rom);      /* a blank part reads $FF */
    earom.dirty = false;
}

/* ------------------------------------------------------------------ */
/* the app hooks                                                       */
/* ------------------------------------------------------------------ */

static void (*guarded_fn)(void);

/* RESET's power-on path up to MAINLN's first frame, on the synthetic clock
 * (the masked delay loop is ~0.56 s of machine time with nothing to show). */
static void boot_body(void)
{
    uint64_t done;
    n_boots++;
    pass_armed = 0;
    fast_clock = 1;
    in_irq = 0;
    reset_arrival_cyc = mach_cyc;
    reset();                                /* altes2.c: RESET .. MAINLN's first wait, or the diag loop head */
    if (g.cpu_loop == LOOP_DIAG) {
        /* TEST closed: the RAM march, ROM checksums, POKEY and EAROM tests
         * ran on host RAM; their CPU time (the oracle's RESET -> $DA8D, ~3 s,
         * screen dark, IRQ masked) is charged here on the synthetic clock,
         * like the TEST-open path's 0.56 s delay loop. */
        n_diag_boots++;
        done = mach_cyc - reset_arrival_cyc;
        if (done < TP_SFTEST_CYC) {
            uint64_t left = TP_SFTEST_CYC - done;
            while (left > 0) {              /* chunks: the POKEYs' queues are cleared below */
                uint32_t step = left < 65536u ? (uint32_t)left : 65536u;
                spend(step);
                left -= step;
            }
        }
    }
    last_boot_cyc = mach_cyc - reset_arrival_cyc;
    boot_synthetic_cyc += last_boot_cyc;
    audio_clear();                          /* live audio starts at a fresh boundary */
    audio_next_cyc = mach_cyc + TP_IRQ_CYCLES;
    diag_present_next = mach_cyc;
    fast_clock = synthetic_only;
    clk_valid = 0;                          /* anchor the clock at the next wait */
}

/* One pass of whichever loop the CPU is in (g.cpu_loop, set by reset()). */
static void pass_body(void)
{
    if (g.cpu_loop == LOOP_DIAG) diag_body();
    else mainln_pass();
}

/* Run fn; a software-watchdog trip anywhere inside re-boots through RESET
 * (the ROM's BRK / JMP RESET) and the pass is over.  A hardware restart
 * (hw_reset, the watchdog bite) re-samples the inputs first: RESET reads the
 * TEST switch as it is at that moment. */
static void run_guarded(void (*fn)(void))
{
    guarded_fn = fn;
    for (;;) {
        switch (setjmp(wd_jmp)) {
        case 0:
            wd_armed = 1;
            guarded_fn();
            wd_armed = 0;
            return;
        case 2:
            plat_input_poll(&cur_in);
            spin_pending += cur_in.spin_delta;
            break;
        default:
            break;
        }
        wd_armed = 0;
        in_irq = 0;
        guarded_fn = boot_body;
    }
}

static void audio_open(void)
{
    audio_live = plat_audio_open(TP_AUDIO_RATE) == 0;
    if (!audio_live)
        fprintf(stderr, "plat_audio_open failed; continuing without POKEY sound\n");
}

/* irq_hz <= 0 = the board's own 246.09 Hz; 240 = AAE's Tempest IRQ rate.
 * Call before tempest_app_init (the POKEYs take their clock there). */
void tempest_app_set_fps_lock(double irq_hz)
{
    mach_scale = irq_hz > 0.0 ? TP_IRQ_HZ / irq_hz : 1.0;
    pokey_audio_clock_hz = (uint32_t)(TP_POKEY_HZ / mach_scale + 0.5);
    if (pokey_audio_clock_hz == 0) pokey_audio_clock_hz = 1;
    clk_valid = 0;
}

double tempest_app_idle_ms(void) { return idle_remain_ms; }

/* cycles <= 0 = the default 1,134,000 (refrun's, AAE's 0.75 s) */
void tempest_app_set_watchdog_cycles(double cycles)
{
    wd_timeout_cyc = cycles >= 2.0 * TP_IRQ_CYCLES ? (uint64_t)cycles : TP_WD_CYCLES;
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
    s->soft_watchdog_trips = wd_trips;
}

void tempest_app_init(void)
{
    memset(&g, 0, sizeof g);
    memset(&cur_in, 0, sizeof cur_in);
    mach_cyc = 0;
    last_kick_cyc = 0;
    next_irq = TP_IRQ_CYCLES;
    spin_pos = 0; spin_pending = 0;
    vg_running = 0; vg_ends_in_halt = 0; vg_cache_valid = 0;
    vg_pic_on = 0; vg_pic_cyc = 0; vg_pic_blank = 0;
    n_pictures = 0; pic_cyc_sum = 0; pic_cyc_min = pic_cyc_max = 0;
    out0_latch = 0; outank_latch = 0;
    fast_ms = 0.0;
    clk_valid = 0;
    tp_z_index = 0;
    pass_owed = 0.0;
    mb_reset(&mb);
    pokey_init();
    earom_load();
    audio_open();
    plat_input_poll(&cur_in);
    cur_in.spin_delta = 0;
    run_guarded(boot_body);
}

double tempest_app_step(double now_ms)
{
    (void)now_ms;                           /* the seam reads the clock itself */
    plat_input_poll(&cur_in);
    spin_pending += cur_in.spin_delta;
    if (g.cpu_loop == LOOP_MAINLN) pass_begin();
    run_guarded(pass_body);
    return 0.0;
}

void tempest_app_exit(void)
{
    if (earom.dirty && plat_nvram_write(earom.rom, sizeof earom.rom) == 0)
        earom.dirty = false;
    if (audio_live) plat_audio_close();
    audio_live = 0;
}

/* ------------------------------------------------------------------ */
/* TEMPEST_SKELETON_MAIN: tests\skeleton.exe [passes]                  */
/* ------------------------------------------------------------------ */
#ifdef TEMPEST_SKELETON_MAIN
int main(int argc, char **argv)
{
    unsigned passes = argc > 1 ? (unsigned)strtoul(argv[1], NULL, 0) : 100;
    synthetic_only = 1;
    if (plat_init()) return 2;
    tempest_app_init();
    for (unsigned i = 0; i < passes; i++) tempest_app_step(0.0);
    tempest_app_exit();
    printf("SKELETON: %u passes, %u IRQs, QSTATE $%02X, soft-watchdog trips %lu, list ends %s\n",
           (unsigned)g.pass_count, (unsigned)g.irq_count, QSTATE, wd_trips, avg_stop_name(last_stop));
    plat_shutdown();
    return (g.pass_count == passes && wd_trips == 0) ? 0 : 1;
}
#endif

/* ------------------------------------------------------------------ */
/* TEMPEST_SELFTEST_MAIN: the headless self-test                       */
/* ------------------------------------------------------------------ */
/*
 *   tests\tempest_selftest.exe [--attract N] [--ref DIR]
 *
 * Built by build_all.bat from THIS file (/DTEMPEST_SELFTEST_MAIN) + the game
 * modules + platform\headless\plat_headless.c, on the synthetic clock: the
 * real seam, the real boot and loop, scripted inputs through hl_inputs.
 * altes2.c is compiled with SYSTEM / DSPSYS renamed so the wrappers below
 * count them (like tests\gate.exe).  Exit 0 only if every check passes.
 */
#ifdef TEMPEST_SELFTEST_MAIN

#include "platform/headless/plat_headless.h"

xy6502 altes2_system_(uint8_t x, uint8_t y);
xy6502 altes2_dspsys(uint8_t a, uint8_t x, uint8_t y);
static unsigned long n_system, n_dspsys;
xy6502 system_(uint8_t x, uint8_t y)           { n_system++; return altes2_system_(x, y); }
xy6502 dspsys(uint8_t a, uint8_t x, uint8_t y) { n_dspsys++; return altes2_dspsys(a, x, y); }

static int  st_fails, st_checks;
static const char *st_ref = "tests\\ref";

static void st_check(int ok, const char *name, const char *detail)
{
    st_checks++;
    if (!ok) st_fails++;
    printf("  [%s] %-34s %s\n", ok ? "PASS" : "FAIL", name, detail);
}

/* per-pass watch */
static unsigned long st_passes, st_qt_bad, st_list_changes, st_csystm, st_attract_left;
static uint64_t st_vram_hash;
static char     st_qt_first[160];

static uint64_t st_fnv(const uint8_t *p, size_t n)
{
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}

static void st_run(int n)
{
    for (int i = 0; i < n; i++) {
        uint64_t h;
        tempest_app_step(0.0);
        st_passes++;
        if (g.cpu_loop == LOOP_MAINLN && (QT1 | QT2 | QT4 | QT5)) {   /* (the diag loop uses the stack page freely) */
            if (!st_qt_bad)
                snprintf(st_qt_first, sizeof st_qt_first, "first at pass %lu: QT1 %02X QT2 %02X QT4 %02X QT5 %02X",
                         st_passes, QT1, QT2, QT4, QT5);
            st_qt_bad++;
        }
        if (QSTATE == K_CSYSTM) st_csystm++;
        h = st_fnv(g.vram, sizeof g.vram);
        if (h != st_vram_hash) st_list_changes++;
        st_vram_hash = h;
    }
}

static int st_wait_state(uint8_t state, int limit)
{
    int i;
    for (i = 0; i < limit && QSTATE != state; i++) st_run(1);
    return QSTATE == state;
}

/* ---- pass 1 against the oracle's dump ----------------------------------- */
static int st_load(const char *ext, uint8_t *buf, size_t len)
{
    char p[600];
    FILE *f;
    size_t n;
    snprintf(p, sizeof p, "%s\\frame_0001.%s", st_ref, ext);
    f = fopen(p, "rb");
    if (!f) return 0;
    n = fread(buf, 1, len, f);
    fclose(f);
    return n == len;
}

static void st_pass1_compare(void)
{
    static uint8_t ram[0x800], vram[0x1000], col[16];
    char detail[400];
    unsigned nram = 0, nstack = 0, nvram = 0, ncol = 0;
    size_t dl = 0;
    if (!st_load("ram", ram, sizeof ram) || !st_load("vram", vram, sizeof vram) || !st_load("col", col, sizeof col)) {
        st_check(1, "pass-1 vs tests\\ref (skipped)", "frame_0001 dumps not found");
        return;
    }
    detail[0] = 0;
    for (unsigned a = 0; a < 0x800; a++) {
        if (g.ram[a] == ram[a]) continue;
        if (a >= 0x100 && a < 0x200) { nstack++; continue; }   /* the ROM's pushes */
        if (nram < 6) dl += (size_t)snprintf(detail + dl, sizeof detail - dl, " $%04X C=%02X ROM=%02X", a, g.ram[a], ram[a]);
        nram++;
    }
    for (unsigned a = 0; a < 0x1000; a++) nvram += g.vram[a] != vram[a];
    for (unsigned a = 0; a < 16; a++)     ncol  += g.colram[a] != col[a];
    {
        char d2[520];
        snprintf(d2, sizeof d2, "RAM %u differ (stack page %u exempt), vector RAM %u, colour %u, IRQs %u%s",
                 nram, nstack, nvram, ncol, (unsigned)g.irq_count, detail);
        st_check(nram == 0 && nvram == 0 && ncol == 0, "pass-1 byte compare vs frame_0001", d2);
    }
}

/* ---- the POKEY protection probe (tests\probe_pokey_prot) ------------------ */
typedef struct {
    machine_state g;
    ad_pokey p[2];
    mathbox  mb;
    uint64_t mach_cyc, next_irq;
    int vg_running, vg_ends_in_halt, vg_pic_on;
    uint64_t vg_start_cyc, vg_busy_until, vg_pic_cyc;
} st_snap;

static void st_save(st_snap *s)
{
    s->g = g; s->p[0] = pokey[0]; s->p[1] = pokey[1]; s->mb = mb;
    s->mach_cyc = mach_cyc; s->next_irq = next_irq;
    s->vg_running = vg_running; s->vg_ends_in_halt = vg_ends_in_halt;
    s->vg_start_cyc = vg_start_cyc; s->vg_busy_until = vg_busy_until;
    s->vg_pic_on = vg_pic_on; s->vg_pic_cyc = vg_pic_cyc;
}

static void st_restore(const st_snap *s)
{
    g = s->g; pokey[0] = s->p[0]; pokey[1] = s->p[1]; mb = s->mb;
    mach_cyc = s->mach_cyc; next_irq = s->next_irq;
    vg_running = s->vg_running; vg_ends_in_halt = s->vg_ends_in_halt;
    vg_start_cyc = s->vg_start_cyc; vg_busy_until = s->vg_busy_until;
    vg_pic_on = s->vg_pic_on; vg_pic_cyc = s->vg_pic_cyc;
}

#define ST_PHASES 6000

/* Over ST_PHASES start phases (offsets k * 97 cycles, ~4.4 periods of the
 * 17-bit chain): INISOU's halted-POKEY check (QT4) and LDRDSP's $AE1F
 * nibble check (QT5), run from a saved attract machine.  Returns the
 * number of phases that set QT4 / QT5. */
static void st_probe_run(const st_snap *s, uint32_t rd, uint32_t wr, unsigned *bad4, unsigned *bad5)
{
    static ad_pokey run[2];                /* the saved chips, clocked on phase by phase */
    uint64_t run_cyc = s->mach_cyc;
    uint32_t r0 = cost_read, w0 = cost_write;
    cost_read = rd; cost_write = wr;
    *bad4 = *bad5 = 0;
    run[0] = s->p[0]; run[1] = s->p[1];
    for (unsigned k = 0; k < ST_PHASES; k++) {
        ad_pokey_advance(&run[0], 97u);
        ad_pokey_advance(&run[1], 97u);
        run_cyc += 97u;
        st_restore(s);
        pokey[0] = run[0]; pokey[1] = run[1]; mach_cyc = run_cyc;
        (void)inisou();
        if (QT4) (*bad4)++;
        st_restore(s);
        pokey[0] = run[0]; pokey[1] = run[1]; mach_cyc = run_cyc;
        ldrdsp();
        if (QT5) (*bad5)++;
    }
    cost_read = r0; cost_write = w0;
    st_restore(s);
}

static void st_probe(void)
{
    static st_snap s;
    unsigned b4, b5, n4, n5, m4, m5;
    char d[200];
    st_save(&s);
    st_probe_run(&s, cost_read, cost_write, &b4, &b5);
    snprintf(d, sizeof d, "read %u / write %u cycles: QT4 set in %u, QT5 set in %u of %d phases",
             (unsigned)cost_read, (unsigned)cost_write, b4, b5, ST_PHASES);
    st_check(b4 == 0 && b5 == 0, "POKEY probe: QT4/QT5 stay 0", d);
    /* negative controls: the probe must bite on wrong spacing */
    st_probe_run(&s, cost_read + 1u, cost_write, &n4, &n5);
    st_probe_run(&s, cost_read, 4u, &m4, &m5);
    snprintf(d, sizeof d, "read %u: QT5 set in %u phases; write 4: QT4 set in %u phases",
             (unsigned)cost_read + 1u, n5, m4);
    st_check(n5 > 0 && m4 > 0, "POKEY probe bites (wrong spacing)", d);
}

/* ---- high-score table: the top 3 entries INIINI keeps from the EAROM ------ */
static void st_top3(uint8_t *out)
{
    for (int i = 0; i < 9; i++) {
        out[i] = g.ram[A_HSCORL + 15 + i];
        out[9 + i] = g.ram[A_INITAL + 15 + i];
    }
}

static int st_trip_armed;
static void st_trip_hook(void)
{
    if (st_trip_armed) {
        /* as if the pass had overrun 128 IRQs: FRTIMR b7 set and an IRQ
         * already due, so the wait takes it and its watchdog check trips */
        st_trip_armed = 0;
        FRTIMR = 0x80;
        spend(TP_IRQ_CYCLES);
    }
}

/* ======================================================================
 * M9 B5: the ROM's self test on the native seam.
 *
 * selftest_boot: tests\scenarios\selftest_boot.txt is replayed pass for pass
 * (the diag loop has no IRQ, so native diag pass N IS the oracle's frame N):
 * RAM (stack page exempt), vector RAM and colour RAM are compared with the
 * refrun dumps in tests\ref_selftest (tests\refrun.exe --frames 1361
 * --capture-every 20 --outdir tests\ref_selftest --script
 * tests\scenarios\selftest_boot.txt), and machine time with its
 * frame_sched.txt, up to the TEST-off watchdog reboot.
 * ====================================================================== */
static const char *st_ref_st = "tests\\ref_selftest";
static const char *st_boot_script = "tests\\scenarios\\selftest_boot.txt";

typedef struct { unsigned pass; char port[16]; long val; } st_line;
static st_line st_lines[256];
static int     st_nlines, st_spin;

static int st_load_script(const char *path)
{
    char buf[256], port[16], v[32];
    FILE *f = fopen(path, "r");
    st_nlines = 0;
    if (!f) return 0;
    while (fgets(buf, sizeof buf, f) && st_nlines < 256) {
        char *h = strchr(buf, '#');
        unsigned p;
        int n;
        if (h) *h = 0;
        n = sscanf(buf, "%u %15s %31s", &p, port, v);
        if (n < 2) continue;
        st_lines[st_nlines].pass = p;
        snprintf(st_lines[st_nlines].port, sizeof st_lines[st_nlines].port, "%s", port);
        st_lines[st_nlines].val = n == 3 ? strtol(v, NULL, 0) : 0;
        st_nlines++;
    }
    fclose(f);
    return st_nlines;
}

/* refrun's script ports on the headless inputs (README_selftest.md) */
static void st_apply(unsigned pass)
{
    for (int i = 0; i < st_nlines; i++) {
        const st_line *l = &st_lines[i];
        uint8_t on = l->val != 0;
        if (l->pass != pass) continue;
        if      (!strcmp(l->port, "test"))     hl_inputs.test = on;
        else if (!strcmp(l->port, "diag"))     hl_inputs.diag = on;
        else if (!strcmp(l->port, "slam"))     hl_inputs.slam = on;
        else if (!strcmp(l->port, "coinl"))    hl_inputs.coin_l = on;
        else if (!strcmp(l->port, "coinc"))    hl_inputs.coin_c = on;
        else if (!strcmp(l->port, "coinr"))    hl_inputs.coin_r = on;
        else if (!strcmp(l->port, "fire"))     hl_inputs.fire = on;
        else if (!strcmp(l->port, "zap"))      hl_inputs.zap = on;
        else if (!strcmp(l->port, "start1"))   hl_inputs.start1 = on;
        else if (!strcmp(l->port, "start2"))   hl_inputs.start2 = on;
        else if (!strcmp(l->port, "cocktail")) hl_dsw_pokey1 = (uint8_t)(on ? (hl_dsw_pokey1 | K_COCKTA) : (hl_dsw_pokey1 & ~K_COCKTA));
        else if (!strcmp(l->port, "inop0"))    hl_dsw_n13 = (uint8_t)l->val;
        else if (!strcmp(l->port, "inop1"))    hl_dsw_l12 = (uint8_t)l->val;
        else if (!strcmp(l->port, "spin"))     st_spin = (int)(int8_t)(uint8_t)l->val;
    }
}

static int st_load_frame(const char *dir, unsigned frame, const char *ext, uint8_t *buf, size_t len)
{
    char p[600];
    FILE *f;
    size_t n;
    snprintf(p, sizeof p, "%s\\frame_%04u.%s", dir, frame, ext);
    f = fopen(p, "rb");
    if (!f) return 0;
    n = fread(buf, 1, len, f);
    fclose(f);
    return n == len;
}

/* -1 = no dump for this frame, else the number of differing bytes */
static int st_cmp_frame(unsigned frame, char *detail, size_t dn)
{
    static uint8_t ram[0x800], vram[0x1000], col[16];
    int nd = 0;
    size_t dl = 0;
    if (!st_load_frame(st_ref_st, frame, "ram", ram, sizeof ram) ||
        !st_load_frame(st_ref_st, frame, "vram", vram, sizeof vram) ||
        !st_load_frame(st_ref_st, frame, "col", col, sizeof col)) return -1;
    detail[0] = 0;
    for (unsigned a = 0; a < 0x800; a++) {
        if (a >= 0x100 && a < 0x200) continue;
        if (g.ram[a] != ram[a]) {
            if (nd < 3) dl += (size_t)snprintf(detail + dl, dn - dl, " $%04X C=%02X ROM=%02X", a, g.ram[a], ram[a]);
            nd++;
        }
    }
    for (unsigned a = 0; a < 0x1000; a++)
        if (g.vram[a] != vram[a]) {
            if (nd < 3) dl += (size_t)snprintf(detail + dl, dn - dl, " $%04X C=%02X ROM=%02X", 0x2000 + a, g.vram[a], vram[a]);
            nd++;
        }
    for (unsigned a = 0; a < 16; a++) nd += g.colram[a] != col[a];
    return nd;
}

#define ST_MAXF 1400
static uint64_t st_or_cyc[ST_MAXF + 2];
static uint8_t  st_or_have[ST_MAXF + 2];

static int st_load_sched(void)
{
    char p[600], buf[160];
    FILE *f;
    int n = 0;
    snprintf(p, sizeof p, "%s\\frame_sched.txt", st_ref_st);
    memset(st_or_have, 0, sizeof st_or_have);
    f = fopen(p, "r");
    if (!f) return 0;
    while (fgets(buf, sizeof buf, f)) {
        unsigned fr; unsigned long irq; unsigned long long cyc;
        if (buf[0] == '#' || sscanf(buf, "%u %lu %llu", &fr, &irq, &cyc) != 3 || fr > ST_MAXF + 1) continue;
        st_or_cyc[fr] = cyc; st_or_have[fr] = 1; n++;
    }
    fclose(f);
    return n;
}

static int      st_apeak;
static uint64_t st_aframes;
static void st_audio_hook(const int16_t *pcm, int frames)
{
    st_aframes += (uint64_t)frames;
    for (int i = 0; i < frames; i++) {
        int v = pcm[i] < 0 ? -(int)pcm[i] : (int)pcm[i];
        if (v > st_apeak) st_apeak = v;
    }
}

/* the self-test result cells MBCOND RAMCND PK1CND PK2CND EARCND + CHKSMS x 12 */
static int st_cells(char *out, size_t n)
{
    int bad = 0;
    size_t l = (size_t)snprintf(out, n, "cells $78-$88:");
    for (unsigned a = K_MBCOND; a < (unsigned)K_CHKSMS + 12u; a++) {
        bad |= g.ram[a];
        l += (size_t)snprintf(out + l, n - l, " %02X", g.ram[a]);
    }
    return bad == 0;
}

static void st_selftest_boot(void)
{
    static uint64_t nat_cyc[ST_MAXF + 2];
    static uint8_t  nat_q[ST_MAXF + 2];
    static const unsigned exp_f[] = { 1, 282, 553, 603, 1153, 1203, 1263, 1313 };
    static const uint8_t  exp_q[] = { 0, 2, 4, 6, 8, 10, 12, 2 };
    char d[700], det[200], cells[200], first_det[240] = "";
    unsigned seen_f[16], f, last_diag = 0;
    uint8_t seen_q[16];
    int nseen = 0, seq_ok, compared = 0, differ = 0, cmp1, shy_peak = 0, segs[8] = { 0 }, n_sched;
    unsigned long frames0, halt0, bad0, trips0 = wd_trips, bites0 = n_wd_bites, dboots0 = n_diag_boots, ticks0;
    uint64_t cyc0, af0;

    if (!st_load_script(st_boot_script)) {
        st_check(0, "self test: scenario", "cannot read tests\\scenarios\\selftest_boot.txt (run from c_src)");
        return;
    }
    n_sched = st_load_sched();
    memset(&hl_inputs, 0, sizeof hl_inputs);
    hl_dsw_n13 = hl_dsw_l12 = hl_dsw_pokey1 = hl_dsw_pokey2 = 0;
    st_spin = 0;
    hl_nvram_len = 0;                                   /* the scenario's blank EAROM */
    st_apply(0);                                        /* 0 test 1 */
    tempest_app_init();
    nat_cyc[1] = mach_cyc; nat_q[1] = QSTATE;
    seen_f[nseen] = 1; seen_q[nseen] = QSTATE; nseen++;
    cmp1 = st_cmp_frame(1, det, sizeof det);
    (void)st_cells(cells, sizeof cells);
    snprintf(d, sizeof d, "loop %s, self-test boots +%lu, IRQs %u, RESET -> $DA8D %llu cycles (oracle %llu), QSTATE %u, "
             "frame_0001 %s%s; %s",
             g.cpu_loop == LOOP_DIAG ? "DIAG" : "MAINLN", n_diag_boots - dboots0, (unsigned)g.irq_count,
             (unsigned long long)mach_cyc, (unsigned long long)st_or_cyc[1], QSTATE,
             cmp1 < 0 ? "not found" : cmp1 == 0 ? "identical" : "DIFFERS:", cmp1 > 0 ? det : "", cells);
    st_check(g.cpu_loop == LOOP_DIAG && n_diag_boots == dboots0 + 1 && g.irq_count == 0 && QSTATE == 0 && cmp1 == 0 &&
             n_sched > 0 && mach_cyc == st_or_cyc[1],
             "self test: power-on, TEST closed", d);
    if (g.cpu_loop != LOOP_DIAG) return;

    frames0 = n_diag_frames; halt0 = n_walk_halt; bad0 = n_walk_bad;
    hl_audio_hook = st_audio_hook;
    st_aframes = 0; ticks0 = audio_ticks; cyc0 = mach_cyc; af0 = 0;
    for (f = 1; f < 1360; f++) {
        unsigned long fr = n_diag_frames;
        int c;
        st_apply(f);
        hl_inputs.spin_delta = st_spin;
        st_apeak = 0;
        tempest_app_step(0.0);
        st_passes++;
        if (g.cpu_loop != LOOP_DIAG) break;
        last_diag = f + 1;
        nat_cyc[f + 1] = mach_cyc; nat_q[f + 1] = QSTATE;
        if (QSTATE == 6 && st_apeak > shy_peak) shy_peak = st_apeak;
        if (QSTATE != nat_q[f] && nseen < 16) { seen_f[nseen] = f + 1; seen_q[nseen] = QSTATE; nseen++; }
        if (n_diag_frames != fr && QSTATE / 2u < 8u && hl_nsegs > segs[QSTATE / 2u]) segs[QSTATE / 2u] = hl_nsegs;
        c = st_cmp_frame(f + 1, det, sizeof det);
        if (c >= 0) {
            compared++;
            if (c > 0) {
                if (!differ) snprintf(first_det, sizeof first_det, ", first frame_%04u:%s", f + 1, det);
                differ++;
            }
        }
    }
    af0 = st_aframes;

    /* result cells, state walk */
    {
        int ok = st_cells(cells, sizeof cells);
        snprintf(d, sizeof d, "at diag pass %u (QSTATE %u): %s", last_diag, QSTATE, cells);
        st_check(ok && last_diag == 1360, "self test: RAM/ROM/Mathbox/POKEY/EAROM ok", d);
    }
    {
        size_t l = 0;
        seq_ok = nseen == 8;
        for (int i = 0; i < nseen; i++) {
            l += (size_t)snprintf(d + l, sizeof d - l, "%s%u@%u", i ? " " : "QSTATE@frame: ", seen_q[i], seen_f[i]);
            if (i < 8 && (seen_q[i] != exp_q[i] || seen_f[i] != exp_f[i])) seq_ok = 0;
        }
        snprintf(d + l, sizeof d - l, " (oracle 0@1 2@282 4@553 6@603 8@1153 10@1203 12@1263 2@1313)");
        st_check(seq_ok, "self test: diag/slam step the screens", d);
    }
    snprintf(d, sizeof d, "%d dumped diag frames compared (RAM exc. stack page, vector RAM, colour RAM), %d differ%s",
             compared, differ, first_det);
    st_check(compared > 50 && differ == 0, "self test: diag frames == refrun dumps", d);

    /* pace against the oracle's machine time, per screen */
    {
        double tot_n = (double)(nat_cyc[1360] - nat_cyc[1]), tot_o = (double)(st_or_cyc[1360] - st_or_cyc[1]);
        double cn[16] = { 0 }, co[16] = { 0 }, worst = 0.0;
        unsigned np[16] = { 0 };
        size_t l;
        int worst_q = -1, ok;
        for (f = 1; f < 1360; f++) {
            unsigned q = nat_q[f] / 2u;
            if (q >= 16) continue;
            cn[q] += (double)(nat_cyc[f + 1] - nat_cyc[f]);
            co[q] += (double)(st_or_cyc[f + 1] - st_or_cyc[f]);
            np[q]++;
        }
        l = (size_t)snprintf(d, sizeof d, "1359 passes: native %.2f passes/s, oracle %.2f (%+.2f%%); per screen passes/s native/oracle:",
                             1359.0 * TP_CPU_HZ / tot_n, 1359.0 * TP_CPU_HZ / tot_o, 100.0 * (tot_o / tot_n - 1.0));
        for (unsigned q = 0; q < 16; q++) {
            double e;
            if (!np[q]) continue;
            e = co[q] / cn[q] - 1.0;
            if (e < 0) e = -e;
            if (e > worst) { worst = e; worst_q = (int)q * 2; }
            l += (size_t)snprintf(d + l, sizeof d - l, " %u:%.1f/%.1f", q * 2, np[q] * TP_CPU_HZ / cn[q], np[q] * TP_CPU_HZ / co[q]);
        }
        snprintf(d + l, sizeof d - l, "; worst %.2f%% (QSTATE %d)", 100.0 * worst, worst_q);
        ok = last_diag == 1360 && tot_o > 0.0 && fabs(tot_o / tot_n - 1.0) < 0.05 && worst < 0.05;
        st_check(ok, "self test: diag pace vs oracle (5%)", d);
    }
    {
        double secs = (double)(mach_cyc - cyc0) / TP_CPU_HZ;
        unsigned long nf = n_diag_frames - frames0;
        snprintf(d, sizeof d, "%lu frames in %.2f machine s (%.2f/s), walks ending HALT %lu, broken +%lu; lit segs max per screen "
                 "BADEAR %d ROMREP %d SHATCH %d SHYSTER %d SINTEN %d SCHEKR %d SIGANA %d",
                 nf, secs, (double)nf / secs, n_walk_halt - halt0, n_walk_bad - bad0,
                 segs[0], segs[1], segs[2], segs[3], segs[4], segs[5], segs[6]);
        st_check(fabs((double)nf / secs - 60.0) < 1.0 && n_walk_bad == bad0 && n_walk_halt - halt0 == nf &&
                 segs[1] > 0 && segs[2] > 0 && segs[3] > 0 && segs[4] > 0 && segs[5] > 0 && segs[6] > 0,
                 "self test: diag screens presented", d);
    }
    {
        double expect_t = (double)(audio_ticks - ticks0) * TP_AUDIO_RATE * TP_IRQ_CYCLES / TP_POKEY_HZ;
        double expect_m = (double)(mach_cyc - cyc0) * TP_AUDIO_RATE / TP_POKEY_HZ;
        snprintf(d, sizeof d, "no IRQ (%u); %llu frames pushed over %.2f machine s (expect %.0f from ticks, %.0f from time), SHYSTER peak %d",
                 (unsigned)g.irq_count, (unsigned long long)af0, (double)(mach_cyc - cyc0) / TP_CPU_HZ, expect_t, expect_m, shy_peak);
        st_check(g.irq_count == 0 && fabs((double)af0 - expect_t) <= 2.0 && fabs((double)af0 - expect_m) <= 400.0 && shy_peak > 0,
                 "self test: diag audio on machine time", d);
    }
    hl_audio_hook = NULL;

    /* 1360 test 0: WDGTST -> hardware watchdog -> RESET, TEST open -> attract */
    if (last_diag == 1360) {
        unsigned long passes0;
        uint64_t c1360 = mach_cyc;
        int left_attract = 0;
        uint64_t or_boot = 0;
        double nat_hang, or_hang;
        {
            /* the TEST-open boot's own length (RESET -> first MAINLN wait) differs by design (M8: the RAM
             * clear is not charged): compare the pass + spin to the bite with the boot taken out, the
             * oracle's boot = its attract run's frame 1 (tests\ref\frame_sched.txt) */
            char p[600], buf[160];
            FILE *sf;
            snprintf(p, sizeof p, "%s\\frame_sched.txt", st_ref);
            sf = fopen(p, "r");
            while (sf && fgets(buf, sizeof buf, sf)) {
                unsigned fr; unsigned long irq; unsigned long long cyc;
                if (buf[0] != '#' && sscanf(buf, "%u %lu %llu", &fr, &irq, &cyc) == 3 && fr == 1) { or_boot = cyc; break; }
            }
            if (sf) fclose(sf);
        }
        st_apply(1360);
        tempest_app_step(0.0);
        st_passes++;
        nat_hang = (double)(mach_cyc - c1360 - last_boot_cyc);
        or_hang = (double)(st_or_cyc[1361] - st_or_cyc[1360]) - (double)or_boot;
        snprintf(d, sizeof d, "bites %lu -> %lu, loop %s; head of pass 1360 -> RESET (timeout %llu): %.0f cycles, oracle %.0f "
                 "(%+.2f%%); boot to the first MAINLN wait native %llu, oracle %llu; trips %lu",
                 bites0, n_wd_bites, g.cpu_loop == LOOP_DIAG ? "DIAG" : "MAINLN", (unsigned long long)wd_timeout_cyc,
                 nat_hang, or_hang, 100.0 * (nat_hang / or_hang - 1.0), (unsigned long long)last_boot_cyc,
                 (unsigned long long)or_boot, wd_trips);
        st_check(n_wd_bites == bites0 + 1 && g.cpu_loop == LOOP_MAINLN && st_or_have[1361] && or_boot > 0 &&
                 fabs(nat_hang / or_hang - 1.0) < 0.01,
                 "self test: TEST off -> watchdog reboot", d);
        passes0 = g.pass_count;
        for (int i = 0; i < 300; i++) {
            st_run(1);
            if (QSTATUS & K_MATRACT) left_attract = 1;
        }
        snprintf(d, sizeof d, "%lu MAINLN passes, QSTATE $%02X, left attract %d, SYSTEM state %lu, trips %lu -> %lu",
                 (unsigned long)(g.pass_count - passes0), QSTATE, left_attract, st_csystm, trips0, wd_trips);
        st_check(g.pass_count - passes0 == 300 && !left_attract && QSTATE != K_CSYSTM && wd_trips == trips0,
                 "self test: back in attract", d);
    }
}

/* EAROM group k (0 initials INITAL+15.., 1 high scores HSCORL+15.., 2 bookkeeping
 * BOOKKS..): image offsets TEAX..TEACNT (last = checksum) */
static int st_group_zero(const uint8_t *img, unsigned k)
{
    uint8_t lo = cpu_rd((uint16_t)(0xDDDD + 2 * k)), hi = cpu_rd((uint16_t)(0xDDDE + 2 * k));
    int z = 1;
    for (unsigned a = lo; a <= hi && a < TP_EAROM_SIZE; a++) z &= img[a] == 0;
    return z;
}

static int st_group_same(const uint8_t *a_img, const uint8_t *b_img, unsigned k)
{
    uint8_t lo = cpu_rd((uint16_t)(0xDDDD + 2 * k)), hi = cpu_rd((uint16_t)(0xDDDE + 2 * k));
    return memcmp(a_img + lo, b_img + lo, (size_t)(hi - lo + 1)) == 0;
}

static int st_option(void) { return (CURSL1 & 6) >> 1; }

/* spin the DSPSYS cursor until the option is in the mask */
static int st_cursor_to(unsigned mask)
{
    for (int i = 0; i < 800 && !((mask >> st_option()) & 1u); i++) {
        hl_inputs.spin_delta = 4;
        st_run(1);
    }
    hl_inputs.spin_delta = 0;
    st_run(12);
    return (mask >> st_option()) & 1u;
}

static void st_selftest_midrun(const uint8_t *game_image)
{
    char d[600], cells[200];
    uint8_t top[18], before[TP_EAROM_SIZE];
    unsigned long sys0, dsp0, trips0 = wd_trips, w0, bites0, resets0, dboots0;
    int ok, real_hs = 0, idle, defaults = 1;

    tempest_app_exit();
    memset(&hl_inputs, 0, sizeof hl_inputs);
    hl_dsw_n13 = hl_dsw_l12 = hl_dsw_pokey1 = hl_dsw_pokey2 = 0;
    memcpy(hl_nvram, game_image, TP_EAROM_SIZE);
    hl_nvram_len = TP_EAROM_SIZE;
    tempest_app_init();
    st_run(150);
    st_top3(top);
    for (int i = 0; i < 9; i++) real_hs |= top[i] != 0x01;

    /* TEST closed in attract: NONSTA -> CSYSTM, SYSTEM + DSPSYS */
    sys0 = n_system; dsp0 = n_dspsys;
    hl_inputs.test = 1;
    ok = st_wait_state(K_CSYSTM, 60);
    st_run(30);
    snprintf(d, sizeof d, "QSTATE $%02X QDSTATE $%02X, SYSTEM +%lu, DSPSYS +%lu, %d segs, table from the game session: %s",
             QSTATE, QDSTATE, n_system - sys0, n_dspsys - dsp0, hl_nsegs, real_hs ? "yes" : "NO");
    st_check(ok && QSTATE == K_CSYSTM && QDSTATE == K_CDSYST && n_system > sys0 && n_dspsys > dsp0 && hl_nsegs > 0 && real_hs,
             "options: TEST in attract -> DSPSYS", d);

    /* option 3 = FIRE + START2 = EAZHIS */
    ok = st_cursor_to(1u << 3);
    memcpy(before, hl_nvram, sizeof before);
    w0 = hl_nvram_writes;
    hl_inputs.fire = 1; hl_inputs.start2 = 1;
    st_run(6);
    hl_inputs.fire = 0; hl_inputs.start2 = 0;
    for (idle = 0; idle < 3000 && (EAFLG != 0 || EAREQU != 0); idle++) st_run(1);
    st_run(10);                                         /* the next EAUPD deselects: flush */
    snprintf(d, sizeof d, "cursor option %d, erase done after %d passes, %u image writes; image: scores zero %d, initials zero %d "
             "(before %d/%d), bookkeeping kept %d; RAM HSCORL %02X",
             st_option(), idle, hl_nvram_writes - w0, st_group_zero(hl_nvram, 0), st_group_zero(hl_nvram, 1),
             st_group_zero(before, 0), st_group_zero(before, 1), st_group_same(hl_nvram, before, 2), g.ram[A_HSCORL + 21]);
    st_check(ok && idle < 3000 && hl_nvram_writes > w0 && st_group_zero(hl_nvram, 0) && st_group_zero(hl_nvram, 1) &&
             !st_group_zero(before, 0) && st_group_same(hl_nvram, before, 2),
             "options: EAZHIS zeroes scores (image)", d);

    /* TEST open: SYSTEM -> CNEWGA, INIINI (EABAD & 3) -> attract with the defaults */
    hl_inputs.test = 0;
    st_run(60);
    st_top3(top);
    for (int i = 0; i < 9; i++) defaults &= top[i] == 0x01;
    snprintf(d, sizeof d, "QSTATE $%02X, QSTATUS $%02X, top-3 scores %02X%02X%02X %02X%02X%02X %02X%02X%02X",
             QSTATE, QSTATUS, top[8], top[7], top[6], top[5], top[4], top[3], top[2], top[1], top[0]);
    st_check(QSTATE != K_CSYSTM && !(QSTATUS & K_MATRACT) && defaults, "options: TEST off -> INIINI, attract", d);

    /* option 0/1 = FIRE + ZAP = JMP RESET with TEST closed */
    hl_inputs.test = 1;
    (void)st_wait_state(K_CSYSTM, 60);
    st_run(10);
    ok = st_cursor_to(0x3u);
    resets0 = n_jmp_resets; dboots0 = n_diag_boots;
    hl_inputs.fire = 1; hl_inputs.zap = 1;
    for (int i = 0; i < 20 && g.cpu_loop == LOOP_MAINLN; i++) st_run(1);
    hl_inputs.fire = 0; hl_inputs.zap = 0;
    st_run(40);
    {
        int cells_ok = st_cells(cells, sizeof cells);
        snprintf(d, sizeof d, "cursor ok %d, JMP RESET %lu -> %lu, self-test boots +%lu, loop %s, QSTATE %u (good EAROM: 2); %s",
                 ok, resets0, n_jmp_resets, n_diag_boots - dboots0, g.cpu_loop == LOOP_DIAG ? "DIAG" : "MAINLN", QSTATE, cells);
        st_check(ok && n_jmp_resets == resets0 + 1 && n_diag_boots == dboots0 + 1 && g.cpu_loop == LOOP_DIAG &&
                 QSTATE == 2 && cells_ok, "options: JMP RESET -> self test", d);
    }
    hl_inputs.diag = 1; st_run(5); hl_inputs.diag = 0; st_run(5);
    {
        uint8_t q = QSTATE;
        unsigned long passes0 = g.pass_count;
        int left_attract = 0;
        bites0 = n_wd_bites;
        hl_inputs.test = 0;
        for (int i = 0; i < 200; i++) {
            st_run(1);
            if (QSTATUS & K_MATRACT) left_attract = 1;
        }
        snprintf(d, sizeof d, "diag step -> QSTATE %u; TEST off: bites %lu -> %lu, %lu MAINLN passes, left attract %d, trips %lu -> %lu",
                 q, bites0, n_wd_bites, (unsigned long)(g.pass_count - passes0), left_attract, trips0, wd_trips);
        st_check(q == 4 && n_wd_bites == bites0 + 1 && g.cpu_loop == LOOP_MAINLN && g.pass_count - passes0 == 199 &&
                 !left_attract && wd_trips == trips0, "options: diag step, watchdog -> attract", d);
    }
}

/* the diag loop on a clock that moves by itself (the Windows build's path) */
static void st_selftest_realclock(void)
{
    char d[400];
    double t0, t1;
    uint64_t c0, c1, boot1;
    unsigned long p0, trips0 = wd_trips;
    tempest_app_exit();
    memset(&hl_inputs, 0, sizeof hl_inputs);
    hl_inputs.test = 1;
    synthetic_only = 0;
    hl_clock_step_ms = 0.01;
    tempest_app_init();
    t0 = hl_now_ms; c0 = mach_cyc; p0 = n_diag_passes;
    for (int i = 0; i < 400; i++) { tempest_app_step(0.0); st_passes++; }
    t1 = hl_now_ms; c1 = mach_cyc; boot1 = boot_synthetic_cyc;
    hl_inputs.test = 0;
    for (int i = 0; i < 100; i++) { tempest_app_step(0.0); st_passes++; }
    {
        double r1 = (t1 - t0) / ((double)(c1 - c0) * 1000.0 / TP_CPU_HZ);
        /* the reboot itself runs on the synthetic clock (as every boot): out of the ratio */
        double r2 = (hl_now_ms - t1) / ((double)(mach_cyc - c1 - (boot_synthetic_cyc - boot1)) * 1000.0 / TP_CPU_HZ);
        snprintf(d, sizeof d, "diag: %lu passes, %.1f ms clock / %.1f ms machine (ratio %.4f, %.1f passes/s); "
                 "watchdog spin + 99 MAINLN passes (reboot excluded): ratio %.4f, loop %s, trips %lu",
                 n_diag_passes - p0, t1 - t0, (double)(c1 - c0) * 1000.0 / TP_CPU_HZ, r1,
                 (double)(n_diag_passes - p0) * 1000.0 / (t1 - t0), r2, g.cpu_loop == LOOP_DIAG ? "DIAG" : "MAINLN", wd_trips);
        st_check(r1 > 0.99 && r1 < 1.01 && r2 > 0.99 && r2 < 1.01 && g.cpu_loop == LOOP_MAINLN && wd_trips == trips0,
                 "self test: real-clock pacing", d);
    }
    hl_clock_step_ms = 0.0;
    synthetic_only = 1;
}

/* ---- M9 B4 Gate E2: the native legs of the EAROM round trip -------------
 *   --earom-boot IMAGE --dump DIR [--passes N]
 *       power on from IMAGE (64 bytes; "blank" = no image), write the boot
 *       pass (MAINLN pass 1) as DIR\frame_0001.ram/.vram/.col - the same
 *       capture point and format as refrun's dumps -, run N more passes,
 *       exit (EAROM flush) and write the chip as DIR\earom_exit.nv.
 *   --earom-write IMAGE [--dump DIR]
 *       power on blank, let the ROM's own EAUPD write a known table: top-3
 *       initials INITAL+15..+23 = $0A..$12, scores HSCORL+15..+23 =
 *       $12 $34 $05 / $78 $56 $07 / $90 $21 $09 (053412 < 075678 < 092190),
 *       bookkeeping BOOKKS+3..+11 ($0409-$0411) = $21..$29 (SECOUL $0406-$0408
 *       keeps counting and is not set), then WRHIIN + WRBOOK, run until
 *       EAFLG = EAREQU = 0, exit (flush), write the chip to IMAGE.
 * tools\gate_e.py drives both and compares them with refrun --earom-in /
 * --earom-out.  No checks are printed; exit 0 = done. */
static int st_write_file(const char *dir, const char *name, const uint8_t *p, size_t n)
{
    char path[700];
    FILE *f;
    if (dir) snprintf(path, sizeof path, "%s\\%s", dir, name);
    else snprintf(path, sizeof path, "%s", name);
    f = fopen(path, "wb");
    if (!f || fwrite(p, 1, n, f) != n) {
        fprintf(stderr, "SELFTEST: cannot write %s\n", path);
        if (f) fclose(f);
        return 1;
    }
    fclose(f);
    return 0;
}

static void st_print_cells(const char *when)
{
    printf("EAROM-TOOL: %s: EABAD %02X EAFLG %02X EAREQU %02X loop %s pass %u IRQs %u\n", when, EABAD, EAFLG, EAREQU,
           g.cpu_loop == LOOP_DIAG ? "DIAG" : "MAINLN", (unsigned)g.pass_count, (unsigned)g.irq_count);
    printf("EAROM-TOOL: %s: INITAL+15..+23", when);
    for (int i = 15; i < 24; i++) printf(" %02X", g.ram[A_INITAL + i]);
    printf("  HSCORL+15..+25");
    for (int i = 15; i < 26; i++) printf(" %02X", g.ram[A_HSCORL + i]);
    printf("  BOOKKS..BOOKKE-1");
    for (int i = 0; i < A_BOOKKE - A_BOOKKS; i++) printf(" %02X", g.ram[A_BOOKKS + i]);
    printf("\n");
}

static int st_earom_tool(const char *boot_img, const char *write_img, const char *dump, int passes)
{
    int rc = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    synthetic_only = 1;
    if (plat_init()) return 2;
    hl_nvram_len = 0;
    if (boot_img && strcmp(boot_img, "blank")) {
        FILE *f = fopen(boot_img, "rb");
        size_t got = f ? fread(hl_nvram, 1, TP_EAROM_SIZE, f) : 0;
        int extra = f ? fgetc(f) : EOF;
        if (f) fclose(f);
        if (got != TP_EAROM_SIZE || extra != EOF) {
            fprintf(stderr, "SELFTEST: --earom-boot %s must be a %u-byte image\n", boot_img, TP_EAROM_SIZE);
            return 2;
        }
        hl_nvram_len = TP_EAROM_SIZE;
    }
    tempest_app_init();                                     /* RESET .. MAINLN pass 1 */
    st_print_cells("boot pass 1");
    if (dump) {
        rc |= st_write_file(dump, "frame_0001.ram", g.ram, sizeof g.ram);
        rc |= st_write_file(dump, "frame_0001.vram", g.vram, sizeof g.vram);
        rc |= st_write_file(dump, "frame_0001.col", g.colram, sizeof g.colram);
    }
    if (write_img) {
        int idle;
        static const uint8_t sc[9] = { 0x12, 0x34, 0x05, 0x78, 0x56, 0x07, 0x90, 0x21, 0x09 };
        st_run(50);
        for (int i = 0; i < 9; i++) {
            g.ram[A_INITAL + 15 + i] = (uint8_t)(0x0A + i);
            g.ram[A_HSCORL + 15 + i] = sc[i];
            g.ram[A_BOOKKS + 3 + i]  = (uint8_t)(0x21 + i);
        }
        wrhiin();
        wrbook();
        for (idle = 0; idle < 3000 && (EAFLG != 0 || EAREQU != 0); idle++) st_run(1);
        printf("EAROM-TOOL: write: EAUPD idle after %d passes\n", idle);
        if (idle >= 3000) rc = 1;
    }
    st_run(passes);
    st_print_cells("exit");
    tempest_app_exit();                                     /* flush */
    if (dump) rc |= st_write_file(dump, "earom_exit.nv", earom.rom, sizeof earom.rom);
    if (write_img) rc |= st_write_file(NULL, write_img, earom.rom, sizeof earom.rom);
    printf("EAROM-TOOL: image");
    for (int i = 0; i < (int)TP_EAROM_SIZE; i++) printf("%s%02X", i % 16 ? " " : "\n    ", earom.rom[i]);
    printf("\nEAROM-TOOL: %s\n", rc ? "ERROR" : "done");
    plat_shutdown();
    return rc;
}

int main(int argc, char **argv)
{
    int attract = 3000, tool_passes = 0;
    const char *boot_img = NULL, *write_img = NULL, *dump = NULL;
    char d[400];

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--attract") && i + 1 < argc) attract = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--ref") && i + 1 < argc) st_ref = argv[++i];
        else if (!strcmp(argv[i], "--earom-boot") && i + 1 < argc) boot_img = argv[++i];
        else if (!strcmp(argv[i], "--earom-write") && i + 1 < argc) write_img = argv[++i];
        else if (!strcmp(argv[i], "--dump") && i + 1 < argc) dump = argv[++i];
        else if (!strcmp(argv[i], "--passes") && i + 1 < argc) tool_passes = atoi(argv[++i]);
        else {
            fprintf(stderr, "usage: tests\\tempest_selftest.exe [--attract N] [--ref DIR]\n"
                            "       tests\\tempest_selftest.exe --earom-boot IMAGE|blank --dump DIR [--passes N]\n"
                            "       tests\\tempest_selftest.exe --earom-write IMAGE [--dump DIR] [--passes N]\n");
            return 2;
        }
    }
    if (boot_img || write_img) {
        if (boot_img && write_img) { fprintf(stderr, "SELFTEST: --earom-boot and --earom-write are exclusive\n"); return 2; }
        return st_earom_tool(boot_img, write_img, dump, tool_passes);
    }
    setvbuf(stdout, NULL, _IONBF, 0);
    synthetic_only = 1;
    if (plat_init()) return 2;
    printf("SELFTEST: headless run of the real seam and loop (synthetic clock), %d attract passes\n", attract);

    /* ---- session 1: power on with a blank EAROM --------------------------- */
    hl_nvram_len = 0;
    tempest_app_init();
    {
        int defaults = 1;
        for (int i = 0; i < 24; i++) defaults &= g.ram[A_HSCORL + i] == 0x01;
        snprintf(d, sizeof d, "boots %lu, trips %lu, IRQs %u, cycles %llu, high scores all 010101: %s, QT4 %02X",
                 n_boots, wd_trips, (unsigned)g.irq_count, (unsigned long long)mach_cyc, defaults ? "yes" : "no", QT4);
        st_check(n_boots == 1 && wd_trips == 0 && defaults && QT4 == 0 && g.irq_count > 0,
                 "boot (blank EAROM, RESET, MAINLN)", d);
    }
    st_pass1_compare();

    /* ---- attract ---------------------------------------------------------- */
    {
        unsigned long list0 = st_list_changes, fr0 = n_frames, pic0 = n_pictures;
        uint64_t picc0 = pic_cyc_sum, cyc0 = mach_cyc;
        uint32_t irq0 = g.irq_count;
        int left_attract = 0;
        for (int i = 0; i < attract; i++) {
            st_run(1);
            if (QSTATUS & K_MATRACT) left_attract = 1;
        }
        snprintf(d, sizeof d, "%d passes, trips %lu, SYSTEM %lu, DSPSYS %lu, QSTATE=SYSTEM %lu, left attract %d",
                 attract, wd_trips, n_system, n_dspsys, st_csystm, left_attract);
        st_check(wd_trips == 0 && n_system == 0 && n_dspsys == 0 && st_csystm == 0 && !left_attract,
                 "attract: no watchdog, no SYSTEM", d);
        {
            double per = (double)(g.irq_count - irq0) / (double)attract;
            snprintf(d, sizeof d, "%.3f IRQs per pass (oracle attract 9.214; model %s), native IRQ access charge %.1f cycles/IRQ (model %.0f)",
                     per, pass_cost_on ? "on" : "off", (double)irq_seam_cyc / (double)g.irq_count, TP_IRQ_SEAM_CYC);
            st_check(!pass_cost_on || (per > 9.10 && per < 9.35), "attract: pass-cost model rate", d);
        }
        snprintf(d, sizeof d, "%lu frames, list changed on %lu passes, walks ending LOOP %lu, SWHALT %lu, broken %lu (last %s), %u lit segs last",
                 n_frames - fr0, st_list_changes - list0, n_frames - fr0 - n_walk_swhalt - n_walk_bad, n_walk_swhalt, n_walk_bad, avg_stop_name(last_stop), (unsigned)last_nlit);
        st_check(n_walk_bad == 0 && st_list_changes - list0 > (unsigned long)attract / 4 && hl_frames > 0,
                 "attract: display list live, LOOP", d);
        {
            /* the picture: the AVG's traversals of the looping list tile the machine time
             * (less the blanked SWHALT stretches), at the list's own draw time - not one per pass */
            unsigned long np = n_pictures - pic0;
            double pc = (double)(pic_cyc_sum - picc0), mc = (double)(mach_cyc - cyc0);
            double hz = pc > 0.0 ? TP_CPU_HZ * (double)np / pc : 0.0;
            snprintf(d, sizeof d, "%lu pictures in %d passes (%.2f per pass), period mean %.2f ms = %.2f Hz, min %.2f max %.2f ms, covering %.1f%% of machine time",
                     np, attract, (double)np / (double)attract, np ? pc / (double)np * 1000.0 / TP_CPU_HZ : 0.0, hz,
                     (double)pic_cyc_min * 1000.0 / TP_CPU_HZ, (double)pic_cyc_max * 1000.0 / TP_CPU_HZ, mc > 0.0 ? 100.0 * pc / mc : 0.0);
            st_check(np > (unsigned long)attract && hz > 35.0 && hz < 61.53 && pc / mc > 0.80 && pc / mc < 1.02,
                     "attract: picture rate from the AVG cycle count, <= 61.52 Hz", d);
        }
        snprintf(d, sizeof d, "extent x %.0f..%.0f y %.0f..%.0f; %.2f%% of %lu lit segs inside x+-290 y+-285",
                 bbox_minx, bbox_maxx, bbox_miny, bbox_maxy,
                 n_lit_total ? 100.0 * (double)n_lit_window / (double)n_lit_total : 0.0, n_lit_total);
        st_check(bbox_valid, "attract: segments reach the backend", d);
    }

    /* ---- the POKEY protection probe --------------------------------------- */
    st_probe();

    /* ---- a scripted game (timings like tests\scenarios\superzapper.txt) ---- */
    {
        uint8_t cr0 = S_S_CRDT, cr1, qs;
        int ok, zap_ok = 0, spin_ok = 0, fire_ok = 0, getini = 0, ended = 0, dir = 0;
        unsigned long game_passes = 0;
        uint8_t maxwav = 0, sc[3] = { 0, 0, 0 };

        hl_inputs.coin_r = 1; st_run(4); hl_inputs.coin_r = 0; st_run(196);
        cr1 = S_S_CRDT;
        snprintf(d, sizeof d, "right mech: credits %u -> %u", cr0, cr1);
        st_check(cr1 > cr0, "coin raises credits", d);

        hl_inputs.start1 = 1; st_run(3); hl_inputs.start1 = 0; st_run(27);
        qs = QSTATUS;
        snprintf(d, sizeof d, "QSTATUS $%02X (b7 game), QSTATE $%02X, credits %u", qs, QSTATE, S_S_CRDT);
        st_check((qs & K_MATRACT) != 0 && S_S_CRDT == cr1 - 1, "START 1 -> game", d);

        hl_inputs.fire = 1; st_run(3); hl_inputs.fire = 0;       /* RATE YOURSELF: level 1 */
        ok = st_wait_state(K_CPLAY, 600);
        snprintf(d, sizeof d, "QSTATE $%02X after rating, wave %u", QSTATE, CURWAV);
        st_check(ok, "rating accepted -> PLAY", d);
        st_run(80);                                              /* flippers leave the pool */

        for (int t = 0; t < 4 && !zap_ok; t++) {                 /* ZAP */
            uint8_t c0;
            if (!st_wait_state(K_CPLAY, 600)) break;
            c0 = SUZCNT;
            hl_inputs.zap = 1; st_run(3); hl_inputs.zap = 0;
            for (int i = 0; i < 30; i++) {
                if (SUZTIM != 0 && SUZCNT > c0) zap_ok = 1;
                st_run(1);
            }
            snprintf(d, sizeof d, "try %d: SUZCNT %u -> %u, SUZTIM %u", t + 1, c0, SUZCNT, SUZTIM);
        }
        st_check(zap_ok, "ZAP uses the superzapper", d);

        for (int t = 0; t < 4 && !spin_ok; t++) {                /* spin */
            uint8_t c0;
            if (!st_wait_state(K_CPLAY, 600)) break;
            c0 = CURSL1;
            for (int i = 0; i < 20 && !spin_ok; i++) {
                hl_inputs.spin_delta = 3;
                st_run(1);
                if (QSTATE == K_CPLAY && CURSL1 != c0) {
                    spin_ok = 1;
                    dir = (int)(((unsigned)CURSL1 - c0) & 0x0F);
                }
            }
            snprintf(d, sizeof d, "try %d: CURSL1 %u -> %u (spin_delta +3/pass: segment index %s)",
                     t + 1, c0, CURSL1, dir == 0 ? "?" : dir < 8 ? "increases" : "decreases");
        }
        st_check(spin_ok, "spin moves the player's segment", d);

        for (int t = 0; t < 4 && !fire_ok; t++) {                /* FIRE */
            uint8_t peak = 0;
            if (!st_wait_state(K_CPLAY, 600)) break;
            hl_inputs.fire = 1;
            for (int i = 0; i < 30 && !fire_ok; i++) {
                st_run(1);
                if (CHACOU > peak) peak = CHACOU;
                if (peak > 0) fire_ok = 1;
            }
            snprintf(d, sizeof d, "try %d: player charges CHACOU peak %u", t + 1, peak);
        }
        st_check(fire_ok, "FIRE spawns a shot", d);

        /* autofire + spin to game over, initials, attract */
        for (int i = 0; i < 12000; i++) {
            if (QSTATE == K_CGETINI) {
                getini = 1;
                hl_inputs.spin_delta = 0;
                hl_inputs.fire = 0; st_run(7);
                hl_inputs.fire = 1; st_run(3);
                hl_inputs.fire = 0;
                continue;
            }
            if (!(QSTATUS & K_MATRACT) && QSTATE != K_CHISCHK) { ended = 1; break; }
            hl_inputs.fire = 1;
            hl_inputs.spin_delta = 3;
            st_run(1);
            game_passes++;
            if (CURWAV > maxwav) maxwav = CURWAV;
            if (LSCORH > sc[0] || (LSCORH == sc[0] && (LSCORM > sc[1] || (LSCORM == sc[1] && LSCORL > sc[2])))) {
                sc[0] = LSCORH; sc[1] = LSCORM; sc[2] = LSCORL;
            }
        }
        hl_inputs.fire = 0; hl_inputs.spin_delta = 0;
        st_run(200);
        snprintf(d, sizeof d, "%lu passes, best wave %u, score %02X%02X%02X, high-score entry %s, now QSTATUS $%02X QSTATE $%02X",
                 game_passes, maxwav + 1u, sc[0], sc[1], sc[2], getini ? "yes" : "no", QSTATUS, QSTATE);
        st_check(ended && !(QSTATUS & K_MATRACT) && QSTATE != K_CGETINI, "game ends, back to attract", d);
        snprintf(d, sizeof d, "trips %lu, SYSTEM %lu, DSPSYS %lu, broken AVG walks %lu", wd_trips, n_system, n_dspsys, n_walk_bad);
        st_check(wd_trips == 0 && n_system == 0 && n_dspsys == 0 && n_walk_bad == 0, "game: no watchdog / SYSTEM", d);

        /* audio */
        {
            double expect = (double)audio_ticks * TP_AUDIO_RATE * TP_IRQ_CYCLES / TP_POKEY_HZ;
            double err = (double)audio_frames - expect;
            if (err < 0) err = -err;
            snprintf(d, sizeof d, "%llu frames pushed at %d Hz, %lu ticks since boot (expect %.1f, off %.2f), peak %d, underrun %lu",
                     (unsigned long long)hl_audio_frames, hl_audio_rate, audio_ticks, expect, err, hl_audio_peak, audio_underrun_frames);
            st_check(hl_audio_rate == TP_AUDIO_RATE && err <= 2.0 && hl_audio_peak > 0, "POKEY audio non-silent, rate-locked", d);
        }

        /* ---- EAROM: write the table, persist, second session ---------------- */
        {
            uint8_t top[18], top2[18];
            int injected = 0, idle, real_hs;
            real_hs = 0;
            for (int i = 0; i < 9; i++) real_hs |= g.ram[A_HSCORL + 15 + i] != 0x01;
            if (!getini || !real_hs) {                           /* the blind game did not rank */
                g.ram[A_HSCORL + 21] = 0x56; g.ram[A_HSCORM + 21] = 0x34; g.ram[A_HSCORH + 21] = 0x12;
                g.ram[A_INITAL + 21] = 0x02; g.ram[A_INITAL + 22] = 0x0C; g.ram[A_INITAL + 23] = 0x13;
                wrhiin();
                injected = 1;
            }
            for (idle = 0; idle < 3000 && (EAFLG != 0 || EAREQU != 0); idle++) st_run(1);
            st_top3(top);
            real_hs = 0;
            for (int i = 0; i < 9; i++) real_hs |= top[i] != 0x01;
            tempest_app_exit();
            snprintf(d, sizeof d, "%s, EAROM idle after %d passes, nvram %u bytes, %u writes, top score %02X%02X%02X",
                     injected ? "table injected + WRHIIN" : "the game's own entry (WRHIIN)", idle,
                     hl_nvram_len, hl_nvram_writes, top[8], top[7], top[6]);
            st_check(idle < 3000 && hl_nvram_len == TP_EAROM_SIZE && hl_nvram_writes > 0 && real_hs,
                     "EAROM written, image flushed", d);

            /* session 2 */
            n_boots = 0;
            tempest_app_init();
            st_top3(top2);
            snprintf(d, sizeof d, "after power-on from the image: top score %02X%02X%02X initials %02X %02X %02X, match %s",
                     top2[8], top2[7], top2[6], top2[15], top2[16], top2[17], memcmp(top, top2, sizeof top) ? "NO" : "yes");
            st_check(memcmp(top, top2, sizeof top) == 0 && n_boots == 1, "EAROM round trip (2 sessions)", d);
        }
    }

    /* ---- software watchdog: a trip re-boots through RESET ------------------ */
    {
        unsigned long trips0 = wd_trips, boots0 = n_boots, pass0;
        st_run(100);
        pass0 = g.pass_count;
        st_trip_armed = 1;
        hl_present_hook = st_trip_hook;
        st_run(1);
        hl_present_hook = NULL;
        st_run(300);
        snprintf(d, sizeof d, "trips %lu -> %lu, boots %lu -> %lu, passes after %lu, QT4 %02X",
                 trips0, wd_trips, boots0, n_boots, (unsigned long)(g.pass_count - pass0), QT4);
        st_check(wd_trips == trips0 + 1 && n_boots == boots0 + 1 && QT4 == 0, "soft watchdog re-boots (longjmp)", d);
    }

    /* ---- the real-clock path (the Windows build's): a clock that moves on
     * its own while the core spins or sleeps; IRQs must follow it ---------- */
    {
        double t0;
        uint32_t i0;
        tempest_app_exit();
        synthetic_only = 0;
        hl_clock_step_ms = 0.01;
        tempest_app_init();                 /* boots on the synthetic clock, then the wall clock */
        t0 = hl_now_ms; i0 = g.irq_count;
        st_run(500);
        {
            double ms = hl_now_ms - t0, expect = (double)(g.irq_count - i0) * 1000.0 / TP_IRQ_HZ;
            snprintf(d, sizeof d, "500 passes: %u IRQs in %.1f ms of clock (expect %.1f, ratio %.4f), trips %lu",
                     (unsigned)(g.irq_count - i0), ms, expect, ms / expect, wd_trips);
            st_check(ms / expect > 0.99 && ms / expect < 1.01 && hl_audio_frames > 0, "real-clock pacing", d);
        }
        hl_clock_step_ms = 0.0;
        synthetic_only = 1;
    }

    /* ---- M9 B5: the self test on the native seam ---------------------------- */
    {
        uint8_t game_image[TP_EAROM_SIZE];
        tempest_app_exit();
        memcpy(game_image, hl_nvram, sizeof game_image);    /* the table the game sessions wrote */
        st_selftest_boot();
        st_selftest_midrun(game_image);
        st_selftest_realclock();
        memset(&hl_inputs, 0, sizeof hl_inputs);
    }

    snprintf(d, sizeof d, "%lu passes checked%s%s", st_passes, st_qt_bad ? ", " : "", st_qt_first);
    st_check(st_qt_bad == 0, "QT1/QT2/QT4/QT5 == 0 throughout", d);

    tempest_app_exit();
    plat_shutdown();
    printf("SELFTEST: %s (%d of %d checks failed)\n", st_fails ? "FAIL" : "PASS", st_fails, st_checks);
    return st_fails ? 1 : 0;
}
#endif /* TEMPEST_SELFTEST_MAIN */
