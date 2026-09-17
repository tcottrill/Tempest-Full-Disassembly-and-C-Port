/* aldis2.c - ALDIS2 ($B1B6-$C79F): display - the DISPLAY mainline, the vector
 * RAM sub-buffer management, the well (projection, spokes, rim, enemy lines),
 * invaders, shots, explosions, the big boom, nymphs, the star field, the
 * cursor and the Mathbox perspective utilities (WORSCR, CASCAL).
 *
 * One function per Atari routine header (60).  Entry labels reached only by
 * fall-through or a branch (ZATVG2, ZATVG1, NONYM, ZQATLI, M10, ZQPOKS, WELPIC,
 * UPCURN, ZQPONS, YVGVCT, WHITIP, NOLABS) are inlined or small static helpers
 * named after their label.  Aliases: DROUTEN = DENORM, INVPIE = FLIPIC,
 * PULS0E = WORSCR.
 *
 * Tables: every ROM table address comes from tools/gen_aldis2.py
 * (aldis2_data.h); the bytes are read with cpu_rd() over progrom/vecrom.
 * RTS dispatches (DSTATE/DROUTAD, INVPIC/INVPIT, SPECIAL/XSUBR) read the same
 * table and call the routine at that address.
 *
 * Mathbox: through hw_mb_write / hw_mb_status / hw_mb_ylow / hw_mb_yhigh in
 * the ROM's exact order (including every busy poll of MSTAT).
 *
 * Registers: A/X/Y inputs are parameters.  Exit registers are returned where a
 * caller uses them (CASCAL Y, CALMAG A, WHICHB A/X, LVLWEL A/X, CALOUT A,
 * SPOKE X, DELTA8 A, IJMPDS Y, SBCSWI X/Y -> DISPLAY's exit X/Y, INIDSP /
 * INICOL X/Y); tests/lockstep.c compares them.  The ALSCO2 display states
 * DSTATE dispatches to need no exit registers: ZATVG2 reloads A/Y and SBCSWI
 * sets X/Y before DISPLAY returns.  ADC/SBC go through adc6502/sbc6502
 * (g.dflag); where the ROM branches on N or V, adc_f/sbc_f give them exactly
 * as the shared core does.
 */
#include "state.h"
#include "hw.h"
#include "game.h"
#include "aldis2_data.h"

#define INDY_ADDR   ((uint16_t)(INDYLO | ((uint16_t)INDYHI << 8)))
#define SECUVG_ADDR ((uint16_t)(SECUVG | ((uint16_t)g.ram[A_SECUVG + 1] << 8)))
#define RUNGVG_ADDR ((uint16_t)(RUNGVG | ((uint16_t)g.ram[A_RUNGVG + 1] << 8)))
#define OLDL_ADDR   ((uint16_t)(OLDLLO | ((uint16_t)OLDLHI << 8)))
#define R(a)        g.ram[a]                    /* RAM cell by address */
#define ROM(a)      cpu_rd((uint16_t)(a))       /* ROM table byte */

/* Mathbox registers (offsets from $6080, tempest_defines.asm) */
#define MB_MAL   0x00
#define MB_MAH   0x01
#define MB_MBH   0x03
#define MB_MEL   0x04
#define MB_MEH   0x05
#define MB_MFL   0x06
#define MB_MFH   0x07
#define MB_MXH   0x09
#define MB_MNL   0x0C
#define MB_MZLL  0x0D
#define MB_MZLH  0x0E
#define MB_MZHL  0x0F
#define MB_MZHH  0x10
#define MB_MSZXD 0x14
#define MB_MXPL  0x15
#define MB_MXPH  0x16

static xy6502 xy(uint8_t x, uint8_t y) { xy6502 r; r.x = x; r.y = y; return r; }

static void vg_put(uint8_t y, uint8_t v) { cpu_wr((uint16_t)(VGLIST_ADDR + y), v); }   /* STA (VGLIST),Y */

static void mb_wait(void) { while (hw_mb_status() & 0x80) ; }                          /* BIT MSTAT / BMI */

/* ADC / SBC with the N, V, Z flags of the shared core (decimal ADC: N and V
 * from the half-corrected sum, Z binary; SBC: all flags binary) */
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

static uint8_t asl_c(uint8_t v, unsigned *c) { *c = (v >> 7) & 1u; return (uint8_t)(v << 1); }
static uint8_t lsr_c(uint8_t v, unsigned *c) { *c = v & 1u; return (uint8_t)(v >> 1); }
static uint8_t rol_c(uint8_t v, unsigned *c) { unsigned n = (v >> 7) & 1u; v = (uint8_t)((v << 1) | *c); *c = n; return v; }
static uint8_t ror_c(uint8_t v, unsigned *c) { unsigned n = v & 1u; v = (uint8_t)((v >> 1) | (*c << 7)); *c = n; return v; }

static void yvgvct(void);
static void welpic(void);
static void nolabs(uint8_t x, uint8_t y);

/* ======================================================================= */
/* DISPLAY MAINLINE AND SUB-BUFFERS                                        */
/* ======================================================================= */

/* DISPLAY ($B1B6): build the next display.  Returns the exit X/Y (they feed
 * MAINLN's next EXSTAT). */
xy6502 display(uint8_t x, uint8_t y)
{
    xy6502 r;
    inimat();                                               /* LB1B6 */
    if (VRAM(A_VECRAM) == ROM(ROM_JMPMAL + 4)) {            /* LB1B9-LB1BF: trying to halt? */
        uint8_t s = SPARE3;                                 /* LB1C1 */
        CK(0xB1C4);
        if (s == 0) return xy(x, y);                        /* LB1C4-LB1C6: not halted yet */
    }
    if (QDSTATE == K_CDPLAY)                                /* LB1C7-LB1CB */
        return denorm();                                    /* LB209 JMP DENORM */
    sbclog(K_BCINFO);                                       /* LB1CD-LB1CF: default info buffer */
    if (!bigtex()) {                                        /* LB1D2-LB1D5 */
        dstate(0x01);                                       /* LB1D7 (BIGTEX exits Y = 1) */
        /* ZATVG2 */
        if (SECUVY != 0) {                                  /* LB1DA-LB1DD: ATARI on screen? */
            unsigned c = 1;                                 /* LB1E3 SEC */
            uint8_t a = 0x0E;                               /* LB1E1 */
            int i;
            for (i = 0x27; i >= 0; i--)                     /* LB1DF, LB1E6-LB1E7 */
                a = sbc6502(a, cpu_rd((uint16_t)(SECUVG_ADDR + i)), &c, NULL);   /* LB1E4 */
            if (a != 0) {                                   /* LB1E9-LB1EA */
                a ^= 0xE5;                                  /* LB1EC */
                if (a != 0) a ^= 0x29;                      /* LB1EE-LB1F0 */
            }
            QT3 = a;                                        /* LB1F2 */
        }
    }
    r = sbcswi(K_BCINFO);                                   /* LB1F5-LB1F7 */
    VRAM(A_VECRAM) = ROM(ROM_JMPMAL + 2);                   /* LB1FA-LB1FD */
    VRAM(A_VECRAM + 1) = ROM(ROM_JMPMAH + 2);               /* LB200-LB203 */
    return r;                                               /* LB20C */
}

/* DSTATE ($B20D): execute display state QDSTATE (DROUTAD RTS dispatch). */
void dstate(uint8_t y)
{
    uint8_t x = QDSTATE;                                    /* LB20D */
    uint8_t lo = ROM(ALDIS2_DROUTAD + x);                   /* LB213 (A at the RTS) */
    uint16_t target = (uint16_t)((lo | (ROM(ALDIS2_DROUTAD + 1 + x) << 8)) + 1);   /* LB20F-LB217 */
    switch (target) {
    case ALDIS2_DENORM: (void)denorm(); break;
    case ROM_DSPSYS:    (void)dspsys(lo, x, y); break;      /* ALTES2 */
    case ALDIS2_DSBOOM: dsboom(); break;
    case ROM_GETDSP:    getdsp(); break;
    case ROM_RQRDSP:    rqrdsp(); break;
    case ROM_LDRDSP:    ldrdsp(); break;
    case ROM_DGOVER:    dgover(); break;
    case ROM_DPLPLA:    dplpla(); break;
    case ROM_DPRSTA:    dprsta(); break;
    case ROM_BOXPRO:    boxpro(); break;
    case ROM_LOGPRO:    logpro(); break;
    case ROM_D2GAME:    d2game(); break;
    default:            hw_soft_watchdog(); break;          /* odd / large QDSTATE: the ROM runs wild */
    }
}

/* DROUTEN = DENORM ($B230): game play display - every object sub-buffer,
 * the info, the well, enemy lines and stars.  Returns SBCSWI's exit X/Y. */
xy6502 denorm(void)
{
    xy6502 r;
    sbclog(K_BCCURS); dspcur(); (void)sbcswi(K_BCCURS);     /* LB230-LB23A: cursor */
    sbclog(K_BCSHOT); dspchg(); (void)sbcswi(K_BCSHOT);     /* LB23D-LB247: charges */
    sbclog(K_BCINVA); dspinv(); (void)sbcswi(K_BCINVA);     /* LB24A-LB254: invaders */
    sbclog(K_BCEXPL); dspexp(); (void)sbcswi(K_BCEXPL);     /* LB257-LB261: explosions */
    sbclog(K_BCNYMP); dspnym(); (void)sbcswi(K_BCNYMP);     /* LB264-LB26E: nymphs */
    sbclog(K_BCINFO); info();                               /* LB271-LB276: scores, messages */
    /* ZATVG1 */
    if (!(QSTATUS & 0x80)) {                                /* LB279-LB27B: attract? */
        unsigned c = 0;                                     /* LB27F CLC */
        uint8_t a = 0xF2;                                   /* LB27D */
        int i;
        for (i = 0x27; i >= 0; i--)                         /* LB280, LB284-LB285 */
            a = adc6502(a, cpu_rd((uint16_t)(SECUVG_ADDR + i)), &c, NULL);   /* LB282 */
        QT6 = a;                                            /* LB287: should be 0 */
    }
    (void)sbcswi(K_BCINFO);                                 /* LB28A-LB28C */
    dspwel();                                               /* LB28F */
    sbclog(K_BCENEL); dspenl(); (void)sbcswi(K_BCENEL);     /* LB292-LB29C: enemy lines */
    sbclog(K_BCSTAR); dstarf(); r = sbcswi(K_BCSTAR);       /* LB29F-LB2A9: star field */
    ROTDIS = 0x00;                                          /* LB2AC-LB2AE */
    VRAM(A_VECRAM) = ROM(ROM_JMPMAL);                       /* LB2B1-LB2B4: master pointer */
    VRAM(A_VECRAM + 1) = ROM(ROM_JMPMAH);                   /* LB2B7-LB2BA */
    return r;                                               /* LB2BD */
}

/* SBCLOG ($B2BE): VGLIST at the vacant buffer (A or B) of group A; VGY = 0. */
void sbclog(uint8_t a)
{
    uint8_t x = a, y = (uint8_t)(a << 1), lo, hi;           /* LB2BE-LB2C0 */
    if (R(A_BUFACT + x) == 0) {                             /* LB2C1-LB2C4: A active */
        lo = ROM(ROM_BUFBSL + y);                           /* LB2C6: build in B */
        hi = ROM(ROM_BUFBSH + y);                           /* LB2C9 */
    } else {
        lo = ROM(ROM_BUFASL + y);                           /* LB2CF: build in A */
        hi = ROM(ROM_BUFASH + y);                           /* LB2D2 */
    }
    VGLIST = lo;                                            /* LB2D5 */
    R(A_VGLIST + 1) = hi;                                   /* LB2D7 */
    VGY = 0x00;                                             /* LB2D9-LB2DB */
}

/* SBCACT ($B2DE): INDYLO at the active buffer of group A; VGY = 0. */
void sbcact(uint8_t a)
{
    uint8_t x = a, y = (uint8_t)(a << 1), lo, hi;           /* LB2DE-LB2E0 */
    if (R(A_BUFACT + x) == 0) {                             /* LB2E1-LB2E4 */
        lo = ROM(ROM_BUFASL + y);                           /* LB2E6: A is active */
        hi = ROM(ROM_BUFASH + y);                           /* LB2E9 */
    } else {
        lo = ROM(ROM_BUFBSL + y);                           /* LB2EF: B is active */
        hi = ROM(ROM_BUFBSH + y);                           /* LB2F2 */
    }
    INDYLO = lo;                                            /* LB2F5 */
    INDYHI = hi;                                            /* LB2F7 */
    VGY = 0x00;                                             /* LB2F9-LB2FB */
}

/* SBCSWI ($B2FE): RTSL at the end of the new buffer of group A, flip BUFACT,
 * point the switch at it.  Exits X = JMPL high byte, Y = 1. */
xy6502 sbcswi(uint8_t a)
{
    uint8_t x, y, v;
    vgrtsl();                                               /* LB2FE-LB302 PHA / JSR / PLA */
    x = a;                                                  /* LB303 */
    y = (uint8_t)(a << 1);                                  /* LB304-LB305 */
    INDYLO = ROM(ROM_BUFSWL + y);                           /* LB306-LB309 */
    INDYHI = ROM(ROM_BUFSWH + y);                           /* LB30B-LB30E */
    v = (uint8_t)(R(A_BUFACT + x) ^ 0x01);                  /* LB310-LB313 */
    R(A_BUFACT + x) = v;                                    /* LB315 */
    if (v == 0) {                                           /* LB318 */
        a = ROM(ROM_JMPALO + y);                            /* LB31A: buffer A */
        x = ROM(ROM_JMPAHI + y);                            /* LB31D */
    } else {
        a = ROM(ROM_JMPBLO + y);                            /* LB323: buffer B */
        x = ROM(ROM_JMPBHI + y);                            /* LB326 */
    }
    cpu_wr(INDY_ADDR, a);                                   /* LB329-LB32B */
    cpu_wr((uint16_t)(INDY_ADDR + 1), x);                   /* LB32D-LB32F */
    return xy(x, 0x01);                                     /* LB331 */
}

