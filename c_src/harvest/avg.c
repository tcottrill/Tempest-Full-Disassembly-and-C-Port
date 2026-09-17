/* avg.c - Space Duel C port: the Analog Vector Generator state machine.
 *
 * Transcribed from the generic USE_AVG path of avg_video_update() in
 * AAE_publish_new_vector_test\aae\aae\vidhrdwr\aae_avg.cpp, the same
 * reference ../disasm/avg.py was verified against (two's-complement deltas,
 * SCAL complement rule, z==1 = STAT intensity). Memory map per DESIGN.md
 * "JMPL address fold (RESOLVED)": flat word addressing from CPU $2000,
 * vector RAM words $000-$3FF, ROM words $400-$FFF, no mirror.
 *
 * Platform-free: only stdint/stddef plus the two project headers.
 */
#include <stdint.h>
#include <stddef.h>
#include "sd_state.h"
#include "sd_vecrom.h"
#include "avg.h"

/* Opcode = word >> 13 (SCAL is STAT with bit 12 set, split at dispatch). */
#define OP_VCTR 0
#define OP_HALT 1
#define OP_SVEC 2
#define OP_STAT 3
#define OP_CNTR 4
#define OP_JSRL 5
#define OP_RTSL 6
#define OP_JMPL 7

#define AVG_MAXSTACK   8        /* aae_avg.cpp MAXSTACK (hardware has 4)   */
#define AVG_MAXFETCH   0x8000   /* runaway guard: 32K word fetches is >4x  *
                                 * the whole addressable AVG space         */

/* Result registers of the most recent run (hardware-status analogue). */
static avg_stop avg_stop_reason;
static int      avg_flags;
static double   avg_time_units;
static uint32_t avg_cycles;

/* ---- cycle-true draw-time model (mame_late_avgdvg.cpp) --------------------
 *
 * The AVG is a state machine clocked from MASTER_CLOCK = 12,096,000 Hz
 * (mame_late_avgdvg.cpp line 45); every state-PROM tick costs 8 master
 * cycles (run_state_machine, line 1271 "cycles += 8"), i.e. the PROM is
 * stepped at 1.512 MHz.  Vector/center durations are charged on top by
 * avg_common_strobe3 (lines 934-965): a VCTR runs 0x8000 - timer master
 * cycles, an SVEC 0x100 - (timer & 0xFF), a CNTR 0x8000 - timer, where
 * `timer` is the shift register filled by the normalizer (avg_strobe0,
 * lines 660-698) and the binary-scale shifter (avg_strobe1, lines 740-756).
 *
 * The per-opcode PROM tick counts below were extracted by tracing the real
 * state PROM (136002-125.n4, ROM_REGION REGION_PROMS of the whole
 * bwidow/spacduel family - drivers/bwidow.cpp line 568) through
 * avg_state_addr()/run_state_machine() with the avg_default handler table
 * (mame_late_avgdvg.cpp lines 1407-1423 - the plain AVG variant spacduel
 * uses via avg_start(), line 1532; no clipping hardware):
 *
 *   op  ticks  states traversed          extra master cycles
 *   VCTR  8    9,8,B,A,C,D,F,0           0x8000 - timer
 *   HALT  2    9,8 (halt visible at F)   -
 *   SVEC  6    9,B,C,D,F,0               0x100 - (timer & 0xFF)
 *   STAT  7    9,8,E,3,2,1,0             -
 *   SCAL  7    9,8,E,3,2,1,0             -
 *   CNTR  5    9,8,C,F,0                 0x8000 - timer
 *   JSRL  5    9,8,C,D,E                 -
 *   RTSL  4    9,8,D,E                   -
 *   JMPL  3    9,8,E                     -
 *
 * plus one idle tick after VGGO before the first latch state.  This per-op
 * accounting reproduces the full PROM-driven state machine cycle-for-cycle
 * on every captured attract frame (scratchpad avg_cycle_sim.py, 2026-08-26).
 */
#define AVG_MASTER_HZ 12096000.0

/* Normalizer shift count (avg_strobe0): both 13-bit delta registers are
 * shifted up until either has its bit 12 differing from bit 11 (so the
 * DAC's bits 3-12 carry real information); dvx==dvy==0 cuts off at 16. */
static int avg_norm_shifts(uint16_t dvy, uint16_t dvx)
{
    int i = 0;

    while (((dvy ^ (uint16_t)(dvy << 1)) & 0x1000) == 0 &&
           ((dvx ^ (uint16_t)(dvx << 1)) & 0x1000) == 0 && i < 16) {
        i++;
        dvy = (uint16_t)((dvy & 0x1000) | ((dvy << 1) & 0x1FFF));
        dvx = (uint16_t)((dvx & 0x1000) | ((dvx << 1) & 0x1FFF));
    }
    return i;
}

