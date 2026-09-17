/* avg.h - Tempest C port: the Analog Vector Generator display-list walker.
 *
 * Adapted from the Space Duel port's walker (harvest/avg.c) with Tempest's
 * semantics, as decoded by ../disasm/avg.py and checked against
 * tempest-main/VGMC.MAC, ALVROM.MAC and the AAE/MAME AVG core that
 * ../tempest.cpp uses (mame_late_avgdvg.cpp: tempest_strobe2/3,
 * avg_common_strobe1/2/3).
 *
 * MEMORY MAP (flat, the AVG's own address space = CPU $2000-$3FFF)
 *   $2000-$2FFF  4K vector RAM  (g.vram, or any 4K byte array)
 *   $3000-$3FFF  4K vector ROM  (vecrom[], or any 4K byte array)
 *   JSRL/JMPL target = $2000 + 2 * (word & $1FFF).  A fetch at $4000 or above
 *   (reachable only through a corrupt operand) stops the walk AVG_STOP_BADADDR.
 *
 * OPCODES (word >> 13; the first word of each instruction, little-endian)
 *   0 VCTR  2 words: dy = s13(w1), dx = s13(w2), z = w2 >> 13
 *   1 HALT
 *   2 SVEC  dx = 2*s5(w), dy = 2*s5(w >> 8), z = (w >> 5) & 7
 *   3 STAT  bit 12 clear.  bit 11 ($0800) set: COLOUR latch = w & $F
 *           (ALVROM CSTAT = $68C0+c); clear: INTENSITY latch = (w >> 4) & $F.
 *           No sparkle / X-flip bits (those are Major Havoc's).
 *     SCAL  bit 12 set: binary scale b = (w >> 8) & 7, linear l = w & $FF.
 *   4 CNTR  beam to centre (any $8xxx word)
 *   5 JSRL  6 RTSL  7 JMPL
 *   Beam intensity of a VCTR/SVEC: z == 1 -> the STAT intensity latch,
 *   otherwise 2*z (0 = dark move).  Colour: the last colour STAT.
 *
 * COORDINATES
 *   Q15 fixed point (1.0 = 32768), origin = CNTR position, +x right, +y up,
 *   unit = one LSB of a 13-bit VCTR delta at scale 1.0.  Scale factor =
 *   (255 - l) / 256 / 2^b, i.e. scale_q15 = (255 - l) << (7 - b), so every
 *   position is an exact integer (and an exact double: see AVG_Q15_TO_F).
 *   This is the ideal-geometry model of disasm/vrender.py; the board's DAC
 *   truncation after normalisation (sub-LSB) is not modelled.
 *   For display (M8): Tempest's monitor is mounted rotated; AAE's
 *   tempest_strobe3 transposes (x, y) -> ((xc + yc) - y, x - xc + yc) in its
 *   own raster space.  The walker stays in AVG space.
 *
 * STACK
 *   The board has a 4-slot stack addressed by a 4-bit counter
 *   (stack[sp & 3], sp = (sp +/- 1) & $F): a 5th nested JSRL silently
 *   overwrites slot 0.  The walker models exactly that and raises
 *   AVG_FLAG_STACK_OVER.  An RTSL at depth 0 ends the walk (AVG_STOP_RTSL):
 *   that is how a subroutine / shape walk finishes.  (Hardware would pop a
 *   stale slot; see AVG_FLAG_* and the open issues in the port notes.)
 *
 * FRAME END
 *   Tempest's master lists (SWNORM $3DB4, SWMSGS $3DC8) end in JMPL VECRAM,
 *   so the AVG never HALTs.  A walk stops when a JMPL at stack depth 0
 *   targets the walk's start address (AVG_STOP_LOOP), on HALT, or when the
 *   instruction budget runs out (AVG_STOP_BUDGET).
 *
 * TIMING (harvest/avg.c's cycle-true model; Tempest has the same state PROM,
 *   136002-125, and avg_common_strobe3 sets the vector time)
 *   The state machine runs from the 12.096 MHz master clock, one state-PROM
 *   tick = 8 master cycles = one 6502 cycle (1.512 MHz).  Ticks per opcode:
 *   VCTR 8, SVEC 6, STAT/SCAL 7, CNTR 5, JSRL 5, RTSL 4, JMPL 3, HALT 2, plus
 *   one idle tick after VGSTART.  On top of that the beam itself: a VCTR
 *   draws for $8000 - timer master cycles, an SVEC for $100 - (timer & $FF),
 *   a CNTR settles for $8000 - timer, where timer is the shift register the
 *   normalizer and the SCAL binary shifter fill - so the time follows the
 *   magnitude class of the deltas and the binary scale, not the drawn length.
 *   avg_result.cycles is that sum.  Because the master lists loop, the AVG
 *   redraws the picture every `cycles` master cycles: that IS the refresh
 *   period of the monitor (avg_cycles_ms), independent of MAINLN's pass rate.
 *
 * Plain C11, no platform dependencies.  avg_walk() works on any avg_mem;
 * avg_mem_g() / avg_run_frame() bind it to the machine state g + vecrom[].
 */