/* BIGTEX ($B332): large text buffer.  Returns C: 1 = first time (master
 * pointer set to messages only), 0 = VGLIST at the big area. */
int bigtex(void)
{
    uint8_t a = ROM(ROM_JMPMAL + 2), x;                     /* LB332 */
    if (a != VRAM(A_VECRAM)) {                              /* LB335-LB338: been here before? */
        VRAM(A_VECRAM) = a;                                 /* LB33A */
        return 1;                                           /* LB33D-LB33E SEC / RTS */
    }
    x = (BUFACT == 0) ? 0x02 : 0x08;                        /* LB33F-LB349: big area 1 / 2 */
    a = ROM(ROM_JMPALO + x);                                /* LB34B */
    SECUVY = 0x00;                                          /* LB34E-LB350 */
    vg_put(0x00, a);                                        /* LB353 */
    vg_put(0x01, ROM(ROM_JMPAHI + x));                      /* LB355-LB359: JMPL to the area */
    VGLIST = ROM(ROM_BUFASL + x);                           /* LB35B-LB35E */
    R(A_VGLIST + 1) = ROM(ROM_BUFASH + x);                  /* LB360-LB363 */
    return 0;                                               /* LB365-LB366 CLC / RTS */
}

/* ======================================================================= */
/* WELL, NYMPHS                                                            */
/* ======================================================================= */

/* DSPWEL ($B367): rebuild the well if asked, then the spoke colours (pulsars,
 * cursor flashlight, bonus flash, superzapper) and the top rungs. */
void dspwel(void)
{
    uint8_t a, x, y;
    unsigned c;
    if (ROTDIS != 0) {                                      /* LB367-LB36A: rebuild well? */
        sbclog(K_BCWELL);                                   /* LB36C-LB36E */
        bldwel();                                           /* LB371 */
        (void)sbcswi(K_BCWELL);                             /* LB374-LB376 */
    }
    sbcact(K_BCWELL);                                       /* LB379-LB37B */
    /* spoke pulse status */
    for (x = K_NLINES - 1; ; x--) {                         /* LB37E-LB386 */
        R(A_SPOKST + x) = 0x00;                             /* LB382 */
        if (x == 0) break;
    }
    if (!(CURMOD & 0x80)) {                                 /* LB388-LB38B: cursor at top? */
        x = WINVMX;                                         /* LB38D */
        do {
            if (R(A_INVAY + x) != 0) {                      /* LB390-LB393: active invader? */
                y = 0x00;                                   /* LB395 */
                if ((R(A_INVAC1 + x) & K_INVABI) == K_ZABPUL) {   /* LB397-LB39E: pulsar? */
                    y++;                                    /* LB3A0 */
                    TEMP0 = y;                              /* LB3A1: pulsar bit D0 */
                    if (!(R(A_INVAC1 + x) & K_INVMOT)) {    /* LB3A3-LB3A8: flipping? */
                        if (!(PULSON & 0x80) && R(A_INVAY + x) < PULPOT) {   /* LB3AA-LB3B5 */
                            TEMP0 = (uint8_t)(TEMP0 + 1);   /* LB3B7: pulse bit D1 */
                            TEMP0 = (uint8_t)(TEMP0 + 1);   /* LB3B9 */
                        }
                        y = R(A_INVAL2 + x);                /* LB3BB-LB3BD: CCW leg */
                        R(A_SPOKST + y) = (uint8_t)(TEMP0 | R(A_SPOKST + y));   /* LB3C0-LB3C3 */
                    }
                    y = R(A_INVAL1 + x);                    /* LB3C6 */
                    R(A_SPOKST + y) = (uint8_t)(TEMP0 | 0x80 | R(A_SPOKST + y));   /* LB3C9-LB3D0: base bit */
                }
            }
            x--;                                            /* LB3D3 */
        } while (!(x & 0x80));                              /* LB3D4 */
    }
    a = K_WELCOL;                                           /* LB3D6 */
    y = SUZTIM;                                             /* LB3D8 */
    if (y != 0 && !(y & 0x80)) {                            /* LB3DB-LB3DD: superzapper active? */
        a = (uint8_t)(QFRAME & 0x07);                       /* LB3DF-LB3E1 */
        if (a == 0x07) a = 0x01;                            /* LB3E3-LB3E7: no black */
    }
    TEMP0 = a;                                              /* LB3E9: default colour */
    y = 0xFF;                                               /* LB3EB */
    x = 0xFF;                                               /* LB3ED */
    TEMP3 = x;                                              /* LB3EF: no bonus flash */
    if (CURSY != 0 && !(CURSL2 & 0x80)) {                   /* LB3F1-LB3F9: cursor alive? */
        x = CURSL1;                                         /* LB3FB */
        y = CURSL2;                                         /* LB3FE */
    }
    TEMP1 = x;                                              /* LB401: flashlight spokes */
    TEMP2 = y;                                              /* LB403 */
    a = BOFLASH;                                            /* LB405 */
    if (!(a & 0x80)) {                                      /* LB408: bonus flash? */
        TEMP3 = (uint8_t)((a & 0x0E) >> 1);                 /* LB40A-LB40D: base colour */
        BOFLASH = (uint8_t)(BOFLASH - 1);                   /* LB40F */
    }
    for (x = K_NLINES - 1; ; x--) {                         /* LB412, LB451-LB452 */
        y = K_WELCOL;                                       /* LB414 */
        a = R(A_SPOKST + x);                                /* LB416 */
        if (a != 0) {                                       /* LB419: pulse? */
            if (a & 0x02) y = (uint8_t)(QFRAME & 0x01);     /* LB41B-LB423: pulsing */
        } else if (x == TEMP1 || x == TEMP2) {              /* LB427-LB42D: cursor flashlight? */
            y = K_CURCOL;                                   /* LB42F */
        } else if (!(BOFLASH & 0x80)) {                     /* LB434-LB437: bonus flash? */
            c = 0;                                          /* LB43A CLC */
            a = (uint8_t)(adc6502(x, TEMP3, &c, NULL) & 0x07);   /* LB439-LB43D */
            if (a == 0x07) a = 0x03;                        /* LB43F-LB443: no black */
            y = a;                                          /* LB445 */
        } else {
            y = TEMP0;                                      /* LB449: default colour */
        }
        cpu_wr((uint16_t)(INDY_ADDR + ROM(ALDIS2_STALOC + x)), y);   /* LB44B-LB44F */
        if (x == 0) break;
    }
    x = K_NLINES - 1;                                       /* LB454: top rungs */
    if (WELTYP & 0x80) x--;                                 /* LB456-LB45B: planar, 1 less */
    do {
        y = 0xC0;                                           /* LB45C: default on */
        if (R(A_SPOKST + x) & 0x80) y = 0x00;               /* LB45E-LB463: pulsar: off */
        PZL = y;                                            /* LB465 */
        y = ROM(ALDIS2_RUNLOC + x);                         /* LB467 */
        a = (uint8_t)((cpu_rd((uint16_t)(RUNGVG_ADDR + y)) & 0x1F) | PZL);   /* LB46A-LB46E */
        cpu_wr((uint16_t)(RUNGVG_ADDR + y), a);             /* LB470 */
        x--;                                                /* LB472 */
    } while (!(x & 0x80));                                  /* LB473 */
}

/* DSPNYM ($B498): the nymphs as dots with fake projection (at most $12). */
void dspnym(void)
{
    uint8_t a, x, y, s;
    unsigned c;
    y = K_NYMCOL;                                           /* LB498 */
    COLOR = y;                                              /* LB49A */
    vgstat(K_MZCOLO, y);                                    /* LB49C-LB49E */
    vgyab1(A_XADJL);                                        /* LB4A1-LB4A3: beam at vanishing point */
    PXL = 0x12;                                             /* LB4A6-LB4A8: max displayable */
    INDEX1 = K_NNYMPH - 1;                                  /* LB4AA-LB4AC */
    y = 0x00;                                               /* LB4AE */
    for (;;) {
        x = INDEX1;                                         /* LB4B0 */
        a = R(A_NYMPY + x);                                 /* LB4B2 */
        if (a != 0) {                                       /* LB4B5: nymph active? */
            c = (a >= 0x50);                                /* LB4BA CMP */
            if (c) INDEX1 = (uint8_t)(INDEX1 - 1);          /* LB4BC-LB4BE: skip every other */
            s = a;                                          /* LB4C0 PHA */
            vg_put(y, (uint8_t)(a & 0x3F));                 /* LB4C1-LB4C3: linear scale */
            a = rol_c(s, &c);                               /* LB4C5-LB4C6 PLA / ROL */
            a = rol_c(a, &c);                               /* LB4C7 */
            a = rol_c(a, &c);                               /* LB4C8 */
            c = 0;                                          /* LB4CB CLC */
            a = (uint8_t)(adc6502((uint8_t)(a & 0x03), 0x01, &c, NULL) | 0x70);   /* LB4C9-LB4CE */
            y++;                                            /* LB4D0 */
            vg_put(y, a);                                   /* LB4D1: binary scale */
            y++;                                            /* LB4D3 */
            x = R(A_NYMPL + x);                             /* LB4D4-LB4D7: nymph line */
            c = 1;                                          /* LB4DB SEC */
            a = sbc6502(R(A_LIFSZL + x), ZADJL, &c, NULL);  /* LB4D8-LB4DC */
            SZL = a;                                        /* LB4DE */
            vg_put(y, a);                                   /* LB4E0: Z LSB */
            y++;                                            /* LB4E2 */
            a = sbc6502(R(A_LIFSZH + x), R(A_ZADJL + 1), &c, NULL);   /* LB4E3-LB4E6 */
            SZH = a;                                        /* LB4E8 */
            vg_put(y, (uint8_t)(a & 0x1F));                 /* LB4EA-LB4EC: Z MSB */
            y++;                                            /* LB4EE */
            a = R(A_LIFSXL + x);                            /* LB4EF */
            SXL = a;                                        /* LB4F2 */
            vg_put(y, a);                                   /* LB4F4: X LSB */
            y++;                                            /* LB4F6 */
            a = R(A_LIFSXH + x);                            /* LB4F7 */
            SXH = a;                                        /* LB4FA */
            vg_put(y, (uint8_t)(a & 0x1F));                 /* LB4FC-LB4FE: X MSB */
            y++;                                            /* LB500: a dot */
            vg_put(y, 0x00);                                /* LB501-LB503 */
            y++;                                            /* LB505 */
            vg_put(y, 0x00);                                /* LB506 */
            y++;                                            /* LB508 */
            vg_put(y, 0x00);                                /* LB509 */
            y++;                                            /* LB50D */
            vg_put(y, 0xA0);                                /* LB50B-LB50E: brightness */
            y++;                                            /* LB510: back to the fake VP */
            c = 0;                                          /* LB515 CLC */
            a = adc6502((uint8_t)(SZL ^ 0xFF), 0x01, &c, NULL);   /* LB511-LB516 */
            vg_put(y, a);                                   /* LB518 */
            y++;                                            /* LB51A */
            a = (uint8_t)(adc6502((uint8_t)(SZH ^ 0xFF), 0x00, &c, NULL) & 0x1F);   /* LB51B-LB521 */
            vg_put(y, a);                                   /* LB523 */
            y++;                                            /* LB525 */
            c = 0;                                          /* LB52A CLC */
            a = adc6502((uint8_t)(SXL ^ 0xFF), 0x01, &c, NULL);   /* LB526-LB52B */
            vg_put(y, a);                                   /* LB52D */
            y++;                                            /* LB52F */
            a = (uint8_t)(adc6502((uint8_t)(SXH ^ 0xFF), 0x00, &c, NULL) & 0x1F);   /* LB530-LB536 */
            vg_put(y, a);                                   /* LB538 */
            y++;                                            /* LB53A */
            if (y >= 0xF0) {                                /* LB53B-LB53D: index maxing out? */
                y--;                                        /* LB53F */
                vgadd(y);                                   /* LB540 */
                y = 0x00;                                   /* LB543 */
            }
            PXL = (uint8_t)(PXL - 1);                       /* LB545 */
            if (PXL & 0x80) { excess(y); return; }          /* LB547: limit reached */
        }
        /* NONYM */
        INDEX1 = (uint8_t)(INDEX1 - 1);                     /* LB549 */
        if (INDEX1 & 0x80) { excess(y); return; }           /* LB54B */
    }                                                       /* LB54D JMP LB4B0 */
}

/* EXCESS ($B550): end of the nymph list (Y = bytes), ZQATLI protection
 * check, scale 1. */
void excess(uint8_t y)
{
    if (y != 0) {                                           /* LB550-LB551 */
        y--;                                                /* LB553 */
        vgadd(y);                                           /* LB554 */
    }
    /* ZQATLI */
    if (QT1 != 0 && WAVEN1 >= 0x0A)                         /* LB557-LB55F */
        FRTIMR = 0x7A;                                      /* LB561-LB563 */
    CK(0xB565);
    vgsca1(0x01);                                           /* LB565-LB567 JMP VGSCA1 */
}

/* VGDOT ($B56A): dot of intensity A at the beam; exits Y = 3. */
void vgdot(uint8_t a)
{
    unsigned c;
    uint8_t lo;
    vg_put(0x00, 0x00);                                     /* LB56A-LB56E PHA / LDY / TYA / STA */
    vg_put(0x01, 0x00);                                     /* LB570-LB571 */
    vg_put(0x02, 0x00);                                     /* LB573-LB574 */
    vg_put(0x03, a);                                        /* LB576-LB578 PLA / STA */
    c = 0;                                                  /* LB57C CLC */
    lo = adc6502(0x04, VGLIST, &c, NULL);                   /* LB57A-LB57D */
    VGLIST = lo;                                            /* LB57F */
    if (c) R(A_VGLIST + 1) = (uint8_t)(R(A_VGLIST + 1) + 1);   /* LB581-LB583 */
}

/* ======================================================================= */
/* CURSOR, INVADERS, CHARGES, EXPLOSIONS                                   */
/* ======================================================================= */

