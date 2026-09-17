/* alwelg.c - ALWELG ($9000-$A8AF): the game mainline - new wave / new life
 * initialisation, the skill-level request, the wave (skill contour) parameter
 * records, the player cursor, nymphs and invaders (flipper, pulsar, tanker,
 * spiker, fuseball) driven by the CAM enemy-motion scripts, charges,
 * collisions, explosions, the big boom, the star field and the superzapper.
 *
 * One function per Atari routine header (115).  Aliases: WTABEND = DOTYPE,
 * TABJSE = JEXIT.  Entry labels reached only by fall-through, a branch or a
 * JMP (TEXIT, ITMIZ2, NEWGN2, NEWGN3, NEGPUL, ATOP, GOTCHA, JUMPSD, GOTJUM,
 * REVFLP, SKIPIT, MOVER, INCP2, INCISQ, GOTEXP, OKATOP, YESCOL, NOCOL,
 * ZQVAVG, LINER, HIT0) are inlined or small static helpers named after their
 * label.
 *
 * Tables: every ROM table address comes from tools/gen_alwelg.py
 * (alwelg_data.h); the bytes are read with cpu_rd() over progrom.  RTS
 * dispatches (DOTYPE/SPARAD, DONEXT/NPARAD, NEWTY2/NYMTAD, JSRCAM/TABJSR) read
 * the same table and call the routine at that address.
 *
 * Registers: A/X/Y inputs are parameters.  X and Y leave the routines in
 * xy6502 results wherever a caller can store them (the sound starters and
 * UPSCOR take the caller's X/Y; the state routines' exit X/Y reach NONSTA
 * and DISPLAY); tests/lockstep.c compares the exit registers.  ADC/SBC go
 * through adc6502/sbc6502 (NONSTA's protection SED can leave D set); where
 * the ROM branches on N or V the flags come from adc_f/sbc_f as the shared
 * core computes them.  RANDOM / RANDO2 are read through hw_random in the
 * ROM's order.
 */
#include "state.h"
#include "hw.h"
#include "game.h"
#include "alwelg_data.h"

#define R(a)        g.ram[(a) & 0x7FF]          /* RAM cell by address */
#define ROM(a)      cpu_rd((uint16_t)(a))       /* ROM table byte */
#define T3_ADDR     ((uint16_t)(TEMP3 | ((uint16_t)TEMP4 << 8)))
#define T3Y(y)      cpu_rd((uint16_t)(T3_ADDR + (y)))           /* LDA (TEMP3),Y */
#define INDY_ADDR   ((uint16_t)(INDYLO | ((uint16_t)INDYHI << 8)))
#define CAMB(y)     ROM(ALWELG_CAM + (y))                        /* LDA CAM,Y */

static xy6502 xy(uint8_t x, uint8_t y) { xy6502 r; r.x = x; r.y = y; return r; }

/* ADC / SBC with the N, V, Z flags of the shared core (decimal ADC: N and V
 * from the half-corrected sum, Z binary; SBC: all flags binary) - as aldis2.c */
typedef struct { int n, v, z; } flags6502;

static uint8_t adc_f(uint8_t a, uint8_t v, unsigned *c, flags6502 *f)
{
    unsigned cin = *c ? 1u : 0u;
    uint8_t t;
    if (g.dflag) {
        unsigned al = (a & 0x0Fu) + (v & 0x0Fu) + cin;
        if (al >= 0x0A) al = ((al + 0x06) & 0x0F) + 0x10;
        t = (uint8_t)((a & 0xF0u) + (v & 0xF0u) + al);
    } else {
        t = (uint8_t)(a + v + cin);
    }
    f->n = (t & 0x80) != 0;
    f->v = ((~(a ^ v)) & (a ^ t) & 0x80) != 0;
    return adc6502(a, v, c, &f->z);
}

static uint8_t sbc_f(uint8_t a, uint8_t v, unsigned *c, flags6502 *f)
{
    unsigned cin = *c ? 1u : 0u;
    uint8_t b = (uint8_t)((int)a - (int)v - (int)(1u - cin));
    f->n = (b & 0x80) != 0;
    f->v = ((a ^ v) & (a ^ b) & 0x80) != 0;
    return sbc6502(a, v, c, &f->z);
}

static uint8_t adc(uint8_t a, uint8_t v, unsigned *c) { return adc6502(a, v, c, NULL); }
static uint8_t sbc(uint8_t a, uint8_t v, unsigned *c) { return sbc6502(a, v, c, NULL); }
static uint8_t rol_c(uint8_t v, unsigned *c) { unsigned n = (v >> 7) & 1u; v = (uint8_t)((v << 1) | *c); *c = n; return v; }
static uint8_t ror_c(uint8_t v, unsigned *c) { unsigned n = v & 1u; v = (uint8_t)((v >> 1) | (*c << 7)); *c = n; return v; }

/* ======================================================================= */
/* INITIALISATION: NEW WAVE, NEW LIFE, SKILL LEVEL REQUEST                 */
/* ======================================================================= */

/* INEWAV ($9009): new wave.  Exits with INIDSP's X/Y. */
xy6502 inewav(uint8_t x, uint8_t y)
{
    xy6502 r;
    (void)x; (void)y;
    r = contour();                                          /* L9009 */
    iniene();                                               /* L900C (exits X = $FF) */
    r = iniobj(0xFF, r.y);                                  /* L900F */
    inisuz();                                               /* L9012 */
    EYH = 0xFA;                                             /* L9015-L9017 */
    CURMOD = 0x00;                                          /* L9019-L901B: cursor at top, not descending */
    EYL = 0x00;                                             /* L901E */
    QDSTATE = K_CDPLAY;                                     /* L9020-L9022 */
    return r;                                               /* L9024 */
}

/* INEWLI ($9025): new life - cursor, skill level, objects (falls into
 * INIOBJ). */
xy6502 inewli(uint8_t x, uint8_t y)
{
    xy6502 r;
    (void)y;
    inicur();                                               /* L9025 */
    r = contour();                                          /* L9028 */
    (void)x;
    return iniobj(r.x, r.y);                                /* INIOBJ */
}

/* INIOBJ ($902B): deactivate charges, invaders, nymphs, explosions; clear
 * the pot; initialise the display.  Exits with INIDSP's X/Y. */
xy6502 iniobj(uint8_t x, uint8_t y)
{
    xy6502 r;
    (void)x;
    inicha();                                               /* L902B */
    iniinv();                                               /* L902E */
    ininym();                                               /* L9031 */
    iniexp();                                               /* L9034 */
    clrpot();                                               /* L9037 */
    r = inidsp(0xFF, y);                                    /* L903A (X = $FF from INIEXP) */
    BOFLASH = 0xFF;                                         /* L903D-L903F: bonus flasher cleared */
    PULSON = 0xFF;                                          /* L9042 */
    ELICNT = 0x00;                                          /* L9045-L9047: clear enemy spike counter */
    return r;                                               /* L904A */
}

/* NEWAV2 ($904B): new wave part 2 - move the eye down the well towards its
 * destination, then play (JMP MOVCUR). */
xy6502 newav2(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c;
    int z;
    CURSY = K_ILINLIY;                                      /* L904B-L904D: player at the top */
    TEMP0 = 0x00;                                           /* L9050-L9052 */
    TEMP2 = 0x00;                                           /* L9054 */
    a = ZADEST;                                             /* L9056 */
    TEMP1 = a;                                              /* L9059 */
    if (a & 0x80) TEMP2 = (uint8_t)(TEMP2 - 1);             /* L905B-L905D */
    for (x = 0x01; ; x--) {                                 /* L905F, L9068-L9069 */
        a = TEMP1;                                          /* L9061 */
        c = (a >> 7) & 1u;                                  /* L9063 ASL */
        TEMP1 = ror_c(TEMP1, &c);                           /* L9064 */
        TEMP0 = ror_c(TEMP0, &c);                           /* L9066 */
        if (x == 0) { x = 0xFF; break; }
    }
    c = 0;                                                  /* L906D CLC */
    a = adc(TEMP0, R(A_ZADEST + 1), &c);                    /* L906B-L906E: update Z centre */
    R(A_ZADEST + 1) = a;                                    /* L9071 */
    ZADJL = adc(TEMP1, ZADJL, &c);                          /* L9074-L9078 */
    R(A_ZADJL + 1) = adc(TEMP2, R(A_ZADJL + 1), &c);        /* L907A-L907E */
    c = 0;                                                  /* L9082 CLC */
    EYL = adc(EYL, 0x18, &c);                               /* L9080-L9085: move eye closer to well */
    a = adc(EYH, 0x00, &c);                                 /* L9087-L9089 */
    EYH = a;                                                /* L908B */
    if (a >= 0xFC) PLAGRO = 0x01;                           /* L908D-L9093: turn off star field */
    c = 1;                                                  /* L9098 SEC */
    (void)sbc(EYL, EYLDES, &c);                             /* L9096-L9099: eye - destination */
    a = EYH;                                                /* L909B */
    z = (a == 0);
    if (!z) {                                               /* L909D */
        a = sbc6502(a, 0xFF, &c, &z);                       /* L909F */
    }
    if (z) {                                                /* L90A1: past destination? */
        EYL = EYLDES;                                       /* L90A3-L90A5: stop at destination */
        EYH = 0xFF;                                         /* L90A7-L90A9 */
        a = K_CPLAY;                                        /* L90AB */
        if (!(QSTATUS & 0x80)) a = K_CENDGA;                /* L90AD-L90B1: attract - end it */
        QSTATE = a;                                         /* L90B3 */
        x = PLAYUP;                                         /* L90B5 */
        R(A_BONUS + x) = 0x00;                              /* L90B7-L90B9: clear bonus */
    }
    ROTDIS = 0xFF;                                          /* L90BC-L90BE: request well picture update */
    return movcur(x, y);                                    /* L90C1 JMP MOVCUR */
}

/* INIRA0 ($90C4): prepare the skill level request - highest start level,
 * limited by the high score option and sales mode (falls into INIRAT). */
xy6502 inira0(uint8_t x, uint8_t y)
{
    uint8_t a = HIWAVE;                                     /* L90C4 */
    x = (uint8_t)(ALWELG_LEVELE - ALWELG_LEVEL);            /* L90C7 */
    do {
        x--;                                                /* L90C9 */
    } while (a < ROM(ALWELG_LEVEL + x));                    /* L90CA-L90CD: until wave <= highest */
    y = 0x04;                                               /* L90CF */
    if (OPTIN3 & 0x04) {                                    /* L90D1-L90D6: max tied to high score? */
        a = R(A_HSCORH + 21);                               /* L90D8 */
        if (a >= 0x30) y++;                                 /* L90DB-L90DF: > 300000 */
        if (a >= 0x50) y++;                                 /* L90E0-L90E4: > 500000 */
        if (a >= 0x70) y++;                                 /* L90E5-L90E9: > 700000 */
    }
    if ((OPTIN1 & 0x43) == 0x40) y = 0x1B;                  /* L90EA-L90F2: sales mode */
    TEMP0 = y;                                              /* L90F4 */
    if (x < TEMP0) x = TEMP0;                               /* L90F6-L90FA */
    HIRATE = x;                                             /* L90FC */
    if (QSTATUS & 0x80)                                     /* L90FF-L9101: attract? */
        HIWAVE = 0x00;                                      /* L9103-L9105 */
    return inirat(x, y);                                    /* INIRAT */
}

/* INIRAT ($9108): set up the player about to start (SWAPEN for the second
 * player), reset the cursor and level window; outside attract enter the
 * skill-level request state (falls into PRORAT). */
xy6502 inirat(uint8_t x, uint8_t y)
{
    uint8_t a;
    xy6502 r;
    x = NEWPLA;                                             /* L9108 */
    PLAYUP = x;                                             /* L910A */
    if (x != 0) {                                           /* L910C: second player */
        r = swapen(x, y);                                   /* L910E */
        x = r.x; y = r.y;
    }
    RITSID = 0x04;                                          /* L9111-L9113: default levels */
    EYH = 0xFF;                                             /* L9115-L9117: stop rumble */
    CURSL1 = 0x00;                                          /* L9119-L911B: initialise cursor */
    CURSPO = 0x00;                                          /* L911E */
    LEFSID = 0x00;                                          /* L9120 */
    TIMHIS = 0x00;                                          /* L9122: no attract delay */
    a = 0x00;
    x = QSTATUS;                                            /* L9125 */
    if (x & 0x80) {                                         /* L9127: attract? */
        TIMHIS = K_SECOND;                                  /* L9129-L912B */
        WELTYP = 0xFF;                                      /* L912E-L9130: prevent wrap */
        QSTATE = K_CREQRAT;                                 /* L9133-L9135 */
        QDSTATE = K_CDREQRA;                                /* L9137-L9139 */
        CURWAV = 0x00;                                      /* L913B-L913D: first colours */
        r = inicol();                                       /* L913F */
        x = r.x; y = r.y;
        a = 0x10;                                           /* L9142: start timer */
    }
    QTMPAUS = a;                                            /* L9144 */
    clrpot();                                               /* L9146 */
    return prorat(x, y);                                    /* PRORAT */
}

/* PRORAT ($9149): skill level request state - count the seconds down,
 * move the level cursor, take the selection. */
xy6502 prorat(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c;
    flags6502 f;
    xy6502 r;
    TIMHIS = (uint8_t)(TIMHIS - 1);                         /* L9149 */
    if (TIMHIS & 0x80) {                                    /* L914C: another second done? */
        g.dflag = 1;                                        /* L914E SED */
        c = 1;                                              /* L9151 SEC */
        a = sbc_f(QTMPAUS, 0x01, &c, &f);                   /* L914F-L9152 */
        QTMPAUS = a;                                        /* L9154 */
        g.dflag = 0;                                        /* L9156 CLD */
        if (f.n) {                                          /* L9157: seconds left at 0? */
            a = K_MFIRE;                                    /* L9159: auto choose */
            SWFINA = a;                                     /* L915B */
        }
        CK(0x915D);
        if (a == 0x03) s3swar(x, y);                        /* L915D-L9161: 3 seconds warning */
        CK(0x9164);
        TIMHIS = K_SECOND;                                  /* L9164-L9166 */
    }
    a = getcur();                                           /* L9169 (exits Y = A) */
    y = a;
    CK(0x916C);
    a = K_MSUZA | K_MFIRE;                                  /* L916C */
    y = QTMPAUS;                                            /* L916E */
    if (y < 0x08) a = K_MSUZA | K_MFIRE | K_MSTRT1 | K_MSTRT2;   /* L9170-L9174 */
    a &= SWFINA;                                            /* L9176 */
    CK(0x9178);
    if (a) {                                                /* L9178: player selecting this level */
        SWFINA = 0x00;                                      /* L917A-L917C */
        CK(0x917E);
        a = CURSL1;                                         /* L917E */
        y = a;                                              /* L9181 */
        x = PLAYUP;                                         /* L9182 */
        R(A_BONUS + x) = a;                                 /* L9184 */
        a = ROM(ALWELG_LEVEL + y);                          /* L9187 */
        if (!(QSTATUS & 0x80)) {                            /* L918A-L918C: attract? */
            y = 0x01;                                       /* L918E */
            LIVES1 = y;                                     /* L9190 */
            a = (uint8_t)(hw_random(0) & 0x07);             /* L9192-L9195: one of the first 8 levels */
        }
        R(A_WAVEN1 + x) = a;                                /* L9197 */
        CURWAV = a;                                         /* L9199 */
        r = inicol();                                       /* L919B */
        r = contour();                                      /* L919E */
        iniene();                                           /* L91A1 (exits X = $FF) */
        inisuz();                                           /* L91A4 */
        QSTATE = K_CNEWLI;                                  /* L91A7-L91A9 */
        clrpot();                                           /* L91AB */
        x = 0xFF; y = r.y;
    }
    a = SWFINA;                                             /* L91AE */
    CK(0x91B0);
    SWFINA = (uint8_t)(a & (uint8_t)~(K_MFAKE | K_MFIRE | K_MSUZA | K_MSTRT1 | K_MSTRT2));   /* L91B0-L91B2 */
    CK(0x91B4);
    return xy(x, y);                                        /* L91B4 */
}

/* BONSCO ($91B5): bonus score for level index A into TEMP0-2.  Exits
 * X = A * 2. */
xy6502 bonsco(uint8_t a, uint8_t x, uint8_t y)
{
    x = (uint8_t)(a << 1);                                  /* L91B5-L91B6 */
    TEMP0 = 0x00;                                           /* L91B7-L91B9: LSB always 0 */
    TEMP1 = ROM(ALWELG_BONPTM + x);                         /* L91BB-L91BE */
    TEMP2 = ROM(ALWELG_BONPTH + x);                         /* L91C0-L91C3 */
    return xy(x, y);                                        /* L91C5 */
}

/* INICUR ($921B): cursor at the top of segment $0E/$0F. */
void inicur(void)
{
    CURSL1 = 0x0E;                                          /* L921B-L921D */
    CURSPO = 0xF0;                                          /* L9220-L9222 */
    CURMOD = 0x00;                                          /* L9224-L9226 */
    CURSL2 = 0x0F;                                          /* L9229-L922B */
    CURSY = K_ILINLIY;                                      /* L922E-L9230 */
}

/* INIENE ($9234): nymph count and enemy line height for a new wave.  Exits
 * X = $FF. */
void iniene(void)
{
    uint8_t a;
    int x;
    NYMCOU = NWNYMC;                                        /* L9234-L9237 */
    a = NWTELI;                                             /* L923A */
    for (x = K_NLINES - 1; x >= 0; x--)                     /* L923D, L9242-L9243 */
        R(A_LINEY + x) = a;                                 /* L923F */
}

/* ININYM ($9246): clear the nymph timers, then place NYMCOU nymphs on random
 * lines.  Exits X = $FF. */
