/* alcoin.c - ALCOIN / COIN65 ($CF24-$D030): Atari's generic coin routine
 * (Downend & Albaugh), called from the IRQ.  Three mechs, indexed X = 2
 * (right), 1 (centre), 0 (left).  The carry flag is state here, so it is a
 * variable (`c`), updated exactly where the ROM's instructions update it.
 * Verified inside every IRQ by tests/lockstep.exe.
 */
#include "state.h"
#include "hw.h"
#include "game.h"

#define CNSTT(x)  g.ram[A_S_CNSTT + (x)]
#define PSTSL(x)  g.ram[A_S_PSTSL + (x)]
#define CCTIM(x)  g.ram[A_S_CCTIM + (x)]
#define S_MODLO   0xCFD9              /* S_MODLO: unit coins per bonus, 8 bytes (ROM table) */

static void s_ext(void);

/* MOOLAH ($CF24): detect coins on the three mechs, apply the multipliers. */
void moolah(void)
{
    int x;
    for (x = 2; x >= 0; x--) {                             /* LCF24, S_DETCT_9 DEX / BMI */
        uint8_t a;
        unsigned c;
        /* S_DETCT ($CF26): LDA S_COINA, shift this mech's bit into C */
        c = (unsigned)(S_COINA >> (x == 2 ? 0 : x == 1 ? 1 : 2)) & 1u;   /* LCF26-LCF31 */
        a = (uint8_t)(CNSTT(x) & 0x1F);                    /* LCF32-LCF34 */
        if (!c) {                                          /* LCF36 BCS S_DETCT_5: input low = coin present */
            if (a != 0) {                                  /* LCF38 BEQ S_DETCT_1 (stick at 0) */
                if (a >= 0x1B) {                           /* LCF3A-LCF3C: first five samples run fast */
                    a = (uint8_t)(a - 1);                  /* S_DETCT_10 SBC #1 (C set) */
                } else if ((S_INTCT & 0x07) == 0x07) {     /* LCF3E-LCF46 */
                    a = (uint8_t)(a - 1);                  /* S_DETCT_10 */
                }
            }
            goto detct1;
        }
        /* S_DETCT_5 ($CF6F): coin absent */
        c = (a >= 0x1B);                                   /* LCF6F CMP #$1B */
        if (!c) {
            unsigned sum = (unsigned)CNSTT(x) + 0x20u;     /* LCF73-LCF75 (C clear) */
            a = (uint8_t)sum;
            c = sum > 0xFF;
            if (!c) goto detct1;                           /* LCF77 */
            if (a != 0) c = 0;                             /* LCF79 BEQ S_DETCT_6 / LCF7B CLC */
        }
        /* S_DETCT_6 ($CF7C) */
        a = 0x1F;
        if (c) goto detct1;                                /* LCF7E: too long or too short */
        CNSTT(x) = a;                                      /* LCF80 */
        if (PSTSL(x) != 0) c = 1;                          /* LCF82-LCF86: give credit a little early */
        PSTSL(x) = 0x78;                                   /* S_DETCT_7 LCF87-LCF89 */
        goto detct8;

    detct1:                                                /* S_DETCT_1 ($CF4A) */
        CNSTT(x) = a;
        if (!(S_LAM & K_S_LMBIT)) S_LMTIM = 0xF0;          /* LCF4C-LCF55: pre-coin slam timer */
        if (S_LMTIM != 0) {                                /* S_DETCT_2 LCF57-LCF59 */
            S_LMTIM = (uint8_t)(S_LMTIM - 1);              /* LCF5B */
            CNSTT(x) = 0;                                  /* LCF5D-LCF5F */
            PSTSL(x) = 0;                                  /* LCF61 */
        }
        c = 0;                                             /* S_DETCT_3 LCF63 CLC */
        if (PSTSL(x) != 0) {                               /* LCF64-LCF66 */
            PSTSL(x) = (uint8_t)(PSTSL(x) - 1);            /* LCF68 */
            if (PSTSL(x) == 0) c = 1;                      /* LCF6A-LCF6D */
        }

    detct8:                                                /* S_DETCT_8 ($CF8B) */
        if (c) {
            /* MECH-MULTIPLIERS */
            a = 0;                                         /* LCF8D */
            if (x == 1) {                                  /* LCF93 BEQ S_DETCT_83 */
                if (S_CMODE & 0x10) a = 1;                 /* LCFA1-LCFA7 */
            } else if (x > 1) {
                a = (uint8_t)((S_CMODE & 0x0C) >> 2);      /* LCF95-LCF9A (C ends clear) */
                if (a != 0) a = (uint8_t)(a + 2);          /* LCF9B-LCF9F: map 1,2,3 to 3,4,5 */
            }
            S_BCCNT = (uint8_t)(a + S_BCCNT + 1u);         /* S_DETCT_85 LCFA9-LCFAD */
            S_CNCT = (uint8_t)(a + S_CNCT + 1u);           /* LCFAF-LCFB3 */
            CCTIM(x) = (uint8_t)(CCTIM(x) + 1);            /* LCFB5 */
        }
    }
    s_bonus();                                             /* LCFB8 BMI S_BONUS */
}