/* DSPCUR ($B586): the player's cursor (claw) between its lines. */
void dspcur(void)
{
    uint8_t a;
    unsigned c;
    COLOR = K_CURCOL;                                       /* LB586-LB588 */
    a = CURSY;                                              /* LB58A */
    if (a == 0 || a >= K_ILINDDY) return;                   /* LB58D-LB591: at bottom? */
    PYL = a;                                                /* LB593: depth */
    TEMPY = a;                                              /* LB595 */
    if (CURSL2 == 0x81) return;                             /* LB597-LB59C: blasted cursor */
    c = 0;                                                  /* LB5A6 CLC */
    a = adc6502((uint8_t)((CURSPO >> 1) & 0x07), K_CNCURS, &c, NULL);   /* LB5A1-LB5A7 */
    onelin(a, CURSL1);                                      /* LB59E, LB5A9 */
}

/* DSPINV ($B5AD): the invaders. */
void dspinv(void)
{
    uint8_t a, x;
    if (CURMOD & 0x80) return;                              /* LB5AD-LB5B0: cursor at top? */
    INDEX1 = K_NINVAD - 1;                                  /* LB5B2-LB5B4 */
    do {
        x = INDEX1;                                         /* LB5B6 */
        a = R(A_INVAY + x);                                 /* LB5B8 */
        if (a != 0) {                                       /* LB5BB: active? */
            PYL = a;                                        /* LB5BD */
            OBJIND = (uint8_t)((R(A_INVAC1 + x) & K_INVSEQ) >> 3);   /* LB5BF-LB5C7: animation */
            invpic((uint8_t)((R(A_INVAC1 + x) & K_INVABI) << 1), x);  /* LB5C9-LB5CF */
        }
        INDEX1 = (uint8_t)(INDEX1 - 1);                     /* LB5D2 */
    } while (!(INDEX1 & 0x80));                             /* LB5D4 */
}

/* INVPIC ($B5D7): picture routine A (= type * 2) of invader X (INVPIT RTS
 * dispatch). */
void invpic(uint8_t a, uint8_t x)
{
    uint8_t y = a;                                          /* LB5D7 */
    uint16_t target = (uint16_t)((ROM(ALDIS2_INVPIT + y) | (ROM(ALDIS2_INVPIT + 1 + y) << 8)) + 1);   /* LB5D8-LB5E0 */
    switch (target) {
    case ALDIS2_FLIPIC: flipic(x); break;
    case ALDIS2_PULPIC: pulpic(x); break;
    case ALDIS2_TANPIC: tanpic(x); break;
    case ALDIS2_TRAPIC: trapic(x); break;
    case ALDIS2_FUSPIC: fuspic(x); break;
    default:            hw_soft_watchdog(); break;          /* type > 4: past the table */
    }
}

/* INVPIE = FLIPIC ($B5EB): flipper X, on its lines or flipping. */
void flipic(uint8_t x)
{
    COLOR = K_FLICOL;                                       /* LB5EB-LB5ED */
    if (!(R(A_INVAC1 + x) & 0x80)) {                        /* LB5EF-LB5F2: flipping? */
        uint8_t y = R(A_INVAL1 + x);                        /* LB5F4: line # */
        onelin(ROM(ALDIS2_FLITAB + OBJIND), y);             /* LB5F7-LB5FC */
    } else {
        (void)ijmpds(x);                                    /* LB602 */
        oneln2(K_CINVA1);                                   /* LB605-LB607 */
    }
}

/* TANPIC ($B60F): tanker X (picture by cargo). */
void tanpic(uint8_t x)
{
    uint8_t y = (uint8_t)(R(A_INVAC2 + x) & K_INVCAR);      /* LB60F-LB614 */
    uint8_t a = ROM(ALDIS2_TANTAB + y);                     /* LB615 */
    scapic(a, R(A_INVAL1 + x));                             /* LB618-LB61B JMP SCAPIC */
}

/* TRAPIC ($B622): trailer (spiral) X, one of 4 pictures by frame. */
void trapic(uint8_t x)
{
    unsigned c = 0;                                         /* LB62A CLC */
    uint8_t y = R(A_INVAL1 + x);                            /* LB622 */
    uint8_t a = adc6502((uint8_t)((QFRAME & 0x03) << 1), K_PTSPI1, &c, NULL);   /* LB625-LB62B */
    scapic(a, y);                                           /* LB62D JMP SCAPIC */
}

/* IJMPDS ($B634): end points of jumping invader X (PXL/PZL base leg,
 * TEMPX/TEMPZ jump end), down scale for the jumper.  Exits Y = WELLID. */
uint8_t ijmpds(uint8_t x)
{
    uint8_t a, y;
    unsigned c;
    flags6502 f;
    TEMPY = PYL;                                            /* LB634-LB636: same Y */
    y = R(A_INVAL1 + x);                                    /* LB638 */
    PXL = R(A_LINEX + y);                                   /* LB63B-LB63E */
    PZL = R(A_LINEZ + y);                                   /* LB640-LB643 */
    y = (uint8_t)(R(A_INVAL2 + x) & 0x0F);                  /* LB645-LB64A */
    c = 0;                                                  /* LB64F CLC */
    a = adc_f((uint8_t)(PXL ^ 0x80), ROM(ALDIS2_JUMPX + y), &c, &f);   /* LB64B-LB650 */
    if (f.v) a = f.n ? 0x7F : 0x80;                         /* LB653-LB65C: overflow: min / max */
    TEMPX = (uint8_t)(a ^ 0x80);                            /* LB65E-LB660 */
    c = 0;                                                  /* LB666 CLC */
    a = adc_f((uint8_t)(PZL ^ 0x80), ROM(ALDIS2_JUMPZ + y), &c, &f);   /* LB662-LB667 */
    if (f.v) a = f.n ? 0x7F : 0x80;                         /* LB66A-LB673 */
    TEMPZ = (uint8_t)(a ^ 0x80);                            /* LB675-LB677 */
    y = WELLID;                                             /* LB679 */
    LINSCA = ROM(ALDIS2_WELLIS + y);                        /* LB67C-LB67F */
    BINSCA = ROM(ALDIS2_WELBIN + y);                        /* LB681-LB684 */
    return y;                                               /* LB686 */
}

/* FUSPIC ($B69B): fuse X (between lines when running up a rung). */
void fuspic(uint8_t x)
{
    uint8_t a, y;
    unsigned c;
    PYL = R(A_INVAY + x);                                   /* LB69B-LB69E */
    y = R(A_INVAL1 + x);                                    /* LB6A0 */
    PXL = R(A_LINEX + y);                                   /* LB6A3-LB6A6 */
    PZL = R(A_LINEZ + y);                                   /* LB6A8-LB6AB */
    a = R(A_INVAL2 + x);                                    /* LB6AD */
    if (a & 0x80) {                                         /* M10 LB6B0: runging? */
        c = 0;                                              /* LB6B3 CLC */
        y = (uint8_t)(adc6502(y, 0x01, &c, NULL) & 0x0F);   /* LB6B2-LB6B8 */
        c = 1;                                              /* LB6BC SEC */
        a = sbc6502(R(A_LINEX + y), PXL, &c, NULL);         /* LB6B9-LB6BD */
        a = delta8(a, x);                                   /* LB6BF */
        c = 0;                                              /* LB6C2 CLC */
        PXL = adc6502(a, PXL, &c, NULL);                    /* LB6C3-LB6C5 */
        c = 1;                                              /* LB6CA SEC */
        a = sbc6502(R(A_LINEZ + y), PZL, &c, NULL);         /* LB6C7-LB6CB */
        a = delta8(a, x);                                   /* LB6CD */
        c = 0;                                              /* LB6D0 CLC */
        PZL = adc6502(a, PZL, &c, NULL);                    /* LB6D1-LB6D3 */
    }
    worscr();                                               /* LB6D5 */
    vgyab1(A_SXL);                                          /* LB6D8-LB6DA: blank vector to fuse */
    VGY = 0x00;                                             /* LB6DD-LB6DF */
    y = cascal();                                           /* LB6E1: perspective scale */
    VGY = y;                                                /* LB6E4 */
    c = 0;                                                  /* LB6EB CLC */
    a = adc6502((uint8_t)((QFRAME & 0x03) << 1), K_PTFUSE, &c, NULL);   /* LB6E6-LB6EC */
    y = a;                                                  /* LB6EE */
    vgadd3(ROM(ROM_PICLO + y), ROM(ROM_PICHI + y), VGY);    /* LB6EF-LB6F7 JMP VGADD3 */
}

/* DELTA8 ($B6FA): A * (INVAL2,X & 7) / 8 (signed); X, Y preserved. */
uint8_t delta8(uint8_t a, uint8_t x)
{
    unsigned c, cs;
    int i;
    TEMP0 = a;                                              /* LB6FA */
    TEMP3 = (uint8_t)(R(A_INVAL2 + x) & 0x07);              /* LB6FC-LB701 */
    TEMP2 = x;                                              /* LB703 */
    a = 0x00;                                               /* LB707 */
    for (i = 2; i >= 0; i--) {                              /* LB705, LB715-LB716 */
        TEMP3 = lsr_c(TEMP3, &c);                           /* LB709 */
        if (c) {                                            /* LB70B */
            c = 0;                                          /* LB70D CLC */
            a = adc6502(a, TEMP0, &c, NULL);                /* LB70E */
        }
        a = asl_c(a, &c);                                   /* LB710 */
        cs = c;                                             /* LB711 PHP */
        a = ror_c(a, &c);                                   /* LB712 */
        c = cs;                                             /* LB713 PLP */
        a = ror_c(a, &c);                                   /* LB714: signed shift right */
    }
    return a;                                               /* LB718-LB71A LDX TEMP2 / RTS */
}

/* PULPIC ($B71B): pulsar X. */
void pulpic(uint8_t x)
{
    uint8_t a;
    unsigned c;
    a = K_TURQOI;                                           /* LB71B: pulse off */
    if (!(PULSON & 0x80)) a = K_WHITE;                      /* LB71D-LB722: pulse on */
    COLOR = a;                                              /* LB724 */
    c = 0;                                                  /* LB729 CLC */
    a = (uint8_t)(adc6502(PULSON, 0x40, &c, NULL) >> 4);    /* LB726-LB72F */
    if (a >= 0x05) a = 0x00;                                /* LB730-LB734 */
    TEMP0 = ROM(ALDIS2_PULTAB + a);                         /* LB736-LB73A */
    if (!(R(A_INVAC1 + x) & 0x80)) {                        /* LB73C-LB73F: flipping? */
        onelin(TEMP0, R(A_INVAL1 + x));                     /* LB741-LB746 */
    } else {
        (void)ijmpds(x);                                    /* LB74C */
        oneln2(TEMP0);                                      /* LB74F-LB751 */
    }
}

/* DSPCHG ($B75B): charges (player and enemy shots), player shot centre colour. */
void dspchg(void)
{
    uint8_t a, x, y;
    unsigned c;
    INDEX1 = K_NCHARG - 1;                                  /* LB75B-LB75D */
    do {
        x = INDEX1;                                         /* LB75F */
        a = R(A_CHARY + x);                                 /* LB761 */
        if (a != 0) {                                       /* LB764: active? */
            PYL = a;                                        /* LB766 */
            TEMPY = a;                                      /* LB768 */
            y = R(A_CHARL1 + x);                            /* LB76C */
            if (x < K_NPCHARG) {                            /* LB76A, LB76F */
                a = K_PTCURS;                               /* LB771: player shot */
            } else {
                c = 0;                                      /* LB77B CLC */
                a = adc6502((uint8_t)((QFRAME << 1) & 0x06), K_PTESHO, &c, NULL);   /* LB776-LB77C: enemy shot */
            }
            scapic(a, y);                                   /* LB77E */
        }
        INDEX1 = (uint8_t)(INDEX1 - 1);                     /* LB781 */
    } while (!(INDEX1 & 0x80));                             /* LB783 */
    y = K_ZYELLO;                                           /* LB785: plenty */
    a = CHACOU;                                             /* LB787 */
    if (a >= K_NPCHARG - 2) {                               /* LB78A-LB78C */
        y = K_ZBLUE;                                        /* LB78E: low */
        if (a >= K_NPCHARG) y = K_ZRED;                     /* LB790-LB794: out */
    }
    hw_color(K_PSHCTR, y);                                  /* LB796 STY COLPORT+PSHCTR */
}

/* DSPEXP ($B79A): explosions, then the ZQPOKS protection check. */
void dspexp(void)
{
    uint8_t a, x, y;
    unsigned c;
    y = K_EXPCOL;                                           /* LB79A */
    COLOR = y;                                              /* LB79C */
    INDEX1 = K_NEXPLO - 1;                                  /* LB79E-LB7A0 */
    do {
        x = INDEX1;                                         /* LB7A2 */
        a = R(A_EXPLOY + x);                                /* LB7A4 */
        if (a != 0) {                                       /* LB7A7: active bang? */
            PYL = a;                                        /* LB7A9 */
            TEMP0 = R(A_EXPLOL + x);                        /* LB7AB-LB7AE */
            y = R(A_EXPLOT + x);                            /* LB7B0 */
            if (y == 0x01) {                                /* LB7B3-LB7B5: charge-player? */
                chplki();                                   /* LB7B7 */
            } else {
                a = (uint8_t)((R(A_EXPLOS + x) >> 1) & 0xFE);   /* LB7BD-LB7C1 */
                if (y >= 0x02) a = 0x00;                    /* LB7C3-LB7C7: no sequence type */
                c = 0;                                      /* LB7C9 CLC */
                a = adc6502(a, ROM(ALDIS2_TEXTYP + y), &c, NULL);   /* LB7CA */
                scapic(a, TEMP0);                           /* LB7CD-LB7CF */
            }
        }
        INDEX1 = (uint8_t)(INDEX1 - 1);                     /* LB7D2 */
    } while (!(INDEX1 & 0x80));                             /* LB7D4 */
    /* ZQPOKS */
    if (QT4 != 0 && CURWAV >= 0x0D)                         /* LB7D6-LB7DF */
        R(0x01FF) = CURWAV;                                 /* LB7E1: kill top of stack */
}