void ininym(void)
{
    uint8_t x, a;
    int i;
    for (i = K_NNYMPH - 1; i >= 0; i--)                     /* L9246-L924E */
        R(A_NYMPY + i) = 0x00;                              /* L924A */
    x = (uint8_t)(NYMCOU - 1);                              /* L9250-L9253 */
    do {
        a = (uint8_t)(hw_random(0) & 0x0F);                 /* L9254-L9257 */
        R(A_NYMPL + x) = a;                                 /* L9259 */
        a = (uint8_t)((uint8_t)(x << 4) | R(A_NYMPL + x));  /* L925C-L9261 */
        if (a == 0) a = 0x0F;                               /* L9264-L9266 */
        R(A_NYMPY + x) = a;                                 /* L9268 */
        x--;                                                /* L926B */
    } while (!(x & 0x80));                                  /* L926C */
}

/* INIINV ($926F): deactivate the invaders and clear the type counters.
 * Exits X = $FF. */
void iniinv(void)
{
    int x;
    for (x = K_NINVAD - 1; x >= 0; x--)                     /* L926F-L9277 */
        R(A_INVAY + x) = 0x00;                              /* L9273 */
    INMCOU = 0x00;                                          /* L9279 */
    INCCOU = 0x00;                                          /* L927C */
    SPINCO = 0x00;                                          /* L927F */
    FLIPCO = 0x00;                                          /* L9282 */
    TANKCO = 0x00;                                          /* L9285 */
    PULSCO = 0x00;                                          /* L9288 */
    FUSECO = 0x00;                                          /* L928B */
}

/* INICHA ($928F): deactivate every charge.  Exits X = $FF. */
void inicha(void)
{
    int x;
    for (x = K_NCHARG - 1; x >= 0; x--)                     /* L928F-L9297 */
        R(A_CHARY + x) = 0x00;                              /* L9293 */
    CHACOU = 0x00;                                          /* L9299 */
    ESHCOU = 0x00;                                          /* L929C */
}

/* INIEXP ($929F): deactivate every explosion.  Exits X = $FF. */
void iniexp(void)
{
    int x;
    for (x = K_NEXPLO - 1; x >= 0; x--)                     /* L929F-L92A7 */
        R(A_EXPLOY + x) = 0x00;                             /* L92A3 */
    EXPCOU = 0x00;                                          /* L92A9 */
}

/* CLRPOT ($92AD): clear the spinner movement. */
void clrpot(void)
{
    TBHD = 0x00;                                            /* L92AD-L92AF */
    CK(0x92B1);
}

/* SWAPEN ($92B2): swap the active player's data with the save area.  Exits
 * X = $FF, Y = the old SAVEP byte 0. */
xy6502 swapen(uint8_t x, uint8_t y)
{
    uint8_t a;
    (void)y;
    for (x = (uint8_t)(A_SAVEND - A_SAVEP - 1); ; x--) {    /* L92B2, L92C1-L92C2 */
        a = R(A_ACTIP + x);                                 /* L92B4 */
        y = R(A_SAVEP + x);                                 /* L92B7 */
        R(A_SAVEP + x) = a;                                 /* L92BA */
        R(A_ACTIP + x) = y;                                 /* L92BD-L92BE */
        if (x == 0) break;
    }
    return xy(0xFF, y);                                     /* L92C4 */
}

/* L92DA-L9317: look up the parameter record list of WTABLE entry INDEX1 for
 * wave TEMP2; returns A at TEXIT. */
static uint8_t wtable_param(void)
{
    uint8_t x = INDEX1, y, a, e;
    unsigned c;
    INDYHI = ROM(ALWELG_WTABLE + x);                        /* L92DA-L92DF */
    INDYLO = ROM(ALWELG_WTABLE - 1 + x);                    /* L92E1-L92E4: byte to set */
    TEMP4 = ROM(ALWELG_WTABLE - 2 + x);                     /* L92E6-L92E9 */
    TEMP3 = ROM(ALWELG_WTABLE - 3 + x);                     /* L92EB-L92EE: record list */
    INDEX2 = 0x01;                                          /* L92F0-L92F2 */
    y = 0x00;                                               /* L92F4 */
    for (;;) {
        a = T3Y(y);                                         /* L92F6 */
        TYPCOD = a;                                         /* L92F8 */
        if (a == 0) return a;                               /* L92FB BEQ TEXIT: end of table */
        a = TEMP2;                                          /* L92FD */
        y++;                                                /* L92FF */
        c = (a >= T3Y(y));                                  /* L9300 */
        y++;                                                /* L9302 */
        if (c) {                                            /* L9303: wave >= start? */
            e = T3Y(y);                                     /* L9305 */
            c = (a >= e);
            if (a == e) c = 0;                              /* L9307-L9309 */
            if (!c) {                                       /* L930A: wave <= end? */
                y++;                                        /* L930C */
                return dotype(y);                           /* L930D JSR DOTYPE, L9310 JMP TEXIT */
            }
        }
        y = donext(y);                                      /* L9313 */
        /* L9316-L9317 CLC / BCC: always loop */
    }
}

/* CONTOUR ($92C5): set the skill parameters of wave CURWAV from the WTABLE
 * records, then the easy / hard options and the derived speeds.  Exits with
 * TIMES8's X/Y. */
xy6502 contour(void)
{
    uint8_t a, x, y;
    unsigned c;
    a = CURWAV;                                             /* L92C5 */
    if (a >= 0x62)                                          /* L92C7-L92C9: past the last level? */
        a = (uint8_t)((hw_random(1) & 0x1F) | 0x40);        /* L92CB-L92D0 */
    TEMP2 = a;                                              /* L92D2 */
    TEMP2 = (uint8_t)(TEMP2 + 1);                           /* L92D4 */
    INDEX1 = (uint8_t)(ALWELG_WTABEND - ALWELG_WTABLE - 1); /* L92D6-L92D8 */
    for (;;) {
        a = wtable_param();                                 /* L92DA-L9317 */
        /* TEXIT */
        cpu_wr(INDY_ADDR, a);                               /* L9319-L931B: save it */
        c = 1;                                              /* L931F SEC */
        a = sbc(INDEX1, 0x04, &c);                          /* L931D-L9320 */
        INDEX1 = a;                                         /* L9322 */
        if (a == 0xFF) break;                               /* L9324-L9326 */
    }
    /* EASY - MED - HARD OPTIONS */
    a = (uint8_t)(OPTIN3 & 0x03);                           /* L9328-L932B */
    if (a == K_ZEASY) {                                     /* L932D-L932F */
        WCHAMX = (uint8_t)(WCHAMX - 1);                     /* L9331: less enemy shots */
        a = (uint8_t)(WINVIL ^ 0xFF);                       /* L9334-L9337 */
        c = a & 1u; a >>= 1;                                /* L9339 LSR */
        c = a & 1u; a >>= 1;                                /* L933A */
        c = a & 1u; a >>= 1;                                /* L933B */
        WINVIL = adc(a, WINVIL, &c);                        /* L933C-L933F: speeds - 1/8 */
        if (CURWAV < 0x11) WTTFRA = (uint8_t)(WTTFRA - 1);  /* L9342-L9348: flip rate at top */
    } else if (a == K_ZHARD) {                              /* L934D-L934F */
        WCHAMX = (uint8_t)(WCHAMX + 1);                     /* L9351: more enemy shots, up to 4 */
        if (WCHAMX >= 0x03) WCHAMX = 0x03;                  /* L9354-L935D */
        a = WINVIL;                                         /* L9360 */
        c = a & 1u; a >>= 1;                                /* L9363 LSR */
        c = a & 1u; a >>= 1;                                /* L9364 */
        c = a & 1u; a >>= 1;                                /* L9365 */
        a |= 0xE0;                                          /* L9366 */
        WINVIL = adc(a, WINVIL, &c);                        /* L9368-L936B: speed + 1/8 */
        a = NWNYMC;                                         /* L936E */
        c = a & 1u; a >>= 1;                                /* L9371 */
        c = a & 1u; a >>= 1;                                /* L9372 */
        c = a & 1u; a >>= 1;                                /* L9373 */
        NWNYMC = adc(a, NWNYMC, &c);                        /* L9374-L9377: attack + 1/8 */
        WPULFI = (uint8_t)(WPULFI | K_ZFIRYE);              /* L937A-L937F: pulsars fire */
    }
    a = times8(R(A_WINVIL + K_ZABTRA), &x, &y);             /* L9382-L9385: spinner */
    R(A_WINVIL + K_ZABTRA) = a;                             /* L9388: speed (frac) */
    R(A_WINVIN + K_ZABTRA) = y;                             /* L938B: speed (int) */
    R(A_ENSIZE + K_ZABTRA) = x;                             /* L938E: collision range */
    a = times8(WCHARL, &x, &y);                             /* L9391-L9394: enemy shot */
    WCHARL = a;                                             /* L9397 */
    WCHARIN = y;                                            /* L939A */
    CHACHA = x;                                             /* L939D: charge-charge collision range */
    a = times8(WINVIL, &x, &y);                             /* L939F-L93A2 */
    WINVIL = a;                                             /* L93A5 */
    R(A_WINVIL + K_ZABTAN) = a;                             /* L93A8 */
    R(A_WINVIN + K_ZABTAN) = y;                             /* L93AB */
    WINVIN = y;                                             /* L93AE */
    R(A_ENSIZE + K_ZABFLI) = x;                             /* L93B1: charge-invader collision range */
    R(A_ENSIZE + K_ZABTAN) = x;                             /* L93B4 */
    R(A_ENSIZE + K_ZABPUL) = x;                             /* L93B7 */
    a = WINVIL;                                             /* L93BA */
    c = (a >> 7) & 1u; a = (uint8_t)(a << 1);               /* L93BD ASL */
    WFUSIL = a;                                             /* L93BE */
    WFUSIH = rol_c(WINVIN, &c);                             /* L93C1-L93C5: fuse = 2 x invader speed */
    R(A_ENSIZE + K_ZABFUS) = (uint8_t)((K_PCVELO + 3) / 2); /* L93C8-L93CA */
    R(A_WINVIL + K_ZABPUL) = 0xA0;                          /* L93CD-L93CF */
    R(A_WINVIN + K_ZABPUL) = 0xFE;                          /* L93D2-L93D4 */
    R(A_WTACAR + 1) = K_ZCARFL;                             /* L93D7-L93D9 */
    R(A_WTACAR + 0) = K_ZCARFL;                             /* L93DC */
    return xy(x, y);                                        /* L93DF */
}

/* TIMES8 ($93E0): A = signed speed; returns A = low byte of A * 8, *y = high
 * byte (sign extended), *x = collision range with the player charge. */
uint8_t times8(uint8_t a, uint8_t *x, uint8_t *y)
{
    uint8_t s;
    unsigned c;
    *y = 0xFF;                                              /* L93E0: all speeds are minus */
    TEMP0 = 0xFF;                                           /* L93E2 */
    c = (a >> 7) & 1u; a = (uint8_t)(a << 1);               /* L93E4 */
    TEMP0 = rol_c(TEMP0, &c);                               /* L93E5 */
    c = (a >> 7) & 1u; a = (uint8_t)(a << 1);               /* L93E7 */
    TEMP0 = rol_c(TEMP0, &c);                               /* L93E8 */
    c = (a >> 7) & 1u; a = (uint8_t)(a << 1);               /* L93EA */
    TEMP0 = rol_c(TEMP0, &c);                               /* L93EB: x 8 */
    *y = TEMP0;                                             /* L93ED */
    s = a;                                                  /* L93EF PHA */
    c = 0;                                                  /* L93F3 CLC */
    a = adc((uint8_t)(*y ^ 0xFF), (uint8_t)(K_PCVELO + 1 + 1 + 2), &c);   /* L93F0-L93F4 */
    *x = (uint8_t)(a >> 1);                                 /* L93F6-L93F7: average of the speeds */
    return s;                                               /* L93F8-L93F9 */
}

/* DOTYPE = WTABEND ($9677): parameter of the record at (TEMP3),Y by record
 * type TYPCOD (SPARAD RTS dispatch).  Returns A. */
uint8_t dotype(uint8_t y)
{
    uint8_t x = TYPCOD;                                     /* L9677 */
    uint16_t target = (uint16_t)((ROM(ALWELG_SPARAD + x) | (ROM(ALWELG_SPARAD + 1 + x) << 8)) + 1);   /* L967A-L9682 */
    switch (target) {
    case ALWELG_SAMALL: return samall(y);
    case ALWELG_ITMIZE: return itmize(y);
    case ALWELG_DOTZAN: return dotzan(y);
    case ALWELG_DOTA:   return dota(y);
    case ALWELG_DOTB:   return dotb(y);
    case ALWELG_DOTR:   return dotr(y);
    default:            hw_soft_watchdog(); return 0;      /* EOT / odd type: the ROM runs wild */
    }
}

/* DONEXT ($9683): Y past the record's parameter field by record type
 * (NPARAD RTS dispatch).  Returns Y. */
uint8_t donext(uint8_t y)
{
    uint8_t x = TYPCOD;                                     /* L9683 */
    uint16_t target = (uint16_t)((ROM(ALWELG_NPARAD + x) | (ROM(ALWELG_NPARAD + 1 + x) << 8)) + 1);   /* L9686-L968E */
    switch (target) {
    case ALWELG_ONEBYT: return onebyt(y);
    case ALWELG_NITMIZ: return nitmiz(y);
    case ALWELG_TWOBYT: return twobyt(y);
    default:            hw_soft_watchdog(); return y;
    }
}

/* ITMIZ2 ($96B9): the itemised byte for wave A. */
static uint8_t itmiz2(uint8_t a, uint8_t y)
{
    unsigned c;
    TEMP0 = y;                                              /* L96B9 */
    y = (uint8_t)(y - 2);                                   /* L96BB-L96BC */
    c = 1;                                                  /* L96BD SEC */
    a = sbc(a, T3Y(y), &c);                                 /* L96BE */
    c = 0;                                                  /* L96C0 CLC */
    a = adc(a, TEMP0, &c);                                  /* L96C1 */
    return samall(a);                                       /* L96C3 TAY, SAMALL */
}

/* DOTZAN ($96AB): type TZANDF - wave index ((TEMP2 - 1) & $0F) + 1, then as
 * ITMIZE.  Returns A. */
uint8_t dotzan(uint8_t y)
{
    uint8_t a;
    unsigned c;
    flags6502 f;
    c = 1;                                                  /* L96AD SEC */
    a = (uint8_t)(sbc(TEMP2, 0x01, &c) & 0x0F);             /* L96AB-L96B0 */
    c = 0;                                                  /* L96B2 CLC */
    a = adc_f(a, 0x01, &c, &f);                             /* L96B3 */
    if (!f.n) return itmiz2(a, y);                          /* L96B5 BPL ITMIZ2 */
    return itmize(y);                                       /* (never: falls into ITMIZE) */
}

/* ITMIZE ($96B7): type TZ - the byte for wave TEMP2.  Returns A. */
uint8_t itmize(uint8_t y)
{
    return itmiz2(TEMP2, y);                                /* L96B7 */
}

/* SAMALL ($96C4): the same byte for each wave in the range.  Returns A. */
uint8_t samall(uint8_t y)
{
    return T3Y(y);                                          /* L96C4-L96C6 */
}

/* TWOBYT ($96C7): skip a two-byte parameter field.  Returns Y. */
uint8_t twobyt(uint8_t y)
{
    return onebyt((uint8_t)(y + 1));                        /* L96C7 */
}

/* ONEBYT ($96C8): skip a one-byte parameter field.  Returns Y. */
uint8_t onebyt(uint8_t y)
{
    return (uint8_t)(y + 2);                                /* L96C8-L96CA */
}

/* NITMIZ ($96CB): skip the itemised bytes of a TZ / TZANDF record.  Returns
 * Y. */
uint8_t nitmiz(uint8_t y)
{
    uint8_t a;
    unsigned c;
    a = T3Y(y);                                             /* L96CB */
    y--;                                                    /* L96CD */
    c = 1;                                                  /* L96CE SEC */
    a = sbc(a, T3Y(y), &c);                                 /* L96CF */
    TEMP0 = a;                                              /* L96D1 */
    c = 1;                                                  /* L96D4 SEC */
    a = adc(y, TEMP0, &c);                                  /* L96D3-L96D5 */
    return (uint8_t)(a + 2);                                /* L96D7-L96DA */
}

/* DOTB ($96DB): type TB - byte + WINVIL.  Returns A. */
uint8_t dotb(uint8_t y)
{
    unsigned c = 0;                                         /* L96DD CLC */
    return adc(T3Y(y), WINVIL, &c);                         /* L96DB-L96E1 */
}

/* DOTA ($96E2): type TA - byte 3 + n * byte 4, n = RANGER.  Returns A. */
uint8_t dota(uint8_t y)
{
    uint8_t a, x;
    unsigned c;
    x = ranger(y);                                          /* L96E2-L96E5 */
    a = T3Y(y);                                             /* L96E6 */
    y++;                                                    /* L96E8 */
    while (x != 0) {                                        /* L96E9-L96EB */
        c = 0;                                              /* L96ED CLC */
        a = adc(a, T3Y(y), &c);                             /* L96EE */
        x--;                                                /* L96F0 */
    }
    return a;                                               /* L96F3 */
}

/* RANGER ($96F4): A = TEMP2 - start wave of the record (Y preserved). */
uint8_t ranger(uint8_t y)
{
    unsigned c;
    TEMP0 = y;                                              /* L96F6 */
    c = 1;                                                  /* L96FA SEC */
    return sbc(TEMP2, T3Y((uint8_t)(y - 2)), &c);           /* L96F4-L96FF */
}

/* DOTR ($9700): type TR - alternate between bytes 3 and 4.  Returns A. */
uint8_t dotr(uint8_t y)
{
    if (ranger(y) & 0x01) y++;                              /* L9700-L9707 */
    return T3Y(y);                                          /* L9708-L970A */
}

/* ======================================================================= */
/* PLAY MAINLINE, CURSOR                                                   */
/* ======================================================================= */

