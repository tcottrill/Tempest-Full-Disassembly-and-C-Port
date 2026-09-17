/* samples.c - recorded-sample playback policy (see samples.h, SOUNDS.md).
 *
 * Mixer-channel model = the hardware voices, so a new sound on a voice
 * cuts the old one exactly where the ROM's script engine would:
 *
 *   channel 0  POKEY1 voice 0: P1 fire / respawn / fuse / shield / tune
 *   channel 1  POKEY1 voice 1: saucer fire, pop
 *   channel 2  POKEY1 voice 2: extra-life jingle
 *   channel 3  POKEY1 voice 3: the same set as ch 0 for player 2
 *   channel 4  POKEY2 voice 0: thrust
 *   channel 5  POKEY2 voices 2+3: explosion
 *   channel 6  POKEY2 voice 1: force-field hum (direct-register sound)
 *
 * Trigger modes:
 *   ONESHOT  play once; a retrigger restarts it (as reseeding the script
 *            pointers does on the real machine).
 *   HELD     looping; the game re-fires the trigger while the control is
 *            held (thrust every other frame per ship, shield every
 *            frame), so the loop stops after HOLD_TIMEOUT frames without
 *            a retrigger.
 *   LATCHED  looping until an explicit stop: the fuse crackle's triggers
 *            are random (1-in-8 per frame), so it cannot time out; the
 *            ROM ends it with StopFuseSound (or by voice stealing).
 */
#include "samples.h"
#include "platform/sd_platform.h"

const char* const sd_sample_names[SD_SMP_COUNT] = {
    "saucerfire", "fire1", "fire2", "respawn1", "respawn2",
    "extralife", "explosion", "thrust", "fuse1", "fuse2",
    "hightune", "shield1", "shield2", "pop", "forcefield",
};

const signed char sd_sample_fallback[SD_SMP_COUNT] = {
    -1, -1, SD_SMP_FIRE1, -1, SD_SMP_RESPAWN1,
    -1, -1, -1, -1, SD_SMP_FUSE1,
    -1, -1, SD_SMP_SHIELD1, -1, -1,
};

enum { CH_P1V0, CH_P1V1, CH_P1V2, CH_P1V3, CH_P2V0, CH_EXPLO, CH_HUM,
       CH_COUNT };

enum { M_ONESHOT, M_HELD, M_LATCHED };

#define HOLD_TIMEOUT 6          /* frames without a retrigger = released */

static const struct {
    uint8_t code;               /* trigger code; index = code >> 4       */
    int8_t  smp, ch, mode;
} trig_map[14] = {
    { 0x0F, SD_SMP_SAUCERFIRE, CH_P1V1,  M_ONESHOT },
    { 0x1F, SD_SMP_FIRE1,      CH_P1V0,  M_ONESHOT },
    { 0x2F, SD_SMP_FIRE2,      CH_P1V3,  M_ONESHOT },
    { 0x3F, SD_SMP_RESPAWN1,   CH_P1V0,  M_ONESHOT },
    { 0x4F, SD_SMP_RESPAWN2,   CH_P1V3,  M_ONESHOT },
    { 0x5F, SD_SMP_EXTRALIFE,  CH_P1V2,  M_ONESHOT },
    { 0x6F, SD_SMP_EXPLOSION,  CH_EXPLO, M_ONESHOT },
    { 0x7F, SD_SMP_THRUST,     CH_P2V0,  M_HELD    },
    { 0x8F, SD_SMP_FUSE1,      CH_P1V0,  M_LATCHED },
    { 0x9F, SD_SMP_FUSE2,      CH_P1V3,  M_LATCHED },
    { 0xAF, SD_SMP_HIGHTUNE,   CH_P1V0,  M_ONESHOT },
    { 0xBF, SD_SMP_SHIELD1,    CH_P1V0,  M_HELD    },
    { 0xCF, SD_SMP_SHIELD2,    CH_P1V3,  M_HELD    },
    { 0xDF, SD_SMP_POP,        CH_P1V1,  M_ONESHOT },
};

static struct {
    int8_t   smp;               /* what the channel is playing, -1 idle  */
    int8_t   mode;
    uint32_t last;              /* frame of the most recent trigger      */
} chs[CH_COUNT] = { { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 },
                    { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 } };

static uint32_t frame_no;
static int      hum_on;
static uint8_t  hum_sfreq;

void sd_sample_trigger(uint8_t code)
{
    int i = code >> 4;

    if (i >= 14 || trig_map[i].code != code)
        return;
    {
        int ch   = trig_map[i].ch;
        int smp  = trig_map[i].smp;
        int mode = trig_map[i].mode;

        /* a loop that is already up just gets its hold refreshed */
        if (mode != M_ONESHOT && chs[ch].smp == smp) {
            chs[ch].last = frame_no;
            return;
        }
        plat_sample_start(ch, smp, mode != M_ONESHOT);
        chs[ch].smp  = (int8_t)smp;
        chs[ch].mode = (int8_t)mode;
        chs[ch].last = frame_no;
    }
}

void sd_sample_fuse_stop(void)
{
    /* StopFuseSound kills whatever is on both player voices (the ROM
     * zeroes script channels 0/1/6/7 unconditionally). */
    plat_sample_stop(CH_P1V0);  chs[CH_P1V0].smp = -1;
    plat_sample_stop(CH_P1V3);  chs[CH_P1V3].smp = -1;
}

void sd_sample_stop_all(void)
{
    int ch;

    for (ch = 0; ch < CH_COUNT; ch++) {
        plat_sample_stop(ch);
        chs[ch].smp = -1;
    }
    hum_on = 0;                 /* force_field_up restarts it next frame
                                 * if the challenge is still running     */
}

void sd_sample_hum(int on, uint8_t sfreq)
{
    if (!on) {
        if (hum_on) {
            plat_sample_stop(CH_HUM);
            hum_on = 0;
        }
        return;
    }
    if (!hum_on) {
        plat_sample_start(CH_HUM, SD_SMP_FORCEFIELD, 1);
        hum_on    = 1;
        hum_sfreq = (uint8_t)~sfreq;        /* force the first set below */
    }
    if (sfreq != hum_sfreq) {
        /* POKEY pitch is base/(2*(divider+1)); SFREQ walks $FF -> $30
         * over the challenge, so with the wav recorded at the final
         * (highest) pitch the ratio is (0x30+1)/(sfreq+1): 0.19 at the
         * start, rising to 1.0 as the challenge ends. */
        hum_sfreq = sfreq;
        plat_sample_freq(CH_HUM, 49.0f / (float)(sfreq + 1));
    }
}

void sd_sample_frame(void)
{
    int ch;

    frame_no++;
    for (ch = 0; ch < CH_COUNT; ch++) {
        if (chs[ch].smp >= 0 && chs[ch].mode == M_HELD &&
            frame_no - chs[ch].last > HOLD_TIMEOUT) {
            plat_sample_stop(ch);
            chs[ch].smp = -1;
        }
    }
}