/* CHPLKI ($B7EB): the charge-player special explosion (splat sequence). */
void chplki(void)
{
    uint8_t a, x, y;
    unsigned c;
    y = TEMP0;                                              /* LB7EB */
    PXL = R(A_LINEXM + y);                                  /* LB7ED-LB7F0: mid point */
    PZL = R(A_LINEZM + y);                                  /* LB7F2-LB7F5 */
    worscr();                                               /* LB7F7 */
    vgyab1(A_SXL);                                          /* LB7FA-LB7FC */
    x = SPXIND;                                             /* LB7FF */
    SPFTIM = (uint8_t)(SPFTIM - 1);                         /* LB802 */
    if (SPFTIM == 0) {                                      /* LB805: frame timer done? */
        x++;                                                /* LB807: next picture */
        SPXIND = x;                                         /* LB808 */
        SPFTIM = ROM(ALDIS2_TSPTIM + x);                    /* LB80B-LB80E */
    }
    y = ROM(ALDIS2_TSPCOD + x);                             /* LB811 */
    if (!(y & 0x80)) special(y);                            /* LB814-LB816 */
    c = 0;                                                  /* LB81D CLC */
    a = adc6502((uint8_t)(SPXIND << 1), K_PTSPLA, &c, NULL);   /* LB819-LB81E */
    y = a;                                                  /* LB820 */
    vgadd2(ROM(ROM_PICLO + y), ROM(ROM_PICHI + y));         /* LB821-LB827 JMP VGADD2 */
}

/* SPECIAL ($B84E): special explosion function Y (XSUBR RTS dispatch). */
void special(uint8_t y)
{
    uint16_t target = (uint16_t)((ROM(ALDIS2_XSUBR + y) | (ROM(ALDIS2_XSUBR + 1 + y) << 8)) + 1);   /* LB84E-LB856 */
    switch (target) {
    case ALDIS2_ALTCOL: altcol(); break;
    case ALDIS2_ROTCOL: rotcol(); break;
    case ALDIS2_SETSHR: setshr(); break;
    case ALDIS2_SHRSCA: shrsca(); break;
    default:            hw_soft_watchdog(); break;
    }
}

/* ALTCOL ($B85F): splat colours. */
void altcol(void)
{
    hw_color(K_PDIRED, K_ZRED);                             /* LB85F-LB861 */
    R(A_COLRAM + K_PDIRED) = K_ZRED;                        /* LB864 */
    hw_color(K_PDIYEL, K_ZYELLO);                           /* LB866-LB868 */
    R(A_COLRAM + K_PDIYEL) = K_ZYELLO;                      /* LB86B */
    R(A_COLRAM + K_PDIWHI) = K_ZWHITE;                      /* LB86D-LB86F */
    hw_color(K_PDIWHI, K_ZWHITE);                           /* LB871 */
}

/* ROTCOL ($B875): rotate the 3 player explosion colours. */
void rotcol(void)
{
    uint8_t a, y = R(A_COLRAM + K_PDIWHI);                  /* LB875 */
    int x;
    for (x = 2; x >= 0; x--) {                              /* LB877, LB884-LB885 */
        a = R(A_COLRAM + K_PDIWHI + x);                     /* LB879-LB87B */
        R(A_COLRAM + K_PDIWHI + x) = y;                     /* LB87C */
        hw_color((uint8_t)(K_PDIWHI + x), y);               /* LB87E-LB87F */
        y = a;                                              /* LB882-LB883 */
    }
}

/* SETSHR ($B888): restore colours, initial shrapnel scales. */
void setshr(void)
{
    (void)inicol();                                         /* LB888 */
    SPLINE = 0x7F;                                          /* LB88B-LB88D */
    SPBINA = 0x04;                                          /* LB890-LB892 */
}

/* SHRSCA ($B896): shrapnel scale into the SCALE sub-list, grow it. */
void shrsca(void)
{
    uint8_t a;
    unsigned c;
    flags6502 f;
    VRAM(A_SCALE) = SPLINE;                                 /* LB896-LB899: linear */
    VRAM(A_SCALE + 1) = (uint8_t)(SPBINA | 0x70);           /* LB89C-LB8A1: binary */
    VRAM(A_SCALE + 3) = 0xC0;                               /* LB8A4-LB8A6: RTSL */
    c = 1;                                                  /* LB8AC SEC */
    a = sbc_f(SPLINE, 0x20, &c, &f);                        /* LB8A9-LB8AD */
    if (f.n) {                                              /* LB8AF: linear overflow? */
        a &= 0x7F;                                          /* LB8B1 */
        SPBINA = (uint8_t)(SPBINA - 1);                     /* LB8B3 */
    }
    SPLINE = a;                                             /* LB8B6 */
}

/* DSBOOM ($B8BA): display state - the big boom (particles drawn into the
 * spare info buffer as a subroutine). */
void dsboom(void)
{
    uint8_t a, x, y;
    vgjsrl((uint8_t)((A_KILLER + 1) >> 8), (uint8_t)(A_KILLER & 0xFF));   /* LB8BA-LB8BE */
    CURNTX = 0x00;                                          /* LB8C1-LB8C3 */
    R(A_CURNTX + 1) = 0x00;                                 /* LB8C5 */
    CURNTY = 0x00;                                          /* LB8C7 */
    R(A_CURNTY + 1) = 0x00;                                 /* LB8C9 */
    CURSY = 0x00;                                           /* LB8CB */
    ZADJL = 0x00;                                           /* LB8CE */
    R(A_ZADJL + 1) = 0x00;                                  /* LB8D0 */
    EYL = 0xE0;                                             /* LB8D2-LB8D4 */
    EYH = 0xFF;                                             /* LB8D6-LB8D8 */
    a = whichb(&x);                                         /* LB8DA */
    R(A_SVGLIST + 1) = a;                                   /* LB8DD */
    SVGLIST = x;                                            /* LB8DF */
    INDEX1 = K_NPARTI - 1;                                  /* LB8E1-LB8E3 */
    do {
        x = INDEX1;                                         /* LB8E5 */
        a = R(A_PARTIY + x);                                /* LB8E7 */
        if (a != 0) {                                       /* LB8EA: active particle? */
            PYL = a;                                        /* LB8EC */
            PXL = R(A_PARTIX + x);                          /* LB8EE-LB8F1 */
            PZL = R(A_PARTIZ + x);                          /* LB8F3-LB8F6 */
            worscr();                                       /* LB8F8 */
            VGBRIT = 0x00;                                  /* LB8FB-LB8FD */
            (void)swapvg();                                 /* LB8FF */
            connec();                                       /* LB902 */
            vgdot(0xA0);                                    /* LB905-LB907 */
            (void)swapvg();                                 /* LB90A */
            vgyabs(A_SXL);                                  /* LB90D-LB90F */
            a = calmag(&y);                                 /* LB912 */
            vgscal(a, y);                                   /* LB915 */
            a = (uint8_t)(INDEX1 & 0x07);                   /* LB918-LB91A */
            if (a == 0x07) a = 0x00;                        /* LB91C-LB920 */
            y = a;                                          /* LB922 */
            COLOR = y;                                      /* LB923 */
            vgstat(K_MZCOLO, y);                            /* LB925-LB927 */
            vgsta1(K_MZBRIT);                               /* LB92A-LB92C */
            a = whichb(&x);                                 /* LB92F */
            vgjsrl(a, x);                                   /* LB932 */
        }
        INDEX1 = (uint8_t)(INDEX1 - 1);                     /* LB935 */
    } while (!(INDEX1 & 0x80));                             /* LB937 */
    (void)swapvg();                                         /* LB939 */
    vgsca1(0x01);                                           /* LB93C-LB93E: restore scale */
    vgrtsl();                                               /* LB941 */
    (void)swapvg();                                         /* falls into SWAPVG */
}

/* SWAPVG ($B944): exchange VGLIST and SVGLIST.  Exits X/Y = old VGLIST. */
xy6502 swapvg(void)
{
    uint8_t x = VGLIST, y = R(A_VGLIST + 1);                /* LB944-LB946 */
    VGLIST = SVGLIST;                                       /* LB948-LB94A */
    SVGLIST = x;                                            /* LB94C */
    R(A_VGLIST + 1) = R(A_SVGLIST + 1);                     /* LB94E-LB950 */
    R(A_SVGLIST + 1) = y;                                   /* LB952 */
    return xy(x, y);                                        /* LB954 */
}

/* CALMAG ($B955): magnification from PYL.  Returns A (binary scale), *y = 0
 * (linear).  (The shift loop leaves A = 0, so A is always 2.) */
uint8_t calmag(uint8_t *y)
{
    uint8_t a = (uint8_t)(PYL >> 4);                        /* LB955-LB95A */
    unsigned c;
    uint8_t n = 0x00;                                       /* LB95B */
    do {
        n++;                                                /* LB95D */
        a = (uint8_t)(a >> 1);                              /* LB95E */
    } while (a != 0);                                       /* LB95F */
    c = 0;                                                  /* LB961 CLC */
    a = adc6502(a, 0x02, &c, NULL);                         /* LB962 */
    *y = 0x00;                                              /* LB964 */
    return a;                                               /* LB966 */
}

/* WHICHB ($B967): start of the spare info sub-buffer: returns A = high,
 * *x = low (BFASTA if BUFACT+BCINFO is non-zero, else BFBSTA). */
uint8_t whichb(uint8_t *x)
{
    if (R(A_BUFACT + K_BCINFO) != 0) {                      /* LB967-LB96A */
        *x = ROM(ROM_BFASTA);                               /* LB96F */
        return ROM(ROM_BFASTA + 1);                         /* LB96C */
    }
    *x = ROM(ROM_BFBSTA);                                   /* LB978 */
    return ROM(ROM_BFBSTA + 1);                             /* LB975 */
}

/* ======================================================================= */
/* PICTURE UTILITIES                                                       */
/* ======================================================================= */

/* SCAPIC ($BCFD): picture A centred between line Y and its neighbour
 * (LINEXM/LINEZM), at depth PYL. */
void scapic(uint8_t a, uint8_t y)
{
    OBJIND = a;                                             /* LBCFD */
    PXL = R(A_LINEXM + y);                                  /* LBCFF-LBD02 */
    PZL = R(A_LINEZM + y);                                  /* LBD04-LBD07 */
    scapi2();                                               /* falls into SCAPI2 */
}

/* SCAPI2 ($BD09): picture OBJIND at PXL/PYL/PZL, scaled and dimmed by depth. */
void scapi2(void)
{
    uint8_t a, y;
    worscr();                                               /* LBD09 */
    vgyab1(A_SXL);                                          /* LBD0C-LBD0E */
    VGY = 0x00;                                             /* LBD11-LBD13 */
    y = cascal();                                           /* LBD15 */
    a = (uint8_t)((BFACTR ^ 0x07) << 1);                    /* LBD18-LBD1C */
    if (a < 0x0A) a = 0x0A;                                 /* LBD1D-LBD21 */
    a = (uint8_t)(a << 4);                                  /* LBD23-LBD26 */
    vg_put(y, a);                                           /* LBD27: brightness */
    y++;                                                    /* LBD29 */
    vg_put(y, 0x60);                                        /* LBD2A-LBD2C */
    y++;                                                    /* LBD2E */
    VGY = y;                                                /* LBD2F */
    y = OBJIND;                                             /* LBD31 */
    vgadd3(ROM(ROM_PICLO + y), ROM(ROM_PICHI + y), VGY);    /* LBD33-LBD3B JMP VGADD3 */
}

/* CASCAL ($BD3E): scale for depth PYL (Mathbox divide), SCAL at VGLIST+VGY.
 * Returns Y = next VG slot; BFACTR set. */
uint8_t cascal(void)
{
    uint8_t a, x, y;
    unsigned c;
    int z;
    a = PYL;                                                /* LBD3E */
    if (a >= 0x10) {                                        /* LBD40-LBD42 */
        c = 1;                                              /* LBD44 SEC */
        a = sbc6502(a, EYL, &c, NULL);                      /* LBD45 */
        hw_mb_write(MB_MXPL, a);                            /* LBD47 */
        a = sbc6502(0x00, EYH, &c, NULL);                   /* LBD4A-LBD4C */
        hw_mb_write(MB_MXPH, a);                            /* LBD4E: Y delta of the point */
        hw_mb_write(MB_MNL, 0x18);                          /* LBD51-LBD53: fractional quotient */
        a = YDEUNI;                                         /* LBD56 */
        hw_mb_write(MB_MZLH, a);                            /* LBD58: Y delta for scale 1 */
        hw_mb_write(MB_MSZXD, a);                           /* LBD5B: start divide */
        mb_wait();                                          /* LBD5E-LBD61 */
        SCFL = hw_mb_ylow();                                /* LBD63-LBD66 */
        a = hw_mb_yhigh();                                  /* LBD68 */
        R(A_SCFL + 1) = a;                                  /* LBD6B */
        x = 0x0F;                                           /* LBD6D */
        hw_mb_write(MB_MNL, x);                             /* LBD6F: restore quotient size */
        c = 1;                                              /* LBD72 SEC */
        a = sbc6502(a, 0x01, &c, &z);                       /* LBD73 */
        if (z) a = 0x01;                                    /* LBD75-LBD77 */
        x = 0x00;                                           /* LBD79 */
        do {
            x++;                                            /* LBD7B */
            SCFL = asl_c(SCFL, &c);                         /* LBD7C */
            a = rol_c(a, &c);                               /* LBD7E */
        } while (!c);                                       /* LBD7F */
        a = (uint8_t)((a >> 1) ^ 0x7F);                     /* LBD81-LBD82 */
        c = 0;                                              /* LBD84 CLC */
        a = adc6502(a, 0x01, &c, NULL);                     /* LBD85 */
        y = a;                                              /* LBD87 */
        a = x;                                              /* LBD88 */
    } else {
        a = 0x01;                                           /* LBD8C: max scale factor */
        y = 0x00;                                           /* LBD8E */
    }
    BFACTR = a;                                             /* LBD90 */
    vg_put(VGY, y);                                         /* LBD92-LBD96: linear factor */
    y = (uint8_t)(VGY + 1);                                 /* LBD94, LBD98 */
    vg_put(y, (uint8_t)(a | 0x70));                         /* LBD99-LBD9C: binary factor */
    y++;                                                    /* LBD9E */
    return y;                                               /* LBD9F */
}

