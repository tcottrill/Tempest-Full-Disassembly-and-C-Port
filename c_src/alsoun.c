/* alsoun.c - ALSOUN ($CB01-$CDDD): sound tables and the POKEY sound driver.
 *
 * 16 channels, 8 per POKEY (even = AUDF frequency, odd = AUDC noise /
 * amplitude).  PNTRS holds, per sound, 16 offsets into the SOUND data (0 =
 * leave that channel alone); a channel steps through 4-byte groups (start
 * value, frames per change, change, number of values).  Verified by
 * tests/lockstep.exe (MODSND inside every IRQ; the start routines per call).
 *
 * The start routines save the caller's X and Y in MTEMP/MTEMP+1 (a store the
 * ROM makes), so they take the caller's X and Y as parameters.
 */
#include "state.h"
#include "hw.h"
#include "game.h"

#define PNTRS    0xCB01               /* PNTRS: 16 offsets per sound (ROM table) */
#define SOUND    0xCBCB               /* SOUND: channel data base (ROM table)    */
#define POINT_X(x)   g.ram[A_POINT + (x)]
#define FRAMES_X(x)  g.ram[A_FRAMES + (x)]
#define COUNT_X(x)   g.ram[A_COUNT + (x)]
#define CURRENT_X(x) g.ram[A_CURRENT + (x)]

/* SNDON ($CCC3): start sound A unless in attract mode. */
void sndon(uint8_t a, uint8_t x, uint8_t y)
{
    if (!(QSTATUS & 0x80)) return;                         /* LCCC3-LCCC5 BIT QSTATUS / BPL NWSNON */
    fsndon(a, x, y);
}

/* FSNDON ($CCC7): start sound A regardless of mode. */
void fsndon(uint8_t a, uint8_t x_in, uint8_t y_in)
{
    int x;
    uint8_t y;
    MTEMP = x_in;                                          /* LCCC7 */
    g.ram[A_MTEMP + 1] = y_in;                             /* LCCC9 */
    y = a;                                                 /* LCCCB */
    for (x = K_NCHANL - 1; x >= 0; x--, y--) {             /* LCCCC, LCCE1-LCCE3 */
        uint8_t p = cpu_rd((uint16_t)(PNTRS + y));         /* LCCCE */
        if (p != 0) {                                      /* LCCD1 */
            SINDEX = (uint8_t)x;                           /* LCCD3 */
            CK(0xCCD5);
            POINT_X(x) = p;                                  /* LCCD5 */
            CK(0xCCD7);
            FRAMES_X(x) = 0x01;                              /* LCCD7-LCCD9: dummy start */
            CK(0xCCDB);
            COUNT_X(x) = 0x01;                               /* LCCDB */
            CK(0xCCDD);
            SINDEX = 0xFF;                                 /* LCCDD-LCCDF */
        }
        CK(0xCCE1);
    }
    /* LCCE5-LCCE7: X, Y restored - no C state */
}

void ipexpl(uint8_t x, uint8_t y) { sndon(K_SIDDI, x, y); }   /* IPEXPL/CPEXPL $CCB0 player dies */
void sboing(uint8_t x, uint8_t y) { sndon(K_SIDLO, x, y); }   /* SBOING $CCB5 cursor moves      */
void sauson(uint8_t x, uint8_t y) { sndon(K_SIDWP, x, y); }   /* SAUSON $CCB9 special score     */
void eslson(uint8_t x, uint8_t y) { sndon(K_SIDES, x, y); }   /* ESLSON $CCBD enemy shot        */
void ccexpl(uint8_t x, uint8_t y) { sndon(K_SIDEX, x, y); }   /* CCEXPL/CIEXPL/EXSNON $CCC1 explosion */
void slaunc(uint8_t x, uint8_t y) { sndon(K_SIDLA, x, y); }   /* SLAUNC $CCEA player fire       */
void souts2(uint8_t x, uint8_t y) { sndon(K_SIDT2, x, y); }   /* SOUTS2 $CCEE thrust in tube    */
void souts3(uint8_t x, uint8_t y) { sndon(K_SIDT3, x, y); }   /* SOUTS3 $CCF2 thrust in space   */
void selico(uint8_t x, uint8_t y) { sndon(K_SIDEL, x, y); }   /* SELICO $CCF6 enemy line        */
void sslams(uint8_t x, uint8_t y) { fsndon(K_SIDSL, x, y); }  /* SSLAMS $CCFA slam (any mode)    */
void s3swar(uint8_t x, uint8_t y) { sndon(K_SIDS3, x, y); }   /* S3SWAR $CCFE 3 seconds warning */
void pulstr(uint8_t x, uint8_t y) { sndon(K_SIDPU, x, y); }   /* PULSTR $CD02 pulsation         */
void pulsto(uint8_t x, uint8_t y) { sndon(K_SIDPO, x, y); }   /* PULSTO $CD06 pulsation off     */

