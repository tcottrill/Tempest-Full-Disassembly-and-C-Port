/* state.h - Tempest C port: the whole machine state.
 *
 * Same state model as the Space Duel port (its DESIGN.md argues it): the
 * machine's own memory arrays, byte-diffable against tests/refrun.exe and
 * checked call by call by tests/lockstep.exe.  Named cells go through the
 * generated aliases in state_defs.h (tools/gen_state.py from
 * ../disasm/tempest_defines.asm); an unnamed cell met during translation gets
 * a documented ZP_xx / RAM_xxx macro below until its meaning is proven.
 */
#ifndef STATE_H
#define STATE_H

#include <stdint.h>

typedef struct {
    uint8_t  ram[0x800];      /* $0000-$07FF: zero page, stack page, game RAM  */
    uint8_t  vram[0x1000];    /* $2000-$2FFF: real AVG display-list bytes      */
    uint8_t  colram[16];      /* $0800-$080F: colour RAM (write-only on board) */

    /* ---- seam bookkeeping (not CPU RAM) ---------------------------------- */
    uint32_t irq_count;       /* IRQs serviced since reset                     */
    uint32_t pass_count;      /* MAINLN passes since reset                     */
    uint32_t stray_writes;    /* cpu_wr() outside RAM/colour/vector RAM        */

    /* ---- CPU facts that outlive one routine (M3) -------------------------- */
    uint8_t  dflag;           /* the 6502 D flag: set by UPSCOR's SED and by
                                 NONSTA's protection SED ($C900), cleared by
                                 UPSCOR's CLD.  ADC/SBC below honour it; ROM
                                 thunks carry it in and out.                   */
    uint8_t  loop_x, loop_y;  /* X and Y at MAINLN's loop head ($C7AD): what
                                 DISPLAY left, fed into EXSTAT/NONSTA (the sound
                                 starters save the caller's X/Y in MTEMP).     */
    uint8_t  cpu_loop;        /* M9: which endless loop the CPU runs - PC state
                                 RAM cannot hold (Space Duel's sd_cpu_loop()).
                                 LOOP_MAINLN: MAINLN, one mainln_pass() per
                                 pass (set by reset() at LD9A5 JMP MAINLN);
                                 LOOP_DIAG: the self test's diagnostic loop,
                                 one diag_pass() per pass (set by romtst() on
                                 reaching $DA8D).  0 after memset = MAINLN.    */
} machine_state;

#define LOOP_MAINLN 0
#define LOOP_DIAG   1

extern machine_state g;

#include "state_defs.h"

/* Generic accessors for indexed / indirect idioms. */
#define RAM(a)   (g.ram[(a) & 0x7FF])
#define VRAM(a)  (g.vram[((a) - 0x2000) & 0xFFF])   /* a = CPU $2000-$2FFF */

/* The display-list pointer is the real zero-page pair VGLIST/VGLIST+1. */
#define VGLIST_ADDR ((uint16_t)(g.ram[A_VGLIST] | ((uint16_t)g.ram[A_VGLIST + 1] << 8)))

/* A 6502 read / write at a computed CPU address ((zp),Y stores, ROM table
 * reads): RAM, colour RAM, vector RAM, vector ROM, program ROM (and its
 * $E000 mirror).  Hardware registers are NOT reachable this way - translated
 * code calls the hw_* seam for those. */
uint8_t cpu_rd(uint16_t addr);
void    cpu_wr(uint16_t addr, uint8_t v);

/* ---- 6502 ADC / SBC with the NMOS decimal mode (g.dflag) ----------------
 * Bit-for-bit the shared core's op_adc/op_sbc (C:\Source2026\shared\ref6502):
 * decimal ADC takes Z from the binary sum; SBC flags are always binary.
 * *c is the carry in and out; *z (may be NULL) receives the Z flag. */
static inline uint8_t adc6502(uint8_t a, uint8_t v, unsigned *c, int *z)
{
    unsigned cin = *c ? 1u : 0u;
    if (g.dflag) {
        unsigned al = (a & 0x0Fu) + (v & 0x0Fu) + cin;
        unsigned t;
        if (al >= 0x0A) al = ((al + 0x06) & 0x0F) + 0x10;
        t = (a & 0xF0u) + (v & 0xF0u) + al;
        if (z) *z = (uint8_t)(a + v + cin) == 0;
        if (t >= 0xA0) t += 0x60;
        *c = t >= 0x100;
        return (uint8_t)t;
    } else {
        unsigned sum = (unsigned)a + v + cin;
        *c = sum > 0xFF;
        if (z) *z = (uint8_t)sum == 0;
        return (uint8_t)sum;
    }
}

static inline uint8_t sbc6502(uint8_t a, uint8_t v, unsigned *c, int *z)
{
    int cin = *c ? 1 : 0;
    int bin = (int)a - (int)v - (1 - cin);
    *c = bin >= 0;
    if (z) *z = (uint8_t)bin == 0;
    if (g.dflag) {
        int al = (int)(a & 0x0F) - (int)(v & 0x0F) - (1 - cin);
        int t;
        if (al < 0) al = ((al - 0x06) & 0x0F) - 0x10;
        t = (int)(a & 0xF0) - (int)(v & 0xF0) + al;
        if (t < 0) t -= 0x60;
        return (uint8_t)(t & 0xFF);
    }
    return (uint8_t)bin;
}

/* ---- unnamed cells (none yet) ------------------------------------------- */

#endif /* STATE_H */