/* ONELIN ($BDA0): line picture A on well line Y (to its CW neighbour). */
void onelin(uint8_t a, uint8_t y)
{
    uint8_t x;
    unsigned c;
    SAVEY = a;                                              /* LBDA0 */
    PXL = R(A_LINEX + y);                                   /* LBDA2-LBDA5 */
    PZL = R(A_LINEZ + y);                                   /* LBDA7-LBDAA */
    TEMPY = PYL;                                            /* LBDAC-LBDAE */
    c = 0;                                                  /* LBDB1 CLC */
    x = (uint8_t)(adc6502(y, 0x01, &c, NULL) & 0x0F);       /* LBDB0-LBDB6 */
    TEMPX = R(A_LINEX + x);                                 /* LBDB7-LBDBA */
    TEMPZ = R(A_LINEZ + x);                                 /* LBDBC-LBDBF */
    LINSCA = 0x00;                                          /* LBDC1-LBDC3: 1/16 scale */
    BINSCA = 0x04;                                          /* LBDC5-LBDC7 */
    oneln2(SAVEY);                                          /* LBDC9, falls into ONELN2 */
}

/* the unit multiples 1..7 of X1L into X2..X7 (or of Z1L), ONELN2 LBE63-LBED5 */
static void multiples(uint16_t l1, uint16_t h2)
{
    /* l1 = address of X1L (Z1L); XnL = l1 + n - 1, XnH = h2 + n - 2 */
    uint8_t a, t;
    unsigned c;
    a = asl_c(R(l1), &c);                                   /* LBE63-LBE65 */
    R(h2) = rol_c(R(h2), &c);                               /* LBE66: X2H */
    R(l1 + 1) = a;                                          /* LBE68: X2L */
    a = asl_c(a, &c);                                       /* LBE6A */
    R(l1 + 3) = a;                                          /* LBE6B: X4L */
    t = rol_c(R(h2), &c);                                   /* LBE6D-LBE6F */
    R(h2 + 2) = t;                                          /* LBE70: X4H */
    a = adc6502(R(l1 + 3), R(l1), &c, NULL);                /* LBE72-LBE74 (no CLC) */
    R(l1 + 4) = a;                                          /* LBE76: X5L */
    a = adc6502(R(h2 + 2), 0x00, &c, NULL);                 /* LBE78-LBE7A */
    R(h2 + 3) = a;                                          /* LBE7C: X5H */
    a = adc6502(R(l1 + 1), R(l1), &c, NULL);                /* LBE7E-LBE80 */
    R(l1 + 2) = a;                                          /* LBE82: X3L */
    a = adc6502(R(h2), 0x00, &c, NULL);                     /* LBE84-LBE86 */
    R(h2 + 1) = a;                                          /* LBE88: X3H */
    R(h2 + 4) = a;                                          /* LBE8A: X6H */
    a = asl_c(R(l1 + 2), &c);                               /* LBE8C-LBE8E */
    R(l1 + 5) = a;                                          /* LBE8F: X6L */
    R(h2 + 4) = rol_c(R(h2 + 4), &c);                       /* LBE91 */
    a = adc6502(a, R(l1), &c, NULL);                        /* LBE93 */
    R(l1 + 6) = a;                                          /* LBE95: X7L */
    a = adc6502(R(h2 + 4), 0x00, &c, NULL);                 /* LBE97-LBE99 */
    R(h2 + 5) = a;                                          /* LBE9B: X7H */
}

/* clamp a signed screen delta (lo in *l, high byte a with SBC flags f) to
 * its absolute value in one byte, ONELN2 LBE10-LBE31 (X) / LBE3E-LBE5B (Z) */
static void unit_abs(uint16_t l, uint8_t a, const flags6502 *f, int is_x)
{
    unsigned c;
    if (!f->n) {                                            /* LBE12 / LBE40 */
        if (!f->z) R(l) = 0xFF;                             /* LBE14-LBE18 / LBE42-LBE46: max out */
        return;
    }
    if (a != 0xFF) {                                        /* LBE1D-LBE1F / LBE4B-LBE4D */
        a = 0xFF;                                           /* LBE21 / LBE4F */
    } else {
        c = 0;                                              /* LBE2A / LBE58 CLC */
        a = adc6502((uint8_t)(R(l) ^ 0xFF), 0x01, &c, NULL);   /* LBE26-LBE2B / LBE54-LBE59 */
        if (is_x && c) a = 0xFF;                            /* LBE2D-LBE2F (X only) */
    }
    R(l) = a;                                               /* LBE31 / LBE5B */
}

/* ONELN2 ($BDCB): line picture Y from (PXL,PYL,PZL) to (TEMPX,TEMPY,TEMPZ),
 * scale LINSCA/BINSCA, colour COLOR. */
void oneln2(uint8_t y)
{
    uint8_t a, x;
    unsigned c;
    flags6502 f;
    if (!(EYH & 0x80) && PYL < EYL) return;                 /* LBDCB-LBDD5: behind the eye */
    SUBCOU = ROM(ALDIS2_PCOUNT + y);                        /* LBDD6-LBDD9 */
    INDEX2 = ROM(ALDIS2_PINDEX + y);                        /* LBDDB-LBDDE */
    vgstat(K_MZCOLO, COLOR);                                /* LBDE0-LBDE4 */
    worscr();                                               /* LBDE7: 1st point */
    vgyab1(A_SXL);                                          /* LBDEA-LBDEC */
    PXL = TEMPX;                                            /* LBDEF-LBDF1 */
    PYL = TEMPY;                                            /* LBDF3-LBDF5 */
    PZL = TEMPZ;                                            /* LBDF7-LBDF9 */
    worscr();                                               /* LBDFB: 2nd point */
    vgscal(BINSCA, LINSCA);                                 /* LBDFE-LBE02 */
    c = 1;                                                  /* LBE07 SEC */
    X1L = sbc6502(SXL, CURNTX, &c, NULL);                   /* LBE05-LBE0A */
    a = sbc_f(SXH, R(A_CURNTX + 1), &c, &f);                /* LBE0C-LBE0E */
    UNITXH = a;                                             /* LBE10 */
    unit_abs(A_X1L, a, &f, 1);
    c = 1;                                                  /* LBE35 SEC */
    Z1L = sbc6502(SZL, CURNTY, &c, NULL);                   /* LBE33-LBE38 */
    a = sbc_f(SZH, R(A_CURNTY + 1), &c, &f);                /* LBE3A-LBE3C */
    UNITZH = a;                                             /* LBE3E */
    unit_abs(A_Z1L, a, &f, 0);
    X2H = 0x00;                                             /* LBE5D-LBE5F */
    Z2H = 0x00;                                             /* LBE61 */
    multiples(A_X1L, A_X2H);                                /* LBE63-LBE9B */
    multiples(A_Z1L, A_Z2H);                                /* LBE9D-LBED5 */
    y = 0x00;                                               /* LBED7 */
    VGY = y;                                                /* LBED9 */
    do {
        y = INDEX2;                                         /* LBEDB */
        a = ROM(ALDIS2_VBASE + 1 + y);                      /* LBEDD */
        if (a == 0x01) a = K_RATS;                          /* LBEE0-LBEE4: depth intensity */
        VGBRIT = a;                                         /* LBEE6 */
        a = ROM(ALDIS2_VBASE + y);                          /* LBEE8 */
        TEMP4 = a;                                          /* LBEEB: sign of the perp multiplier */
        y = (uint8_t)(y + 2);                               /* LBEED-LBEEE */
        INDEX2 = y;                                         /* LBEEF */
        x = a;                                              /* LBEF1 */
        y = (uint8_t)(a & 0x07);                            /* LBEF2-LBEF4: |unit| */
        a = (uint8_t)(x << 1);                              /* LBEF5-LBEF6 */
        TEMP2 = a;                                          /* LBEF7: sign of the unit multiplier */
        x = (uint8_t)((a >> 4) & 0x07);                     /* LBEF9-LBEFF: |perp| */
        if (!((TEMP2 ^ UNITXH) & 0x80)) {                   /* LBF00-LBF04 */
            SXL = R(A_X0L + y);                             /* LBF06-LBF09 */
            a = R(A_X0H + y);                               /* LBF0B */
        } else {
            c = 0;                                          /* LBF16 CLC */
            SXL = adc6502((uint8_t)(R(A_X0L + y) ^ 0xFF), 0x01, &c, NULL);   /* LBF11-LBF19 */
            a = adc6502((uint8_t)(R(A_X0H + y) ^ 0xFF), 0x00, &c, NULL);     /* LBF1B-LBF20 */
        }
        SXH = a;                                            /* LBF22 */
        if ((TEMP4 ^ UNITZH) & 0x80) {                      /* LBF24-LBF28 */
            c = 0;                                          /* LBF2C CLC */
            SXL = adc6502(R(A_Z0L + x), SXL, &c, NULL);     /* LBF2A-LBF2F */
            a = adc6502(R(A_Z0H + x), SXH, &c, NULL);       /* LBF31-LBF33 */
        } else {
            c = 1;                                          /* LBF3A SEC */
            SXL = sbc6502(SXL, R(A_Z0L + x), &c, NULL);     /* LBF38-LBF3D */
            a = sbc6502(SXH, R(A_Z0H + x), &c, NULL);       /* LBF3F-LBF41 */
        }
        SXH = a;                                            /* LBF43 */
        if (!((TEMP2 ^ UNITZH) & 0x80)) {                   /* LBF45-LBF49: Z vector */
            SZL = R(A_Z0L + y);                             /* LBF4B-LBF4E */
            a = R(A_Z0H + y);                               /* LBF50 */
        } else {
            c = 0;                                          /* LBF5B CLC */
            SZL = adc6502((uint8_t)(R(A_Z0L + y) ^ 0xFF), 0x01, &c, NULL);   /* LBF56-LBF5E */
            a = adc6502((uint8_t)(R(A_Z0H + y) ^ 0xFF), 0x00, &c, NULL);     /* LBF60-LBF65 */
        }
        SZH = a;                                            /* LBF67 */
        if ((TEMP4 ^ UNITXH) & 0x80) {                      /* LBF69-LBF6D */
            c = 1;                                          /* LBF71 SEC */
            SZL = sbc6502(SZL, R(A_X0L + x), &c, NULL);     /* LBF6F-LBF74 */
            a = sbc6502(SZH, R(A_X0H + x), &c, NULL);       /* LBF76-LBF78 */
        } else {
            c = 0;                                          /* LBF7F CLC */
            SZL = adc6502(SZL, R(A_X0L + x), &c, NULL);     /* LBF7D-LBF82 */
            a = adc6502(SZH, R(A_X0H + x), &c, NULL);       /* LBF84-LBF86 */
        }
        SZH = a;                                            /* LBF88 */
        y = VGY;                                            /* LBF8A */
        vg_put(y, SZL);                                     /* LBF8C-LBF8E: Z LSB */
        y++;                                                /* LBF90 */
        vg_put(y, (uint8_t)(SZH & 0x1F));                   /* LBF91-LBF95: Z MSB */
        y++;                                                /* LBF97 */
        vg_put(y, SXL);                                     /* LBF98-LBF9A: X LSB */
        y++;                                                /* LBF9C */
        vg_put(y, (uint8_t)((SXH & 0x1F) | VGBRIT));        /* LBF9D-LBFA3: X MSB, intensity */
        y++;                                                /* LBFA5 */
        VGY = y;                                            /* LBFA6 */
        SUBCOU = (uint8_t)(SUBCOU - 1);                     /* LBFA8 */
    } while (SUBCOU != 0);                                  /* LBFAA-LBFAC JMP LBEDB */
    y = VGY;                                                /* LBFAF */
    y--;                                                    /* LBFB1 */
    vgadd(y);                                               /* LBFB2 JMP VGADD */
}

/* PULS0E = WORSCR ($C098): project (PXL,PYL,PZL) through the eye (EXL,EYL,
 * EZL) with two Mathbox divides into SXL/SXH, SZL/SZH (+ XADJL/ZADJL,
 * saturated). */
