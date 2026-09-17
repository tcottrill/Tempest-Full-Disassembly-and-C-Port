/* hw.h - the hardware seam for the Tempest C port.
 *
 * Translated code touches hardware ONLY through these calls (Space Duel
 * CONVENTIONS rule 5).  Implementations:
 *   app_loop.c        the game (headless skeleton today, platform layer at M5)
 *   tests/lockstep.c  replays, in order, the I/O the real ROM did on
 *                     tests/refrun.c's hardware model during the same call
 * Register names are the defines file's.
 */
#ifndef HW_H
#define HW_H

#include <stdint.h>

/* ---- inputs ------------------------------------------------------------- */
uint8_t hw_in1(void);                 /* IN1   $0C00 coins/slam/test/diag, b6 VG HALT, b7 3 kHz */
uint8_t hw_inop0(void);               /* INOP0 $0D00 option switches N13 */
uint8_t hw_inop1(void);               /* INOP1 $0E00 option switches L12 */
uint8_t hw_allpot(int chip);          /* ALLPOT $60C8 (spinner, cocktail) / ALLPO2 $60D8 (buttons) */
uint8_t hw_random(int chip);          /* RANDOM $60CA / RANDO2 $60DA - read for read with the oracle */

/* ---- POKEY writes: AUDF/AUDC/AUDCTL, POTGO ($x0B), SKCTL ($x0F) ---------- */
void    hw_pokey_write(int chip, uint8_t reg, uint8_t val);

/* ---- Mathbox ($6040 status, $6060/$6070 result, $6080-$609F start) ------ */
void    hw_mb_write(uint8_t offset, uint8_t val);
uint8_t hw_mb_status(void);
uint8_t hw_mb_ylow(void);
uint8_t hw_mb_yhigh(void);

/* ---- EAROM -------------------------------------------------------------- */
void    hw_earom_write(uint8_t offset, uint8_t val);  /* EADAL $6000+offset */
void    hw_earom_ctl(uint8_t val);                    /* EACTL $6040 */
uint8_t hw_earom_read(void);                          /* EAIN  $6050 */

/* ---- outputs ------------------------------------------------------------ */
void    hw_out0(uint8_t v);           /* OUT0   $4000 coin counters, invert X/Y */
void    hw_outank(uint8_t v);         /* OUTANK $60E0 start LEDs, flip */
void    hw_color(uint8_t idx, uint8_t v);  /* COLPORT $0800+idx (writes g.colram) */
void    hw_vgstart(void);             /* VGSTART $4800 (strobe) */
void    hw_vgstop(void);              /* VGSTOP  $5800 (strobe) */
void    hw_watchdog(void);            /* WTCHDG/INTACK $5000 (strobe; a no-op in the game) */

/* ---- CPU facts the ROM inspects ----------------------------------------- */
/* IRQ's TSX at $D70A: the 6502 stack pointer after the IRQ's pushes.  The
 * software watchdog BRKs below $D0.  The game returns a healthy constant. */
uint8_t hw_sp(void);
/* IRQ's software-watchdog trip ($D713 BRK / $D714 JMP RESET): the game
 * restarts (reset()); probes flag it. */
void    hw_soft_watchdog(void);
/* RESET's CLI ($D9A4, M8): RESET ran from its SEI with the IRQ masked; the
 * 3 kHz / 12 interrupt line has been pending since (the power-on delay loop
 * alone is ~140 IRQ periods), so exactly one IRQ is taken here, before
 * MAINLN's first instruction.  The game seam services it (and drops the
 * periods lost while masked); tests\gate.c fires the trace's START marker. */
void    hw_cli(void);

/* ---- CPU restarts (M9, ALTES2) -------------------------------------------
 * Both NEVER RETURN on a finished seam: the implementation abandons the C call
 * stack (longjmp) back to its run loop, which calls reset() again - exactly
 * what the 6502 does at the RESET vector.  RAM, vector RAM, colour RAM, the
 * POKEYs, the Mathbox and the EAROM keep their state (RESET itself clears RAM).
 * The translated code returns straight after either call in case a stand-in
 * returns.
 *
 * hw_reset():  JMP RESET inside running code - DSPSYS option 0/1 ($D861, the
 *              self-test option; TEST switch closed, so RESET enters the power-on
 *              self test over live RAM).  (The IRQ's $D714 JMP RESET stays
 *              hw_soft_watchdog(), which a seam may implement through this.)
 *              lockstep (B3): consume the IO_RESET event, finish the open checks,
 *              longjmp.  gate (B3): consume 'X', longjmp to the pass loop.
 *              app_loop (B5): longjmp to boot_body.
 * hw_watchdog_hang(): WDGTST ($DAF7 BNE WDGTST) - the diag loop saw the TEST
 *              switch open and spins without kicking until the hardware watchdog
 *              resets the CPU.  lockstep/gate (B3): the next event must be the
 *              RESET (cause watchdog); app_loop (B5): longjmp to boot_body
 *              (optionally after the watchdog timeout of machine time).
 * Stand-ins until B3/B5: altes2.c defines both as hw_soft_watchdog() unless
 * compiled with /DALTES2_SEAM_HAS_RESET_HOOKS (define it on every build line
 * whose seam implements them). */
void    hw_reset(void);
void    hw_watchdog_hang(void);

/* ---- timing ------------------------------------------------------------- */
/* MAINLN's "LDA FRTIMR / CMP #9 / BCC" wait ($C7A7): service IRQs until
 * FRTIMR >= 9.  IRQ rate is the board's 246.09 Hz (PLAN.md decision 3). */
void    hw_wait_frame(void);

#endif /* HW_H */