/* PLAY ($970B): play state at the top of the well. */
xy6502 play(uint8_t x, uint8_t y)
{
    xy6502 r = movcur(x, y);                                /* L970B */
    r = firepc(r.x, r.y);                                   /* L970E */
    r = prosuz(r.x, r.y);                                   /* L9711 */
    r = movnym(r.x, r.y);                                   /* L9714 */
    r = movinv(r.x, r.y);                                   /* L9717 */
    r = movcha(r.x, r.y);                                   /* L971A */
    r = fireic(r.x, r.y);                                   /* L971D */
    r = collis(r.x, r.y);                                   /* L9720 */
    r = proexp(r.x, r.y);                                   /* L9723 */
    return analyz(r.x, r.y);                                /* L9726 JMP ANALYZ */
}

/* PLDROP ($9729): drop mode - the player shoots down the tube to the next
 * well. */
xy6502 pldrop(uint8_t x, uint8_t y)
{
    xy6502 r;
    ELICNT = (uint8_t)(ELICNT & 0x7F);                      /* L9729-L972E: clear warning request */
    r = movcur(x, y);                                       /* L9731 */
    r = movcud(r.x, r.y);                                   /* L9734 */
    r = proexp(r.x, r.y);                                   /* L9737 */
    r = firepc(r.x, r.y);                                   /* L973A */
    r = movcha(r.x, r.y);                                   /* L973D */
    if (CURSL2 & 0x80)                                      /* L9740-L9743: cursor dead? */
        r = analyz(r.x, r.y);                               /* L9745 */
    return r;                                               /* L9748 */
}

/* MOVCUR ($9749): move the cursor by the spinner (attract: AUTOCU), with
 * the planar well's edges.  Exits X = WELTYP. */
xy6502 movcur(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c;
    if (CURSL2 & 0x80) return xy(x, y);                     /* L9749-L974E: cursor dead */
    x = 0x00;                                               /* L974F */
    if (!(QSTATUS & 0x80)) {                                /* L9751-L9753: attract? */
        a = autocu(&x, &y);                                 /* L9755: auto movement */
    } else {
        a = TBHD;                                           /* L975B: manual */
        CK(0x975D);
        if (a & 0x80) {                                     /* L975D: maximise knob reading */
            if (a < 0xE1) a = 0xE1;                         /* L975F-L9763 */
        } else {
            if (a >= 0x1F) a = 0x1F;                        /* L9768-L976C */
        }
        TBHD = x;                                           /* L976E */
    }
    CK(0x9770);
    TEMP2 = a;                                              /* L9770 */
    c = 1;                                                  /* L9774 SEC */
    a = adc((uint8_t)(a ^ 0xFF), CURSPO, &c);               /* L9772-L9775: invert, update master position */
    TEMP3 = a;                                              /* L9777: new CURSPO */
    x = WELTYP;                                             /* L9779 */
    if (x != 0) {                                           /* L977C: planar surface? */
        if (a >= 0xF0) {                                    /* L977E-L9780: split cursor? */
            a = 0xEF;                                       /* L9782: move away from edge */
            TEMP3 = a;                                      /* L9784 */
        }
        if ((a ^ TEMP2) & 0x80) {                           /* L9786-L9788 */
            if ((TEMP3 ^ CURSPO) & 0x80) {                  /* L978A-L978E: wrapped around? */
                a = (CURSPO & 0x80) ? 0xEF : 0x00;          /* L9790-L9799: old position low / high */
                TEMP3 = a;                                  /* L979B */
            }
        }
    }
    a = (uint8_t)(TEMP3 >> 4);                              /* L979D-L97A2 */
    TEMP1 = a;                                              /* L97A3: new CURSL1 */
    c = 0;                                                  /* L97A5 CLC */
    TEMP2 = (uint8_t)(adc(a, 0x01, &c) & 0x0F);             /* L97A6-L97AA: new CURSL2 */
    if (TEMP1 != CURSL1) sboing(x, y);                      /* L97AC-L97B3: new position - sound */
    CURSL1 = TEMP1;                                         /* L97B6-L97B8 */
    CURSL2 = TEMP2;                                         /* L97BB-L97BD */
    CURSPO = TEMP3;                                         /* L97C0-L97C2 */
    return xy(x, y);                                        /* L97C4 */
}

/* AUTOCU ($97C5): attract-mode cursor: towards the highest invader.
 * Returns A = the movement (or the last INVAY read); *x = its index, *y. */
uint8_t autocu(uint8_t *x, uint8_t *y)
{
    uint8_t a = 0, i;
    TEMP0 = 0xFF;                                           /* L97C5-L97C7 */
    TEMP1 = 0xFF;                                           /* L97C9 */
    i = WINVMX;                                             /* L97CB */
    do {
        a = R(A_INVAY + i);                                 /* L97CE */
        if (a != 0 && a < TEMP0) {                          /* L97D1-L97D5: alive, highest? */
            TEMP0 = a;                                      /* L97D7 */
            TEMP1 = i;                                      /* L97D9 */
        }
        i--;                                                /* L97DB */
    } while (!(i & 0x80));                                  /* L97DC */
    *x = TEMP1;                                             /* L97DE */
    if (*x & 0x80) return a;                                /* L97E0 */
    a = poldel(R(A_INVAL1 + *x), CURSL1);                   /* L97E2-L97E8: how far, which way? */
    *y = CURSL1;                                            /* L97E5 */
    *y = a;                                                 /* L97EB */
    if (a == 0) return a;                                   /* L97EC: already there */
    return (a & 0x80) ? 0x09 : 0xF7;                        /* L97EE-L97F5 */
}

/* MOVCUD ($97F8): move the dropping cursor down the tube; eye, star field,
 * acceleration, collision with the enemy lines. */
xy6502 movcud(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c;
    if (CURSL2 & 0x80) return xy(x, y);                     /* L97F8-L97FD: player dead */
    if (!(CURMOD & 0x80)) return xy(x, y);                  /* L97FE-L9803: not dropping */
    if (CURSY == K_ILINLIY) souts2(x, y);                   /* L9804-L980B: still at top - rumble */
    c = 0;                                                  /* L9811 CLC */
    CURSYL = adc(CURSYL, CURSVL, &c);                       /* L980E-L9815: update depth */
    a = adc(CURSY, CURSVH, &c);                             /* L9818-L981B */
    CURSY = a;                                              /* L981E */
    if (!c) c = (a >= K_ILINDDY);                           /* L9821-L9823 */
    if (c) {                                                /* L9825: past bottom? */
        QSTATE = K_CENDWAV;                                 /* L9827-L9829: space mode */
        souts3(x, y);                                       /* L982B */
        CURSY = 0xFF;                                       /* L982E-L9830 */
    }
    if (CURSY >= 0x50 && PLAGRO == 0) {                     /* L9833-L983D */
        instar();                                           /* L983F */
        x = 0xFF;
    }
    c = 0;                                                  /* L9844 CLC */
    EYLL = adc(EYLL, CURSVL, &c);                           /* L9842-L9848: update eye */
    a = adc(EYL, CURSVH, &c);                               /* L984A-L984C */
    if (c) EYH = (uint8_t)(EYH + 1);                        /* L984F-L9851 */
    if (a != EYL) ROTDIS = (uint8_t)(ROTDIS + 1);           /* L9853-L9857: new well display */
    EYL = a;                                                /* L985A */
    a = (uint8_t)(CURWAV << 2);                             /* L985C-L985F: wave acceleration */
    if (a >= 0x30) a = 0x30;                                /* L9860-L9864 */
    c = 0;                                                  /* L9866 CLC */
    a = adc(a, 0x20, &c);                                   /* L9867: base acceleration */
    c = 0;                                                  /* L9869 CLC */
    CURSVL = adc(a, CURSVL, &c);                            /* L986A-L986D */
    CURSVH = adc(CURSVH, 0x00, &c);                         /* L9870-L9875 */
    if (CURSY < K_ILINDDY) {                                /* L9878-L987D: still on lines */
        x = K_NLINES - 1;                                   /* L987F */
        do {
            a = R(A_LINEY + x);                             /* L9881 */
            if (a != 0 && x == CURSL1 && a < CURSY) {       /* L9884-L988E: cursor hits the line */
                pulsto(x, y);                               /* L9890 */
                inppsq(x, y);                               /* L9893 */
                PLAGRO = 0x00;                              /* L9896-L9898 */
                inicha();                                   /* L989B */
                x = 0xFF;
            }
            x--;                                            /* L989E */
        } while (!(x & 0x80));                              /* L989F */
    }
    return xy(x, y);                                        /* L98A1 */
}

/* ======================================================================= */
/* NYMPHS                                                                  */
/* ======================================================================= */

/* MOVNYM ($98A2): move the nymphs up; convert them to invaders; off-limit
 * lines.  Exits X = $FF. */
xy6502 movnym(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c;
    xy6502 r;
    (void)x;
    y = 0x00;                                               /* L98A2 */
    NEOFLI = y;                                             /* L98A4: clear new off limits */
    c = 0;                                                  /* L98AA CLC */
    a = adc(INMCOU, INCCOU, &c);                            /* L98A7-L98AB */
    if (a >= WINVMX && a != WINVMX) y = 0xFF;               /* L98AE-L98B5: slots booked */
    if (SUZTIM != 0) y = 0xFF;                              /* L98B7-L98BC: avoid kamikaze */
    TEMPY = y;                                              /* L98BE */
    x = K_NNYMPH - 1;                                       /* L98C0 */
    do {
        a = R(A_NYMPY + x);                                 /* L98C2 */
        if (a != 0) {                                       /* L98C5: active? */
            if (!(TEMPY & 0x80)) {                          /* L98C7-L98C9: up movement ok? */
                c = 1;                                      /* L98CB SEC */
                a = sbc(a, 0x01, &c);                       /* L98CC */
                R(A_NYMPY + x) = a;                         /* L98CE */
                if (a == 0) {                               /* L98D1: convert? */
                    r = conymp(x, y);                       /* L98D3 */
                    x = r.x; y = r.y;
                } else if (a == 0x3F) {                     /* L98D9-L98DB: entering alone zone? */
                    y = R(A_NYMPL + x);                     /* L98DD */
                    a = (uint8_t)((NEOFLI | NEOFLI) & ROM(ROM_D70MSK + y));   /* L98E0-L98E6 */
                    if (a != 0)                             /* L98E9: occupied? */
                        R(A_NYMPY + x) = (uint8_t)(R(A_NYMPY + x) + 1);       /* L98EB: back off */
                }
            }
            a = R(A_NYMPY + x);                             /* L98EE */
            if (a >= 0x40) {                                /* L98F1-L98F3 */
                if ((QFRAME & 0x01) == 0) {                 /* L98F5-L98F9: time to rotate? */
                    c = 0;                                  /* L98FE CLC */
                    R(A_NYMPL + x) = (uint8_t)(adc(R(A_NYMPL + x), 0x01, &c) & 0x0F);   /* L98FB-L9903 */
                }
            } else if (a >= 0x20) {                         /* L9909-L990B: alone zone? */
                y = R(A_NYMPL + x);                         /* L990D */
                NEOFLI = (uint8_t)(ROM(ROM_D70MSK + y) | NEOFLI);   /* L9910-L9916: line off limits */
            }
        }
        x--;                                                /* L9919 */
    } while (!(x & 0x80));                                  /* L991A */
    OLOFLI = NEOFLI;                                        /* L991C-L991F */
    return xy(x, y);                                        /* L9922 */
}

/* CONYMP ($9923): convert nymph X to an invader.  X preserved. */
xy6502 conymp(uint8_t x, uint8_t y)
{
    TEMP0 = K_ILINDDY;                                      /* L9923-L9925: start at bottom */
    TEMP1 = R(A_NYMPL + x);                                 /* L9927-L992A: start line */
    SAVEX = x;                                              /* L992C */
    y = nymcha(x, y);                                       /* L992E */
    x = SAVEX;                                              /* L9931 */
    if (TEMP0 != 0) {                                       /* L9933-L9935 */
        if (actinv(x, y) != 0) {                            /* L9937-L993A: slot found? */
            NYMCOU = (uint8_t)(NYMCOU - 1);                 /* L993C */
            R(A_NYMPY + x) = 0x00;                          /* L993F-L9941 */
            return xy(x, y);                                /* L9944 */
        }
    }
    TEMPY = 0xFF;                                           /* L9945-L9947: stop up movement */
    R(A_NYMPY + x) = (uint8_t)(R(A_NYMPY + x) + 1);         /* L9949: back to old position */
    return xy(x, y);                                        /* L994C */
}

/* ACTINV ($994D): activate an invader in a free slot with TEMP0 (Y),
 * TEMP1 (CW line), TEMP2/TEMP3 (characteristics), TEMP4 (CAM).  Returns
 * A = $10 if a slot was found, else 0; X and Y preserved. */
uint8_t actinv(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c;
    SAVEY = y;                                              /* L994D */
    y = WINVMX;                                             /* L994F */
    do {
        if (R(A_INVAY + y) == 0) {                          /* L9952-L9955: slot? */
            R(A_INVAY + y) = TEMP0;                         /* L9957-L9959 */
            a = TEMP1;                                      /* L995C */
            if (a == 0x0F && (WELTYP & 0x80))               /* L995E-L9965: planar split? */
                a = (uint8_t)(hw_random(0) & 0x0E);         /* L9967-L996A */
            R(A_INVAL1 + y) = a;                            /* L996C: CW line */
            c = 0;                                          /* L996F CLC */
            R(A_INVAL2 + y) = (uint8_t)(adc(a, 0x01, &c) & 0x0F);   /* L9970-L9974: CCW line */
            R(A_INVACT + y) = 0x00;                         /* L9977-L9979: timer */
            R(A_INVAC2 + y) = TEMP3;                        /* L997C-L997E */
            R(A_INVCAM + y) = TEMP4;                        /* L9981-L9983 */
            INMCOU = (uint8_t)(INMCOU + 1);                 /* L9986 */
            a = TEMP2;                                      /* L9989 */
            R(A_INVAC1 + y) = a;                            /* L998B */
            a &= K_INVABI;                                  /* L998E-L9990 */
            SAVEY = x;                                      /* L9992 */
            R(A_FLIPCO + a) = (uint8_t)(R(A_FLIPCO + a) + 1);   /* L9994-L9995: type counter */
            return 0x10;                                    /* L9998-L999C */
        }
        y--;                                                /* L999D */
    } while (!(y & 0x80));                                  /* L999E */
    return 0x00;                                            /* L99A0-L99A4 */
}

/* NYMCHA ($99A5): choose the type of the new invader (openings per type,
 * minimums, smart launch, random).  Returns the exit Y. */
uint8_t nymcha(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c;
    for (x = 0x04; ; x--) {                                 /* L99A5-L99AD */
        R(A_OPFLIP + x) = 0x00;                             /* L99A9 */
        if (x == 0) break;
    }
    for (x = 0x04; ; x--) {                                 /* L99AF, L99BD-L99BE */
        c = 1;                                              /* L99B4 SEC */
        a = sbc(R(A_WFLMAX + x), R(A_FLIPCO + x), &c);      /* L99B1-L99B5 */
        if (c) R(A_OPFLIP + x) = a;                         /* L99B8-L99BA: openings */
        if (x == 0) break;
    }
    y = WINVMX;                                             /* L99C0 */
    do {
        if (R(A_INVAY + y) != 0) {                          /* L99C3-L99C6: alive? */
            a = (uint8_t)(R(A_INVAC2 + y) & K_INVCAR);      /* L99C8-L99CB: carrier? */
            if (a != 0) {                                   /* L99CD */
                x = a;                                      /* L99CF */
                if (x == K_ZCARFU) x = K_ZABFUS + 1;        /* L99D0-L99D4 */
                R(A_OPFLIP - 1 + x) = (uint8_t)(R(A_OPFLIP - 1 + x) - 1);   /* L99D6 */
                R(A_OPFLIP - 1 + x) = (uint8_t)(R(A_OPFLIP - 1 + x) - 1);   /* L99D9 */
            }
        }
        y--;                                                /* L99DC */
    } while (!(y & 0x80));                                  /* L99DD */
    x = 0x04;                                               /* L99DF */
    c = 0;                                                  /* L99E4 CLC */
    a = adc(WINVMX, 0x01, &c);                              /* L99E1-L99E5 */
    do {
        c = 1;                                              /* L99E7 SEC */
        a = sbc(a, R(A_FLIPCO + x), &c);                    /* L99E8 */
        x--;                                                /* L99EB */
    } while (!(x & 0x80));                                  /* L99EC */
    for (x = 0x04; ; x--) {                                 /* L99EE, L99F8-L99F9 */
        if (a < R(A_OPFLIP + x)) R(A_OPFLIP + x) = a;       /* L99F0-L99F5 */
        if (x == 0) break;
    }
    y = 0x00;                                               /* L99FD */
    for (x = 0x04; ; x--) {                                 /* L99FB, L9A05-L9A06 */
        if (R(A_OPFLIP + x) != 0) y++;                      /* L99FF-L9A04: types with openings */
        if (x == 0) break;
    }
    if (y == 0) goto fail;                                  /* L9A08-L9A09 */
    y--;                                                    /* L9A0B */
    if (y == 0) {                                           /* L9A0C: only 1 type? */
        x = 0x04;                                           /* L9A0E */
        do {
            if (R(A_OPFLIP + x) != 0 && R(A_WFLMIN + x) != 0) {   /* L9A10-L9A18 */
                if (newtyp(x, &y) != 0) return y;           /* L9A1A-L9A1F */
            }
            x--;                                            /* L9A20 */
        } while (!(x & 0x80));                              /* L9A21 */
        goto fail;                                          /* L9A23-L9A24 */
    }
    SXL = y;                                                /* L9A26 */
    x = 0x04;                                               /* L9A28 */
    do {
        if (R(A_OPFLIP + x) != 0 && R(A_FLIPCO + x) < R(A_WFLMIN + x)) {   /* L9A2A-L9A35 */
            if (newtyp(x, &y) != 0) return y;               /* L9A37-L9A3C */
        }
        x--;                                                /* L9A3D */
    } while (!(x & 0x80));                                  /* L9A3E */
    if (OPSPIN != 0 && OPTANK != 0) {                       /* L9A40-L9A48: smart launch */
        y = TEMP1;                                          /* L9A4A */
        a = R(A_LINEY + y);                                 /* L9A4C */
        if (a == 0) a = 0xFF;                               /* L9A4F-L9A51 */
        x = (uint8_t)(A_OPSPIN - A_OPFLIP);                 /* L9A53: short line - spinner */
        if (a < 0xCC) x = (uint8_t)(A_OPTANK - A_OPFLIP);   /* L9A55-L9A59: long line - tanker */
        if (newtyp(x, &y) != 0) return y;                   /* L9A5B-L9A60 */
    }
    x = (uint8_t)((hw_random(1) & 0x03) + 1);               /* L9A61-L9A67: random type (not 0) */
    y = 0x04;                                               /* L9A68 */
    do {
        if (R(A_WFLMIN + x) != 0 && R(A_OPFLIP + x) != 0) { /* L9A6A-L9A72 */
            if (newtyp(x, &y) != 0) return y;               /* L9A74-L9A79 */
        }
        x--;                                                /* L9A7A */
        if (x & 0x80) x = 0x04;                             /* L9A7B-L9A7D: wrap */
        y--;                                                /* L9A7F */
    } while (!(y & 0x80));                                  /* L9A80 */
fail:
    TEMP0 = 0x00;                                           /* L9A82-L9A84: signal failure */
    return y;                                               /* L9A86 */
}

