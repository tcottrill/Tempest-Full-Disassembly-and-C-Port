/* plat_headless.c - Tempest C port: headless backend (see plat_headless.h).
 *
 * The whole tempest_platform.h contract with no window, no sound device and
 * no real clock.  Harvested from the Space Duel port, adapted in place (M8).
 */
#include <stddef.h>
#include <string.h>
#include "plat_headless.h"

plat_inputs hl_inputs;                 /* zeroed = nothing pressed */
uint8_t hl_dsw_n13    = 0x00;
uint8_t hl_dsw_l12    = 0x00;
uint8_t hl_dsw_pokey1 = 0x00;
uint8_t hl_dsw_pokey2 = 0x00;

double hl_now_ms = 0.0;
double hl_clock_step_ms = 0.0;

hl_seg  hl_segs[HL_MAX_SEGS];
int     hl_nsegs;
int     hl_frames;
uint8_t hl_flip;

void (*hl_vec_line_hook)(float, float, float, float, uint32_t, int) = NULL;
void (*hl_present_hook)(void) = NULL;
void (*hl_audio_hook)(const int16_t *, int) = NULL;

int      hl_audio_rate;
uint64_t hl_audio_frames;
int      hl_audio_peak;

uint8_t  hl_nvram[HL_NVRAM_MAX];
unsigned hl_nvram_len = 0;
unsigned hl_nvram_writes;

/* ---- lifecycle -------------------------------------------------------- */

int  plat_init(void)     { return 0; }
void plat_shutdown(void) { }

/* ---- video: capture --------------------------------------------------- */

void plat_video_begin(uint8_t flip) { hl_nsegs = 0; hl_flip = flip; }

void plat_video_line(float x0, float y0, float x1, float y1, uint32_t rgb, int intensity)
{
    if (hl_nsegs < HL_MAX_SEGS) {
        hl_seg *s = &hl_segs[hl_nsegs++];
        s->x0 = x0; s->y0 = y0; s->x1 = x1; s->y1 = y1;
        s->rgb = rgb; s->intensity = intensity;
    }
    if (hl_vec_line_hook) hl_vec_line_hook(x0, y0, x1, y1, rgb, intensity);
}

void plat_video_present(void)
{
    hl_frames++;
    if (hl_present_hook) hl_present_hook();
}

/* ---- input ------------------------------------------------------------ */

void plat_input_poll(plat_inputs *in)
{
    *in = hl_inputs;
    hl_inputs.spin_delta = 0;          /* counts are "since the last poll" */
}

uint8_t plat_dsw_n13(void)    { return hl_dsw_n13; }
uint8_t plat_dsw_l12(void)    { return hl_dsw_l12; }
uint8_t plat_dsw_pokey1(void) { return hl_dsw_pokey1; }
uint8_t plat_dsw_pokey2(void) { return hl_dsw_pokey2; }

/* ---- audio: counted, not played --------------------------------------- */

int plat_audio_open(int sample_rate)
{
    hl_audio_rate   = sample_rate;
    hl_audio_frames = 0;
    hl_audio_peak   = 0;
    return 0;
}

void plat_audio_push(const int16_t *pcm, int frames)
{
    int i;
    if (!pcm || frames <= 0) return;
    hl_audio_frames += (uint64_t)frames;
    for (i = 0; i < frames; i++) {
        int v = pcm[i] < 0 ? -(int)pcm[i] : (int)pcm[i];
        if (v > hl_audio_peak) hl_audio_peak = v;
    }
    if (hl_audio_hook) hl_audio_hook(pcm, frames);
}

void plat_audio_close(void) { hl_audio_rate = 0; }

/* ---- time: simulated --------------------------------------------------- */

double plat_now_ms(void)     { hl_now_ms += hl_clock_step_ms; return hl_now_ms; }
void   plat_sleep_ms(int ms) { hl_now_ms += (double)ms; }

/* ---- NVRAM: in-memory image -------------------------------------------- */

int plat_nvram_read(void *buf, unsigned len)
{
    if (hl_nvram_len == 0 || len > hl_nvram_len || len > HL_NVRAM_MAX)
        return 1;
    memcpy(buf, hl_nvram, len);
    return 0;
}

int plat_nvram_write(const void *buf, unsigned len)
{
    if (len > HL_NVRAM_MAX) return 1;
    memcpy(hl_nvram, buf, len);
    hl_nvram_len = len;
    hl_nvram_writes++;
    return 0;
}

/* ---- misc -------------------------------------------------------------- */

void plat_status_text(const char *s) { (void)s; }