/* S_BONUS ($CFBD): bonus adder. */
void s_bonus(void)
{
    uint8_t y = (uint8_t)(S_CMODE >> 5);                   /* LCFBD-LCFC4 */
    uint8_t a = (uint8_t)(S_BCCNT - cpu_rd((uint16_t)(S_MODLO + y)));  /* LCFC5-LCFC8 */
    if (!(a & 0x80)) {                                     /* LCFCB BMI S_EXTB */
        S_BCCNT = a;                                       /* LCFCD */
        S_BC = (uint8_t)(S_BC + 1);                        /* LCFCF */
        if (y == 3) S_BC = (uint8_t)(S_BC + 1);            /* LCFD1-LCFD5 (mode 3: 2 for 4) */
    }
    s_extb();
}

/* S_EXTB ($CFE1): coins to credits, then the electro-mechanical counters. */
void s_extb(void)
{
    uint8_t a = (uint8_t)(S_CMODE & 0x03);                 /* LCFE1-LCFE3 */
    uint8_t y = a;                                         /* LCFE5 */
    if (a != 0) {                                          /* LCFE6: 0 = free play */
        unsigned c = a & 1u, sum;
        a = (uint8_t)((a >> 1) + c);                       /* LCFE8-LCFE9: price 0,1,1,2 */
        a = (uint8_t)~a;                                   /* LCFEB */
        sum = (unsigned)a + S_CNCT + 1u;                   /* LCFED-LCFEE: coinct - price */
        a = (uint8_t)sum;
        if (sum <= 0xFF) {                                 /* LCFF0 BCS S_CNVRT_33 */
            a = (uint8_t)(a + S_BC);                       /* LCFF2 (C clear) */
            if (a & 0x80) { s_ext(); return; }             /* LCFF4 BMI S_EXT */
            S_BC = a;                                      /* LCFF6 */
            a = 0;                                         /* LCFF8 */
        }
        if (y < 2) S_S_CRDT = (uint8_t)(S_S_CRDT + 1);     /* S_CNVRT_33 LCFFA-LCFFE */
        S_S_CRDT = (uint8_t)(S_S_CRDT + 1);                /* S_CNVRT_1 LD000 */
    }
    S_CNCT = a;                                            /* S_CNVRT_2 LD002 */
    s_ext();
}

/* S_EXT ($D004): run and start the coin-counter pulses. */
static void s_ext(void)
{
    uint8_t y = 0;
    int x;
    if (S_INTCT & 1) return;                               /* LD004-LD007 */
    for (x = 2; x >= 0; x--) {                             /* LD00B-LD01B */
        uint8_t a = CCTIM(x);
        if (a == 0 || a < 0x10) continue;                  /* LD00F-LD013 */
        CCTIM(x) = (uint8_t)(a + 0xF0);                    /* LD015 ADC #$EF (C set), LD018 */
        y++;                                               /* LD017 */
    }
    if (y != 0) return;                                    /* LD01D-LD01E */
    for (x = 2; x >= 0; x--) {                             /* LD020-LD02E */
        uint8_t a = CCTIM(x);
        if (a == 0) continue;
        a = (uint8_t)(a + 0xEF);                           /* LD026-LD027 */
        CCTIM(x) = a;                                      /* LD029 */
        if (a & 0x80) return;                              /* LD02B */
    }
}
