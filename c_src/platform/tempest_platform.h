/* tempest_platform.h - the platform contract for the Tempest C port (M8).
 *
 * The core (the translated modules + avg.c + mathbox.c + the chip models +
 * app_loop.c, the game seam) is platform-agnostic and calls ONLY these
 * functions for video, input, audio, time and storage.  Each backend under
 * platform/<name>/ implements the whole contract in plain C; the build links
 * exactly one backend per binary (link-time binding, no function pointers).
 *
 * Backends:
 *   headless/  plat_headless.c: injectable inputs, captured segments, a
 *              simulated clock, in-memory NVRAM.  tests\tempest_selftest.exe
 *              and tests\skeleton.exe.
 *   windows/   plat_win.c (M8 part 2): window, GL beam renderer, audio, raw
 *              input -> tempest_win.exe (build_win.bat).  sd_platform.h, the
 *              harvested Space Duel contract, is kept but no longer used.
 *
 * This file replaces sd_platform.h for Tempest; sd_platform.h is left as it
 * was harvested (PLAN.md decision: nothing gets renamed or removed).
 *
 * Hardware truth (which port bit means what, polarity, POKEY clocking, the
 * spinner counter) lives in app_loop.c.  Key bindings are backend policy.
 */
#ifndef TEMPEST_PLATFORM_H
#define TEMPEST_PLATFORM_H

#include <stdint.h>

/* ---- lifecycle --------------------------------------------------------- */
int  plat_init(void);            /* 0 = ok */
void plat_shutdown(void);

/* ---- video: the colour segment sink --------------------------------------
 * app_loop.c walks the AVG display list from $2000 (avg.c, avg_run_frame)
 * once per MAINLN pass - at MAINLN's frame wait, after DISPLAY built the list
 * (~27.3 passes/s: 246.09 Hz IRQ / 9) - and hands every LIT beam movement to
 * the backend between begin and present.  Dark moves are not sent.  The host
 * display resamples (present the newest frame each vsync).
 *
 * Coordinates (x0, y0, x1, y1): AVG space, exactly avg.c's - origin = the
 * beam centre (CNTR), +x right, +y up, 1.0 = one LSB of a 13-bit VCTR delta
 * at scale 1.0 (avg_seg Q15 / 32768).  NOT rotated and NOT flipped: the
 * backend applies the cabinet's geometry:
 *   - the monitor is mounted rotated (AAE/MAME ORIENTATION_ROTATE_270;
 *     AAE_DRIVER_SCREEN(1024, 768, 0, 580, 0, 570));
 *   - flip = the OUT0 ($4000) invert bits as the ROM last latched them,
 *     raw: $08 = K_MVINVX (video invert X), $10 = K_MVINVY (video invert Y).
 *     RESET sets TOUT0 = K_MVINVY on the upright, and COCFLI changes it for
 *     player 2 on a cocktail.  MAME's tempest driver feeds these to the AVG
 *     as flip_x / flip_y (mirror about the centre).  Confirmed on screen
 *     (M8 part 2, NOTES_m8.md): with flip = $10 this space is already the
 *     cabinet's upright, readable picture (text along +x, score at +y);
 *     relative to it X mirrors when $08 is set and Y when $10 is clear.
 *   Measured extent (headless self-test, attract): see NOTES_m8.md.
 * rgb: R | G << 8 | B << 16 | 0xFF << 24 (colordefs.h MAKE_RGB order), from
 *   the colour RAM byte ($0800-$080F, active low) of the segment's colour
 *   latch through avg_colram_rgb().
 * intensity: 1..15 (the STAT intensity latch or the vector's own z*2);
 *   AAE scales it << 4.
 * A zero-length segment is a dot. */
void plat_video_begin(uint8_t flip);
void plat_video_line(float x0, float y0, float x1, float y1,
                     uint32_t rgb, int intensity);
void plat_video_present(void);

