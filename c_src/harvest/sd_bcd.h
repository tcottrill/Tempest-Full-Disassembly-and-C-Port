/* sd_bcd.h - NMOS 6502 decimal-mode ADC, bit-accurate including the N flag.
 *
 * The IRQ's SED counters branch on N after a decimal ADC ($86A0 BPL). On the
 * NMOS 6502, N comes from the intermediate high nibble BEFORE the decimal
 * adjust, so it is modeled here rather than read off the final byte.
 */
#ifndef SD_BCD_H
#define SD_BCD_H

#include <stdint.h>

typedef struct { uint8_t r; int c; int n; } bcd_res;

static inline bcd_res bcd_adc(uint8_t a, uint8_t b, int cin)
{
    unsigned lo = (a & 0x0F) + (b & 0x0F) + (unsigned)(cin ? 1 : 0);
    unsigned hi = (a >> 4) + (b >> 4);
    bcd_res out;
    if (lo > 9) { lo += 6; }
    hi += (lo > 0x0F) ? 1 : 0;
    out.n = (hi & 0x08) != 0;            /* N from the pre-adjust nibble */
    if (hi > 9) hi += 6;
    out.c = hi > 0x0F;
    out.r = (uint8_t)(((hi & 0x0F) << 4) | (lo & 0x0F));
    return out;
}

/* NMOS 6502 decimal-mode SBC (Bruce Clark's canonical algorithm).  Unlike
 * ADC, decimal SBC sets N/V/Z/C exactly as the binary subtraction does -
 * the decimal fix-up touches the accumulator only - so .c/.n here come off
 * the plain binary result.  cin is the carry IN (1 = no borrow).
 */
static inline bcd_res bcd_sbc(uint8_t a, uint8_t b, int cin)
{
    int borrow = cin ? 0 : -1;
    int bin = (int)a - (int)b + borrow;
    int al  = (int)(a & 0x0F) - (int)(b & 0x0F) + borrow;
    int res;
    bcd_res out;
    if (al < 0) { al = ((al - 0x06) & 0x0F) - 0x10; }
    res = (int)(a & 0xF0) - (int)(b & 0xF0) + al;
    if (res < 0) { res -= 0x60; }
    out.r = (uint8_t)res;
    out.c = (bin >= 0);                  /* C = no borrow, from the binary sub */
    out.n = ((bin & 0x80) != 0);         /* N likewise binary, not pre-adjust  */
    return out;
}

#endif /* SD_BCD_H */