/* NEWTYP ($9A87): try to launch type X (falls into NEWTY2).  Returns A
 * (0 = failure), *y = exit Y; X preserved. */
uint8_t newtyp(uint8_t x, uint8_t *y)
{
    return newty2(x, x, y);                                 /* L9A87 TXA */
}

/* NEWTY2 ($9A88): characteristics of type A (NYMTAD RTS dispatch). */
uint8_t newty2(uint8_t a, uint8_t x, uint8_t *y)
{
    uint16_t target;
    *y = (uint8_t)(a << 1);                                 /* L9A88-L9A89 */
    target = (uint16_t)((ROM(ALWELG_NYMTAD + *y) | (ROM(ALWELG_NYMTAD + 1 + *y) << 8)) + 1);   /* L9A8A-L9A92 */
    switch (target) {
    case ALWELG_NEWFLI: return newfli(y);
    case ALWELG_NEWPUL: return newpul(y);
    case ALWELG_NEWTAN: return newtan(x, y);
    case ALWELG_NEWSPI: return newspi(y);
    case ALWELG_NEWFUS: return newfus(y);
    default:            hw_soft_watchdog(); return 0;      /* type > 4: the ROM runs wild */
    }
}

/* NEWGN3 ($9AF6): common exit - TEMP2 = type Y, TEMP4 = CAM A; returns
 * TEMP0 (the success signal). */
static uint8_t newgn3(uint8_t a, uint8_t y)
{
    TEMP2 = y;                                              /* L9AF6 */
    TEMP4 = a;                                              /* L9AF8 */
    return TEMP0;                                           /* L9AFA-L9AFC */
}

/* NEWGN2 ($9AF1): TEMP3 = A, CAM from TNEWCAM. */
static uint8_t newgn2(uint8_t a, uint8_t y)
{
    TEMP3 = a;                                              /* L9AF1 */
    return newgn3(ROM(ALWELG_TNEWCAM + y), y);              /* L9AF3 */
}

/* NEWFLI ($9A9D): flipper. */
uint8_t newfli(uint8_t *y)
{
    uint8_t a;
    TEMP3 = ROM(ALWELG_TNEWI2 + K_ZABFLI);                  /* L9A9D-L9AA0: INVAC2 */
    a = WFLICAM;                                            /* L9AA2 */
    *y = K_ZABFLI;                                          /* L9AA5 */
    return newgn3(a, *y);                                   /* L9AA7 BEQ NEWGN3 */
}

/* NEWPUL ($9AA9): pulsar. */
uint8_t newpul(uint8_t *y)
{
    uint8_t a = (uint8_t)(ROM(ALWELG_TNEWI2 + K_ZABPUL) | WPULFI);   /* L9AA9-L9AAC: pulsar fire? */
    *y = K_ZABPUL;                                          /* L9AAF */
    return newgn2(a, *y);                                   /* L9AB1 BNE NEWGN2 */
}

/* NEWFUS ($9AB3): fuse. */
uint8_t newfus(uint8_t *y)
{
    *y = K_ZABFUS;                                          /* L9AB3 */
    return newgen(*y);                                      /* L9AB5 BNE NEWGEN */
}

/* NEWSPI ($9AB7): spinner (trailer). */
uint8_t newspi(uint8_t *y)
{
    *y = K_ZABTRA;                                          /* L9AB7 */
    return newgen(*y);                                      /* L9AB9 BNE NEWGEN */
}

/* NEWTAN ($9ABB): tanker - a random carrier type with openings.  Returns
 * A (0 = none), *y; X preserved (INDEX3). */
uint8_t newtan(uint8_t x, uint8_t *y)
{
    uint8_t a;
    *y = (uint8_t)(hw_random(0) & 0x03);                    /* L9ABB-L9AC0 */
    TEMP2 = 0x04;                                           /* L9AC1-L9AC3 */
    INDEX3 = x;                                             /* L9AC5: save X */
    for (;;) {
        TEMP2 = (uint8_t)(TEMP2 - 1);                       /* L9AC7 */
        if (TEMP2 & 0x80) return 0x00;                      /* L9AC9-L9ACF: failure for all (X restored) */
        (*y)--;                                             /* L9AD0 */
        if (*y & 0x80) *y = 0x03;                           /* L9AD1-L9AD3: cycle 0..3 */
        x = R(A_WTACAR + *y);                               /* L9AD5: tanker type */
        if (x == K_ZCARFU) x = K_ZABFUS + 1;                /* L9AD8-L9ADC */
        if (R(A_OPFLIP - 1 + x) != 0) break;                /* L9ADE-L9AE1: openings for the type */
    }
    a = (uint8_t)(R(A_WTACAR + *y) | K_ZFIRYE);             /* L9AE3-L9AE8: tanker contents */
    *y = K_ZABTAN;                                          /* L9AEA */
    return newgn2(a, *y);                                   /* L9AEC BNE NEWGN2 */
}

/* NEWGEN ($9AEE): type Y with TNEWI2 / TNEWCAM.  Returns TEMP0; Y
 * preserved. */
uint8_t newgen(uint8_t y)
{
    return newgn2(ROM(ALWELG_TNEWI2 + y), y);               /* L9AEE */
}

/* SPLCHA ($9B07): characteristics of a split invader (TEMP2 = type, TEMP0 =
 * split depth); no flipping too close to the player.  X, Y preserved. */
void splcha(uint8_t x, uint8_t y)
{
    uint8_t yy;
    SAVEY = y;                                              /* L9B07 */
    if (TEMP0 < 0x20) {                                     /* L9B09-L9B0F: too close to player? */
        (void)newgen(TEMP2);                                /* L9B11-L9B12 */
    } else {
        (void)newty2(TEMP2, x, &yy);                        /* L9B18 */
    }
    /* L9B1B LDY SAVEY */
}

/* ======================================================================= */
/* INVADERS: CAM INTERPRETER AND CAM ROUTINES                              */
/* ======================================================================= */

/* MOVINV ($9B1E): run each active invader's CAM script until it exits;
 * then the pulsar pulse status. */
xy6502 movinv(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c;
    xy6502 r;
    if (!(CURSL2 & 0x80)) {                                 /* L9B1E-L9B21: player dead or dropping? */
        x = WINVMX;                                         /* L9B23 */
        INDEX1 = x;                                         /* L9B26 */
        do {
            x = INDEX1;                                     /* L9B28 */
            if (R(A_INVAY + x) != 0) {                      /* L9B2A-L9B2D: active? */
                EXICAM = 0x01;                              /* L9B2F-L9B31: no exit */
                CAMPC = R(A_INVCAM + x);                    /* L9B34-L9B37 */
                do {
                    y = CAMPC;                              /* L9B3A-L9B3D */
                    a = CAMB(y);                            /* L9B3E: CAM code */
                    r = jsrcam(a, x);                       /* L9B41 */
                    x = r.x; y = r.y;
                    CAMPC = (uint8_t)(CAMPC + 1);           /* L9B44 */
                } while (EXICAM != 0);                      /* L9B47-L9B4A */
                R(A_INVCAM + x) = CAMPC;                    /* L9B4C-L9B4F */
            }
            INDEX1 = (uint8_t)(INDEX1 - 1);                 /* L9B52 */
        } while (!(INDEX1 & 0x80));                         /* L9B54 */
    }
    /* UPDATE PULSE STATUS */
    c = 0;                                                  /* L9B59 CLC */
    a = adc(PULSON, PULTIM, &c);                            /* L9B56-L9B5A */
    y = a;                                                  /* L9B5D */
    a ^= PULSON;                                            /* L9B5E */
    PULSON = y;                                             /* L9B61 */
    if (a & 0x80) {                                         /* L9B64: status change? */
        if (y & 0x80) {                                     /* L9B66-L9B67: go off? */
            pulsto(x, y);                                   /* L9B69 */
        } else if (R(A_FLIPCO + K_ZABPUL) != 0 && !(CURSL2 & 0x80)) {   /* L9B6F-L9B77 */
            pulstr(x, y);                                   /* L9B79 */
        }
    }
    a = PULSON;                                             /* L9B7C */
    if (!(a & 0x80)) {                                      /* L9B7F: bounce between -27 and +15 */
        if (a < 0x0F) return xy(x, y);                      /* L9B81-L9B86 */
    } else {
        if (a >= 0xC1) return xy(x, y);                     /* L9B88-L9B8A */
    }
    /* NEGPUL */
    c = 0;                                                  /* L9B91 CLC */
    PULTIM = adc((uint8_t)(PULTIM ^ 0xFF), 0x01, &c);       /* L9B8C-L9B94: negate increment */
    return xy(x, y);                                        /* L9B97 */
}

/* JSRCAM ($9B98): execute CAM code A for invader X (TABJSR RTS dispatch).
 * Returns the handler's exit X/Y. */
xy6502 jsrcam(uint8_t a, uint8_t x)
{
    uint8_t y = a;                                          /* L9B98 */
    uint16_t target = (uint16_t)((ROM(ALWELG_TABJSR + y) | (ROM(ALWELG_TABJSR + 1 + y) << 8)) + 1);   /* L9B99-L9BA1 */
    switch (target) {
    case ALWELG_JEXIT:   return jexit(x, y);
    case ALWELG_JSLOOP:  return jsloop(x, y);
    case ALWELG_JSKIP0:  return jskip0(x, y);
    case ALWELG_JSETPC:  return jsetpc(x, y);
    case ALWELG_JELOOP:  return jeloop(x, y);
    case ALWELG_JNOOP:   return jnoop(x, y);
    case ALWELG_JSMOVE:  return jsmove(x, y);
    case ALWELG_JSTRAI:  return jstrai(x, y);
    case ALWELG_JSLOPB:  return jslopb(x, y);
    case ALWELG_JJUMPS:  return jjumps(x, y);
    case ALWELG_JJUMPM:  return jjumpm(x, y);
    case ALWELG_JCHROT:  return jchrot(x, y);
    case ALWELG_JKITST:  return jkitst(x, y);
    case ALWELG_JBR0PC:  return jbr0pc(x, y);
    case ALWELG_JELTST:  return jeltst(x, y);
    case ALWELG_JFUSEUP: return jfuseup(x, y);
    case ALWELG_JFUSKI:  return jfuski(x, y);
    case ALWELG_JPULMO:  return jpulmo(x, y);
    case ALWELG_JCHPLA:  return jchpla(x, y);
    case ALWELG_JCHKPU:  return jchkpu(x, y);
    default:             hw_soft_watchdog(); return xy(x, y);   /* odd / large code: the ROM runs wild */
    }
}

/* TABJSE = JEXIT ($9BCA): exit the CAM for this frame. */
xy6502 jexit(uint8_t x, uint8_t y)
{
    EXICAM = 0x00;                                          /* L9BCA-L9BCC */
    return jnoop(x, y);                                     /* JNOOP */
}

/* JNOOP ($9BCF): no operation. */
xy6502 jnoop(uint8_t x, uint8_t y)
{
    return xy(x, y);                                        /* L9BCF */
}

/* JSLOOP ($9BD0): set the CAM loop counter from the operand. */
xy6502 jsloop(uint8_t x, uint8_t y)
{
    CAMPC = (uint8_t)(CAMPC + 1);                           /* L9BD0 */
    y = CAMPC;                                              /* L9BD3 */
    R(A_INVLOO + x) = CAMB(y);                              /* L9BD6-L9BD9: new loop value */
    return xy(x, y);                                        /* L9BDC */
}

/* JSLOPB ($9BDD): as JSLOOP, the operand is the zero-page cell of the value. */
xy6502 jslopb(uint8_t x, uint8_t y)
{
    CAMPC = (uint8_t)(CAMPC + 1);                           /* L9BDD */
    y = CAMPC;                                              /* L9BE0 */
    y = CAMB(y);                                            /* L9BE3-L9BE6: cell */
    R(A_INVLOO + x) = R(y);                                 /* L9BE7-L9BEA: LDA $0000,Y */
    return xy(x, y);                                        /* L9BED */
}

/* JSKIP0 ($9BEE): skip the next (two-byte) CAM line if CAMSTA = 0. */
xy6502 jskip0(uint8_t x, uint8_t y)
{
    if (CAMSTA == 0) {                                      /* L9BEE-L9BF1 */
        CAMPC = (uint8_t)(CAMPC + 1);                       /* L9BF3 */
        CAMPC = (uint8_t)(CAMPC + 1);                       /* L9BF6 */
    }
    return xy(x, y);                                        /* L9BF9 */
}

/* JBR0PC ($9BFA): branch if CAMSTA = 0. */
xy6502 jbr0pc(uint8_t x, uint8_t y)
{
    CAMPC = (uint8_t)(CAMPC + 1);                           /* L9BFA */
    if (CAMSTA == 0) {                                      /* L9BFD-L9C00 */
        y = CAMPC;                                          /* L9C02 */
        CAMPC = CAMB(y);                                    /* L9C05-L9C08: new PC */
    }
    return xy(x, y);                                        /* L9C0B */
}

/* JELOOP ($9C0C): decrement the loop counter; reloop unless 0. */
xy6502 jeloop(uint8_t x, uint8_t y)
{
    R(A_INVLOO + x) = (uint8_t)(R(A_INVLOO + x) - 1);       /* L9C0C */
    if (R(A_INVLOO + x) != 0) return jsetpc(x, y);          /* L9C0F BNE JSETPC */
    CAMPC = (uint8_t)(CAMPC + 1);                           /* L9C11: exit loop */
    return xy(x, y);                                        /* L9C14-L9C15 -> L9C20 RTS */
}

/* JSETPC ($9C17): CAMPC = operand. */
xy6502 jsetpc(uint8_t x, uint8_t y)
{
    y = CAMPC;                                              /* L9C17 */
    CAMPC = CAMB(y + 1);                                    /* L9C1A-L9C1D */
    return xy(x, y);                                        /* L9C20 */
}

/* JELTST ($9C21): CAMSTA = 0 if the invader is on its line's enemy line,
 * else 1. */
xy6502 jeltst(uint8_t x, uint8_t y)
{
    uint8_t a;
    y = R(A_INVAL1 + x);                                    /* L9C21 */
    a = R(A_LINEY + y);                                     /* L9C24 */
    if (a == 0) a = 0xFF;                                   /* L9C27-L9C29: worst case (dead) */
    CAMSTA = (a >= R(A_INVAY + x)) ? 0x01 : 0x00;           /* L9C2B-L9C37 */
    return xy(x, y);                                        /* L9C3A */
}

/* JCHKPU ($9C3B): CAMSTA = $80 if the pulsars pulse now or within 4
 * frames, else 0. */
xy6502 jchkpu(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c;
    a = (uint8_t)(PULTIM << 2);                             /* L9C3B-L9C3F */
    c = 0;                                                  /* L9C40 CLC */
    a = adc(a, PULSON, &c);                                 /* L9C41 */
    CAMSTA = (uint8_t)((a & PULSON & 0x80) ^ 0x80);         /* L9C44-L9C4B */
    return xy(x, y);                                        /* L9C4E */
}

/* JCHROT ($9C4F): change the jump direction. */
xy6502 jchrot(uint8_t x, uint8_t y)
{
    R(A_INVAC1 + x) = (uint8_t)(R(A_INVAC1 + x) ^ K_INVROT);   /* L9C4F-L9C54 */
    return xy(x, y);                                        /* L9C57 */
}

/* JSMOVE ($9C58): move invader X one step along its line (up, or down). */
xy6502 jsmove(uint8_t x, uint8_t y)
{
    y = (uint8_t)(R(A_INVAC1 + x) & K_INVABI);              /* L9C58-L9C5D: invader type */
    if (R(A_INVAC2 + x) & 0x80) {                           /* L9C5E-L9C61: going up? */
        (void)jsmovd(x, y);                                 /* JSMOVD */
        return xy(x, y);
    }
    return jsmovu(x, y);                                    /* JSMOVU */
}

/* JSMOVU ($9C63): move up by the type-Y speed; at the top convert to a
 * chaser, near the top split a carrier. */
xy6502 jsmovu(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c;
    c = 0;                                                  /* L9C66 CLC */
    R(A_INVAYL + x) = adc(R(A_INVAYL + x), R(A_WINVIL + y), &c);   /* L9C63-L9C6A: move up */
    a = adc(R(A_INVAY + x), R(A_WINVIN + y), &c);           /* L9C6D-L9C70 */
    R(A_INVAY + x) = a;                                     /* L9C73 */
    if (a <= CURSY)                                         /* L9C76-L9C7B: at top? */
        return chaser(x, y);                                /* ATOP L9C7D */
    if (a < 0x20 && (R(A_INVAC2 + x) & K_INVCAR)) {         /* L9C83-L9C8C: carrier too close to top? */
        y = x;                                              /* L9C8E-L9C90 TXA / PHA / TAY */
        kilinv(x, y);                                       /* L9C91: split carrier */
    }
    return xy(x, y);                                        /* L9C94-L9C97 -> L9CB5 RTS */
}

