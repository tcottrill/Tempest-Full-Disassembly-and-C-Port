/* avg.h - Space Duel C port: the Analog Vector Generator state machine.
 *
 * A C11 transcription of the AVG interpreter, walking the REAL memory:
 * g.vram (CPU $2000-$27FF) and sd_vecrom (CPU $2800-$3FFF). Semantics follow
 * the reference implementation the project verified avg.py against:
 * AAE_publish_new_vector_test\aae\aae\vidhrdwr\aae_avg.cpp (generic USE_AVG
 * path, avg_video_update), cross-read with mame_late_avgdvg.cpp. See
 * DESIGN.md "JMPL address fold (RESOLVED)" for the memory map evidence.
 *
 * MEMORY MAP (flat, no mirror - resolved in DESIGN.md):
 *   AVG word address $000-$3FF -> g.vram      (CPU $2000-$27FF, display list)
 *   AVG word address $400-$7FF -> sd_vecrom   (CPU $2800-$2FFF, ship-picture
 *                                 ROM: real bytes, but never legitimately
 *                                 executed - fetching here sets a flag)
 *   AVG word address $800-$FFF -> sd_vecrom   (CPU $3000-$3FFF, vector ROM)
 *   AVG word address >= $1000  -> program ROM, not carried here: the walk
 *                                 stops with AVG_STOP_BADFETCH.
 *
 * COORDINATE SPACE emitted to the segment callback:
 *   - origin (0,0) is the beam center = the CNTR target = screen center;
 *     a run starts there (aae_avg.cpp seeds currentx/y = xcenter/ycenter);
 *   - +x is right, +y is UP (the hardware deflection sense; aae_avg.cpp
 *     does "currenty -= deltay" only because its raster y grows down);
 *   - unit = one LSB of the 13-bit VCTR delta at scale factor 1.0.
 *     Scale factor = ((~w) & 0xFF)/256 / 2^((w>>8)&7): the linear part is
 *     COMPLEMENTED, so SCAL $7000 is ~full size (255/256) and the game's
 *     ambient SCAL 1,$00 is ~0.498. Scale starts at 0.0 like the reference,
 *     so vectors before the first SCAL have zero length.
 *   - no clipping/window is modeled: the generic AVG of the bwidow/spacduel
 *     hardware has none in the reference (clipping exists only in the
 *     bzone/mhavoc variants).
 *
 * COLOR / INTENSITY:
 *   STAT ($6xxx, bit12 clear; the game writes $64xx = (lum<<4)|color on this
 *   color hardware): color = w & 7, intensity register = (w>>4) & 0xF
 *   (aae_avg.cpp STAT default arm; mame_late_avgdvg.cpp avg_strobe2).
 *   A vector's 3-bit z field: 0 = dark move, 1 = use the STAT intensity
 *   register, else lum = z<<1 (aae doubles z and tests z==2 for the
 *   register). The callback gets lum 0-15; dark moves ARE reported
 *   (lum == 0) so callers can trace the beam - filter on lum > 0 to draw.
 *
 * TIMING (cycle-true - the mame_late_avgdvg.cpp state-machine model):
 *   avg_frame_cycles() / avg_frame_time_ms() give the draw time of the last
 *   run in AVG master-clock cycles / milliseconds. Clock: MASTER_CLOCK =
 *   12,096,000 Hz (mame_late_avgdvg.cpp line 45); the 256x4 state PROM
 *   (136002-125.n4) is stepped every 8 master cycles (run_state_machine,
 *   line 1271), i.e. at 1.512 MHz. Per-opcode PROM tick counts were traced
 *   from the real PROM (VCTR 8, HALT 2, SVEC 6, STAT/SCAL 7, CNTR 5,
 *   JSRL 5, RTSL 4, JMPL 3 ticks of 8 cycles; avg.c has the full table),
 *   plus the strobe3 draw durations from the timer shift register:
 *   VCTR/CNTR run 0x8000 - timer, SVEC 0x100 - (timer & 0xFF) master
 *   cycles, where the timer is filled by the normalizer (avg_strobe0) and
 *   the SCAL binary-scale shifter (avg_strobe1). The per-op accounting
 *   reproduces the full PROM-driven state machine cycle-for-cycle on every
 *   captured attract frame. avg_draw_time_units() keeps the older AAE
 *   approximation (sum of max(|dx|,|dy|) beam units, aae_avg.cpp
 *   vector_timer, 1500 ns/unit) for continuity with earlier measurements.
 *
 * JSRL stack: 8 entries deep like aae_avg.cpp (MAXSTACK); the real hardware
 * has a 4-level stack (mame_late_avgdvg.cpp stack[sp & 3]) - Space Duel
 * never nests deeper than the hardware allows, so the difference is inert.
 * Underflow/overflow stop the walk like the reference. A JSRL/JMPL with
 * word operand 0 stops the walk (aae_avg.cpp treats target 0 as done).
 */
#ifndef AVG_H
#define AVG_H

#include <stdint.h>

/* Why the walk ended (query after a run). */
typedef enum {
    AVG_STOP_HALT = 0,       /* HALT opcode - the normal end               */
    AVG_STOP_JUMP0,          /* JSRL/JMPL word operand 0 (reference quirk) */
    AVG_STOP_STACK_OVER,     /* 8th nested JSRL                            */
    AVG_STOP_STACK_UNDER,    /* RTSL with an empty stack                   */
    AVG_STOP_BADFETCH,       /* PC reached word $1000+ (program ROM)       */
    AVG_STOP_RUNAWAY         /* fetch-count guard tripped (no HALT found)  */
} avg_stop;

/* Diagnostic flag bits (query after a run). */
#define AVG_FLAG_SHIPROM 0x01  /* PC entered words $400-$7FF (CPU $2800-$2FFF:
                                * ship-picture ROM - real bytes were fetched,
                                * as the hardware would, but nothing
                                * legitimate ever executes there) */

/* Walk the display list from word address 0 (CPU $2000) - what VGGO does.
 * seg() is called once per VCTR/SVEC, dark moves included (lum == 0);
 * color is the 3-bit STAT color, lum the resolved 4-bit intensity.
 * out_words_walked (may be NULL) receives the number of 16-bit words
 * fetched, dark or lit, including both words of each VCTR. */
void avg_run(void (*seg)(float x0, float y0, float x1, float y1,
                         int color, int lum),
             int *out_words_walked);

/* Same, from an arbitrary start (word address = (CPU - $2000) / 2). */
void avg_run_from(uint16_t word_addr,
                  void (*seg)(float x0, float y0, float x1, float y1,
                              int color, int lum),
                  int *out_words_walked);

avg_stop avg_last_stop(void);       /* why the last run ended             */
int      avg_last_flags(void);      /* AVG_FLAG_* bits from the last run  */
double   avg_draw_time_units(void); /* legacy AAE model: beam units       */

/* Cycle-true draw time of the last run (mame_late_avgdvg.cpp model):
 * 12.096 MHz master-clock cycles from VGGO until the CPU sees VG HALT.
 * ms = cycles / 12096.0. */
uint32_t avg_frame_cycles(void);
double   avg_frame_time_ms(void);

#endif /* AVG_H */