#ifndef AVG_H
#define AVG_H

#include <stddef.h>
#include <stdint.h>

#define AVG_VRAM_BASE     0x2000
#define AVG_VROM_BASE     0x3000
#define AVG_SPACE_END     0x4000
#define AVG_STACK_SLOTS   4
#define AVG_FRAME_BUDGET  20000u     /* default instruction budget per walk   */

#define AVG_MASTER_HZ     12096000.0 /* AVG state machine clock (TIMING)      */
#define AVG_CYC_PER_CPU   8u         /* master cycles per 6502 cycle / PROM tick */
#define AVG_VGGO_LEADIN   8u         /* one idle PROM tick after VGSTART      */
#define avg_cycles_ms(c)  ((double)(c) * (1000.0 / AVG_MASTER_HZ))

#define AVG_Q15_ONE       32768
#define AVG_Q15_TO_F(v)   ((double)(v) / 32768.0)

/* Why a walk ended. */
typedef enum {
    AVG_STOP_HALT = 0,     /* HALT instruction                                */
    AVG_STOP_LOOP,         /* JMPL at depth 0 back to the start address       */
    AVG_STOP_RTSL,         /* RTSL at depth 0 (end of a subroutine walk)      */
    AVG_STOP_BUDGET,       /* instruction budget exhausted                    */
    AVG_STOP_BADADDR,      /* fetch outside $2000-$3FFF                       */
    AVG_STOP_PREVIEW_EXIT  /* AVG_OPT_PREVIEW: left the vector ROM            */
} avg_stop;

/* Diagnostic flags (avg_result.flags). */
#define AVG_FLAG_STACK_OVER  0x01u  /* nesting exceeded the 4 hardware slots   */
#define AVG_FLAG_VRAM_CALL   0x02u  /* AVG_OPT_PREVIEW skipped a JSRL/JMPL into
                                     * vector RAM                             */

/* Walk options (avg_cfg.options). */
#define AVG_OPT_PREVIEW      0x01u  /* disasm/vrender.py render() rules, for the
                                     * shape preview: execute only $3000-$3FFF;
                                     * a JSRL into vector RAM is skipped, a JMPL
                                     * into vector RAM returns (or ends the walk
                                     * at depth 0); the walk also ends when the
                                     * PC leaves $3000-$3FFE                  */

/* One beam movement (every VCTR / SVEC, dark moves included). */
typedef struct {
    int32_t  x0, y0, x1, y1;   /* Q15 AVG coordinates                          */
    uint8_t  intensity;        /* 0..15 (0 = dark).  GL/AAE scale: << 4        */
    int8_t   color;            /* colour latch 0..15; -1 = no colour STAT yet
                                  (only if the caller seeded color = -1)       */
    uint8_t  colram;           /* colour RAM byte for that latch ($FF if none)  */
    uint8_t  svec;             /* 1 = SVEC, 0 = VCTR                           */
    uint32_t rgb;              /* R | G<<8 | B<<16 | $FF<<24 (colordefs.h
                                  MAKE_RGB order) from colram, white if none    */
    uint16_t pc;               /* CPU address of the instruction               */
} avg_seg;