/* JSMOVD ($9C99): move down by the type-Y speed, stop at the bottom.
 * Returns A = the Y position; X, Y preserved. */
uint8_t jsmovd(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c;
    c = 1;                                                  /* L9C9C SEC */
    R(A_INVAYL + x) = sbc(R(A_INVAYL + x), R(A_WINVIL + y), &c);   /* L9C99-L9CA0 */
    a = sbc(R(A_INVAY + x), R(A_WINVIN + y), &c);           /* L9CA3-L9CA6 */
    R(A_INVAY + x) = a;                                     /* L9CA9 */
    if (a >= K_ILINDDY) {                                   /* L9CAC-L9CAE: at bottom? */
        a = 0xF2;                                           /* L9CB0 */
        R(A_INVAY + x) = a;                                 /* L9CB2 */
    }
    return a;                                               /* L9CB5 */
}

/* JPULMO ($9CB6): pulsar move (faster outside the power zone, reverses at
 * PULPOT), and the pulse kills a cursor on its lines. */
xy6502 jpulmo(uint8_t x, uint8_t y)
{
    uint8_t a;
    xy6502 r;
    y = K_ZABPUL;                                           /* L9CB6 */
    if (!(R(A_INVAC2 + x) & 0x80)) {                        /* L9CB8-L9CBB: going up? */
        if (R(A_INVAY + x) >= PULPOT) y = K_ZABFLI;         /* L9CBD-L9CC5: not in power zone: faster */
        r = jsmovu(x, y);                                   /* L9CC7 */
        x = r.x; y = r.y;
    } else {
        a = jsmovd(x, y);                                   /* L9CCD */
        y = NYMCOU;                                         /* L9CD0 */
        if (y == 0) a = 0xFF;                               /* L9CD3-L9CD5: nymphs gone - send it up */
        if (a >= PULPOT)                                    /* L9CD7-L9CDA: time to reverse? */
            R(A_INVAC2 + x) = (uint8_t)(R(A_INVAC2 + x) ^ K_INVDIR);   /* L9CDC-L9CE1 */
    }
    if (!(PULSON & 0x80) &&                                 /* L9CE4-L9CE7: pulsar on? */
        R(A_INVAY + x) < PULPOT &&                          /* L9CE9-L9CEF: in range? */
        CURSL1 == R(A_INVAL1 + x) &&                        /* L9CF1-L9CF7 */
        CURSL2 == R(A_INVAL2 + x))                          /* L9CF9-L9CFF: on cursor lines? */
        inppsq(x, y);                                       /* L9D01: kill cursor */
    return xy(x, y);                                        /* L9D04 */
}

/* CHASER ($9D06): invader X reached the top - convert it to a chaser (a
 * pulsar with nymphs left goes back down). */
xy6502 chaser(uint8_t x, uint8_t y)
{
    xy6502 r;
    R(A_INVAY + x) = CURSY;                                 /* L9D06-L9D09: exactly at top */
    if ((R(A_INVAC1 + x) & K_INVABI) == K_ZABPUL && NYMCOU != 0) {   /* L9D0C-L9D18 */
        R(A_INVAC2 + x) = (uint8_t)(R(A_INVAC2 + x) ^ K_INVDIR);     /* L9D1A-L9D1F: send it down */
        return xy(x, y);                                    /* L9D22 */
    }
    if (R(A_INVAC1 + x) & 0x80) {                           /* L9D23-L9D26: still flipping? */
        R(A_INVAY + x) = (uint8_t)(R(A_INVAY + x) + 1);     /* L9D28: finish flip first */
        return xy(x, y);                                    /* L9D2B */
    }
    INMCOU = (uint8_t)(INMCOU - 1);                         /* L9D2C */
    if (INCCOU != 0x01) {                                   /* L9D2F-L9D34: other than 1 chaser? */
        r = jchpla(x, y);                                   /* L9D36: shortest way */
        y = r.y;
    } else {
        y = K_NINVAD - 1;                                   /* L9D3C: the other chaser's opposite */
        do {
            if (R(A_INVAY + y) != 0) {                      /* L9D3E-L9D41 */
                INDEX2 = y;                                 /* L9D43 */
                if (x != INDEX2 && R(A_INVAY + y) == CURSY) break;   /* L9D45-L9D4F: GOTCHA */
            }
            y--;                                            /* L9D51 */
        } while (!(y & 0x80));                              /* L9D52 */
        /* GOTCHA */
        R(A_INVAC1 + x) = (uint8_t)((R(A_INVAC1 + y) & K_INVROT) ^ K_INVROT);   /* L9D54-L9D5B */
    }
    CAMPC = (uint8_t)(ALWELG_TOPPER - ALWELG_CAM - 1);      /* L9D5E-L9D60: chaser CAM */
    INCCOU = (uint8_t)(INCCOU + 1);                         /* L9D63 */
    return xy(x, y);                                        /* L9D66 */
}

/* JCHPLA ($9D67): jump direction the shortest way to the player.  Exits
 * Y = INVAL1(X). */
xy6502 jchpla(uint8_t x, uint8_t y)
{
    uint8_t a;
    y = R(A_INVAL1 + x);                                    /* L9D67-L9D6A */
    a = poldel(CURSL1, y);                                  /* L9D6B-L9D6E */
    if (!(a & 0x80))                                        /* L9D71 ASL, L9D75 BCS */
        a = (uint8_t)(R(A_INVAC1 + x) | K_INVROT);          /* L9D72, L9D77: CCW */
    else
        a = (uint8_t)(R(A_INVAC1 + x) & (uint8_t)~K_INVROT);   /* L9D7C: CW */
    R(A_INVAC1 + x) = a;                                    /* L9D7E */
    return xy(x, y);                                        /* L9D81 */
}

/* JJUMPM ($9D82): continue the jump one angle step; CAMSTA = 0 when done
 * (fuses: at a junction). */
xy6502 jjumpm(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c;
    xy6502 r;
    y = R(A_INVAL2 + x);                                    /* L9D82 */
    if ((R(A_INVAC1 + x) & K_INVROT) == 0) y++;             /* L9D85-L9D8C: CW (jump rotation CCW) */
    else y--;                                               /* L9D90: CCW */
    a = (uint8_t)((y & 0x0F) | 0x80);                       /* L9D91-L9D94: new jump angle */
    R(A_INVAL2 + x) = a;                                    /* L9D96 */
    if ((R(A_INVAC1 + x) & K_INVABI) == K_ZABFUS) {         /* L9D99-L9DA0: fuse */
        if ((R(A_INVAL2 + x) & 0x07) == 0) {                /* L9DA2-L9DA7: at a junction? */
            if (R(A_INVAL2 + x) & 0x08) {                   /* L9DA9-L9DAE: moving CCW? */
                c = 0;                                      /* L9DB3 CLC */
                R(A_INVAL1 + x) = (uint8_t)(adc(R(A_INVAL1 + x), 0x01, &c) & 0x0F);   /* L9DB0-L9DB8 */
            }
            R(A_INVAC1 + x) = (uint8_t)(R(A_INVAC1 + x) & (uint8_t)~K_INVMOT);   /* L9DBB-L9DC0: back to line */
            R(A_INVAL2 + x) = 0x20;                         /* L9DC3-L9DC5: invincible */
            R(A_INVAC2 + x) = (uint8_t)(R(A_INVAC2 + x) ^ K_INVDIR);   /* L9DC8-L9DCD: reverse */
            if (NYMCOU == 0) {                              /* L9DD0-L9DD3: nymphs gone? */
                if (R(A_INVAY + x) == CURSY) {              /* L9DD5-L9DDB: at top? */
                    r = fuchpl(x, y);                       /* L9DDD: chase player */
                    x = r.x; y = r.y;
                } else {
                    R(A_INVAC2 + x) = (uint8_t)(R(A_INVAC2 + x) & K_INVDIR);   /* L9DE3-L9DE8: send up */
                }
            }
        }
    } else {
        y = R(A_INVAL1 + x);                                /* L9DEE */
        a = calsan((uint8_t)(R(A_INVAC1 + x) ^ K_INVROT), &y);   /* L9DF1-L9DF6: final jump angle */
        if (a == R(A_INVAL2 + x)) {                         /* L9DF9-L9DFC */
            a = (uint8_t)(R(A_INVAC1 + x) & (uint8_t)~K_INVMOT);   /* L9DFE-L9E01 */
            R(A_INVAC1 + x) = a;                            /* L9E03: back to mover */
            if ((a & K_INVROT) == 0) {                      /* L9E06-L9E08 */
                a = R(A_INVAL1 + x);                        /* L9E0A: CW */
                R(A_INVAL2 + x) = a;                        /* L9E0D */
                c = 1;                                      /* L9E10 SEC */
                R(A_INVAL1 + x) = (uint8_t)(sbc(a, 0x01, &c) & 0x0F);   /* L9E11-L9E15 */
            } else {
                c = 0;                                      /* L9E1E CLC */
                R(A_INVAL2 + x) = (uint8_t)(adc(R(A_INVAL1 + x), 0x01, &c) & 0x0F);   /* L9E1B-L9E23: CCW */
            }
        }
    }
    CAMSTA = (uint8_t)(R(A_INVAC1 + x) & K_INVMOT);         /* L9E26-L9E2B: 0 = jump done */
    return xy(x, y);                                        /* L9E2E */
}

/* JKITST ($9E2F): a chaser on the cursor's legs kills it. */
xy6502 jkitst(uint8_t x, uint8_t y)
{
    if (!(R(A_INVAC1 + x) & 0x80) &&                        /* L9E2F-L9E32: moving */
        R(A_INVAL1 + x) == CURSL1 &&                        /* L9E34-L9E3A */
        R(A_INVAL2 + x) == CURSL2)                          /* L9E3C-L9E42 */
        inipsq(x, y);                                       /* L9E44: destroy cursor */
    return xy(x, y);                                        /* L9E47 */
}

/* JFUSKI ($9E48): a fuse at the cursor's height and line kills it. */
xy6502 jfuski(uint8_t x, uint8_t y)
{
    if (R(A_INVAY + x) == CURSY && R(A_INVAL1 + x) == CURSL1)   /* L9E48-L9E56 */
        infpsq(x, y);                                       /* L9E58 */
    return xy(x, y);                                        /* L9E5B */
}

/* JUMPSD ($9E5F): start a jump in the direction of INVAC1(X). */
static xy6502 jumpsd(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c;
    a = (uint8_t)(R(A_INVAC1 + x) | K_ZMOTJM);              /* L9E5F-L9E62 */
    R(A_INVAC1 + x) = a;                                    /* L9E64: jumps status */
    if ((a & K_INVABI) == K_ZABFUS) {                       /* L9E67-L9E6B: fuse? */
        if ((R(A_INVAC1 + x) & K_INVROT) == 0) {            /* L9E6D-L9E72 */
            a = 0x81;                                       /* L9E74: CCW */
        } else {
            c = 1;                                          /* L9E7C SEC */
            R(A_INVAL1 + x) = (uint8_t)(sbc(R(A_INVAL1 + x), 0x01, &c) & 0x0F);   /* L9E79-L9E81: CW */
            a = 0x87;                                       /* L9E84 */
        }
        R(A_INVAL2 + x) = a;                                /* L9E86 */
    } else {
        if (R(A_INVAC1 + x) & K_INVROT) {                   /* L9E8C-L9E91: moving CCW? */
            c = 0;                                          /* L9E96 CLC */
            R(A_INVAL1 + x) = (uint8_t)(adc(R(A_INVAL1 + x), 0x01, &c) & 0x0F);   /* L9E93-L9E9B: adjust base leg */
        }
        a = R(A_INVAC1 + x);                                /* L9E9E */
        y = R(A_INVAL1 + x);                                /* L9EA1 */
        R(A_INVAL2 + x) = calsan(a, &y);                    /* L9EA4-L9EA7: starting angle */
    }
    return xy(x, y);                                        /* L9EAA */
}

/* JJUMPS ($9E5C): start a jump (verify the direction first; JUMPSD). */
xy6502 jjumps(uint8_t x, uint8_t y)
{
    oktojm(x);                                              /* L9E5C */
    return jumpsd(x, y);                                    /* JUMPSD */
}

/* OKTOJM ($9EAB): on a planar well turn a jump off the edge around. */
void oktojm(uint8_t x)
{
    if (WELTYP == 0) return;                                /* L9EAB-L9EAE */
    if (R(A_INVAC1 + x) & K_INVROT) {                       /* L9EB0-L9EB5: CCW */
        if (R(A_INVAL1 + x) >= 0x0E)                        /* L9EB7-L9EBC: at right edge? */
            R(A_INVAC1 + x) = (uint8_t)(R(A_INVAC1 + x) & (uint8_t)~K_INVROT);   /* L9EBE-L9EC3 */
    } else {
        if (R(A_INVAL1 + x) == 0)                           /* L9EC9-L9ECC: at left edge? */
            R(A_INVAC1 + x) = (uint8_t)(R(A_INVAC1 + x) | K_INVROT);   /* L9ECE-L9ED3 */
    }
}

/* CALSAN ($9ED7): starting jump angle for base leg *y and direction A.
 * Returns A; *y in/out. */
uint8_t calsan(uint8_t a, uint8_t *y)
{
    unsigned c;
    if (a & K_INVROT) {                                     /* L9ED7-L9ED9: moving CCW? */
        (*y)--;                                             /* L9EDB */
        *y &= 0x0F;                                         /* L9EDC-L9EDF */
        c = 0;                                              /* L9EE3 CLC */
        a = (uint8_t)(adc(R(A_LINANG + *y), 0x08, &c) & 0x0F);   /* L9EE0-L9EE6: base leg on right side */
    } else {
        a = R(A_LINANG + *y);                               /* L9EEB: CW */
    }
    return (uint8_t)(a | 0x80);                             /* L9EEE-L9EF0: jump code */
}

/* JFUSEUP ($9EF1): fuse up / down motion along its line, with the jump
 * decisions. */
xy6502 jfuseup(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c;
    int too_high;
    y = K_ZABFUS;                                           /* L9EF1 */
    if (!(R(A_INVAC2 + x) & 0x80)) {                        /* L9EF3-L9EF6: up */
        c = 0;                                              /* L9EFB CLC */
        R(A_INVAYL + x) = adc(R(A_INVAYL + x), WFUSIL, &c); /* L9EF8-L9EFF */
        a = adc(R(A_INVAY + x), WFUSIH, &c);                /* L9F02-L9F05 */
        R(A_INVAY + x) = a;                                 /* L9F08 */
        if (a < CURSY) {                                    /* L9F0B-L9F0E: at top? */
            R(A_INVAY + x) = CURSY;                         /* L9F10-L9F13 */
            too_high = 1;                                   /* (C clear from the CMP) */
        } else {
            y = NYMCOU;                                     /* L9F19 */
            if (y == 0) return xy(x, y);                    /* L9F1C, L9F29: none left - head for top */
            y = CURWAV;                                     /* L9F1E */
            if (y >= 0x11) too_high = 0;                    /* L9F20-L9F22 */
            else too_high = (a < 0x20);                     /* L9F24: early wave - turn back before top */
        }
        if (too_high) {                                     /* L9F2A */
            if (WFUSCH & 0x80) return fuchpl(x, y);         /* L9F2C-L9F31: chase player at top */
            return lefrit(x, y);                            /* L9F37: random */
        }
        return mayblr(x, y);                                /* L9F3D */
    }
    a = jsmovd(x, y);                                       /* L9F43: move down */
    if (a >= 0x80) {                                        /* L9F46-L9F48: bottom of range? */
        if (WFUSCH & 0x40) return fuchpl(x, y);             /* L9F4A-L9F4F: chase player on tube */
        return lefrit(x, y);                                /* L9F55 */
    }
    return mayblr(x, y);                                    /* L9F5B */
}

/* MAYBLR ($9F5F): fuse jump decision (maybe left or right). */
xy6502 mayblr(uint8_t x, uint8_t y)
{
    if (R(A_INVAY + x) & 0x20) {                            /* L9F5F-L9F64 */
        uint8_t a = hw_random(1);                           /* L9F66 */
        if (a >= WFUFRQ) {                                  /* L9F69-L9F6C */
            if (WFUSCH & 0x40) {                            /* L9F6E-L9F71: chase players on tube? */
                if (!(x & 0x01)) return lefrit(x, y);       /* L9F73-L9F75 TXA / LSR / BCC LEFRIT */
                return fuchpl(x, y);                        /* L9F77 */
            }
            return lefrit(x, y);                            /* L9F7D */
        }
    }
    return xy(x, y);                                        /* L9F80 */
}

/* FUCHPL ($9F81): fuse chases the player (backwards), start the jump. */
xy6502 fuchpl(uint8_t x, uint8_t y)
{
    xy6502 r = jchpla(x, y);                                /* L9F81 */
    r = jchrot(r.x, r.y);                                   /* L9F84: fuse is backwards */
    return gotjum(r.x, r.y);                                /* L9F87 JMP GOTJUM */
}

/* LEFRIT ($9F8A): randomly left or right (falls into GOTJUM). */
xy6502 lefrit(uint8_t x, uint8_t y)
{
    uint8_t a = (uint8_t)(R(A_INVAC1 + x) & (uint8_t)~K_INVROT);   /* L9F8A-L9F8D */
    if (hw_random(0) & 0x40) a |= K_INVROT;                 /* L9F8F-L9F94 BIT RANDOM / BVC */
    R(A_INVAC1 + x) = a;                                    /* L9F96 */
    return gotjum(x, y);                                    /* GOTJUM */
}

/* GOTJUM ($9F99): planar edges turn the fuse back; left/right fuse CAM,
 * start the jump. */