void worscr(void)
{
    uint8_t a, x;
    unsigned c;
    flags6502 f;
    c = 1;                                                  /* LC09A SEC */
    a = sbc6502(PYL, EYL, &c, NULL);                        /* LC098-LC09B */
    hw_mb_write(MB_MXPL, a);                                /* LC09D */
    a = sbc_f(0x00, EYH, &c, &f);                           /* LC0A0-LC0A2 */
    hw_mb_write(MB_MXPH, a);                                /* LC0A4 */
    if (f.n) {                                              /* LC0A7: point behind the eye? */
        hw_mb_write(MB_MXPH, 0x00);                         /* LC0A9-LC0AB: put it at the eye */
        hw_mb_write(MB_MXPL, 0x01);                         /* LC0AE-LC0B0 */
    }
    a = PZL;                                                /* LC0B3 */
    if (a >= EZL) {                                         /* LC0B5-LC0B7 */
        c = 1;
        a = sbc6502(a, EZL, &c, NULL);                      /* LC0B9 */
        x = 0x00;                                           /* LC0BB */
    } else {
        c = 1;                                              /* LC0C2 SEC */
        a = sbc6502(EZL, PZL, &c, NULL);                    /* LC0C0-LC0C3 */
        x = 0xFF;                                           /* LC0C5 */
    }
    hw_mb_write(MB_MZLH, a);                                /* LC0C7 */
    hw_mb_write(MB_MSZXD, a);                               /* LC0CA: divide Z */
    R(A_MTEMP + 2) = x;                                     /* LC0CD */
    a = PXL;                                                /* LC0CF */
    if (a >= EXL) {                                         /* LC0D1-LC0D3 */
        c = 1;
        a = sbc6502(a, EXL, &c, NULL);                      /* LC0D5 */
        x = 0x00;                                           /* LC0D7 */
    } else {
        c = 1;                                              /* LC0DE SEC */
        a = sbc6502(EXL, PXL, &c, NULL);                    /* LC0DC-LC0DF */
        x = 0xFF;                                           /* LC0E1 */
    }
    R(A_MTEMP + 1) = a;                                     /* LC0E3 */
    R(A_MTEMP + 3) = x;                                     /* LC0E5 */
    mb_wait();                                              /* LC0E7-LC0EA */
    SZL = hw_mb_ylow();                                     /* LC0EC-LC0EF */
    SZH = hw_mb_yhigh();                                    /* LC0F1-LC0F4 */
    a = R(A_MTEMP + 1);                                     /* LC0F6 */
    hw_mb_write(MB_MZLH, a);                                /* LC0F8 */
    hw_mb_write(MB_MSZXD, a);                               /* LC0FB: divide X */
    if (!(R(A_MTEMP + 2) & 0x80)) {                         /* LC0FE-LC100 */
        c = 0;                                              /* LC104 CLC */
        SZL = adc6502(SZL, ZADJL, &c, NULL);                /* LC102-LC107 */
        a = adc_f(SZH, R(A_ZADJL + 1), &c, &f);             /* LC109-LC10B */
        if (f.v) {                                          /* LC10D */
            SZL = 0xFF;                                     /* LC10F-LC111 */
            a = 0x7F;                                       /* LC113 */
        }
        SZH = a;                                            /* LC115 */
    } else {
        c = 1;                                              /* LC11C SEC */
        SZL = sbc6502(ZADJL, SZL, &c, NULL);                /* LC11A-LC11F */
        a = sbc_f(R(A_ZADJL + 1), SZH, &c, &f);             /* LC121-LC123 */
        if (f.v) {                                          /* LC125 */
            SZL = 0x00;                                     /* LC127-LC129 */
            a = 0x80;                                       /* LC12B */
        }
        SZH = a;                                            /* LC12D */
    }
    mb_wait();                                              /* LC12F-LC132 */
    SXL = hw_mb_ylow();                                     /* LC134-LC137 */
    SXH = hw_mb_yhigh();                                    /* LC139-LC13C */
    x = R(A_MTEMP + 3);                                     /* LC13E */
    if (!(x & 0x80)) {                                      /* LC140 */
        c = 0;                                              /* LC144 CLC */
        SXL = adc6502(SXL, XADJL, &c, NULL);                /* LC142-LC147 */
        a = adc_f(SXH, R(A_XADJL + 1), &c, &f);             /* LC149-LC14B */
        if (f.v) {                                          /* LC14D */
            SXL = 0xFF;                                     /* LC14F-LC151 */
            a = 0x7F;                                       /* LC153 */
        }
        SXH = a;                                            /* LC155 */
        return;                                             /* LC157 */
    }
    c = 1;                                                  /* LC15A SEC */
    SXL = sbc6502(XADJL, SXL, &c, NULL);                    /* LC158-LC15D */
    a = sbc_f(R(A_XADJL + 1), SXH, &c, &f);                 /* LC15F-LC161 */
    if (f.v) {                                              /* LC163 */
        SXL = 0x00;                                         /* LC165-LC167 */
        a = 0x80;                                           /* LC169 */
    }
    SXH = a;                                                /* LC16B */
}

/* ======================================================================= */
/* INITIALISATION, WELL                                                    */
/* ======================================================================= */

/* INIDSP ($C16E): initialise the display (score template, eye, well, VG halt
 * request, colours).  Entry X/Y are not used; exits X/Y of INICOL. */
xy6502 inidsp(uint8_t x, uint8_t y)
{
    uint8_t a;
    (void)x; (void)y;
    initem();                                               /* LC16E */
    EXL = 0x80;                                             /* LC171-LC173: eye centred */
    ROTDIS = 0xFF;                                          /* LC175-LC177 */
    iniwls();                                               /* LC17A */
    a = SPARE3;                                             /* LC17D */
    CK(0xC180);
    if (a == 0) hw_vgstop();                                /* LC180-LC182: VG halted as requested? */
    SPARE3 = 0x00;                                          /* LC185-LC187 */
    CK(0xC18A);
    VRAM(A_VECRAM) = ROM(ROM_JMPMAL + 4);                   /* LC18A-LC18D: request halt */
    VRAM(A_VECRAM + 1) = ROM(ROM_JMPMAH + 4);               /* LC190-LC193 */
    return inicol();                                        /* falls into INICOL */
}

/* INICOL ($C196): colour RAM from COLTAB for the wave (both halves of each
 * byte).  Exits X = table index - 8, Y = $FF. */
xy6502 inicol(void)
{
    uint8_t a, x;
    int y;
    a = (uint8_t)(CURWAV & 0x70);                           /* LC196-LC198 */
    if (a >= 0x5F) a = 0x5F;                                /* LC19A-LC19E */
    x = (uint8_t)((a >> 1) | 0x07);                         /* LC1A0-LC1A3 */
    for (y = 7; y >= 0; y--) {                              /* LC1A4, LC1BF-LC1C0 */
        a = (uint8_t)(ROM(ALDIS2_COLTAB + x) & 0x0F);       /* LC1A6-LC1A9 */
        R(A_COLRAM + y) = a;                                /* LC1AB */
        hw_color((uint8_t)y, a);                            /* LC1AE */
        a = (uint8_t)(ROM(ALDIS2_COLTAB + x) >> 4);         /* LC1B1-LC1B7 */
        R(A_COLRAM + 8 + y) = a;                            /* LC1B8 */
        hw_color((uint8_t)(8 + y), a);                      /* LC1BB */
        x--;                                                /* LC1BE */
    }
    return xy(x, 0xFF);                                     /* LC1C2 */
}

/* INIMAT ($C1C3): zero ONELIN's X0/X1/Z0/Z1 and the unused Mathbox
 * registers, quotient size 15. */
void inimat(void)
{
    X1H = 0x00;                                             /* LC1C3-LC1C5 */
    Z1H = 0x00;                                             /* LC1C7 */
    X0H = 0x00;                                             /* LC1C9 */
    X0L = 0x00;                                             /* LC1CB */
    Z0H = 0x00;                                             /* LC1CD */
    Z0L = 0x00;                                             /* LC1CF */
    hw_mb_write(MB_MAL, 0x00);                              /* LC1D1-LC1D3 */
    hw_mb_write(MB_MAH, 0x00);                              /* LC1D6 */
    hw_mb_write(MB_MEL, 0x00);                              /* LC1D9 */
    hw_mb_write(MB_MEH, 0x00);                              /* LC1DC */
    hw_mb_write(MB_MFL, 0x00);                              /* LC1DF */
    hw_mb_write(MB_MFH, 0x00);                              /* LC1E2 */
    hw_mb_write(MB_MXH, 0x00);                              /* LC1E5 */
    hw_mb_write(MB_MBH, 0x00);                              /* LC1E8 */
    hw_mb_write(MB_MZLL, 0x00);                             /* LC1EB */
    hw_mb_write(MB_MZLH, 0x00);                             /* LC1EE */
    hw_mb_write(MB_MZHL, 0x00);                             /* LC1F1 */
    hw_mb_write(MB_MZHH, 0x00);                             /* LC1F4 */
    hw_mb_write(MB_MNL, 0x0F);                              /* LC1F7-LC1F9 */
}

/* INIWLS ($C235): initialise the well of the player's wave: eye, vanishing
 * point (at once for a new life, else ZADEST steps), line coordinates and
 * angles, mid points. */
void iniwls(void)
{
    uint8_t a, x, y, idx;
    unsigned c;
    int i;
    x = PLAYUP;                                             /* LC235 */
    idx = lvlwel(R(A_WAVEN1 + x), &x);                      /* LC237-LC239, LC23C PHA */
    y = WELLID;                                             /* LC23D */
    c = 0;                                                  /* LC245 CLC */
    a = adc6502((uint8_t)(ROM(ALDIS2_HOLEYL + y) ^ 0xFF), 0x01, &c, NULL);   /* LC240-LC246 */
    EYL = a;                                                /* LC248 */
    EYLDES = a;                                             /* LC24A */
    c = 1;                                                  /* LC24E SEC */
    YDEUNI = sbc6502(0x10, EYL, &c, NULL);                  /* LC24C-LC251: delta for unit scale */
    EYH = 0xFF;                                             /* LC253-LC255 */
    EZL = ROM(ALDIS2_HOLEZL + y);                           /* LC257-LC25A */
    WELTYP = ROM(ALDIS2_HOLRAP + y);                        /* LC25C-LC25F */
    if (QNXTSTA == K_CNWLF2) {                              /* LC262-LC266 */
        ZADJL = ROM(ALDIS2_HOLZAD + y);                     /* LC268-LC26B: at centre (new life) */
        R(A_ZADJL + 1) = ROM(ALDIS2_HOLZDH + y);            /* LC26D-LC270 */
    } else {
        c = 1;                                              /* LC278 SEC */
        ZADEST = sbc6502(ROM(ALDIS2_HOLZAD + y), ZADJL, &c, NULL);   /* LC275-LC27B: move up slowly */
        a = sbc6502(ROM(ALDIS2_HOLZDH + y), R(A_ZADJL + 1), &c, NULL);   /* LC27E-LC281 */
        for (i = 3; i >= 0; i--) {                          /* LC284, LC28A-LC28B */
            a = lsr_c(a, &c);                               /* LC286 */
            ZADEST = ror_c(ZADEST, &c);                     /* LC287 */
        }
    }
    XADJL = 0x00;                                           /* LC28D-LC28F: X screen centre */
    R(A_XADJL + 1) = 0x00;                                  /* LC291 */
    LEVELY = 0x00;                                          /* LC293-LC295: top & bottom on screen */
    R(A_LEVELY + 1) = 0x00;                                 /* LC298 */
    ROTFLG = (uint8_t)((A_VECRAM + 0xC00) >> 8);            /* LC29B-LC29D: subroutine buffer PC */
    y = idx;                                                /* LC2A0-LC2A1 PLA / TAY */
    for (x = K_NLINES - 1; ; x--) {                         /* LC2A2, LC2C2-LC2C3 */
        R(A_LINEX + x) = ROM(ALDIS2_NEWLIX + y);            /* LC2A4-LC2A7 */
        R(A_LINEZ + x) = ROM(ALDIS2_NEWLIZ + y);            /* LC2AA-LC2AD */
        R(A_LINSXH + x) = 0x00;                             /* LC2B0-LC2B2 */
        R(A_LINSZH + x) = 0x00;                             /* LC2B5 */
        R(A_LINSTA + x) = 0x00;                             /* LC2B8 */
        R(A_LINANG + x) = ROM(ALDIS2_ILINANG + y);          /* LC2BB-LC2BE */
        y--;                                                /* LC2C1 */
        if (x == 0) break;
    }
    y = 0x00;                                               /* LC2C5: mid points */
    x = 0x0F;                                               /* LC2C7 */
    for (;;) {
        c = 1;                                              /* LC2CC SEC */
        a = adc6502(R(A_LINEX + y), R(A_LINEX + x), &c, NULL);   /* LC2C9-LC2CD */
        R(A_LINEXM + x) = ror_c(a, &c);                     /* LC2D0-LC2D1 */
        c = 1;                                              /* LC2D7 SEC */
        a = adc6502(R(A_LINEZ + y), R(A_LINEZ + x), &c, NULL);   /* LC2D4-LC2D8 */
        R(A_LINEZM + x) = ror_c(a, &c);                     /* LC2DB-LC2DC */
        y--;                                                /* LC2DF */
        if (y & 0x80) y = 0x0F;                             /* LC2E0-LC2E2 */
        x--;                                                /* LC2E4 */
        if (x & 0x80) break;                                /* LC2E5 */
    }
}

/* LVLWEL ($C2E8): well for level A (random past 98): returns A = sequence
 * index (WELLID * 16 + 15), *x = cycle; Y = WELSEQ index; WELLID set. */
uint8_t lvlwel(uint8_t a, uint8_t *x)
{
    uint8_t n = (uint8_t)(ALDIS2_WELSEN - ALDIS2_WELSEQ), cyc = 0x00;   /* LC2E8 */
    unsigned c;
    if (a >= 0x62) a = (uint8_t)(hw_random(0) & 0x5F);      /* LC2EA-LC2F1 */
    while (a >= n) {                                        /* LC2F3-LC2F5, LC2FB-LC2FD */
        cyc++;                                              /* LC2F7 */
        c = 1;                                              /* LC2F8 SEC */
        a = sbc6502(a, n, &c, NULL);                        /* LC2F9 */
    }
    WELLID = ROM(ALDIS2_WELSEQ + a);                        /* LC2FF-LC303 */
    *x = cyc;
    return (uint8_t)((WELLID << 4) | 0x0F);                 /* LC306-LC30C */
}

/* BLDWEL ($C30D): project the well's far and near outlines (if they were on
 * screen), then build the well picture (WELPIC). */
void bldwel(void)
{
    uint8_t a;
    if (R(A_LEVELY + 1) == 0) {                             /* LC30D-LC310: bottom on screen last time? */
        PYL = K_ILINDDY;                                    /* LC312-LC314 */
        a = calout(K_ILINDDY, 0x4F);                        /* LC316-LC318 */
        R(A_LEVELY + 1) = a;                                /* LC31B: off screen flag */
        if (a != 0) LEVELY = a;                             /* LC31E-LC320: then so is top */
        if (LEVELY == 0) {                                  /* LC323-LC326 */
            PYL = K_ILINLIY;                                /* LC328-LC32A: top of well */
            chkdep();                                       /* LC32C */
            a = calout(PYL, 0x0F);                          /* LC32F-LC333 */
            LEVELY = a;                                     /* LC336 */
        }
    }
    welpic();
}

/* WELPIC ($C339): spokes and rims of the well. */
static void welpic(void)
{
    uint8_t x, y;
    vgsca1(0x01);                                           /* LC339-LC33B */
    y = K_WELCOL;                                           /* LC33E */
    COLOR = y;                                              /* LC340 */
    if (R(A_LEVELY + 1) != 0) return;                       /* LC342-LC347: off screen */
    if (ROTFLG == 0) return;                                /* LC348-LC34D: well off */
    x = K_NLINES - 1;                                       /* LC34E */
    do {
        x = spoke(K_RATS, x);                               /* LC350-LC352 */
        x--;                                                /* LC355 */
    } while (!(x & 0x80));                                  /* LC356 */
    y = K_WELCOL;                                           /* LC358: rim */
    COLOR = y;                                              /* LC35A */
    vgstat(K_MZCOLO, y);                                    /* LC35C-LC35E */
    outlin(R(A_LEVELY + 1), 0x4F);                          /* LC361-LC366 */
    outlin(LEVELY, 0x0F);                                   /* LC369-LC36B, falls into OUTLIN */
}