/* The memory the AVG sees. */
typedef struct {
    const uint8_t *vram;       /* 4096 bytes, CPU $2000-$2FFF                  */
    const uint8_t *vrom;       /* 4096 bytes, CPU $3000-$3FFF                  */
    const uint8_t *colram;     /* 16 entries ($0800-$080F) or NULL             */
} avg_mem;

/* Beam / latch state; carried across walks if the caller wants to. */
typedef struct {
    int64_t  x, y;             /* Q15                                          */
    int32_t  scale_q15;        /* current scale factor, Q15                    */
    int      bin_scale;        /* SCAL binary field b: it also shifts the
                                  vector timer (TIMING)                        */
    int      color;           /* colour latch (-1 allowed as "unset")         */
    int      intensity;        /* STAT intensity latch 0..15                   */
    uint16_t stack[AVG_STACK_SLOTS];
    uint8_t  sp;               /* 4-bit hardware counter                       */
    int      depth;            /* logical nesting depth (for diagnostics)      */
} avg_state;

typedef void (*avg_seg_fn)(void *ctx, const avg_seg *s);

typedef struct {
    avg_seg_fn seg;            /* per-segment callback, may be NULL            */
    void      *ctx;
    avg_seg   *segs;           /* optional array sink, may be NULL             */
    size_t     seg_cap;
    uint32_t   max_ops;        /* 0 = AVG_FRAME_BUDGET                         */
    unsigned   options;        /* AVG_OPT_*                                    */
} avg_cfg;

typedef struct {
    avg_stop stop;
    unsigned flags;            /* AVG_FLAG_*                                   */
    uint16_t stop_pc;          /* address of the instruction that ended it     */
    uint32_t ops;              /* instructions executed (incl. the last one)   */
    uint32_t cycles;           /* draw time of the walk in AVG master cycles
                                  (12.096 MHz; TIMING), without the VGGO
                                  lead-in tick                                 */
    uint32_t nseg;             /* VCTR + SVEC executed                         */
    uint32_t nseg_stored;      /* segments written to cfg->segs                */
    uint32_t nlit;             /* segments with intensity > 0                  */
    uint32_t nz;               /* segments with z != 0 (refrun's "bright")     */
    uint32_t vram_calls;       /* AVG_OPT_PREVIEW skipped vram JSRL/JMPL       */
    int      max_depth;        /* deepest logical nesting reached              */
    int      have_bbox;        /* bbox over the endpoints of lit segments      */
    int32_t  minx, miny, maxx, maxy;
} avg_result;

/* Power-on-like state: centre, scale 0 (the latch is 0 until a SCAL),
 * colour 0, intensity 0, empty stack. */
void avg_state_init(avg_state *st);

/* Walk from CPU address `start` ($2000-$3FFE).  st may be NULL (fresh
 * avg_state_init state).  cfg may be NULL (budget only, no output). */
avg_result avg_walk(const avg_mem *m, uint16_t start, avg_state *st,
                    const avg_cfg *cfg);

/* g.vram + vecrom[] + g.colram. */
avg_mem avg_mem_g(void);

/* What the AVG draws for one pass of the live machine: avg_walk from $2000
 * over avg_mem_g() with a fresh state. */
avg_result avg_run_frame(const avg_cfg *cfg);

/* Colour RAM byte -> RGB (tempest_strobe3: active low; b0 red-low, b1 red,
 * b2 blue, b3 green; r = $F3*b1 + $0C*b0, g = $F3*b3, b = $F3*b2). */
uint32_t avg_colram_rgb(uint8_t v);

/* SCAL b,l scale factor as Q15: (255 - l) << (7 - b). */
int32_t avg_scal_q15(int bin, int lin);

const char *avg_stop_name(avg_stop s);

#endif /* AVG_H */