/* The timer shift register after nnorm normalizer shifts (avg_strobe0) and
 * nbin binary-scale shifts (avg_strobe1). op1 = the SVEC path (OP1 set):
 * each shift also ORs 0x80 in and the register is masked to its low byte. */
static uint16_t avg_timer_reg(int nnorm, int nbin, int op1)
{
    uint16_t t = 0;
    int      i;

    for (i = 0; i < nnorm; i++) {
        t >>= 1;
        t |= (uint16_t)(op1 ? 0x4080 : 0x4000);
    }
    if (op1)
        t &= 0xFF;
    for (i = 0; i < nbin; i++) {
        t >>= 1;
        t |= (uint16_t)(op1 ? 0x4080 : 0x4000);
    }
    if (op1)
        t &= 0xFF;
    return t;
}

/* 13-bit two's complement (VGMC.MAC .WORD DY&^H1FFF; aae twos_comp_val).
 * NOT sign-magnitude - that is the older DVG's convention. */
static int s13(uint16_t v)
{
    v &= 0x1FFF;
    return (v & 0x1000) ? (int)v - 0x2000 : (int)v;
}

/* 5-bit two's complement half-delta of the short vector. */
static int s5(uint16_t v)
{
    v &= 0x1F;
    return (v & 0x10) ? (int)v - 0x20 : (int)v;
}

/* Fetch the little-endian word at AVG word address wa (flat map).
 * Returns 0 and stops the caller when wa is past the carried ROMs. */
static int avg_fetch(uint16_t wa, uint16_t *out)
{
    uint32_t off = ((uint32_t)wa & 0x1FFFu) << 1;   /* byte offset from $2000 */

    if (off < 0x0800u) {                            /* CPU $2000-$27FF: RAM   */
        *out = (uint16_t)(g.vram[off] | ((uint16_t)g.vram[off + 1] << 8));
        return 1;
    }
    if (off < 0x2000u) {                            /* CPU $2800-$3FFF: ROM   */
        if (off < 0x1000u)
            avg_flags |= AVG_FLAG_SHIPROM;          /* $2800-$2FFF: never     *
                                                     * legitimate AVG data    */
        *out = (uint16_t)(sd_vecrom[off - 0x0800u]
                          | ((uint16_t)sd_vecrom[off - 0x0800u + 1] << 8));
        return 1;
    }
    *out = 0;                                       /* program ROM: not here  */
    return 0;
}