/* OUTLIN ($C36E): rim through the 16 near (Y = $0F) or far (Y = $4F) points
 * if A (off-screen flag) is 0; RUNGVG = the rim's list start. */
void outlin(uint8_t a, uint8_t y)
{
    uint8_t x;
    unsigned c;
    if (a != 0) return;                                     /* LC36E: on screen? */
    INDEX1 = y;                                             /* LC370 */
    SXL = R(A_LINSXL + y);                                  /* LC372-LC375 */
    SXH = R(A_LINSXH + y);                                  /* LC377-LC37A */
    SZL = R(A_LINSZL + y);                                  /* LC37C-LC37F */
    SZH = R(A_LINSZH + y);                                  /* LC381-LC384 */
    vgyabs(A_SXL);                                          /* LC386-LC388 */
    RUNGVG = VGLIST;                                        /* LC38B-LC38D: for rung changes */
    R(A_RUNGVG + 1) = R(A_VGLIST + 1);                      /* LC38F-LC391 */
    x = K_NLINES - 1;                                       /* LC393 */
    if (WELTYP != 0) x--;                                   /* LC395-LC39A: planar: beam off for 1st line */
    VGBRIT = K_RATS;                                        /* LC39B-LC39D */
    INDEX2 = x;                                             /* LC39F */
    do {
        INDEX1 = (uint8_t)(INDEX1 - 1);                     /* LC3A1 */
        if ((INDEX1 & 0x0F) == 0x0F) {                      /* LC3A3-LC3A9: index wrapping? */
            c = 0;                                          /* LC3AD CLC */
            INDEX1 = adc6502(INDEX1, 0x10, &c, NULL);       /* LC3AB-LC3B0 */
        }
        lintos();                                           /* LC3B2 */
        INDEX2 = (uint8_t)(INDEX2 - 1);                     /* LC3B5 */
    } while (!(INDEX2 & 0x80));                             /* LC3B7 */
}

/* CONNEC ($C3BA): vector from the current point to SX/SZ, which becomes the
 * current point (UPCURN $C3D6); beam on. */
void connec(void)
{
    unsigned c;
    c = 1;                                                  /* LC3BC SEC */
    XCOMP = sbc6502(SXL, CURNTX, &c, NULL);                 /* LC3BA-LC3BF */
    R(A_XCOMP + 1) = sbc6502(SXH, R(A_CURNTX + 1), &c, NULL);   /* LC3C1-LC3C5 */
    c = 1;                                                  /* LC3C9 SEC */
    YCOMP = sbc6502(SZL, CURNTY, &c, NULL);                 /* LC3C7-LC3CC */
    R(A_YCOMP + 1) = sbc6502(SZH, R(A_CURNTY + 1), &c, NULL);   /* LC3CE-LC3D2 */
    /* UPCURN */
    vgvctr(A_XCOMP);                                        /* LC3D4-LC3D6 */
    CURNTX = SXL;                                           /* LC3D9-LC3DB */
    R(A_CURNTX + 1) = SXH;                                  /* LC3DD-LC3DF */
    CURNTY = SZL;                                           /* LC3E1-LC3E3 */
    R(A_CURNTY + 1) = SZH;                                  /* LC3E5-LC3E7 */
    VGBRIT = K_RATS;                                        /* LC3E9-LC3EB */
}

/* SPOKE ($C3EE): spokes X and X-1 with the rung between (intensity A).
 * Returns X = entry X - 1. */
uint8_t spoke(uint8_t a, uint8_t x)
{
    uint8_t s = a;                                          /* LC3F0 PHA */
    INDEX1 = x;                                             /* LC3EE */
    vgstat(K_MZCOLO, COLOR);                                /* LC3F1-LC3F5 */
    liftos();                                               /* LC3F8: far point */
    vgyabs(A_SXL);                                          /* LC3FB-LC3FD */
    VGBRIT = s;                                             /* LC400-LC403 PLA / STA / PHA */
    lintos();                                               /* LC404: far to near */
    INDEX1 = (uint8_t)(INDEX1 - 1);                         /* LC407 */
    VGBRIT = 0x00;                                          /* LC409-LC40D */
    vgstat(K_MZCOLO, COLOR);                                /* LC40F-LC411 */
    lintos();                                               /* LC414: near to adjacent near */
    VGBRIT = s;                                             /* LC417-LC418 */
    liftos();                                               /* LC41A */
    connec();                                               /* LC41D: to far point */
    return INDEX1;                                          /* LC420-LC422 */
}

/* LINTOS ($C423): near point INDEX1 into SX/SZ, then CONNEC. */
void lintos(void)
{
    uint8_t x = INDEX1;                                     /* LC423 */
    SXL = R(A_LINSXL + x);                                  /* LC425-LC428 */
    SXH = R(A_LINSXH + x);                                  /* LC42A-LC42D */
    SZL = R(A_LINSZL + x);                                  /* LC42F-LC432 */
    SZH = R(A_LINSZH + x);                                  /* LC434-LC437 */
    connec();                                               /* LC439 JMP CONNEC */
}

/* LIFTOS ($C43C): far point INDEX1 into SX/SZ. */
void liftos(void)
{
    uint8_t x = INDEX1;                                     /* LC43C */
    SXL = R(A_LIFSXL + x);                                  /* LC43E-LC441 */
    SXH = R(A_LIFSXH + x);                                  /* LC443-LC446 */
    SZL = R(A_LIFSZL + x);                                  /* LC448-LC44B */
    SZH = R(A_LIFSZH + x);                                  /* LC44D-LC450 */
}

/* CHKDEP ($C453): nudge PYL away from an eye that is too close. */
void chkdep(void)
{
    uint8_t a;
    unsigned c;
    if (EYH != 0) return;                                   /* LC453-LC455 */
    c = 1;                                                  /* LC459 SEC */
    a = sbc6502(PYL, EYL, &c, NULL);                        /* LC457-LC45A */
    if (c) c = (a >= 0x0C);                                 /* LC45C-LC45E */
    if (c) return;                                          /* LC460: eye not too close */
    c = 0;                                                  /* LC464 CLC */
    a = adc6502(EYL, 0x0F, &c, NULL);                       /* LC462-LC465 */
    if (!c) c = (a >= 0xF0);                                /* LC467-LC469 */
    if (c) a = 0xF0;                                        /* LC46B-LC46D: not past end of well */
    PYL = a;                                                /* LC46F */
}

/* CALOUT ($C473): project the 16 outline points at depth A into the near
 * (X = $0F) or far (X = $4F) screen arrays, clipped; returns A = LINSCA
 * (0 = all on screen). */
uint8_t calout(uint8_t a, uint8_t x)
{
    uint8_t y;
    PYL = a;                                                /* LC473 */
    INDEX2 = x;                                             /* LC475 */
    LINSCA = 0x00;                                          /* LC477-LC479 */
    INDEX1 = 0x0F;                                          /* LC47B-LC47D */
    do {
        x = INDEX1;                                         /* LC47F */
        PXL = R(A_LINEX + x);                               /* LC481-LC484 */
        PZL = R(A_LINEZ + x);                               /* LC486-LC489 */
        worscr();                                           /* LC48B */
        x = INDEX2;                                         /* LC48E */
        y = SXL;                                            /* LC490 */
        a = SXH;                                            /* LC492 */
        if (!(a & 0x80)) {                                  /* LC494: X off screen? */
            if (a >= 0x04) {                                /* LC496-LC498 */
                y = 0xFF;                                   /* LC49A */
                a = 0x03;                                   /* LC49C */
                LINSCA = (uint8_t)(LINSCA + 1);             /* LC49E */
            }
        } else if (a < 0xFC) {                              /* LC4A3-LC4A5 */
            y = 0x01;                                       /* LC4A7 */
            a = 0xFC;                                       /* LC4A9 */
            LINSCA = (uint8_t)(LINSCA + 1);                 /* LC4AB */
        }
        R(A_LINSXH + x) = a;                                /* LC4AD */
        R(A_LINSXL + x) = y;                                /* LC4B0-LC4B1 */
        y = SZL;                                            /* LC4B4 */
        a = SZH;                                            /* LC4B6 */
        if (!(a & 0x80)) {                                  /* LC4B8: Z off screen? */
            if (a >= 0x04) {                                /* LC4BA-LC4BC */
                y = 0xFF;                                   /* LC4BE */
                a = 0x03;                                   /* LC4C0 */
                LINSCA = (uint8_t)(LINSCA + 1);             /* LC4C2 */
            }
        } else if (a < 0xFC) {                              /* LC4C7-LC4C9 */
            a = 0xFC;                                       /* LC4CB */
            y = 0x01;                                       /* LC4CD */
            LINSCA = (uint8_t)(LINSCA + 1);                 /* LC4CF */
        }
        R(A_LINSZH + x) = a;                                /* LC4D1 */
        R(A_LINSZL + x) = y;                                /* LC4D4-LC4D5 */
        INDEX2 = (uint8_t)(INDEX2 - 1);                     /* LC4D8 */
        INDEX1 = (uint8_t)(INDEX1 - 1);                     /* LC4DA */
    } while (!(INDEX1 & 0x80));                             /* LC4DC */
    return LINSCA;                                          /* LC4DE-LC4E0 */
}

/* DSPHOL ($C4E1): small well shape of level A (rating display). */
void dsphol(uint8_t a)
{
    uint8_t x, y, d;
    unsigned c;
    a = lvlwel(a, &x);                                      /* LC4E1 */
    SAVEY = a;                                              /* LC4E4: well index */
    SAVEX = x;                                              /* LC4E6: cycle */
    VGBRIT = 0x00;                                          /* LC4E8-LC4EA */
    vgsca1(0x05);                                           /* LC4EC-LC4EE: really small */
    x = (uint8_t)(SAVEX & 0x07);                            /* LC4F1-LC4F5 */
    y = ROM(ALDIS2_SPWECO + x);                             /* LC4F6: colour for the cycle */
    COLOR = y;                                              /* LC4F9 */
    vgstat(K_MZCOLO, y);                                    /* LC4FB-LC4FD */
    x = WELLID;                                             /* LC500 */
    a = SAVEY;                                              /* LC503 */
    if (ROM(ALDIS2_HOLRAP + x) == 0) {                      /* LC505-LC508: planar? */
        c = 1;                                              /* LC50A SEC */
        a = sbc6502(a, 0x0F, &c, NULL);                     /* LC50B: closed: start at the first point */
    }
    y = a;                                                  /* LC50D */
    a = ROM(ALDIS2_NEWLIZ + y);                             /* LC50E */
    PYL = a;                                                /* LC511 */
    x = (uint8_t)(a ^ 0x80);                                /* LC513-LC515 */
    a = ROM(ALDIS2_NEWLIX + y);                             /* LC516 */
    PXL = a;                                                /* LC519 */
    vgvtr1((uint8_t)(a ^ 0x80), x);                         /* LC51B-LC51D: beam at 1st point */
    VGBRIT = 0xC0;                                          /* LC520-LC522 */
    INDEX2 = K_NLINES - 1;                                  /* LC524-LC526 */
    do {
        y = SAVEY;                                          /* LC528 */
        a = ROM(ALDIS2_NEWLIX + y);                         /* LC52A */
        x = a;                                              /* LC52D */
        c = 1;                                              /* LC52E SEC */
        d = sbc6502(a, PXL, &c, NULL);                      /* LC52F-LC531: delta X (PHA) */
        PXL = x;                                            /* LC532 */
        a = ROM(ALDIS2_NEWLIZ + y);                         /* LC534 */
        y = a;                                              /* LC537 */
        c = 1;                                              /* LC538 SEC */
        x = sbc6502(a, PYL, &c, NULL);                      /* LC539-LC53B: delta Z */
        PYL = y;                                            /* LC53C */
        vgvtr1(d, x);                                       /* LC53E-LC53F */
        SAVEY = (uint8_t)(SAVEY - 1);                       /* LC542 */
        INDEX2 = (uint8_t)(INDEX2 - 1);                     /* LC544 */
    } while (!(INDEX2 & 0x80));                             /* LC546 */
    vgsca1(0x01);                                           /* LC548-LC54A JMP VGSCA1 */
}

/* DSTARF ($C54D): star field planes (eye moved temporarily), then the
 * ZQPONS protection check. */
void dstarf(void)
{
    uint8_t a, x, y, s_eyl, s_eyh, s_ydeuni;
    unsigned c;
    if (PLAGRO != 0) {                                      /* LC54D-LC550 */
        s_eyl = EYL;                                        /* LC552-LC554 */
        s_eyh = EYH;                                        /* LC555-LC557 */
        s_ydeuni = YDEUNI;                                  /* LC558-LC55A */
        EYL = 0xE8;                                         /* LC55B-LC55D */
        EYH = 0xFF;                                         /* LC55F-LC561 */
        YDEUNI = 0x28;                                      /* LC563-LC565 */
        INDEX1 = K_NPLANE - 1;                              /* LC567-LC569 */
        do {
            x = INDEX1;                                     /* LC56B */
            a = R(A_PLANEY + x);                            /* LC56D */
            if (a != 0) {                                   /* LC570: active plane? */
                PYL = a;                                    /* LC572 */
                PXL = 0x80;                                 /* LC574-LC576: centre of world */
                PZL = 0x80;                                 /* LC578-LC57A */
                if (CURWAV < 0x05) {                        /* LC57C-LC580 */
                    a = K_BLUE;                             /* LC582: blue stars in waves 1-4 */
                } else {
                    a = (uint8_t)(x & 0x07);                /* LC587-LC588 */
                    if (a == 0x07) a = 0x04;                /* LC58A-LC58E */
                }
                COLOR = a;                                  /* LC590 */
                y = a;                                      /* LC592 */
                vgstat(K_MZCOLO, y);                        /* LC593-LC595 */
                c = 0;                                      /* LC59C ASL: C = 0 (A <= 3) */
                a = adc6502((uint8_t)((INDEX1 & 0x03) << 1), K_PTSTR1, &c, NULL);   /* LC598-LC59D */
                OBJIND = a;                                 /* LC59F */
                scapi2();                                   /* LC5A1 */
            }
            INDEX1 = (uint8_t)(INDEX1 - 1);                 /* LC5A4 */
        } while (!(INDEX1 & 0x80));                         /* LC5A6 */
        YDEUNI = s_ydeuni;                                  /* LC5A8-LC5A9 */
        EYH = s_eyh;                                        /* LC5AB-LC5AC */
        EYL = s_eyl;                                        /* LC5AE-LC5AF */
    }
    /* ZQPONS */
    if (QT5 != 0 && LSCORH >= 0x15) {                       /* LC5B1-LC5BA: POKEY check failed, score >= 150000 */
        x = LSCORL;                                         /* LC5BC */
        R(0x0200 + x) = (uint8_t)(R(0x0200 + x) + 1);       /* LC5BE */
    }
}