/* ---- input ---------------------------------------------------------------
 * Polled once per MAINLN pass by app_loop.c, which encodes the ports:
 *   spin_delta  signed spinner counts since the previous poll.  The core
 *               adds them to the 4-bit counter on POKEY 1's ALLPOT b0-b3
 *               (at most +-7 per IRQ, the rest carried), the way
 *               tests\refrun.exe's scenario port "spin N" steps it: positive
 *               = the counter counts up (see NOTES_m8.md for the on-screen
 *               direction).
 *   fire zap start1 start2
 *               POKEY 2 ALLPO2 b4 b3 b5 b6, active high (1 = pressed).
 *   coin_l coin_c coin_r slam test diag
 *               IN1 $0C00 b2 b1 b0 b3 b4 b5, 1 = active (the core inverts:
 *               the lines are active low).  test = the self-test (TEST)
 *               switch, a latching switch on the board: closed at power-on
 *               (or at a JMP RESET / watchdog reboot) -> the ROM's power-on
 *               self test and diag loop; closed while the game runs ->
 *               SYSTEM / DSPSYS (options, bookkeeping); opened in the diag
 *               loop -> the hardware watchdog reboots into the game.
 *               diag = the diagnostic step button: held ~3 diag passes (as
 *               slam) steps the self-test screens.  M9 B5.
 *   quit        host affordance, never reaches the machine.
 * Polled once per pass of either loop (MAINLN ~27 Hz; the self test's diag
 * loop ~90-140 Hz) and before a hardware restart.
 * All fields other than spin_delta are 0/1. */
typedef struct {
    int     spin_delta;
    uint8_t fire, zap, start1, start2;
    uint8_t coin_l, coin_c, coin_r;
    uint8_t slam, test, diag;
    uint8_t quit;
} plat_inputs;

void plat_input_poll(plat_inputs *in);

/* Option switches, raw as the board reads them (0 = the AAE/refrun
 * defaults: 1 coin 1 play, English, 3 lives, 20000 bonus, medium,
 * rating 1-9, upright).
 *   plat_dsw_n13()    INOP0 $0D00 (N13 on the AVG PCB): coinage b0-1, right
 *                     coin mech b2-3, left coin mech b4, bonus coins b5-7
 *   plat_dsw_l12()    INOP1 $0E00 (L12), as INILIT reads it: b1-2 language,
 *                     b3-5 bonus life interval, b6-7 lives per game
 *                     (b0 is not read by INILIT)
 *   plat_dsw_pokey1() POKEY 1 ALLPOT option bits: b4 cocktail (K_COCKTA),
 *                     b5 special option (K_MOPTI4); b0-3 belong to the
 *                     spinner and are ignored
 *   plat_dsw_pokey2() POKEY 2 ALLPO2 option bits: b0-2 difficulty/rating
 *                     (K_MOPT13); b3-6 belong to the buttons and are ignored */
uint8_t plat_dsw_n13(void);
uint8_t plat_dsw_l12(void);
uint8_t plat_dsw_pokey1(void);
uint8_t plat_dsw_pokey2(void);

/* ---- audio ---------------------------------------------------------------
 * The two POKEYs (c012294.c) are clocked by the core on the machine
 * timeline (1.512 MHz, 6144 cycles per IRQ) and mixed with saturation into
 * ONE mono signed 16-bit stream at 44100 Hz, pushed one IRQ tick at a time
 * (~179.2 frames per push at the authentic rate).  In the self test's diag
 * loop (IRQ masked) the same ticks are pushed from machine time (M9 B5).
 * plat_audio_open returns 0 when the stream is up, nonzero for no audio
 * (the core keeps clocking the chips but pushes nothing). */
int  plat_audio_open(int sample_rate);
void plat_audio_push(const int16_t *pcm, int frames);
void plat_audio_close(void);