void avg_run_from(uint16_t word_addr,
                  void (*seg)(float x0, float y0, float x1, float y1,
                              int color, int lum),
                  int *out_words_walked)
{
    uint16_t stack[AVG_MAXSTACK];
    int      sp = 0;
    uint16_t pc = (uint16_t)(word_addr & 0x1FFF);
    int      fetched = 0;

    double x = 0.0, y = 0.0;        /* beam at center, like the reference  */
    double scale = 0.0;             /* aae starts scale 0: pre-SCAL vectors*
                                     * have zero length                    */
    int statz = 0;                  /* STAT intensity register             */
    int color = 0;
    int bin_scale = 0;              /* SCAL binary part, for the timer     */

    int done = 0;

    avg_stop_reason = AVG_STOP_RUNAWAY;   /* until proven otherwise        */
    avg_flags = 0;
    avg_time_units = 0.0;
    avg_cycles = 8;                 /* VGGO lead-in: one idle PROM tick    */

    while (!done && fetched < AVG_MAXFETCH) {
        uint16_t w0, w1 = 0;
        int op, dx = 0, dy = 0, z = -1;

        if (!avg_fetch(pc, &w0)) {
            avg_stop_reason = AVG_STOP_BADFETCH;
            break;
        }
        pc++;
        fetched++;

        op = (w0 >> 13) & 7;
        switch (op) {

        case OP_VCTR:                       /* two words: dy then dx|z     */
            if (!avg_fetch(pc, &w1)) {
                avg_stop_reason = AVG_STOP_BADFETCH;
                done = 1;
                break;
            }
            pc++;
            fetched++;
            dy = s13(w0);
            dx = s13(w1);
            z = (w1 >> 13) & 7;
            avg_cycles += 8u * 8u + (uint32_t)(0x8000 -
                avg_timer_reg(avg_norm_shifts((uint16_t)(w0 & 0x1FFF),
                                              (uint16_t)(w1 & 0x1FFF)),
                              bin_scale, 0));
            goto draw;

        case OP_SVEC:                       /* one word, half-resolution   */
            dx = s5(w0) * 2;
            dy = s5((uint16_t)(w0 >> 8)) * 2;
            z = (w0 >> 5) & 7;
            avg_cycles += 6u * 8u + (uint32_t)(0x100 -
                (avg_timer_reg(avg_norm_shifts(
                                   (uint16_t)(((w0 >> 8) & 0x1F) << 8),
                                   (uint16_t)((w0 & 0x1F) << 8)),
                               bin_scale, 1) & 0xFF));
        draw: {
                double fdx = (double)dx * scale;
                double fdy = (double)dy * scale;
                double ax = (fdx < 0.0) ? -fdx : fdx;
                double ay = (fdy < 0.0) ? -fdy : fdy;
                int lum = (z == 1) ? statz : (z << 1);   /* z==1: use STAT */
                double x0 = x, y0 = y;

                x += fdx;
                y += fdy;                   /* +y up; aae negates for raster */
                avg_time_units += (ax > ay) ? ax : ay;   /* aae vector_timer */
                if (seg != NULL)
                    seg((float)x0, (float)y0, (float)x, (float)y, color, lum);
            }
            break;

        case OP_STAT:                       /* $6xxx; bit 12 set = SCAL    */
            avg_cycles += 7u * 8u;
            if (w0 & 0x1000) {              /* SCAL: complemented linear   */
                int bshift = (w0 >> 8) & 7;
                int linear = (~w0) & 0xFF;
                scale = ((double)linear / 256.0) / (double)(1 << bshift);
                bin_scale = bshift;
            } else {                        /* STAT/COLOR ($64xx here)     */
                statz = (w0 >> 4) & 0x0F;
                color = w0 & 0x07;
            }
            break;

        case OP_CNTR:                       /* $8xxx: beam to center       */
            /* w0 & 0xFF is the settling-timer seed: it lands in dvy and
             * the normalizer shifts it up; the centering then runs for
             * 0x8000 - timer master cycles (avg_common_strobe3 OP2 arm). */
            avg_cycles += 5u * 8u + (uint32_t)(0x8000 -
                avg_timer_reg(avg_norm_shifts((uint16_t)(w0 & 0xFF), 0),
                              0, 0));
            x = 0.0;
            y = 0.0;
            break;

        case OP_JSRL: {
            uint16_t a = w0 & 0x1FFF;
            avg_cycles += 5u * 8u;
            if (a == 0) {                   /* reference: target 0 = done  */
                avg_stop_reason = AVG_STOP_JUMP0;
                done = 1;
                break;
            }
            if (sp >= AVG_MAXSTACK - 1) {   /* aae: overflow ends the frame */
                avg_stop_reason = AVG_STOP_STACK_OVER;
                done = 1;
                break;
            }
            stack[sp++] = pc;
            pc = a;
            break;
        }

        case OP_RTSL:
            avg_cycles += 4u * 8u;
            if (sp == 0) {                  /* aae: underflow ends the frame */
                avg_stop_reason = AVG_STOP_STACK_UNDER;
                done = 1;
                break;
            }
            pc = stack[--sp];
            break;

        case OP_JMPL: {
            uint16_t a = w0 & 0x1FFF;
            avg_cycles += 3u * 8u;
            if (a == 0) {
                avg_stop_reason = AVG_STOP_JUMP0;
                done = 1;
                break;
            }
            pc = a;
            break;
        }

        case OP_HALT:
        default:                            /* op is 3 bits: HALT is last  */
            avg_cycles += 2u * 8u;          /* halt visible after 2 ticks  */
            avg_stop_reason = AVG_STOP_HALT;
            done = 1;
            break;
        }
    }
    /* Falling out with !done means the fetch guard tripped: the list never
     * reached a HALT (diagnostic: avg_last_stop() == AVG_STOP_RUNAWAY). */

    if (out_words_walked != NULL)
        *out_words_walked = fetched;
}

void avg_run(void (*seg)(float x0, float y0, float x1, float y1,
                         int color, int lum),
             int *out_words_walked)
{
    avg_run_from(0, seg, out_words_walked);   /* VGGO: word 0 = CPU $2000 */
}

avg_stop avg_last_stop(void)
{
    return avg_stop_reason;
}

int avg_last_flags(void)
{
    return avg_flags;
}

double avg_draw_time_units(void)
{
    return avg_time_units;
}

uint32_t avg_frame_cycles(void)
{
    return avg_cycles;
}

double avg_frame_time_ms(void)
{
    return (double)avg_cycles * (1000.0 / AVG_MASTER_HZ);
}
