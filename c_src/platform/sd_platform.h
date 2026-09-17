/* sd_platform.h - the platform contract for the Space Duel C port.
 *
 * Modeled on the Omega Race port's omega_platform.h. The core (game
 * modules + avg.c + app_loop.c) is platform-agnostic and calls ONLY
 * these functions for rendering, input, audio, time and storage. Each
 * backend under platform/<name>/ implements the whole contract in plain
 * C; the build links exactly one backend per binary. Link-time binding,
 * no function pointers - /W4 catches signature drift.
 *
 * Backends:
 *   windows/   the Windows core (Win32 + OpenGL 3.3); owns WinMain().
 *              Builds sd_win.exe.
 *   headless/  shared harness backend (probes): injectable inputs and
 *              captured plat_video_line segments, no window/clock; the
 *              audio stream is counted, not played.
 */
#ifndef SD_PLATFORM_H
#define SD_PLATFORM_H

#include <stdint.h>

/* ---- lifecycle --------------------------------------------------------- */

int  plat_init(void);      /* window/audio up; 0 = ok, nonzero = fail */
void plat_shutdown(void);

/* ---- video: the color segment sink -------------------------------------
 * Space Duel is a COLOR vector game: the AVG's STAT word carries a 3-bit
 * color (RGB bits) and a 4-bit intensity. The AVG walker (core, avg.c)
 * emits every visible segment through plat_video_line:
 *
 *   color 0..7   STAT bits 2..0; per the AVG hardware (and the AAE/MAME
 *                transcription, aae_avg.cpp draw_avg/VECTOR_COLOR111):
 *                bit2 = RED, bit1 = GREEN, bit0 = BLUE.
 *                1=blue 2=green 3=cyan 4=red 5=magenta 6=yellow 7=white.
 *   lum   0..15  beam intensity (STAT bits 7..4, or the VCTR word's own
 *                Z). The GUI maps it (lum<<4)|0x0f, matching AAE.
 *
 * Coordinates are the AVG's screen window for this board: x 0..520,
 * y 0..395, y up (AAE drv_spacduel AAE_DRIVER_SCREEN(1024,768, 0,520,
 * 0,395)). A zero-length segment is a point. A raster backend draws
 * between begin/present; a vector-CRT backend would buffer and replay
 * to the DACs after present. */
void plat_video_begin(void);                    /* start of frame / clear */
void plat_video_line(float x0, float y0, float x1, float y1,
                     int color, int lum);
void plat_video_present(void);                  /* flip                   */
void plat_video_blank(void);                    /* a dropped frame: NOTHING
                                                   new drawn - black, or
                                                   the fading afterglow if
                                                   the phosphor is on
                                                   (app_loop.c, "dropped
                                                   frames")               */

/* ---- input: abstract controls ------------------------------------------
 * Polled once per displayed frame by the core (app_loop.c), which owns
 * the hardware truth - the IN0/IN1 bit encodings live there, not here.
 * Key bindings are backend policy. All fields are 0/1. No spinner:
 * Space Duel rotates with two buttons. */
typedef struct {
    uint8_t fire, thrust, shield;
    uint8_t rotate_left, rotate_right;
    uint8_t start1, start2;
    uint8_t game_select;     /* cabinet "select game" button             */
    uint8_t coin1, coin2, coin3;  /* left / right / utility coin lines   */
    uint8_t test;            /* cabinet self-test switch line            */
    uint8_t diag_step;       /* cabinet diagnostic-step button           */
    uint8_t diag;            /* host affordance: F2 = enter diagnostics  */
    uint8_t quit;
} plat_inputs;

void plat_input_poll(plat_inputs* in);

/* Option (DIP) switch banks, read through the two POKEYs' ALLPOT lines
 * (POKEY1 $1008, POKEY2 $1408). Platform-owned so a hardware target can
 * wire the real switches; the core interprets the bits. */
uint8_t plat_dsw_pokey1(void);
/* Diagnostic: 0 = run the POKEY cores one clock at a time, 1 (default) =
 * step event-free clock runs together (identical output, far cheaper).
 * Windows: [sound] pokey_skip in sd_win.ini. */