xy6502 gotjum(uint8_t x, uint8_t y)
{
    if (WELTYP != 0) {                                      /* L9F99-L9F9C: planar surface? */
        int rev;
        if ((R(A_INVAC1 + x) & K_INVROT) == 0)              /* L9F9E-L9FA3: going CCW? */
            rev = (R(A_INVAL1 + x) >= 0x0F);                /* L9FA5-L9FAA: at right edge? */
        else
            rev = (R(A_INVAL1 + x) == 0);                   /* L9FAF-L9FB2: at left edge? */
        if (rev)                                            /* REVFLP */
            R(A_INVAC1 + x) = (uint8_t)(R(A_INVAC1 + x) ^ K_INVROT);   /* L9FB4-L9FB9: go back */
    }
    CAMPC = (uint8_t)(ALWELG_FUSELR - ALWELG_CAM);          /* L9FBC-L9FBE: left right fuse CAM */
    return jumpsd(x, y);                                    /* L9FC1 JMP JUMPSD */
}

/* JSTRAI ($9FC4): trailer (spiker) - lay / extend the spike; turn at the
 * top and the bottom; converts to a carrier when the nymphs are gone. */
xy6502 jstrai(uint8_t x, uint8_t y)
{
    uint8_t a;
    CAMSTA = 0x01;                                          /* L9FC4-L9FC6 */
    y = R(A_INVAL1 + x);                                    /* L9FC9 */
    if (R(A_LINEY + y) == 0)                                /* L9FCC-L9FCF: line vacant? */
        R(A_LINEY + y) = (uint8_t)(K_ILINDDY + 1);          /* L9FD1-L9FD3: start low */
    a = R(A_INVAY + x);                                     /* L9FD6 */
    if (a < R(A_LINEY + y)) {                               /* L9FD9-L9FDC: new enemy line? */
        R(A_LINEY + y) = a;                                 /* L9FDE */
        R(A_LINSTA + y) = 0x80;                             /* L9FE1-L9FE3: request recalc */
    }
    a = R(A_INVAY + x);                                     /* L9FE6 */
    if (a < 0x20) {                                         /* L9FE9-L9FEB: max height? */
        R(A_INVAC2 + x) = (uint8_t)(R(A_INVAC2 + x) | K_ZDIRDO);   /* L9FED-L9FF2: send it down */
        R(A_INVAY + x) = 0x20;                              /* L9FF5-L9FF7 */
    } else if (a >= 0xF2) {                                 /* L9FFD-L9FFF: min height? */
        y = astral(x);                                      /* LA001: reassign, reverse */
        R(A_INVAY + x) = 0xF0;                              /* LA004-LA006 */
        if (NYMCOU == 0) {                                  /* LA009-LA00C */
            R(A_INVAC2 + x) = (uint8_t)((R(A_INVAC2 + x) & (uint8_t)~K_INVCAR) | K_ZCARFL);   /* LA00E-LA015: tanker of flippers */
            R(A_INVAC1 + x) = (uint8_t)((R(A_INVAC1 + x) & (uint8_t)~K_INVABI) | K_ZABTAN);   /* LA018-LA01F */
            CAMSTA = 0x00;                                  /* LA022-LA024: converted */
        }
    }
    return xy(x, y);                                        /* LA027 */
}

/* ASTRAL ($A028): move trailer X to the neediest line (from a random
 * start), send it up.  Returns Y. */
uint8_t astral(uint8_t x)
{
    uint8_t a, y;
    unsigned c;
    TEMP4 = 0x00;                                           /* LA028-LA02A */
    OPSPIN = K_NLINES - 1;                                  /* LA02C-LA02E: loop line counter */
    y = (uint8_t)(hw_random(1) & 0x0F);                     /* LA031-LA036: random start */
    do {
        if (!(y == 0x0F && WELTYP != 0)) {                  /* LA037-LA03E: skip planar far right */
            a = R(A_LINEY + y);                             /* LA040 */
            if (a == 0) a = 0xFF;                           /* LA043-LA045: dead - worst case */
            if (a >= TEMP4) {                               /* LA047-LA049: neediest so far? */
                TEMP4 = a;                                  /* LA04B */
                TEMP0 = y;                                  /* LA04D */
            }
        }
        /* SKIPIT */
        y--;                                                /* LA04F */
        if (y & 0x80) y = K_NLINES - 1;                     /* LA050-LA052 */
        OPSPIN = (uint8_t)(OPSPIN - 1);                     /* LA054 */
    } while (!(OPSPIN & 0x80));                             /* LA057 */
    a = TEMP0;                                              /* LA059: reassign */
    R(A_INVAL1 + x) = a;                                    /* LA05B */
    c = 0;                                                  /* LA05E CLC */
    R(A_INVAL2 + x) = (uint8_t)(adc(a, 0x01, &c) & 0x0F);   /* LA05F-LA063 */
    R(A_INVAC2 + x) = (uint8_t)(R(A_INVAC2 + x) & (uint8_t)~K_INVDIR);   /* LA066-LA06B: back up */
    return y;                                               /* LA06E */
}

/* KILINV ($A06F): kill invader Y; a carrier splits into up to two new
 * invaders.  X and Y preserved. */
void kilinv(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c;
    a = R(A_INVAY + y);                                     /* LA06F */
    TEMP0 = a;                                              /* LA072 */
    if (a == CURSY && (R(A_INVAC1 + y) & K_INVABI) != K_ZABFUS)   /* LA074-LA080: chaser */
        INCCOU = (uint8_t)(INCCOU - 1);                     /* LA082 */
    else
        INMCOU = (uint8_t)(INMCOU - 1);                     /* MOVER LA088 */
    R(A_INVAY + y) = 0x00;                                  /* LA08B-LA08D: deactivate */
    a = (uint8_t)(R(A_INVAC1 + y) & K_INVABI);              /* LA090-LA093 */
    SAVEX = x;                                              /* LA095 */
    R(A_FLIPCO + a) = (uint8_t)(R(A_FLIPCO + a) - 1);       /* LA097-LA098: type counter */
    a = (uint8_t)(R(A_INVAC2 + y) & K_INVCAR);              /* LA09B-LA0A0 */
    if (a == 0) return;                                     /* LA0A2: split type? */
    c = 1;                                                  /* LA0A4 SEC */
    a = sbc(a, 0x01, &c);                                   /* LA0A5 */
    if (a == K_ZABTAN) a = K_ZABFUS;                        /* LA0A7-LA0AB: tanker is really fuse */
    TEMP2 = a;                                              /* LA0AD: resultant mutation */
    c = 1;                                                  /* LA0B2 SEC */
    a = (uint8_t)(sbc(R(A_INVAL1 + y), 0x01, &c) & 0x0F);   /* LA0AF-LA0B5 */
    if (a >= 0x0F && (WELTYP & 0x80)) a = 0x00;             /* LA0B7-LA0C0: no wrap on plane */
    TEMP1 = a;                                              /* LA0C2: CW line */
    splcha(x, y);                                           /* LA0C4 */
    CAMPC = TEMP4;                                          /* LA0C7-LA0C9: in case the dead slot is used */
    CAMPC = (uint8_t)(CAMPC - 1);                           /* LA0CC */
    EXICAM = 0x00;                                          /* LA0CF-LA0D1 */
    if (actinv(x, y) == 0) return;                          /* LA0D4-LA0D7: any slots? */
    c = 0;                                                  /* LA0DB CLC */
    a = (uint8_t)(adc(TEMP1, 0x02, &c) & 0x0F);             /* LA0D9-LA0DE */
    if (a == 0x0F && (WELTYP & 0x80)) a = 0x0E;             /* LA0E0-LA0E9 */
    TEMP1 = a;                                              /* LA0EB: CCW line */
    TEMP2 = (uint8_t)(TEMP2 | K_ZROCCW);                    /* LA0ED-LA0F1 */
    (void)actinv(x, y);                                     /* LA0F3 */
}

/* ======================================================================= */
/* CHARGES, EXPLOSIONS, COLLISIONS                                         */
/* ======================================================================= */

/* MOVCHA ($A18F): move the player charges (down, spike collisions) and the
 * enemy charges (up, cursor hits).  Exits X = 0. */
xy6502 movcha(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c;
    xy6502 r;
    x = K_NPCHARG + K_NICHARG - 1;                          /* LA18F */
    INDEX1 = x;                                             /* LA191 */
    do {
        x = INDEX1;                                         /* LA193 */
        a = R(A_CHARY + x);                                 /* LA195 */
        if (a != 0) {                                       /* LA198: active? */
            if (x < K_NPCHARG) {                            /* LA19A-LA19C: toward invader */
                c = 0;                                      /* (C clear from the CPX) */
                a = adc(a, K_PCVELO, &c);                   /* LA19E */
                y = R(A_CHARCO + x);                        /* LA1A0 */
                if (y != 0) {                               /* LA1A3: in collision with a line? */
                    c = 1;                                  /* LA1A5 SEC */
                    a = sbc(a, 0x04, &c);                   /* LA1A6: slow it down */
                }
                R(A_CHARY + x) = a;                         /* LA1A8 */
                r = lifect(x, y);                           /* LA1AB */
                x = r.x; y = r.y;
                if (R(A_CHARY + x) >= K_ILINDDY) {          /* LA1AE-LA1B3: at end? */
                    CHACOU = (uint8_t)(CHACOU - 1);         /* LA1B5 */
                    R(A_CHARY + x) = 0x00;                  /* LA1B8-LA1BA: deactivate */
                }
            } else {
                c = 0;                                      /* LA1C3 CLC: toward player */
                R(A_CHARYL + x) = adc(R(A_CHARYL + x), WCHARL, &c);   /* LA1C0-LA1C7 */
                a = adc(R(A_CHARY + x), WCHARIN, &c);       /* LA1CA-LA1CD */
                if (a < CURSY) {                            /* LA1D0-LA1D3: at top? */
                    ESHCOU = (uint8_t)(ESHCOU - 1);         /* LA1D5 */
                    chatop(x, y);                           /* LA1D7 */
                    a = 0x00;                               /* LA1DA: deactivate */
                }
                R(A_CHARY + x) = a;                         /* LA1DC */
            }
        }
        INDEX1 = (uint8_t)(INDEX1 - 1);                     /* LA1DF */
    } while (!(INDEX1 & 0x80));                             /* LA1E1 */
    return xy(x, y);                                        /* LA1E3 */
}

/* CHATOP ($A1E4): an enemy charge at the top on the cursor's line kills it. */
void chatop(uint8_t x, uint8_t y)
{
    if (CURSL1 != R(A_CHARL1 + x)) return;                  /* LA1E4-LA1EA */
    if (CURSL2 & 0x80) return;                              /* LA1EC-LA1EF: already dead? */
    incpsq(x, y);                                           /* LA1F1 */
    CURSL2 = 0x81;                                          /* LA1F4-LA1F6: blasted code */
}

/* LIFECT ($A1FA): player charge X against the enemy line of its line:
 * shorten the line, score, exhaust the charge. */
xy6502 lifect(uint8_t x, uint8_t y)
{
    uint8_t a;
    xy6502 r;
    y = R(A_CHARL1 + x);                                    /* LA1FA */
    if (R(A_LINEY + y) == 0) return xy(x, y);               /* LA1FD-LA200: line dead? */
    a = R(A_CHARY + x);                                     /* LA202 */
    if (a >= R(A_LINEY + y)) {                              /* LA205-LA208: charge on enemy line? */
        if (a >= K_ILINDDY) a = 0x00;                       /* LA20A-LA20E: line dead */
        R(A_LINEY + y) = a;                                 /* LA210 */
        R(A_CHARCO + x) = (uint8_t)(R(A_CHARCO + x) + 1);   /* LA213: collision counter */
        R(A_LINSTA + y) = 0xC0;                             /* LA216-LA218: recalc */
        selico(x, y);                                       /* LA21B: sound */
        x = 0xFF;                                           /* LA21E: score from the temps */
        TEMP1 = 0x00;                                       /* LA220-LA222 */
        TEMP2 = 0x00;                                       /* LA224 */
        TEMP0 = 0x01;                                       /* LA226-LA228: 1 point */
        r = upscor(x, y);                                   /* LA22A */
        y = r.y;
        x = INDEX1;                                         /* LA22D: restore charge index */
    }
    if (R(A_CHARCO + x) >= 0x02) {                          /* LA22F-LA234: exhausted? */
        R(A_CHARY + x) = 0x00;                              /* LA236-LA238 */
        CHACOU = (uint8_t)(CHACOU - 1);                     /* LA23B */
    }
    return xy(x, y);                                        /* LA23E */
}

/* FIREPC ($A23F): fire a player charge (attract: automatically when an
 * enemy charge is near). */
xy6502 firepc(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c;
    flags6502 f;
    xy6502 r;
    if (CURSL2 & 0x80) return xy(x, y);                     /* LA23F-LA242: player alive? */
    if (!(QSTATUS & 0x80)) {                                /* LA244-LA246: attract - auto fire */
        TEMP0 = CURMOD;                                     /* LA248-LA24B */
        x = K_NICHARG + K_NINVAD - 1;                       /* LA24D */
        do {
            if (R(A_CHARY + K_NPCHARG + x) != 0) {          /* LA24F-LA252 */
                c = 1;                                      /* LA257 SEC */
                a = sbc_f(R(A_CHARL1 + K_NPCHARG + x), CURSL1, &c, &f);   /* LA254-LA258 */
                if (f.n) {                                  /* LA25B */
                    c = 0;                                  /* LA25F CLC */
                    a = adc((uint8_t)(a ^ 0xFF), 0x01, &c); /* LA25D-LA260 */
                }
                if (a < 0x02) TEMP0 = (uint8_t)(TEMP0 + 1); /* LA262-LA266: too close - fire */
            }
            x--;                                            /* LA268 */
        } while (!(x & 0x80));                              /* LA269 */
        a = TEMP0;                                          /* LA26B */
    } else {
        a = SWSTAT;                                         /* LA270 */
        CK(0xA272);
        a &= K_MFIRE;                                       /* LA272 */
    }
    if (a == 0) return xy(x, y);                            /* LA274: fire? */
    x = K_NPCHARG - 1;                                      /* LA276 */
    do {
        if (R(A_CHARY + x) == 0) {                          /* LA278-LA27B: vacancy? */
            CHACOU = (uint8_t)(CHACOU + 1);                 /* LA27D */
            R(A_CHARY + x) = CURSY;                         /* LA280-LA283: start at cursor */
            R(A_CHARL1 + x) = CURSL1;                       /* LA286-LA289 */
            R(A_CHARL2 + x) = CURSL2;                       /* LA28C-LA28F */
            R(A_CHARCO + x) = 0x00;                         /* LA292-LA294 */
            slaunc(x, y);                                   /* LA297: launch sound */
            r = colchk(CURSY, x, y);                        /* LA29A-LA29D */
            y = r.y;
            x = 0x00;                                       /* LA2A0: exit loop */
        }
        x--;                                                /* LA2A2 */
    } while (!(x & 0x80));                                  /* LA2A3 */
    return xy(x, y);                                        /* LA2A5 */
}

/* FIREIC ($A2A6): invaders fire (timer, chance by the charges on screen).
 * Exits X = $FF. */
xy6502 fireic(uint8_t x, uint8_t y)
{
    uint8_t a;
    if (CURSL2 & 0x80) return xy(x, y);                     /* LA2A6-LA2A9: player alive? */
    x = K_NINVAD - 1;                                       /* LA2AB */
    do {
        a = R(A_INVAY + x);                                 /* LA2AD */
        if (a != 0 && a >= K_ILINLIY + 0x20 &&              /* LA2B0-LA2B4: active, low enough? */
            (R(A_INVAC2 + x) & K_INVFIR)) {                 /* LA2B6-LA2BB: firepower */
            R(A_INVACT + x) = (uint8_t)(R(A_INVACT + x) - 1);   /* LA2BD: fire timer */
            if (R(A_INVACT + x) & 0x80) {                   /* LA2C0 */
                R(A_INVACT + x) = (uint8_t)(R(A_INVACT + x) + 1);   /* LA2C2 */
                if ((R(A_INVAC1 + x) & K_INVMOT) == 0) {    /* LA2C5-LA2CA: moving? */
                    a = hw_random(0);                       /* LA2CC */
                    y = ESHCOU;                             /* LA2CF */
                    if (a >= ROM(ALWELG_CHANCE + y)) {      /* LA2D1-LA2D4: in the fire window? */
                        y = WCHAMX;                         /* LA2D6 */
                        do {
                            if (R(A_CHARY + K_NPCHARG + y) == 0) {   /* LA2D9-LA2DC: vacancy? */
                                R(A_CHARY + K_NPCHARG + y) = R(A_INVAY + x);    /* LA2DE-LA2E1 */
                                R(A_CHARL1 + K_NPCHARG + y) = R(A_INVAL1 + x);  /* LA2E4-LA2E7 */
                                R(A_CHARL2 + K_NPCHARG + y) = R(A_INVAL2 + x);  /* LA2EA-LA2ED */
                                R(A_INVACT + x) = WCHARFR;                      /* LA2F0-LA2F3: restart timer */
                                eslson(x, y);                                   /* LA2F6 */
                                ESHCOU = (uint8_t)(ESHCOU + 1);                 /* LA2F9 */
                                y = 0x00;                                       /* LA2FB: exit loop */
                            }
                            y--;                            /* LA2FD */
                        } while (!(y & 0x80));              /* LA2FE */
                    }
                }
            }
        }
        x--;                                                /* LA300 */
    } while (!(x & 0x80));                                  /* LA301 */
    return xy(x, y);                                        /* LA303 */
}

/* INCFS2 ($A309): a fuse hit by player charge X (Y = shot index): mark the
 * shot used, explode, kill, score 250/500/750.  Exits X = INDEX1 and
 * UPSCOR's Y. */