/* MODSND ($CD0A): the per-IRQ channel stepper. */
void modsnd(void)
{
    int x;
    for (x = K_NCHANL - 1; x >= 0; x--) {                  /* LCD0A, LCD8E-LCD91 */
        uint8_t a = POINT_X(x);                              /* LCD0C */
        if (a == 0) continue;                              /* LCD0E */
        if ((uint8_t)x == SINDEX) continue;                /* LCD10-LCD12 */
        FRAMES_X(x) = (uint8_t)(FRAMES_X(x) - 1);              /* LCD14 */
        if (FRAMES_X(x) != 0) continue;                      /* LCD16 */
        COUNT_X(x) = (uint8_t)(COUNT_X(x) - 1);                /* LCD18 */
        if (COUNT_X(x) == 0) {                               /* LCD1A: next change group */
            for (;;) {                                     /* RESOUN ($CD1C) */
                uint16_t t;
                POINT_X(x) = (uint8_t)(POINT_X(x) + 2);        /* LCD1C-LCD1E */
                t = (uint16_t)(SOUND + 2u * POINT_X(x));     /* LCD20-LCD24: ASL, carry = page */
                CURRENT_X(x) = cpu_rd((uint16_t)(t + K_STVAL));   /* LCD26-LCD29 / LCD36-LCD39 */
                COUNT_X(x) = cpu_rd((uint16_t)(t + K_NUMBER));    /* LCD2B-LCD2E / LCD3B-LCD3E */
                a = cpu_rd((uint16_t)(t + K_FRCNT));            /* LCD30 / LCD40 */
                FRAMES_X(x) = a;                             /* LCD43 */
                if (a != 0) break;                         /* LCD45 */
                POINT_X(x) = 0;                              /* LCD47: kill it */
                if (CURRENT_X(x) == 0) break;                /* LCD49-LCD4B */
                POINT_X(x) = CURRENT_X(x);                     /* LCD4D-LCD4F: restart location */
            }
        } else {                                           /* LCD54: change within the group (A = POINT) */
            uint16_t t = (uint16_t)(SOUND + 2u * a);       /* LCD54-LCD56 */
            uint8_t old;
            FRAMES_X(x) = cpu_rd((uint16_t)(t + K_FRCNT));   /* LCD58-LCD5B / LCD63-LCD66 */
            a = cpu_rd((uint16_t)(t + K_CHANGE));          /* LCD5D / LCD68 */
            old = CURRENT_X(x);                              /* LCD6B */
            CURRENT_X(x) = (uint8_t)(a + CURRENT_X(x));        /* LCD6D-LCD70 */
            if (x & 1)                                     /* LCD72-LCD74: amplitude channel */
                CURRENT_X(x) = (uint8_t)(((old ^ CURRENT_X(x)) & 0xF0) ^ CURRENT_X(x));  /* LCD76-LCD7D */
        }
        if (x >= 8) hw_pokey_write(1, (uint8_t)(x - 8), CURRENT_X(x));   /* LCD7F-LCD85 STA AUDF2-8,X */
        else        hw_pokey_write(0, (uint8_t)x, CURRENT_X(x));         /* LCD8B STA AUDF1,X */
    }
}

/* INISOU ($CD95): initialise sounds; also records halted POKEYs in QT4.
 * Exits with X = $FF and Y = the first RANDO2 read (NEWLIF tail-calls it, so
 * this is what reaches NONSTA's SSLAMS). */
xy6502 inisou(void)
{
    uint8_t a, y;
    int x;
    xy6502 r;
    hw_pokey_write(0, 0x0F, 0x00);                         /* LCD95-LCD97 SKCTL */
    hw_pokey_write(1, 0x0F, 0x00);                         /* LCD9A */
    QT4 = 0x00;                                            /* LCD9D */
    x = 4;                                                 /* ZPOKST LCDA0 */
    a = hw_random(0);                                      /* LCDA2 */
    y = hw_random(1);                                      /* LCDA5 */
    do {
        int same = (a == hw_random(0));                    /* LCDA8-LCDAB */
        if (same) same = (y == hw_random(1));              /* LCDAD-LCDB0 */
        if (!same) {                                       /* running POKEYs */
            QT4 = a;                                       /* LCDB2 */
            x = 0;                                         /* LCDB5 */
        }
        x--;                                               /* LCDB7 */
    } while (x >= 0);                                      /* LCDB8 */
    hw_pokey_write(0, 0x0F, 0x07);                         /* LCDBA-LCDBC */
    hw_pokey_write(1, 0x0F, 0x07);                         /* LCDBF */
    for (x = 7; x >= 0; x--) {                             /* LCDC2-LCDD1 */
        hw_pokey_write(0, (uint8_t)x, 0x00);               /* LCDC6 */
        hw_pokey_write(1, (uint8_t)x, 0x00);               /* LCDC9 */
        POINT_X(x) = 0x00;                                   /* LCDCC */
        CURRENT_X(x) = 0x00;                                 /* LCDCE */
    }
    hw_pokey_write(0, 0x08, K_AUDCV);                      /* LCDD3-LCDD5 AUDCTL */
    hw_pokey_write(1, 0x08, K_AUDCV2);                     /* LCDD8-LCDDA AUD2CTL */
    r.x = 0xFF;                                            /* LCDD1 BPL falls through at X = $FF */
    r.y = y;
    return r;
}