/* ---- time ---------------------------------------------------------------- */
double plat_now_ms(void);        /* monotonic, any epoch */
void   plat_sleep_ms(int ms);    /* scheduling hint; may return early */

/* ---- storage: the EAROM image ----------------------------------------------
 * One opaque blob: the ER2055's 64 bytes, raw, address order (the core owns
 * the layout).  read: 0 = ok and buf filled, nonzero = no stored image (the
 * core then starts from a blank part, every cell $FF - PLAN.md decision 4).
 * write: 0 = ok.  The core writes when the ROM deselects the chip after an
 * erase/write changed a cell, and at tempest_app_exit. */
int plat_nvram_read(void *buf, unsigned len);
int plat_nvram_write(const void *buf, unsigned len);

/* ---- misc ---------------------------------------------------------------- */
void plat_status_text(const char *s);    /* ~1/s pacing stats; may be a no-op */

/* ---- app hooks: what a backend's main loop calls (app_loop.c) ------------
 *   tempest_app_init()  power on: chips, EAROM image, RESET's power-on path
 *                       (on a synthetic clock), up to MAINLN's first frame.
 *   tempest_app_step()  poll inputs, run ONE pass of the loop the CPU is in:
 *                       a MAINLN pass (blocks inside the frame wait until the
 *                       pass's IRQs are due on the clock (plat_now_ms),
 *                       presenting the frame at the wait), or a diag-loop
 *                       pass of the self test (blocks until machine time is
 *                       due, presents at most 60 frames per machine second).
 *                       Returns 0.0 (a pass ran).  A software-watchdog trip,
 *                       DSPSYS's JMP RESET or a hardware watchdog bite inside
 *                       the pass re-boots through RESET.
 *   tempest_app_exit()  flush the EAROM image. */
void   tempest_app_init(void);
double tempest_app_step(double now_ms);
void   tempest_app_exit(void);

/* Host policy (M8 part 2): underclock the board so its IRQ runs at irq_hz
 * (<= 0 = the board's 246.09 Hz; 240 = AAE's rate).  Game speed, sound tempo
 * and pitch scale together.  Call before tempest_app_init. */
void   tempest_app_set_fps_lock(double irq_hz);

/* Inside plat_sleep_ms: the time to the next IRQ the core is idling for, ms
 * (> 2 whenever plat_sleep_ms is called).  A backend must not block longer. */
double tempest_app_idle_ms(void);

/* Diagnostics for a backend's pass log (M8 part 2): the last pass's hardware
 * accesses outside the IRQ, its display-list ops and the work the pass-cost
 * model charged (cycles, before the clamp). */
void   tempest_app_pass_info(unsigned long *io_r, unsigned long *io_w, uint32_t *ops, double *work);

/* M9 B5: the hardware watchdog's timeout in CPU cycles (1.512 MHz) since the
 * last $5000 strobe; <= 0 = the default 1,134,000 (0.75 s, refrun's / AAE's).
 * Only the self test's exit (TEST opened in the diag loop) waits for it. */
void   tempest_app_set_watchdog_cycles(double cycles);

/* M9 B5: counters for a backend's status line / log (cumulative since start). */
typedef struct {
    uint64_t      machine_cycles;       /* machine time, CPU cycles since tempest_app_init */
    int           diag;                 /* 1 = the CPU is in the self test's diag loop */
    unsigned long diag_passes;          /* diag-loop passes run */
    unsigned long boots;                /* RESET arrivals (power-on included) */
    unsigned long selftest_boots;       /* of which into the self test (TEST closed) */
    unsigned long watchdog_bites;       /* hardware watchdog reboots (WDGTST) */
    unsigned long jmp_resets;           /* DSPSYS JMP RESET */
    unsigned long soft_watchdog_trips;  /* the IRQ's software watchdog */
} tempest_app_stats;
void   tempest_app_get_stats(tempest_app_stats *s);

#endif /* TEMPEST_PLATFORM_H */