xy6502 incfs2(uint8_t x, uint8_t y)
{
    uint8_t a, s;
    unsigned c;
    xy6502 r;
    INDEX1 = x;                                             /* LA309 */
    R(A_CHARCO + x) = 0xFF;                                 /* LA30B-LA30D: shot used */
    c = 1;                                                  /* LA311 SEC */
    y = sbc(y, K_NICHARG, &c);                              /* LA310-LA314: shot index -> invader index */
    TEMP4 = R(A_INVAL1 + y);                                /* LA315-LA318 */
    a = (uint8_t)(hw_random(1) & 0x07);                     /* LA31A-LA31D */
    if (a >= 0x03) a = 0x00;                                /* LA31F-LA323: 0 (250), 1 (500), 2 (750) */
    s = a;                                                  /* LA325 PHA */
    c = 0;                                                  /* LA326 CLC */
    a = adc(a, K_CFTYPE, &c);                               /* LA327 */
    gexifu(a, x, y);                                        /* LA329: explosion */
    kilinv(x, y);                                           /* LA32C: kill fuse */
    c = 0;                                                  /* LA330 CLC */
    x = adc(s, 0x05, &c);                                   /* LA32F-LA333 */
    r = upscor(x, y);                                       /* LA334 */
    return xy(INDEX1, r.y);                                 /* LA337-LA339 */
}

/* DEADCU ($A352): kill the cursor with explosion type A.  X, Y preserved. */
void deadcu(uint8_t a, uint8_t x, uint8_t y)
{
    TEMP3 = a;                                              /* LA352: explosion code */
    TEMP0 = CURSY;                                          /* LA354-LA357: position */
    TEMP4 = CURSL1;                                         /* LA359-LA35C */
    cpexpl(x, y);                                           /* LA35E: noise */
    genex2(x, y);                                           /* LA361 */
    CURSL2 = 0x81;                                          /* LA364-LA366: kill cursor, no display */
    SPFTIM = 0x01;                                          /* LA369-LA36B: explosion timer */
}

/* INCP2 ($A34D): special explosion picture A, kill the cursor. */
static void incp2(uint8_t a, uint8_t x, uint8_t y)
{
    SPXIND = a;                                             /* LA34D */
    deadcu(K_CPTYPE, x, y);                                 /* LA350 */
}

/* INIPSQ ($A33A): a chaser destroys the cursor. */
void inipsq(uint8_t x, uint8_t y)
{
    deadcu(K_IPTYPE, x, y);                                 /* LA33A-LA33C */
    CURSL2 = (uint8_t)(CURSL2 - 1);                         /* LA33F: display cursor */
}

/* INFPSQ ($A343): a fuse kills the cursor. */
void infpsq(uint8_t x, uint8_t y)
{
    incp2(K_FPSPXI, x, y);                                  /* LA343-LA345 */
}

/* INPPSQ ($A347): a pulsar / an enemy line kills the cursor. */
void inppsq(uint8_t x, uint8_t y)
{
    incp2(K_PPSPXI, x, y);                                  /* LA347-LA349 */
}

/* INCPSQ ($A34B): an enemy charge kills the cursor. */
void incpsq(uint8_t x, uint8_t y)
{
    incp2((uint8_t)(K_CPSPXI & 0xFF), x, y);                /* LA34B LDA #<CPSPXI */
}

/* INCCSQ ($A36F): player charge X hits enemy charge Y: explosion,
 * deactivate the enemy shot, mark the player shot used.  X, Y preserved. */
void inccsq(uint8_t x, uint8_t y)
{
    ccexpl(x, y);                                           /* LA36F */
    TEMP0 = R(A_CHARY + K_NPCHARG + y);                     /* LA372-LA375 */
    TEMP4 = R(A_CHARL1 + K_NPCHARG + y);                    /* LA377-LA37A */
    genexp(K_CCTYPE, x, y);                                 /* LA37C-LA37E */
    R(A_CHARY + K_NPCHARG + y) = 0x00;                      /* LA381-LA383: deactivate shot */
    ESHCOU = (uint8_t)(ESHCOU - 1);                         /* LA386 */
    R(A_CHARCO + x) = 0xFF;                                 /* LA388-LA38A: shot used */
}

/* INCISQ ($A398): kill invader Y with an explosion and score it.  Exits
 * with UPSCOR's X/Y. */
static xy6502 incisq(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c;
    if ((R(A_INVAC1 + y) & (K_ZROCCW | K_ZMOTJM)) == (K_ZROCCW | K_ZMOTJM)) {   /* LA398-LA39F: flipping CCW? */
        c = 1;                                              /* LA3AA SEC */
        a = (uint8_t)(sbc(R(A_INVAL1 + y), 0x01, &c) & 0x0F);   /* LA3A7-LA3AD: adjust base line */
    } else {
        a = R(A_INVAL1 + y);                                /* LA3A1: base leg */
    }
    TEMP4 = a;                                              /* LA3AF */
    gexifu(K_CITYPE, x, y);                                 /* LA3B1-LA3B3: bang picture */
    kilinv(x, y);                                           /* LA3B6 */
    y = (uint8_t)(R(A_INVAC1 + y) & K_INVABI);              /* LA3B9-LA3BE */
    x = ROM(ALWELG_INVPIN + y);                             /* LA3BF: points index */
    return upscor(x, y);                                    /* LA3C2 JMP UPSCOR */
}

/* INCIS2 ($A38E): player charge X hits invader shot-index Y: mark the shot
 * used, then INCISQ. */
xy6502 incis2(uint8_t x, uint8_t y)
{
    unsigned c;
    R(A_CHARCO + x) = 0xFF;                                 /* LA38E-LA390: shot used */
    c = 1;                                                  /* LA394 SEC */
    y = sbc(y, K_NICHARG, &c);                              /* LA393-LA397: invader index */
    return incisq(x, y);                                    /* INCISQ */
}

/* GEXIFU ($A3CA): explosion type A at invader Y's depth, with the bang
 * sound.  X, Y preserved. */
void gexifu(uint8_t a, uint8_t x, uint8_t y)
{
    ciexpl(x, y);                                           /* LA3CA-LA3CB PHA / JSR */
    TEMP0 = R(A_INVAY + y);                                 /* LA3CE-LA3D1 */
    genexp(a, x, y);                                        /* LA3D3 PLA */
}

/* GENEXP ($A3D4): start an explosion of type A (TEMP0 = depth, TEMP4 =
 * line).  X, Y preserved. */
void genexp(uint8_t a, uint8_t x, uint8_t y)
{
    TEMP3 = a;                                              /* LA3D4 */
    genex2(x, y);                                           /* GENEX2 */
}

/* GENEX2 ($A3D6): explosion slot (a vacancy, else the one furthest along)
 * with TEMP3 type, TEMP0 depth, TEMP4 line.  X, Y preserved. */
void genex2(uint8_t x, uint8_t y)
{
    uint8_t a;
    SAVEX = x;                                              /* LA3D6 */
    SAVEY = y;                                              /* LA3D8 */
    TEMP1 = 0x00;                                           /* LA3DA-LA3DC */
    TEMP2 = 0x00;                                           /* LA3DE */
    x = K_NEXPLO - 1;                                       /* LA3E0 */
    for (;;) {
        if (R(A_EXPLOY + x) == 0) goto gotexp;              /* LA3E2-LA3E5: vacancy */
        a = R(A_EXPLOS + x);                                /* LA3E7 */
        if (a >= TEMP1) {                                   /* LA3EA-LA3EC: furthest along so far? */
            TEMP1 = a;                                      /* LA3EE */
            TEMP2 = x;                                      /* LA3F0 */
        }
        x--;                                                /* LA3F2 */
        if (x & 0x80) break;                                /* LA3F3 */
    }
    EXPCOU = (uint8_t)(EXPCOU - 1);                         /* LA3F5: incremented later */
    x = TEMP2;                                              /* LA3F8 */
gotexp:
    R(A_EXPLOS + x) = 0x00;                                 /* LA3FA-LA3FC: start sequence */
    R(A_EXPLOT + x) = TEMP3;                                /* LA3FF-LA401: type */
    R(A_EXPLOY + x) = TEMP0;                                /* LA404-LA406: depth */
    R(A_EXPLOL + x) = TEMP4;                                /* LA409-LA40B: line */
    EXPCOU = (uint8_t)(EXPCOU + 1);                         /* LA40E */
    /* LA411-LA413 LDX SAVEX / LDY SAVEY */
}

/* PROEXP ($A416): advance the explosions, count the active ones. */
xy6502 proexp(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c;
    if (EXPCOU == 0) return xy(x, y);                       /* LA416-LA419: any bangs? */
    EXPCOU = 0x00;                                          /* LA41B-LA41D */
    x = K_NEXPLO - 1;                                       /* LA420 */
    do {
        if (R(A_EXPLOY + x) != 0) {                         /* LA422-LA425: active? */
            a = R(A_EXPLOS + x);                            /* LA427 */
            y = R(A_EXPLOT + x);                            /* LA42A */
            c = 0;                                          /* LA42D CLC */
            a = adc(a, ROM(ALWELG_TEXINC + y), &c);         /* LA42E */
            R(A_EXPLOS + x) = a;                            /* LA431 */
            if (a >= ROM(ALWELG_TEXPDN + y))                /* LA434-LA437: done? */
                R(A_EXPLOY + x) = 0x00;                     /* LA439-LA43B */
            else
                EXPCOU = (uint8_t)(EXPCOU + 1);             /* LA441 */
        }
        x--;                                                /* LA444 */
    } while (!(x & 0x80));                                  /* LA445 */
    return xy(x, y);                                        /* LA447 */
}

/* COLLIS ($A454): collision check for every active player charge. */
xy6502 collis(uint8_t x, uint8_t y)
{
    xy6502 r;
    x = K_NPCHARG - 1;                                      /* LA454 */
    do {
        uint8_t a = R(A_CHARY + x);                         /* LA456 */
        if (a != 0) {                                       /* LA459 */
            r = colchk(a, x, y);                            /* LA45B */
            x = r.x; y = r.y;
        }
        x--;                                                /* LA45E */
    } while (!(x & 0x80));                                  /* LA45F */
    return xy(x, y);                                        /* LA461 */
}

/* COLCHK ($A463): player charge X at depth A against the enemy charges and
 * invaders.  Exits X (preserved), Y = $FF. */
xy6502 colchk(uint8_t a, uint8_t x, uint8_t y)
{
    uint8_t s;
    unsigned c;
    TEMPX = a;                                              /* LA463 */
    y = K_NICHARG - 1 + K_NINVAD;                           /* LA465 */
    do {
        a = R(A_CHARY + K_NPCHARG + y);                     /* LA467 */
        if (a != 0) {                                       /* LA46A: active? */
            if (a >= TEMPX) {                               /* LA46C-LA46E: absolute delta */
                c = 1;
                a = sbc(a, TEMPX, &c);                      /* LA470 */
            } else {
                c = 1;                                      /* LA477 SEC */
                a = sbc(TEMPX, R(A_CHARY + K_NPCHARG + y), &c);   /* LA475-LA478 */
            }
            if (y < K_NICHARG) {                            /* LA47B-LA47D: enemy shot */
                if (a < CHACHA &&                           /* LA47F-LA481: in range? */
                    (R(A_CHARL1 + K_NPCHARG + y) ^ R(A_CHARL1 + x)) == 0)   /* LA483-LA489: same line? */
                    inccsq(x, y);                           /* LA48B */
            } else {
                s = a;                                      /* LA491 PHA: invader, save delta */
                INDEX2 = y;                                 /* LA492 */
                y = (uint8_t)(R(A_INVAC1 - K_NICHARG + y) & K_INVABI);   /* LA494-LA499 */
                a = s;                                      /* LA49A PLA */
                if (a < R(A_ENSIZE + y)) {                  /* LA49B-LA49E: in range by type? */
                    if (y == K_ZABFUS) {                    /* LA4A0-LA4A2: fuse? */
                        y = INDEX2;                         /* LA4A4 */
                        if (R(A_INVAY - K_NICHARG + y) != CURSY &&             /* LA4A6-LA4AC: not at top */
                            R(A_CHARL1 + x) == R(A_INVAL1 - K_NICHARG + y) &&  /* LA4AE-LA4B4: same base line */
                            (R(A_INVAL2 - K_NICHARG + y) & 0x80))              /* LA4B6-LA4B9: vulnerable? */
                            (void)incfs2(x, y);             /* LA4BB */
                    } else {
                        int hit;
                        y = INDEX2;                         /* LA4C1: flipper, tanker, spinner, pulsar */
                        if (R(A_INVAL2 - K_NICHARG + y) & 0x80) {   /* LA4C3-LA4C6: flipping? */
                            hit = (R(A_INVAL1 - K_NICHARG + y) == R(A_CHARL2 + x));   /* LA4C8-LA4CE */
                            if (!hit) hit = (R(A_INVAL1 - K_NICHARG + y) == R(A_CHARL1 + x));   /* OKATOP */
                        } else if (R(A_INVAY - K_NICHARG + y) == CURSY) {     /* LA4D2-LA4D8: at top? */
                            hit = 0;
                        } else {
                            hit = (R(A_INVAL1 - K_NICHARG + y) == R(A_CHARL1 + x));   /* OKATOP LA4DA-LA4E0 */
                        }
                        if (hit) {                          /* YESCOL */
                            INDEX1 = x;                     /* LA4E2 */
                            (void)incis2(x, y);             /* LA4E4 */
                            x = INDEX1;                     /* LA4E7 */
                        }
                    }
                }
                y = INDEX2;                                 /* NOCOL LA4E9 */
            }
        }
        y--;                                                /* LA4EB */
    } while (!(y & 0x80));                                  /* LA4EC-LA4EE */
    if (R(A_CHARCO + x) == 0xFF) {                          /* LA4F1-LA4F6: charge spent? */
        R(A_CHARY + x) = 0x00;                              /* LA4F8-LA4FA */
        CHACOU = (uint8_t)(CHACOU - 1);                     /* LA4FD */
        R(A_CHARCO + x) = 0x00;                             /* LA500 */
    }
    return xy(x, y);                                        /* LA503 */
}

/* ANALYZ ($A504): the player's status - death (drop the invaders, end of
 * life / game), the POKEY/checksum protection, end of wave (INDROP), abort
 * by start in free play. */
xy6502 analyz(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c;
    if (CURSL2 & 0x80) {                                    /* LA504-LA507: cursor dead? */
        if ((CHACOU | ESHCOU | EXPCOU) != 0) return xy(x, y);   /* LA509-LA511: charges or bangs? */
        x = WINVMX;                                         /* LA513: drop everybody into the well */
        do {
            a = R(A_INVAY + x);                             /* LA516 */
            if (a != 0) {                                   /* LA519 */
                c = 0;                                      /* LA51B CLC */
                a = adc(a, 0x0F, &c);                       /* LA51C */
                if (!c) c = (a >= K_ILINDDY);               /* LA51E-LA520 */
                if (c) a = 0x00;                            /* LA522-LA524: at bottom - deactivate */
                R(A_INVAY + x) = a;                         /* LA526 */
            }
            x--;                                            /* LA529 */
        } while (!(x & 0x80));                              /* LA52A */
        x = PLAYUP;                                         /* LA52C */
        if (R(A_LIVES1 + x) == 0x01) {                      /* LA52E-LA532: game over? */
            LEVELY = 0x00;                                  /* LA534-LA536: recalc well top */
            ROTDIS = 0x01;                                  /* LA539-LA53B: redisplay well */
            c = 1;                                          /* LA540 SEC */
            EYL = sbc(EYL, 0x20, &c);                       /* LA53E-LA543: shrink hole */
            a = sbc(EYH, 0x00, &c);                         /* LA545-LA547 */
            EYH = a;                                        /* LA549 */
            c = (a == 0xFA);                                /* LA54B-LA550: far enough? */
        } else {
            c = 0;                                          /* LA557 CLC */
            a = adc(CURSY, 0x0F, &c);                       /* LA554-LA558: cursor down */
            CURSY = a;                                      /* LA55A */
            if (!c) c = (a >= K_ILINDDY);                   /* LA55D-LA55F */
        }
        if (c) {                                            /* LA561: end of life phase */
            QSTATE = K_CENDLI;                              /* LA563-LA565 */
            inicha();                                       /* LA567 */
            x = 0xFF;
            c = 0;                                          /* LA56D CLC */
            a = adc(INMCOU, INCCOU, &c);                    /* LA56A-LA56E */
            c = 0;                                          /* LA571 CLC */
            a = adc(a, NYMCOU, &c);                         /* LA572: to the nymphs */
            if (a >= K_NNYMPH - 1) a = K_NNYMPH - 1;        /* LA575-LA579 */
            NYMCOU = a;                                     /* LA57B: for next life */
        }
        return xy(x, y);                                    /* LA57E-LA57F -> LA5CA RTS */
    }
    /* ZQVAVG */
    if ((QT3 | QT6) != 0 && LSCORH > 0x17) {                /* LA581-LA58D: protection */
        x = LSCORL;                                         /* LA58F */
        R(x) = (uint8_t)(R(x) + 1);                         /* LA591 INC $00,X */
    }
    if (CURMOD != 0) return xy(x, y);                       /* LA593-LA596: top mode? */
    if ((NYMCOU | EXPCOU) == 0) {                           /* LA598-LA59E: all nymphs converted? */
        y = WINVMX;                                         /* LA5A0 */
        do {
            a = R(A_INVAY + y);                             /* LA5A3 */
            if (a != 0 && a >= 0x11) goto liner;            /* LA5A6-LA5AA: a liner (not at top) */
            y--;                                            /* LA5AC */
        } while (!(y & 0x80));                              /* LA5AD */
        indrop();                                           /* LA5AF */
        inicha();                                           /* LA5B2 */
        x = 0xFF;
    }
liner:
    a = SWSTRT;                                             /* LA5B5 */
    CK(0xA5B7);
    if ((a & (K_MSTRT2 | K_MSTRT1)) != 0 &&                 /* LA5B7-LA5B9: either start pressed? */
        (QSTATUS & 0x80) &&                                 /* LA5BB-LA5BD: not attract */
        (OPTIN1 & 0x43) == 0x40) {                          /* LA5BF-LA5C5: free play & abort enabled? */
        indrop();                                           /* LA5C7 */
        x = 0xFF;
    }
    return xy(x, y);                                        /* LA5CA */
}

/* INDROP ($A5CB): start the cursor drop mode (with the spike warning pause
 * on early waves).  Exits X = $FF. */
