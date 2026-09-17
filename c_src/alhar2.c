/* alhar2.c - ALHAR2 ($D703-$D7E0): the interrupt handler (NMI = IRQ =
 * $D704), 246.09 Hz.  Verified per interrupt by tests/lockstep.exe.
 */
#include "state.h"
#include "hw.h"
#include "game.h"

#define LITSON 0xD7DD    /* LITSON: start-LED bits per state (ROM table, 4 bytes) */

/* IRQ ($D704): software watchdog, then SOFTOK. */
void irq(void)
{
    uint8_t sp;
    /* LD704-LD709 PHA/TXA/PHA/TYA/PHA/CLD: register save - no C state (dropped) */
    sp = hw_sp();                                   /* LD70A TSX */
    if (sp < 0xD0 || (FRTIMR & 0x80)) {             /* LD70B-LD711: stack too deep / overrun */
        hw_soft_watchdog();                         /* LD713 BRK, LD714 JMP RESET */
        return;
    }
    softok();                                       /* LD711 BPL SOFTOK */
}

/* SOFTOK ($D717): the IRQ proper.  Entered with A = FRTIMR. */
void softok(void)
{
    uint8_t a, y, d, sw, x;

    hw_watchdog();                                  /* LD717 STA WTCHDG (strobe) */

    /* .SBTTL SWITCHES */
    hw_pokey_write(0, 0x0B, FRTIMR);                /* LD71A STA POTGO */
    a = (uint8_t)(hw_allpot(0) ^ 0x0F);             /* LD71D-LD720 */
    y = a;                                          /* LD722 TAY */
    COCTAL = (uint8_t)(a & K_COCKTA);               /* LD723-LD725 */
    d = (uint8_t)((uint8_t)(y - OTB) & 0x0F);       /* LD728-LD72C: TYA / SEC / SBC OTB / AND */
    if (d >= 0x08) d |= 0xF0;                       /* LD72E-LD732: sign-extend the delta */
    TBHD = (uint8_t)(d + TBHD);                     /* LD734-LD737 */
    OTB = y;                                        /* LD739 */
    hw_pokey_write(1, 0x0B, TBHD);                  /* LD73B STA POTGO2 */
    y = hw_allpot(1);                               /* LD73E LDY ALLPO2 */
    S_COINA = hw_in1();                             /* LD741-LD744 */

    /* debounce: new SWSTAT = majority of (input, DBSW, SWSTAT), per bit */
    a = DBSW;                                       /* LD746 */
    DBSW = y;                                       /* LD748 */
    y = a;                                          /* LD74A TAY (old DBSW) */
    SWSTAT = (uint8_t)((a & DBSW) | SWSTAT);        /* LD74B-LD74F */
    SWSTAT = (uint8_t)((y | DBSW) & SWSTAT);        /* LD751-LD756 */
    sw = SWSTAT;                                    /* LD758 TAY */
    SWFINA = (uint8_t)(((sw ^ SWRELE) & SWSTAT) | SWFINA);  /* LD759-LD75F */
    SWRELE = sw;                                    /* LD761 */

    /* .SBTTL OUTPUTS */
    a = TOUT0;                                      /* LD763 */
    if (S_CCTIM & 0x80) a |= K_MLCCNT;              /* LD765-LD769 */
    if (g.ram[A_S_CCTIM + 1] & 0x80) a |= K_MMCCNT; /* LD76B-LD76F */
    if (g.ram[A_S_CCTIM + 2] & 0x80) a |= K_MRCCNT; /* LD771-LD775 */
    hw_out0(a);                                     /* LD777 */

    /* start lights */
    x = (uint8_t)(NUMPLA + 1);                      /* LD77A-LD77C */
    if (QSTATUS == 0) {                             /* LD77D-LD77F: attract mode? */
        x = 0;                                      /* LD781 */
        if (S_INTCT >= 0x40) {                      /* LD783-LD787: blink on time? */
            x = S_S_CRDT;                           /* LD789 */
            if (x >= 0x02) x = 0x03;                /* LD78B-LD78F */
        }
    }
    a = cpu_rd((uint16_t)(LITSON + x));             /* LD791 */
    TNKOUT = (uint8_t)(((a ^ TNKOUT) & (K_MLED1 | K_MLED2)) ^ TNKOUT); /* LD794-LD79A */
    hw_outank(TNKOUT);                              /* LD79C */

    moolah();                                       /* LD79F PROCESS COINS */
    modsnd();                                       /* LD7A2 PROCESS SOUNDS */

    FRTIMR = (uint8_t)(FRTIMR + 1);                 /* LD7A5 */
    S_INTCT = (uint8_t)(S_INTCT + 1);               /* LD7A7 */
    if (S_INTCT == 0) {                             /* LD7A9: another second? */
        SECOUL = (uint8_t)(SECOUL + 1);             /* LD7AB up timer */
        if (SECOUL == 0) {
            SECOUM = (uint8_t)(SECOUM + 1);         /* LD7B0 */
            if (SECOUM == 0) SECOUH = (uint8_t)(SECOUH + 1);   /* LD7B5 */
        }
        if (QSTATUS & 0x40) {                       /* LD7B8-LD7BA: game timer mode? */
            SECOPL = (uint8_t)(SECOPL + 1);         /* LD7BC play timer */
            if (SECOPL == 0) {
                SECOPM = (uint8_t)(SECOPM + 1);     /* LD7C1 */
                if (SECOPM == 0) SECOPH = (uint8_t)(SECOPH + 1);   /* LD7C6 */
            }
        }
    }

    if (hw_in1() & 0x40) {                          /* LD7C9-LD7CC: VG done (halted)? */
        SPARE3 = (uint8_t)(SPARE3 + 1);             /* LD7CE */
        hw_vgstop();                                /* LD7D1 */
        hw_vgstart();                               /* LD7D4 */
    }
    g.irq_count++;
    /* LD7D7-LD7DC PLA/TAY/PLA/TAX/PLA/RTI - dropped */
}
