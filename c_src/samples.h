/* samples.h - recorded-sample playback policy for the Space Duel C port.
 *
 * The catalogue and the hook design are documented in SOUNDS.md. The
 * translated sound module (sound.c) calls the sd_sample_* hooks below at
 * the points where the ROM starts/stops sounds; samples.c turns those
 * into plat_sample_* calls on the platform seam. Everything here is
 * host-side policy: no g.ram access, no effect on oracle parity.
 *
 * A sound plays only if its wav file exists (see sd_sample_names below;
 * the Windows backend looks for samples\<name>.wav, then <name>.wav
 * inside samples\spacduel.zip). Missing files stay silent.
 */
#ifndef SD_SAMPLES_H
#define SD_SAMPLES_H

#include <stdint.h>

/* Sample indices. Order is the trigger-code order (index = code >> 4)
 * with the non-scripted force-field hum appended. "1"/"2" = player 1/2
 * (ship 0 / ship 1). */
enum {
    SD_SMP_SAUCERFIRE = 0,  /* $0F saucer shot                            */
    SD_SMP_FIRE1,           /* $1F player 1 fire                          */
    SD_SMP_FIRE2,           /* $2F player 2 fire                          */
    SD_SMP_RESPAWN1,        /* $3F player 1 ship materialize (Reenter2)   */
    SD_SMP_RESPAWN2,        /* $4F player 2 ship materialize (Reenter)    */
    SD_SMP_EXTRALIFE,       /* $5F bonus-life jingle                      */
    SD_SMP_EXPLOSION,       /* $6F any explosion                          */
    SD_SMP_THRUST,          /* $7F thrust (loopable; held)                */
    SD_SMP_FUSE1,           /* $8F fuse crackle P1 (loopable; latched)    */
    SD_SMP_FUSE2,           /* $9F fuse crackle P2 (loopable; latched)    */
    SD_SMP_HIGHTUNE,        /* $AF high-score tune                        */
    SD_SMP_SHIELD1,         /* $BF shield P1 (loopable; held)             */
    SD_SMP_SHIELD2,         /* $CF shield P2 (loopable; held)             */
    SD_SMP_POP,             /* $DF rock pop                               */
    SD_SMP_FORCEFIELD,      /* challenge-stage hum (loopable; pitch rises,
                             * record it at the END-of-challenge pitch -
                             * it is shifted DOWN up to 5.2x early on)    */
    SD_SMP_COUNT
};

/* wav base names, indexed by the enum: samples\<name>.wav */
extern const char* const sd_sample_names[SD_SMP_COUNT];

/* If <name>.wav is missing, the backend may load this index's file
 * instead (-1 = none). Lets one wav serve both players. */
extern const signed char sd_sample_fallback[SD_SMP_COUNT];

/* ---- hooks called by the translated code (sound.c) -------------------- */
void sd_sample_trigger(uint8_t code);   /* badhab(): a sound starts       */
void sd_sample_fuse_stop(void);         /* stop_fuse_sound()              */
void sd_sample_stop_all(void);          /* inisou()                       */
void sd_sample_hum(int on, uint8_t sfreq); /* force_field_up(), per frame */

/* ---- called by app_loop.c once per displayed frame --------------------
 * Ages the held (retriggered) sounds: thrust/shield loops stop when the
 * game stops re-firing their trigger. */
void sd_sample_frame(void);

#endif /* SD_SAMPLES_H */