void indrop(void)
{
    int x;
    QSTATE = K_CDROP;                                       /* LA5CB-LA5CD */
    CURMOD = (uint8_t)(CURMOD | 0x80);                      /* LA5CF-LA5D4: drop mode */
    CURSVL = 0x00;                                          /* LA5D7-LA5D9: downward acceleration */
    CURSYL = 0x00;                                          /* LA5DC */
    EYLL = 0x00;                                            /* LA5DF */
    ELICNT = 0x00;                                          /* LA5E1 */
    CURSVH = 0x02;                                          /* LA5E4-LA5E6 */
    for (x = K_NLINES - 1; x >= 0; x--)                     /* LA5E9, LA5F3-LA5F4 */
        if (R(A_LINEY + x) != 0)                            /* LA5EB-LA5EE */
            ELICNT = (uint8_t)(ELICNT + 1);                 /* LA5F0: live spikes */
    if (ELICNT != 0 && CURWAV < 0x07) {                     /* LA5F6-LA5FF: warn player? */
        QTMPAUS = (uint8_t)(6 * K_QUASEC);                  /* LA601-LA603 */
        QSTATE = K_CPAUSE;                                  /* LA605-LA607 */
        QNXTSTA = K_CDROP;                                  /* LA609-LA60B */
        ELICNT = 0x80;                                      /* LA60D-LA60F: warning flag */
    }
    SUZTIM = 0xFF;                                          /* LA612-LA614: deactivate superzapper */
}

/* ======================================================================= */
/* BIG BOOM, STAR FIELD, SUPERZAPPER, POLAR DELTA                          */
/* ======================================================================= */

/* PRBOOM ($A618): big boom state - move and decelerate the particles,
 * launch more while the timer runs, then get initials. */
xy6502 prboom(uint8_t x, uint8_t y)
{
    BOOMFL = BOOMTI;                                        /* LA618-LA61B: boom off flag */
    x = K_NPARTI - 1;                                       /* LA61E */
    INDEX1 = x;                                             /* LA620 */
    do {
        x = INDEX1;                                         /* LA622 */
        if (R(A_PARTIY + x) == 0) {                         /* LA624-LA627: active particle? */
            if (BOOMTI != 0) timlau(x, y);                  /* LA629-LA62E */
        } else {
            y = uparpo(x);                                  /* LA634 */
            y = decpar(x);                                  /* LA637 */
            BOOMFL = 0xFF;                                  /* LA63A-LA63C: boom active */
        }
        INDEX1 = (uint8_t)(INDEX1 - 1);                     /* LA63F */
    } while (!(INDEX1 & 0x80));                             /* LA641 */
    if ((QFRAME & 0x01) == 0 && BOOMTI != 0)                /* LA643-LA64C */
        BOOMTI = (uint8_t)(BOOMTI - 1);                     /* LA64E: stop at 0 */
    if (BOOMFL == 0)                                        /* LA651-LA654 */
        QSTATE = K_CGETINI;                                 /* LA656-LA658: get initials */
    return xy(x, y);                                        /* LA65A */
}

/* TIMLAU ($A65B): launch particle X from the centre with a random velocity.
 * X and Y are preserved. */
void timlau(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c;
    int n;
    if ((QFRAME & 0x00) != 0) return;                       /* LA65B-LA65F */
    R(A_PARTIX + x) = 0x80;                                 /* LA661-LA663: centre */
    R(A_PARTIY + x) = 0x80;                                 /* LA666 */
    R(A_PARTIZ + x) = 0x80;                                 /* LA669 */
    a = hw_random(1);                                       /* LA66C */
    R(A_PARLXV + x) = a;                                    /* LA66F: fractional X velocity */
    a = fixtop(a, &n);                                      /* LA672 */
    R(A_PARTXV + x) = a;                                    /* LA675 */
    a = hw_random(0);                                       /* LA678 */
    R(A_PARLYV + x) = a;                                    /* LA67B */
    a = fixtop(a, &n);                                      /* LA67E */
    if (!n) {                                               /* LA681 */
        c = 0;                                              /* LA685 CLC */
        a = adc((uint8_t)(a ^ 0xFF), 0x01, &c);             /* LA683-LA686 */
    }
    R(A_PARTYV + x) = a;                                    /* LA688 */
    a = hw_random(0);                                       /* LA68B */
    R(A_PARLZV + x) = a;                                    /* LA68E */
    R(A_PARTZV + x) = fixtop(a, &n);                        /* LA691-LA694 */
    ciexpl(x, y);                                           /* LA697: make noise */
    CK(0xA69A);
}

/* FIXTOP ($A69B): 0..7 from RANDO2, negated when bit 0 of A was 1.  Returns
 * A, *n = the N flag. */
uint8_t fixtop(uint8_t a, int *n)
{
    unsigned c = a & 1u;                                    /* LA69B LSR */
    flags6502 f;
    a = (uint8_t)(hw_random(1) & 0x07);                     /* LA69C-LA69F */
    *n = 0;
    if (c) {                                                /* LA6A1 */
        c = 0;                                              /* LA6A5 CLC */
        a = adc_f((uint8_t)(a ^ 0xFF), 0x01, &c, &f);       /* LA6A3-LA6A6 */
        *n = f.n;
    }
    return a;                                               /* LA6A8 */
}

/* one axis of UPARPO: position + velocity, off screen outside $10-$EF */
static uint8_t uparpo_axis(uint8_t v, uint8_t p, unsigned *c, int *off)
{
    uint8_t a;
    if (!(v & 0x80)) {
        a = adc(v, p, c);                                   /* + velocity */
        *c = (a >= 0xF0);
        if (*c) *off = 1;
    } else {
        a = adc(v, p, c);                                   /* - velocity */
        *c = (a >= 0x10);
        if (!*c) *off = 1;
    }
    return a;
}

/* UPARPO ($A6A9): update particle X's position.  Returns Y (its new Y, 0 if
 * off screen). */
uint8_t uparpo(uint8_t x)
{
    uint8_t a, y;
    unsigned c;
    int off = 0;
    c = 0;                                                  /* LA6AC CLC */
    R(A_PARLIY + x) = adc(R(A_PARLYV + x), R(A_PARLIY + x), &c);   /* LA6A9-LA6B0: Y fractional */
    a = uparpo_axis(R(A_PARTYV + x), R(A_PARTIY + x), &c, &off);   /* LA6B3-LA6CB */
    if (off) a = 0x00;                                      /* LA6BF / LA6CB */
    y = a;                                                  /* LA6CD */
    c = 0;                                                  /* LA6D1 CLC */
    R(A_PARLIX + x) = adc(R(A_PARLXV + x), R(A_PARLIX + x), &c);   /* LA6CE-LA6D5: X fractional */
    off = 0;
    a = uparpo_axis(R(A_PARTXV + x), R(A_PARTIX + x), &c, &off);   /* LA6D8-LA6F0 */
    if (off) y = 0x00;                                      /* LA6E4 / LA6F0 */
    R(A_PARTIX + x) = a;                                    /* LA6F2 */
    c = 0;                                                  /* LA6F8 CLC */
    R(A_PARLIZ + x) = adc(R(A_PARLZV + x), R(A_PARLIZ + x), &c);   /* LA6F5-LA6FC: Z fractional */
    off = 0;
    a = uparpo_axis(R(A_PARTZV + x), R(A_PARTIZ + x), &c, &off);   /* LA6FF-LA717 */
    if (off) y = 0x00;                                      /* LA70B / LA717 */
    R(A_PARTIZ + x) = a;                                    /* LA719 */
    R(A_PARTIY + x) = y;                                    /* LA71C-LA71D */
    return y;                                               /* LA720 */
}

/* DECPAR ($A721): decelerate particle X; all three velocities 0 deactivates
 * it.  Returns Y (the Z velocity's high byte). */
uint8_t decpar(uint8_t x)
{
    uint8_t a, y;
    TEMP0 = 0xFD;                                           /* LA721-LA723: velocity = 0 counter */
    y = R(A_PARTXV + x);                                    /* LA728 */
    a = decele(R(A_PARLXV + x), &y);                        /* LA725-LA72B */
    R(A_PARLXV + x) = a;                                    /* LA72E */
    R(A_PARTXV + x) = y;                                    /* LA731-LA732 */
    y = R(A_PARTYV + x);                                    /* LA738 */
    a = decele(R(A_PARLYV + x), &y);                        /* LA735-LA73B */
    R(A_PARLYV + x) = a;                                    /* LA73E */
    R(A_PARTYV + x) = y;                                    /* LA741-LA742 */
    y = R(A_PARTZV + x);                                    /* LA748 */
    a = decele(R(A_PARLZV + x), &y);                        /* LA745-LA74B */
    R(A_PARLZV + x) = a;                                    /* LA74E */
    R(A_PARTZV + x) = y;                                    /* LA751-LA752 */
    if (TEMP0 == 0)                                         /* LA755-LA757 */
        R(A_PARTIY + x) = 0x00;                             /* LA759: deactivate particle */
    return y;                                               /* LA75C */
}

/* DECELE ($A75D): decelerate the velocity A (low) / *y (high) by DECELO
 * towards 0.  Returns A (low), *y = high. */
uint8_t decele(uint8_t a, uint8_t *y)
{
    unsigned c;
    TEMP2 = *y;                                             /* LA75D */
    if (!(TEMP2 & 0x80)) {                                  /* LA75F-LA761 BIT / BMI */
        c = 1;                                              /* LA763 SEC: + so subtract */
        TEMP1 = sbc(a, ROM(ALWELG_DECELO), &c);             /* LA764-LA767 */
        a = sbc(TEMP2, 0x00, &c);                           /* LA769-LA76B */
        if (c) goto done;                                   /* LA76D BCC HIT0 */
    } else {
        c = 0;                                              /* LA772 CLC: - so add */
        TEMP1 = adc(a, ROM(ALWELG_DECELO), &c);             /* LA773-LA776 */
        a = adc(TEMP2, 0x00, &c);                           /* LA778-LA77A */
        if (!c) goto done;                                  /* LA77C */
    }
    /* HIT0 */
    TEMP0 = (uint8_t)(TEMP0 + 1);                           /* LA77E: velocity = 0 counter */
    a = 0x00;                                               /* LA780 */
    TEMP1 = 0x00;                                           /* LA782 */
done:
    *y = a;                                                 /* LA784 TAY */
    return TEMP1;                                           /* LA785-LA787 */
}

/* INBOOM ($A789): initialise the particles, start the boom display.  Exits
 * X = $FF. */
xy6502 inboom(uint8_t x, uint8_t y)
{
    int i;
    (void)x;
    for (i = K_NPARTI - 1; i >= 0; i--)                     /* LA789-LA791 */
        R(A_PARTIY + i) = 0x00;                             /* LA78D: deactivate particle */
    BOOMTI = 0x20;                                          /* LA793-LA795: 1/5 second units */
    BOOMFL = 0x20;                                          /* LA798: activate boom */
    QDSTATE = K_CDBOOM;                                     /* LA79B-LA79D */
    ZADJL = 0x00;                                           /* LA79F-LA7A1 */
    R(A_ZADJL + 1) = 0x00;                                  /* LA7A3 */
    return xy(0xFF, y);                                     /* LA7A5 */
}

/* POLDEL ($A7A6): number of lines line A is from line Y the shortest way
 * (-8..+7, minus = clockwise; planar wells: the plain difference).  Y is
 * preserved.  Returns A. */
uint8_t poldel(uint8_t a, uint8_t y)
{
    unsigned c;
    TEMP1 = y;                                              /* LA7A6 */
    c = 1;                                                  /* LA7A8 SEC */
    a = sbc(a, TEMP1, &c);                                  /* LA7A9 */
    TEMP1 = a;                                              /* LA7AB */
    if (!(WELTYP & 0x80)) {                                 /* LA7AD-LA7B0: planar? */
        a &= 0x0F;                                          /* LA7B2 */
        if (a & ROM(ALWELG_EIGHT)) a |= 0xF8;               /* LA7B4-LA7B9: shortest route */
    }
    return a;                                               /* LA7BB */
}

/* INSTAR ($A7BD): the star planes: all off but the last, far away; growing.
 * Exits X = $FF. */
void instar(void)
{
    int x;
    for (x = K_NPLANE - 1; x >= 0; x--)                     /* LA7BD-LA7C5 */
        R(A_PLANEY + x) = 0x00;                             /* LA7C1 */
    R(A_PLANEY + K_NPLANE - 1) = 0xF0;                      /* LA7C7-LA7C9: last plane far away */
    PLAGRO = 0xFF;                                          /* LA7CC-LA7CE: growing */
}

/* PRSTAR ($A7D2): process the planes of stars (PLAGRO minus: still growing,
 * 0: off; set to 0 when every plane is dead). */
xy6502 prstar(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c;
    if (PLAGRO == 0) return xy(x, y);                       /* LA7D2-LA7D5 */
    TEMP0 = 0x00;                                           /* LA7D7-LA7D9: count of active planes */
    x = K_NPLANE - 1;                                       /* LA7DB */
    INDEX1 = x;                                             /* LA7DD */
    do {
        x = INDEX1;                                         /* LA7DF */
        a = R(A_PLANEY + x);                                /* LA7E1 */
        if (a != 0) {                                       /* LA7E4: plane active? */
            c = 1;                                          /* LA7E6 SEC */
            a = sbc(a, 0x07, &c);                           /* LA7E7: update plane position */
            if (c) c = (a >= 0x10);                         /* LA7E9-LA7EB */
            if (!c) {                                       /* LA7ED: too close? */
                y = PLAGRO;                                 /* LA7EF */
                a = (y & 0x80) ? 0xF0 : 0x00;               /* LA7F2-LA7F9: restart far / deactivate */
            }
        } else {
            y = PLAGRO;                                     /* LA7FE: still growing? */
            if (y & 0x80) {                                 /* LA801 */
                c = 0;                                      /* LA804 CLC */
                a = adc(x, 0x01, &c);                       /* LA803-LA805: previous plane */
                if (a >= K_NPLANE) a = 0x00;                /* LA807-LA80B */
                y = a;                                      /* LA80D */
                a = R(A_PLANEY + y);                        /* LA80E */
                if (a != 0)                                 /* LA811 */
                    a = (a < 0xD5) ? 0xF0 : 0x00;           /* LA813-LA81C: start a new plane */
            }
        }
        R(A_PLANEY + x) = a;                                /* LA81E */
        TEMP0 = (uint8_t)(a | TEMP0);                       /* LA821-LA823 */
        INDEX1 = (uint8_t)(INDEX1 - 1);                     /* LA825 */
    } while (!(INDEX1 & 0x80));                             /* LA827 */
    if (TEMP0 == 0) PLAGRO = 0x00;                          /* LA829-LA82D */
    return xy(x, y);                                        /* LA830 */
}

/* INISUZ ($A831): superzapper use counter and timer to 0. */
void inisuz(void)
{
    SUZCNT = 0x00;                                          /* LA831-LA833 */
    SUZTIM = 0x00;                                          /* LA836 */
}

/* PROSUZ ($A83A): process the superzapper - start it on the button (two
 * uses per wave), run its timer and the wipe-outs. */
xy6502 prosuz(uint8_t x, uint8_t y)
{
    uint8_t sw;
    if (QSTATUS & 0x80) {                                   /* LA83A-LA83C: attract? */
        if (SUZTIM == 0) {                                  /* LA83E-LA841: zap active? */
            if (!(CURSL2 & 0x80)) {                         /* LA843-LA846: cursor alive? */
                sw = SWFINA;                                /* LA848 */
                CK(0xA84A);
                if (sw & K_MSUZA) {                         /* LA84A-LA84C: zap pressed? */
                    if (SUZCNT < K_CSUMAX) {                /* LA84E-LA853: zaps left? */
                        SUZCNT = (uint8_t)(SUZCNT + 1);     /* LA855: zap counter */
                        SUZTIM = 0x01;                      /* LA858-LA85A: start zap timer */
                    }
                    sw = SWFINA;                            /* LA85D */
                    CK(0xA85F);
                    SWFINA = (uint8_t)(sw & (uint8_t)~(K_MSUZA | K_MFAKE));   /* LA85F-LA861 */
                }
            }
            CK(0xA863);
        } else {
            SUZTIM = (uint8_t)(SUZTIM + 1);                 /* LA866: zap active */
            x = SUZCNT;                                     /* LA869 */
            if (SUZTIM >= ROM(ALWELG_TIMAX + x))            /* LA86C-LA872: timer expired? */
                SUZTIM = 0x00;                              /* LA874-LA876: deactivate zap */
            {
                xy6502 r = kilene(x, y);                    /* LA879: wipe out invaders & charges */
                x = r.x; y = r.y;
            }
        }
    }
    sw = SWFINA;                                            /* LA87C */
    CK(0xA87E);
    SWFINA = (uint8_t)(sw & (uint8_t)~K_MFAKE);             /* LA87E-LA880: clear "not processed" */
    CK(0xA882);
    return xy(x, y);                                        /* LA882 */
}

/* KILENE ($A888): superzapper wipe-out - every CSUINT+1 frames the first
 * live invader explodes; none left ends the zap. */
xy6502 kilene(uint8_t x, uint8_t y)
{
    uint8_t a = SUZTIM;                                     /* LA888 */
    if (a < K_CSUSTA) return xy(x, y);                      /* LA88B-LA88D */
    if (a & K_CSUINT) return xy(x, y);                      /* LA88F-LA891: time for another? */
    y = WINVMX;                                             /* LA893 */
    do {
        if (R(A_INVAY + y) != 0) return exikil(x, y);       /* LA896-LA899: first live one */
        y--;                                                /* LA89B */
    } while (!(y & 0x80));                                  /* LA89C */
    SUZTIM = 0x00;                                          /* LA89E-LA8A0: all dead - deactivate */
    return xy(x, y);                                        /* LA8A3 */
}

/* EXIKIL ($A8A4): kill invader Y (no carrier split) with an explosion. */
xy6502 exikil(uint8_t x, uint8_t y)
{
    R(A_INVAC2 + y) = (uint8_t)(R(A_INVAC2 + y) & (uint8_t)~K_INVCAR);   /* LA8A4-LA8A9 */
    return incisq(x, y);                                    /* LA8AC JMP INCISQ */
}
