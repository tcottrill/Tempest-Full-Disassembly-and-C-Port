/* plat_headless.h - injection/capture hooks for the headless backend.
 *
 * Harvested from the Space Duel port (itself from Omega Race) and adapted in
 * place at M8 to the Tempest contract, platform/tempest_platform.h.  Harness
 * builds (tests\tempest_selftest.exe, tests\skeleton.exe) link
 * plat_headless.c as their backend: the whole contract with no window, no
 * sound device and no real clock.  Inputs and option switches are injected;
 * every plat_video_line segment of the current frame is captured; the audio
 * stream is counted (frames, peak); NVRAM is an in-memory image; time is a
 * simulated clock the harness (or plat_sleep_ms) advances.
 */
#ifndef PLAT_HEADLESS_H
#define PLAT_HEADLESS_H

#include <stdint.h>
#include "../tempest_platform.h"

/* ---- injected inputs -------------------------------------------------- */
extern plat_inputs hl_inputs;      /* copied out by plat_input_poll; spin_delta
                                      is consumed (zeroed) by each poll       */
extern uint8_t hl_dsw_n13;         /* INOP0, default 0x00                     */
extern uint8_t hl_dsw_l12;         /* INOP1, default 0x00                     */
extern uint8_t hl_dsw_pokey1;      /* ALLPOT option bits, default 0x00        */
extern uint8_t hl_dsw_pokey2;      /* ALLPO2 option bits, default 0x00        */

/* ---- simulated clock --------------------------------------------------- */
extern double hl_now_ms;           /* plat_now_ms returns this;
                                      plat_sleep_ms(ms) advances it           */
extern double hl_clock_step_ms;    /* > 0: every plat_now_ms call also advances
                                      the clock by this much (a stand-in for
                                      a wall clock that moves while the core
                                      spins; 0 = default, fully simulated)   */

/* ---- captured video ---------------------------------------------------- */
typedef struct {
    float    x0, y0, x1, y1;
    uint32_t rgb;
    int      intensity;
} hl_seg;

#define HL_MAX_SEGS 8192
extern hl_seg  hl_segs[HL_MAX_SEGS]; /* the current frame's segments         */
extern int     hl_nsegs;             /* reset by plat_video_begin            */
extern int     hl_frames;            /* frames completed (present count)     */
extern uint8_t hl_flip;              /* the last plat_video_begin flip bits  */

/* optional hooks, NULL = just the buffer / discard */
extern void (*hl_vec_line_hook)(float x0, float y0, float x1, float y1,
                                uint32_t rgb, int intensity);
extern void (*hl_present_hook)(void);
extern void (*hl_audio_hook)(const int16_t *pcm, int frames);

/* ---- the POKEY audio stream, counted ----------------------------------- */
extern int      hl_audio_rate;     /* plat_audio_open's rate, 0 = closed     */
extern uint64_t hl_audio_frames;   /* frames pushed since open               */
extern int      hl_audio_peak;     /* loudest |sample| pushed since open     */

/* ---- NVRAM: in-memory image -------------------------------------------- */
#define HL_NVRAM_MAX 256
extern uint8_t  hl_nvram[HL_NVRAM_MAX];
extern unsigned hl_nvram_len;      /* 0 = no stored image (read fails)       */
extern unsigned hl_nvram_writes;   /* plat_nvram_write calls                 */

#endif /* PLAT_HEADLESS_H */