/* DSPENL ($C5C2): enemy lines (spikes): per line the fixed codes, then
 * either a copy of last frame's far point / vectors or new ones. */
void dspenl(void)
{
    uint8_t a, x, y, s_lo, s_hi;
    int i;
    if (R(A_LEVELY + 1) != 0) return;                       /* LC5C2-LC5C7: well on? */
    if (EYH == 0 && EYL >= 0xF0) return;                    /* LC5C8-LC5D2: eye past end */
    vgsca1(0x01);                                           /* LC5D3-LC5D5 */
    s_lo = VGLIST;                                          /* LC5D8-LC5DA: save for next time */
    s_hi = R(A_VGLIST + 1);                                 /* LC5DB-LC5DD */
    INDEX2 = 0x00;                                          /* LC5DE-LC5E0 */
    VGY = 0x00;                                             /* LC5E2 */
    x = K_NLINES - 1;                                       /* LC5E4 */
    if (WELTYP != 0) x--;                                   /* LC5E6-LC5EB: planar: 1 less */
    INDEX1 = x;                                             /* LC5EC */
    do {
        y = VGY;                                            /* LC5EE-LC5F0 */
        for (i = 3; i >= 0; i--) {                          /* LC5F7-LC5F9 */
            vg_put(y, ROM(ALDIS2_ENLFIX + i));              /* LC5F2-LC5F5 */
            y++;
        }
        VGY = y;                                            /* LC5FB */
        if (ROTDIS == 0) {                                  /* LC5FD-LC600: redo well? */
            x = INDEX2;                                     /* LC602 */
            if (!(R(A_LINSTA + x) & 0x80)) {                /* LC604-LC607: action at near point? */
                y = VGY;                                    /* LC60B */
                for (i = 0x0B; i >= 0; i--) {               /* LC609, LC612-LC613 */
                    vg_put(y, cpu_rd((uint16_t)(OLDL_ADDR + y)));   /* LC60D-LC60F */
                    y++;                                    /* LC611 */
                }
                VGY = y;                                    /* LC615 */
            } else {
                y = VGY;                                    /* LC61A: copy the far point */
                a = cpu_rd((uint16_t)(OLDL_ADDR + y));      /* LC61C */
                vg_put(y, a);                               /* LC61E */
                CURNTY = a;                                 /* LC620 */
                y++;                                        /* LC622 */
                a = cpu_rd((uint16_t)(OLDL_ADDR + y));      /* LC623 */
                vg_put(y, a);                               /* LC625 */
                if (a >= 0x10) a |= 0xE0;                   /* LC627-LC62B: sign extend */
                R(A_CURNTY + 1) = a;                        /* LC62D */
                y++;                                        /* LC62F */
                a = cpu_rd((uint16_t)(OLDL_ADDR + y));      /* LC630 */
                vg_put(y, a);                               /* LC632 */
                CURNTX = a;                                 /* LC634 */
                y++;                                        /* LC636 */
                a = cpu_rd((uint16_t)(OLDL_ADDR + y));      /* LC637 */
                vg_put(y, a);                               /* LC639 */
                if (a >= 0x10) a |= 0xE0;                   /* LC63B-LC63F */
                R(A_CURNTX + 1) = a;                        /* LC641 */
                y++;                                        /* LC643 */
                VGY = y;                                    /* LC644 */
                tipact();                                   /* LC646 */
            }
        } else {
            fixstu();                                       /* LC64C */
            tipact();                                       /* LC64F */
        }
        x = INDEX2;                                         /* LC652 */
        R(A_LINSTA + x) = (uint8_t)(R(A_LINSTA + x) << 1);  /* LC654: clear line status */
        INDEX2 = (uint8_t)(INDEX2 + 1);                     /* LC657 */
        INDEX1 = (uint8_t)(INDEX1 - 1);                     /* LC659 */
    } while (!(INDEX1 & 0x80));                             /* LC65B */
    OLDLHI = s_hi;                                          /* LC65D-LC65E */
    OLDLLO = s_lo;                                          /* LC660-LC661 */
    y = VGY;                                                /* LC663 */
    y--;                                                    /* LC665 */
    vgadd(y);                                               /* LC666 JMP VGADD */
}

/* FIXSTU ($C66D): far point of enemy line INDEX2 = mean of the far points of
 * the line and its neighbour, then a blank vector there (YVGVCT). */
void fixstu(void)
{
    uint8_t a, x, y;
    unsigned c;
    a = INDEX2;                                             /* LC66D */
    x = a;                                                  /* LC66F */
    c = 0;                                                  /* LC670 CLC */
    y = (uint8_t)(adc6502(a, 0x01, &c, NULL) & 0x0F);       /* LC671-LC675 */
    c = 1;                                                  /* LC679 SEC: round */
    SXL = adc6502(R(A_LIFSXL + x), R(A_LIFSXL + y), &c, NULL);   /* LC676-LC67D */
    a = adc6502(R(A_LIFSXH + x), R(A_LIFSXH + y), &c, NULL);     /* LC67F-LC682 */
    SXH = a;                                                /* LC685 */
    (void)asl_c(a, &c);                                     /* LC687 */
    SXH = ror_c(SXH, &c);                                   /* LC688 */
    SXL = ror_c(SXL, &c);                                   /* LC68A */
    c = 1;                                                  /* LC68F SEC */
    SZL = adc6502(R(A_LIFSZL + x), R(A_LIFSZL + y), &c, NULL);   /* LC68C-LC693 */
    a = adc6502(R(A_LIFSZH + x), R(A_LIFSZH + y), &c, NULL);     /* LC695-LC698 */
    SZH = a;                                                /* LC69B */
    (void)asl_c(a, &c);                                     /* LC69D */
    SZH = ror_c(SZH, &c);                                   /* LC69E */
    SZL = ror_c(SZL, &c);                                   /* LC6A0 */
    yvgvct();                                               /* falls into YVGVCT */
}

/* YVGVCT ($C6A2): blank vector SZ/SX at VGLIST+VGY, current point = SX/SZ. */
static void yvgvct(void)
{
    uint8_t y = VGY;                                        /* LC6A2 */
    vg_put(y, SZL);                                         /* LC6A4-LC6A6 */
    y++;                                                    /* LC6A8 */
    CURNTY = SZL;                                           /* LC6A9 */
    R(A_CURNTY + 1) = SZH;                                  /* LC6AB-LC6AD */
    vg_put(y, (uint8_t)(SZH & 0x1F));                       /* LC6AF-LC6B1 */
    y++;                                                    /* LC6B3 */
    vg_put(y, SXL);                                         /* LC6B4-LC6B6 */
    y++;                                                    /* LC6B8 */
    CURNTX = SXL;                                           /* LC6B9 */
    R(A_CURNTX + 1) = SXH;                                  /* LC6BB-LC6BD */
    vg_put(y, (uint8_t)(SXH & 0x1F));                       /* LC6BF-LC6C1 */
    y++;                                                    /* LC6C3 */
    VGY = y;                                                /* LC6C4 */
}

/* TIPACT ($C6C7): tip of enemy line INDEX2: 4 x SCAL 1,0 if inactive, else a
 * vector to the near point and a shatter picture or a white dot. */
void tipact(void)
{
    uint8_t a, x, y;
    unsigned c;
    int i;
    x = INDEX2;                                             /* LC6C7 */
    a = R(A_LINEY + x);                                     /* LC6C9 */
    if (a == 0) {                                           /* LC6CC: line active? */
        y = VGY;                                            /* LC6CE */
        for (i = 3; i >= 0; i--) {                          /* LC6D0, LC6DC-LC6DD */
            vg_put(y, 0x00);                                /* LC6D2-LC6D4: SCAL 1,0 (no-op) */
            y++;                                            /* LC6D6 */
            vg_put(y, 0x71);                                /* LC6D7-LC6D9 */
            y++;                                            /* LC6DB */
        }
        VGY = y;                                            /* LC6DF */
        return;
    }
    PYL = a;                                                /* LC6E4 */
    chkdep();                                               /* LC6E6 */
    PXL = R(A_LINEXM + x);                                  /* LC6E9-LC6EC: mid point */
    PZL = R(A_LINEZM + x);                                  /* LC6EE-LC6F1 */
    worscr();                                               /* LC6F3 */
    fconnec();                                              /* LC6F6: vector to near point */
    x = INDEX2;                                             /* LC6F9 */
    if (R(A_LINSTA + x) & 0x40) {                           /* LC6FB-LC700: shattered? */
        y = cascal();                                       /* LC702 */
        c = 0;                                              /* LC70A CLC */
        a = adc6502((uint8_t)(hw_random(0) & 0x02), K_PTSPAR, &c, NULL);   /* LC705-LC70B */
        x = a;                                              /* LC70D */
        a = ROM(ROM_PICHI + x);                             /* LC70E */
        y++;                                                /* LC711 */
        vg_put(y, a);                                       /* LC712 */
        y--;                                                /* LC714 */
        vg_put(y, ROM(ROM_PICLO + x));                      /* LC715-LC718 */
        y++;                                                /* LC71A */
        y++;                                                /* LC71B */
        VGY = y;                                            /* LC71C */
    } else {
        /* WHITIP */
        y = VGY;                                            /* LC721 */
        vg_put(y, K_WHITE);                                 /* LC723-LC725 */
        y++;                                                /* LC727 */
        vg_put(y, 0x68);                                    /* LC728-LC72A: STAT white */
        y++;                                                /* LC72C */
        vg_put(y, cpu_rd(A_JSRDOT));                        /* LC72D-LC730: JSRL dot */
        y++;                                                /* LC732 */
        vg_put(y, cpu_rd((uint16_t)(A_JSRDOT + 1)));        /* LC733-LC736 */
        y++;                                                /* LC738 */
        VGY = y;                                            /* LC739 */
    }
}

/* FCONNEC ($C73C): vector of intensity $A0 from the current point to SX/SZ
 * at VGLIST+VGY (current point unchanged). */
void fconnec(void)
{
    uint8_t a, y = VGY;                                     /* LC73C */
    unsigned c;
    c = 1;                                                  /* LC740 SEC */
    a = sbc6502(SZL, CURNTY, &c, NULL);                     /* LC73E-LC741 */
    vg_put(y, a);                                           /* LC743 */
    y++;                                                    /* LC745 */
    a = (uint8_t)(sbc6502(SZH, R(A_CURNTY + 1), &c, NULL) & 0x1F);   /* LC746-LC74A */
    vg_put(y, a);                                           /* LC74C */
    y++;                                                    /* LC74E */
    c = 1;                                                  /* LC751 SEC */
    a = sbc6502(SXL, CURNTX, &c, NULL);                     /* LC74F-LC752 */
    vg_put(y, a);                                           /* LC754 */
    y++;                                                    /* LC756 */
    a = (uint8_t)((sbc6502(SXH, R(A_CURNTX + 1), &c, NULL) & 0x1F) | 0xA0);   /* LC757-LC75D */
    vg_put(y, a);                                           /* LC75F */
    y++;                                                    /* LC761 */
    VGY = y;                                                /* LC762 */
}

/* VGYAB1 ($C765): SCAL 1,0 then the absolute position (VGYABS). */
void vgyab1(uint8_t x)
{
    vg_put(0x00, 0x00);                                     /* LC765-LC768 */
    vg_put(0x01, 0x71);                                     /* LC76A-LC76D: binary 1, linear 0 */
    nolabs(x, 0x02);                                        /* LC76F-LC770 BNE NOLABS */
}

/* VGYABS ($C772): CNTR and a blank vector to the screen point at zero page
 * X (X lo/hi, Z lo/hi); current point updated; VGADD. */
void vgyabs(uint8_t x)
{
    nolabs(x, 0x00);                                        /* LC772 */
}

/* NOLABS ($C774) */
static void nolabs(uint8_t x, uint8_t y)
{
    uint8_t a;
    vg_put(y, 0x40);                                        /* LC774-LC776: VG centre */
    y++;                                                    /* LC77A */
    vg_put(y, 0x80);                                        /* LC778-LC77B */
    y++;                                                    /* LC77D */
    a = R((uint8_t)(x + 2));                                /* LC77E */
    CURNTY = a;                                             /* LC780 */
    vg_put(y, a);                                           /* LC782: delta Z */
    y++;                                                    /* LC784 */
    a = R((uint8_t)(x + 3));                                /* LC785 */
    R(A_CURNTY + 1) = a;                                    /* LC787 */
    vg_put(y, (uint8_t)(a & 0x1F));                         /* LC789-LC78B */
    a = R(x);                                               /* LC78D */
    CURNTX = a;                                             /* LC78F */
    y++;                                                    /* LC791 */
    vg_put(y, a);                                           /* LC792: delta X */
    a = R((uint8_t)(x + 1));                                /* LC794 */
    R(A_CURNTX + 1) = a;                                    /* LC796 */
    y++;                                                    /* LC79A */
    vg_put(y, (uint8_t)(a & 0x1F));                         /* LC798-LC79B */
    vgadd(y);                                               /* LC79D JMP VGADD */
}