int     plat_pokey_skip(void);
uint8_t plat_dsw_pokey2(void);

/* ---- audio --------------------------------------------------------------
 * Two seams:
 *  - plat_sample_*: the recorded-sample path (live since 2026-08-30).
 *    The channel/sample map is core policy (samples.c; indices and wav
 *    names in samples.h); the backend plays wav #sample on a mixer
 *    channel. plat_sample_freq scales the playing sample's pitch
 *    relative to its native rate (1.0 = as recorded) - used for the
 *    force-field hum's rising tone. All no-ops when the wav is missing.
 *  - plat_audio_*: the POKEYs' own output (since 2026-09-03). The core
 *    owns two real POKEY chips (c012294.c, driven register-for-register by
 *    the translated ROM through sd_hw_pokey_write) and renders their
 *    mixed output itself; this seam is only a continuously-fed mono
 *    16-bit PCM stream, pushed a block at a time (one 246 Hz IRQ tick's
 *    worth, ~179 frames at 44.1 kHz) rather than loaded as a sample.
 *    plat_audio_open brings the stream up at sample_rate and returns 0,
 *    or nonzero when there is no audio (the core then stops rendering);
 *    plat_audio_push queues `frames` samples; plat_audio_close tears it
 *    down. Same contract as the Asteroids Deluxe port's ad_platform.h. */
void plat_sample_start(int channel, int sample, int loop);
void plat_sample_stop(int channel);
void plat_sample_freq(int channel, float ratio);
int  plat_audio_open(int sample_rate);
void plat_audio_push(const int16_t* pcm, int frames);
void plat_audio_close(void);

/* ---- time --------------------------------------------------------------- */

double plat_now_ms(void);            /* monotonic, any epoch              */
void   plat_sleep_ms(int ms);        /* scheduling hint; may return early */

/* ---- storage: NVRAM (EAROM) as one opaque blob --------------------------
 * The core owns the layout (the sd_c.nv byte order); the backend stores
 * bytes. Return 0 = ok, nonzero = no stored image / write failed. */
int plat_nvram_read(void* buf, unsigned len);
int plat_nvram_write(const void* buf, unsigned len);

/* ---- misc --------------------------------------------------------------- */

void plat_leds_out(uint8_t out_shadow); /* $0C00 shadow: coin counters,
                                           start LEDs, flip; real pins on
                                           a hardware target              */
void plat_status_text(const char* s);   /* pacing/fps stats, ~1/s; the
                                           window title on PC, no-op
                                           elsewhere                      */

/* ---- core app hooks - what the windows backend's WinMain calls ----------
 * Implemented by app_loop.c once it exists; until then by main_stub.c
 * (the temporary color-path demo). Mirrors Omega Race's omega_app_*. */
void   sd_app_init(void);
double sd_app_step(double now_ms);  /* run machine time + maybe one frame;
                                       returns ms until next due frame
                                       (0 = a frame just ran)             */
void   sd_app_exit(void);           /* flush NVRAM etc. before shutdown  */
void   sd_app_set_fps_lock(double fps); /* > 0: underclock the whole
                                       machine so the frame gate lands on
                                       this rate (e.g. 60 for a 60 Hz
                                       panel; 2.48% slow). 0 = authentic
                                       61.5234 Hz. Call any time.        */
void   sd_app_set_refresh(double hz);   /* the panel's refresh rate: while
                                       the CPU is in a test loop the
                                       picture is presented at this rate
                                       and a refresh no VGGO reached is
                                       blank (the 44 fps screen's steady
                                       flicker). <= 0: never.            */
void   sd_app_set_dropped_frame(int irqs, double hold_ms);
                                    /* a pass that spends `irqs` IRQ
                                       periods shows the cabinet's dropped
                                       frame (nothing drawn) from the
                                       moment the AVG finishes its list to
                                       the next VGGO, plus `hold_ms`.
                                       7 = the ROM's own overrun passes;
                                       0 = never.  hold 0 = the tube's own
                                       timing.                            */

#endif /* SD_PLATFORM_H */
