/* alsco2.c - ALSCO2 ($A8B0-$B1B5): score / lives / high-score display
 * templates, messages, option check, high-score detection and initials
 * entry, the high-score ladder, the rating display and the logo.
 *
 * One function per Atari routine header (43) plus the entry labels the ROM
 * jumps to (HACKER, LDROUT) and the two logo display states BOXPRO / LOGPRO
 * (entered through ALDIS2's DROUTAD table).  Fall-through tails shared by
 * two entries are small static helpers named after their label or address.
 *
 * Tables: the ALLANG message tables and the addresses of the ROM tables read
 * here come from tools/gen_allang.py (allang_data.h), never typed.  The fixed
 * table indexed by a register (MSGLBS) is read from the generated array;
 * literals are reached through the RAM pointers LITRAL / INDYLO with
 * cpu_rd(), as the ROM does; the other ROM tables (VGMSGA, SCORES, TCOMOD ...)
 * with cpu_rd() at the generated address.
 *
 * Registers: A/X/Y inputs are parameters.  Exit X/Y are returned (xy6502) by
 * the routines whose exit registers reach translated code today - the ROUTAD
 * state routines HISCHK, GETINI, LOGINI, NEWGAM's INICHK and what they call
 * (GAMSTA, INIINI, INILIT, INTLDR) - plus the documented results of GINICO /
 * GETCUR / HEXBCD (A, Y) and NWHEXZ (C, X).  tests/lockstep.c compares all of
 * them with the ROM.  The display routines do not model their exit
 * registers: ALDIS2 (C since M5) does not use them - after DSTATE, ZATVG2
 * reloads A/Y and SBCSWI sets X/Y; after INFO, ZATVG1 reloads.
 * ADC/SBC go through adc6502/sbc6502 (g.dflag) everywhere.
 */
#include "state.h"
#include "hw.h"
#include "game.h"
#include "allang_data.h"

#define INDY_ADDR   ((uint16_t)(INDYLO | ((uint16_t)INDYHI << 8)))
#define LITRAL_ADDR ((uint16_t)(LITRAL | ((uint16_t)g.ram[A_LITRAL + 1] << 8)))

static xy6502 xy(uint8_t x, uint8_t y) { xy6502 r; r.x = x; r.y = y; return r; }

static void vg_put(uint8_t y, uint8_t v)     { cpu_wr((uint16_t)(VGLIST_ADDR + y), v); }  /* STA (VGLIST),Y */
static void scobuf_put(uint8_t x, uint8_t v) { cpu_wr((uint16_t)(A_SCOBUF + x), v); }     /* STA SCOBUF,X  */

/* MSGLBS,X from the generated table; an index past it (the ROM's callers
 * only pass message numbers <= $3A) would read on into the literals. */
static uint8_t msglbs(uint8_t x)
{
    return x < sizeof allang_msglbs ? allang_msglbs[x] : cpu_rd((uint16_t)(ALLANG_MSGLBS + x));
}

/* N flag of an SBC: the binary result on the NMOS part, D or not */
static int sbc_n(uint8_t a, uint8_t v, unsigned c)
{
    return (((unsigned)a - v - (c ? 0u : 1u)) & 0x80u) != 0;
}

static void info_lives(void);
static void msgen2(void);
static void zatc4s(void);

/* ======================================================================= */
/* INFO DISPLAY - MESSAGES                                                 */
/* ======================================================================= */

/* INFO ($A8B4): display score & lives info (and the attract messages). */
void info(void)
{
    uint8_t x, a;
    VGSIZE = 0x01;                                          /* LA8B4-LA8B6 */
    vgsca1(0x01);                                           /* LA8B8 */
    nwcolo(K_LETCOL);                                       /* LA8BB-LA8BD */
    if (QSTATUS & 0x80) {                                   /* LA8C0-LA8C2 BMI: game mode */
        info_lives();
        return;
    }
    x = K_MGAMOV;                                           /* LA8C4: "GAME OVER" */
    if ((QFRAME & 0x20) == 0) {                             /* LA8C6-LA8CA */
        x = K_MINSER;                                       /* LA8CC: flash INSERT COINS */
        if (S_S_CRDT != 0 && !(TCMFLG & 0x80))              /* LA8CE-LA8D4 */
            x = K_MPRESS;                                   /* LA8D6 */
    }
    msgs(x);                                                /* LA8D8 */
    vgcntr_ab0d();                                          /* LA8DB */
    a = cpu_rd(A_VGMSGA);                                   /* LA8DE: blank out level */
    cpu_wr(A_SCLEVEL, a);                                   /* LA8E1 */
    cpu_wr((uint16_t)(A_SCLEVEL + 2), a);                   /* LA8E4 */
    hacker();
}

/* HACKER ($A8E7): credits & ATARI, then the lives / scores part of INFO
 * (GENPLA jumps here - rev 3's change from JMP INFO). */
void hacker(void)
{
    dspcrd();                                               /* LA8E7 */
    info_lives();
}

/* $A8EA: lives, scores, high score, the INFO buffer JSRL, starfield messages. */
static void info_lives(void)
{
    uint8_t a, x, y;
    upscli(0x01, 0x00);                                     /* LA8EA-LA8EE: player 1 */
    if (!(QSTATUS & 0x80))                                  /* LA8F1-LA8F3 */
        a = (uint8_t)(RSCORL | RSCORM | RSCORH);            /* LA8F5-LA8FC: attract: P2 score if not 0 */
    else
        a = NUMPLA;                                         /* LA8FE: 2 players? */
    if (a != 0)                                             /* LA900 */
        upscli(0x01, 0x01);                                 /* LA902-LA905 */
    if (QSTATE != K_CPLAY) {                                /* LA908-LA90C */
        INDYLO = (uint8_t)((A_HSCORL + 21 + 2) & 0xFF);     /* LA90E-LA910 */
        INDYHI = (uint8_t)((A_HSCORL + 21 + 2) >> 8);       /* LA912-LA914 */
        nwdigs(cpu_rd(ROM_HISLOC));                         /* LA916-LA919 */
        /* ZATC4V: verify the call to the ATARI literal */
        a = 0xA7;                                           /* LA91C-LA91E */
        y = K_ZATC4C;
        do {
            a ^= cpu_rd((uint16_t)(ROM_ZATC4S + y));        /* LA920 */
            y--;                                            /* LA923 */
        } while (!(y & 0x80));                              /* LA924 */
        QT2 = a;                                            /* LA926 */
        x = cpu_rd(ROM_HIILOC);                             /* LA929 */
        INDEX2 = 0x02;                                      /* LA92C-LA92E */
        do {
            y = INDEX2;                                     /* LA930 */
            a = g.ram[A_INITAL + 21 + y];                   /* LA932: get initial */
            y = (uint8_t)(a << 1);                          /* LA935-LA936 */
            scobuf_put(x, cpu_rd((uint16_t)(A_VGMSGA + 22 + y)));   /* LA937-LA93A */
            x = (uint8_t)(x + 2);                           /* LA93D-LA93E */
            INDEX2 = (uint8_t)(INDEX2 - 1);                 /* LA93F */
        } while (!(INDEX2 & 0x80));                         /* LA941 */
    }
    vgjsrl((uint8_t)((A_SCOBUF + 1) >> 8), (uint8_t)(A_SCOBUF & 0xFF));   /* LA943-LA947 */
    if (ELICNT & 0x80)                                      /* LA94A-LA94D: warning? */
        msgs(K_MSPIKE);                                     /* LA94F-LA951 */
    /* starfield messages */
    if (QSTATE != K_CNEWV2) return;                         /* LA954-LA958 */
    if (!(QSTATUS & 0x80)) return;                          /* LA95A-LA95C */
    if (g.ram[A_BONUS + PLAYUP] != 0) {                     /* LA95E-LA963: display bonus? */
        msgs(K_MBONPT);                                     /* LA965-LA967 */
        bodspl(g.ram[A_BONUS + PLAYUP]);                    /* LA96A-LA96F */
    }
    msgs(K_MSUPZA);                                         /* LA972-LA974 */
    msgs(K_MAPROA);                                         /* LA977-LA979 */
}

/* UPSCLI ($A97F): update player Y's scale (A = proposed binary scale),
 * lives and (unless playing and not up) score in the score template. */
void upscli(uint8_t a, uint8_t y)
{
    uint8_t x;
    TEMP2 = y;                                              /* LA97F-LA983 (LDX QSTATE / CPX: flags unused) */
    if (y == PLAYUP && (QSTATUS & 0x80))                    /* LA985-LA98B */
        a = 0x00;                                           /* LA98D: player up in a game: big score */
    a |= 0x70;                                              /* LA98F */
    x = cpu_rd((uint16_t)(ROM_SCALOC + y));                 /* LA991 */
    scobuf_put(x, a);                                       /* LA994 */
    x = cpu_rd((uint16_t)(ROM_LIVLOC + y));                 /* LA997 */
    INDEX2 = g.ram[A_LIVES1 + y];                           /* LA99A-LA99D */
    if (INDEX2 != 0 && y == PLAYUP)                         /* LA99F-LA9A3 */
        INDEX2 = (uint8_t)(INDEX2 - 1);                     /* LA9A5: one life is the cursor */
    for (y = 1; y < 7; y++) {                               /* LA9A7, LA9BA-LA9BD */
        a = cpu_rd(A_LSYMBL);                               /* LA9A9: life picture */
        if (y > INDEX2) a = cpu_rd(A_LSYMB0);               /* LA9AC-LA9B2: no life: blank */
        scobuf_put(x, a);                                   /* LA9B5 */
        x = (uint8_t)(x + 2);                               /* LA9B8-LA9B9 */
    }
    y = TEMP2;                                              /* LA9BF */
    if (QSTATE == K_CPLAY && y != PLAYUP) return;           /* LA9C1-LA9C9 -> LA9FB RTS */
    /* ANYWAY ($A9CB) */
    x = cpu_rd((uint16_t)(ROM_SCOLOC + y));                 /* LA9CB */
    INDYLO = cpu_rd((uint16_t)(ROM_SCOSOL + y));            /* LA9CE-LA9D1 */
    INDYHI = 0x00;                                          /* LA9D3-LA9D5 */
    nwdigs(x);                                              /* falls into NWDIGS */
}

/* NWDIGS ($A9D7): 6 BCD digits from the 3 bytes ending at @INDYLO (most
 * significant first, zero suppressed, the last digit always shown) into
 * the template at SCOBUF+X. */
void nwdigs(uint8_t x)
{
    int c;
    TEMP1 = 0x02;                                           /* LA9D7-LA9D9 */
    c = 1;                                                  /* LA9DB SEC */
    do {
        /* LA9DC PHP / LA9E5 PLP: C survives the LSRs */
        x = nwhexz((uint8_t)(cpu_rd(INDY_ADDR) >> 4), x, &c);   /* LA9DD-LA9E6 */
        if (TEMP1 == 0) c = 0;                              /* LA9E9-LA9ED: always display last digit */
        x = nwhexz(cpu_rd(INDY_ADDR), x, &c);               /* LA9EE-LA9F2 */
        INDYLO = (uint8_t)(INDYLO - 1);                     /* LA9F5 */
        TEMP1 = (uint8_t)(TEMP1 - 1);                       /* LA9F7 */
    } while (!(TEMP1 & 0x80));                              /* LA9F9 */
}

/* NWHEXZ ($A9FC): one BCD digit (low nibble of A) with zero suppression
 * into SCOBUF+X.  ENTRY *c set = suppress; EXIT *c clear once a non-zero
 * digit was shown; returns X + 2. */
uint8_t nwhexz(uint8_t a, uint8_t x, int *c)
{
    uint8_t y = (uint8_t)(a & 0x0F);                        /* LA9FC-LA9FE */
    if (y != 0) *c = 0;                                     /* LA9FF-LAA01 */
    if (!*c) y++;                                           /* LAA02-LAA04: display zero */
    y = (uint8_t)(y << 1);                                  /* LAA05-LAA08 PHP / TYA / ASL / TAY */
    scobuf_put(x, cpu_rd((uint16_t)(A_VGMSGA + y)));        /* LAA09-LAA0C */
    return (uint8_t)(x + 2);                                /* LAA0F-LAA12 INX / INX / PLP */
}

/* INITEM ($AA13): copy the ROM score template into vector RAM (SCOBUF),
 * level number, RTSL. */
void initem(void)
{
    uint8_t x, y, a, lo;
    unsigned c;
    x = NUMPLA;                                             /* LAA13 */
    if (!(QSTATUS & 0x80) && (RSCORL | RSCORM | RSCORH) != 0)   /* LAA15-LAA1F */
        x = 0x01;                                           /* LAA21: player 2 score if not 0 */
    VGLIST = (uint8_t)(A_SCOBUF & 0xFF);                             /* LAA23-LAA25 */
    g.ram[A_VGLIST + 1] = (uint8_t)((A_SCOBUF + 1) >> 8);   /* LAA27-LAA29 */
    y = cpu_rd((uint16_t)(ROM_SCECOU + x));                 /* LAA2B-LAA2E */
    c = 1;                                                  /* LAA2F SEC */
    lo = adc6502(y, VGLIST, &c, NULL);                      /* LAA30 ADC VGLIST / LAA32 PHA */
    do {
        vg_put(y, cpu_rd((uint16_t)(ROM_SCORES + y)));      /* LAA33-LAA36 */
        y--;                                                /* LAA38 */
    } while (y != 0);                                       /* LAA39 */
    vg_put(y, cpu_rd((uint16_t)(ROM_SCORES + y)));          /* LAA3B-LAA3E */
    if (QSTATUS & 0x80) {                                   /* LAA40-LAA42 */
        g.ram[A_VGLIST + 1] = (uint8_t)((A_SCLEVEL + 1) >> 8);  /* LAA44-LAA46 */
        VGLIST = (uint8_t)(A_SCLEVEL & 0xFF);                        /* LAA48-LAA4A: point at level JSRL */
        c = 0;                                              /* LAA4E CLC */
        a = adc6502(CURWAV, 0x01, &c, NULL);                /* LAA4C-LAA4F */
        dsp1hx(a);                                          /* LAA51 */
    }
    VGLIST = lo;                                            /* LAA54-LAA55 PLA / STA */
    vgrtsl();                                               /* LAA57 JMP VGRTSL */
}

/* DPLPLA ($AA5A): display state - "PLAY" "PLAYER n" and the info. */
void dplpla(void)
{
    msgs(K_MPLAY);                                          /* LAA5A-LAA5C */
    dplrno();                                               /* LAA5F JMP GENPLA: LAA69 */
    hacker();                                               /* LAA6C JMP HACKER */
}

/* DGOVER ($AA62): display state - "GAME OVER" (Y $30) then GENPLA. */
void dgover(void)
{
    msgen3(0x30, K_MGAMOV);                                 /* LAA62-LAA66 */
    dplrno();                                               /* GENPLA LAA69 */
    hacker();                                               /* LAA6C */
}

/* DPRSTA ($AA6F): display state - all info plus "PRESS START" mid-screen. */
void dprsta(void)
{
    info();                                                 /* LAA6F */
    msgen3(0x00, K_MPRESS);                                 /* LAA72-LAA76 JMP MSGEN3 */
}

/* D2GAME ($AA79): display state - 2 game minimum / flashing insert coins. */
void d2game(void)
{
    msgen3(0x00, K_M2GAME);                                 /* LAA79-LAA7D */
    if ((QFRAME & 0x1F) < 0x10)                             /* LAA80-LAA86 */
        msgen3(0xE0, K_MINSER);                             /* LAA88-LAA8C */
    info();                                                 /* LAA8F JMP INFO */
}

/* DPLRNO ($AA92): "PLAYER " then the number. */
void dplrno(void)
{
    msgs(K_MPLAYR);                                         /* LAA92-LAA94 */
    dplrx();                                                /* falls into DPLRX */
}

/* DPLRX ($AA97): big player number (PLAYUP + 1). */
void dplrx(void)
{
    nwsca1(0x00);                                           /* LAA97-LAA99 */
    dplrxx(PLAYUP);                                         /* LAA9C, falls into DPLRXX */
}

/* DPLRXX ($AA9E): player number X + 1. */
void dplrxx(uint8_t x)
{
    SXL = (uint8_t)(x + 1);                                 /* LAA9E-LAA9F */
    digtys(A_SXL, 0x01);                                    /* LAAA1-LAAA5 JMP DIGTYS */
}

/* DSPCRD ($AAA8): info display - coin mode, 2 game minimum or bonus life
 * interval, copyright, credits. */
void dspcrd(void)
{
    uint8_t x = cpu_rd((uint16_t)(ROM_TCOMOD + (S_CMODE & 0x03)));   /* LAAA8-LAAB0 */
    msgs(x);                                                /* LAAB1: coin mode */
    SECUVY = (uint8_t)(SECUVY - 1);                         /* LAAB4 */
    if ((OPTIN2 & K_OM2GAM) != 0 && (QFRAME & 0x20) == 0) { /* LAAB7-LAAC1 */
        msgs(K_M2GAME);                                     /* LAAC3-LAAC5 */
        zatc4s();                                           /* LAAC8-LAAC9 CLV / BVC ZATC4S */
        return;
    }
    dbolou();                                               /* DBOLOU */
}

/* DBOLOU ($AACB): bonus life interval, then copyright and credits. */
void dbolou(void)
{
    bolout();                                               /* LAACB */
    zatc4s();
}

/* ZATC4S ($AACE): copyright, "CREDITS", the count (max 40), half credit. */
static void zatc4s(void)
{
    uint8_t a;
    msgs(K_MATARI);                                         /* LAACE-LAAD0 */
    msgs(K_MCREDI);                                         /* LAAD3-LAAD5 */
    a = S_S_CRDT;                                           /* ZATC4E LAAD8 */
    if (a >= 0x28) {                                        /* LAADA-LAADC: max 40 credits */
        a = 0x28;                                           /* LAADE */
        S_S_CRDT = a;                                       /* LAAE0 */
    }
    dsp1hx(a);                                              /* LAAE2 */
    a = S_CNCT;                                             /* LAAE5 */
    CK(0xAAE7);            /* free play: every IRQ's S_CNVRT_2 stores S_CNCT */
    if (a != 0)                                             /* LAAE7: partial credits? */
        vgjsrl(cpu_rd((uint16_t)(ROM_IHALF + 1)), cpu_rd(ROM_IHALF));   /* LAAE9-LAAEF */
}

/* HEXBCD ($AAF5): binary A to BCD (decimal mode shifts).  Returns A = TEMP0;
 * exits Y = $FF, D clear. */
uint8_t hexbcd(uint8_t a)
{
    int y;
    unsigned c;
    g.dflag = 1;                                            /* LAAF5 SED */
    TEMP0 = a;                                              /* LAAF6 */
    TEMP3 = 0x00;                                           /* LAAF8-LAAFA */
    for (y = 7; y >= 0; y--) {                              /* LAAFC, LAB06-LAB07 */
        c = (TEMP0 & 0x80) ? 1u : 0u;                       /* LAAFE ASL TEMP0 */
        TEMP0 = (uint8_t)(TEMP0 << 1);
        a = adc6502(TEMP3, TEMP3, &c, NULL);                /* LAB00-LAB02 */
        TEMP3 = a;                                          /* LAB04 */
    }
    g.dflag = 0;                                            /* LAB09 CLD */
    TEMP0 = a;                                              /* LAB0A */
    return a;                                               /* LAB0C */
}

/* VGCNTR ($AB0D, listed VGCNTR_AB0D): fast centre - bytes $20,$80. */
void vgcntr_ab0d(void)
{
    vgadd2(0x20, 0x80);                                     /* LAB0D-LAB11 */
}

/* MSGS ($AB14): message X (= number * 2) at its own Y position. */
void msgs(uint8_t x)
{
    msgen3(msglbs((uint8_t)(x + 1)), x);                    /* LAB14 LDA MSGLBS+1,X */
}

/* MSGEN3 ($AB17): message X at Y position A. */
void msgen3(uint8_t a, uint8_t x)
{
    uint8_t y;
    SAVEX = x;                                              /* LAB17 */
    TEMP2 = a;                                              /* LAB19 */
    y = SAVEX;                                              /* LAB1B */
    INDYLO = cpu_rd((uint16_t)(LITRAL_ADDR + y));           /* LAB1D-LAB1F: literal pointer */
    y++;                                                    /* LAB21 */
    INDYHI = cpu_rd((uint16_t)(LITRAL_ADDR + y));           /* LAB22-LAB24 */
    if (x == K_MATARI) {                                    /* ZSECL0 LAB26-LAB28: copyright? */
        SECUVG = VGLIST;                                    /* LAB2A-LAB2C: save start */
        g.ram[A_SECUVG + 1] = g.ram[A_VGLIST + 1];          /* LAB2E-LAB30 */
    }
    TEMP1 = cpu_rd(INDY_ADDR);                              /* LAB32-LAB36: X position from the literal */
    vgcntr_ab0d();                                          /* MSGENT LAB38 */
    msgen2();
}

/* MSGEN2 ($AB3B): position the beam at (TEMP1, TEMP2), then MSGNOP ($AB4D):
 * colour, scale and the literal's characters as JSRLs. */
static void msgen2(void)
{
    uint8_t y, x, a;
    VGBRIT = 0x00;                                          /* LAB3B-LAB3D */
    VGSIZE = 0x01;                                          /* LAB3F-LAB41 */
    vgsca1(0x01);                                           /* LAB43 */
    vgvtr1(TEMP1, TEMP2);                                   /* LAB46-LAB4A */
    /* MSGNOP */
    y = SAVEX;                                              /* LAB4D */
    INDYLO = cpu_rd((uint16_t)(LITRAL_ADDR + y));           /* LAB4F-LAB51 */
    y++;                                                    /* LAB53 */
    INDYHI = cpu_rd((uint16_t)(LITRAL_ADDR + y));           /* LAB54-LAB56 */
    x = SAVEX;                                              /* LAB58 */
    a = msglbs(x);                                          /* LAB5A (PHA) */
    nwcolo((uint8_t)(a >> 4));                              /* LAB5E-LAB63 */
    nwsca1((uint8_t)(a & 0x0F));                            /* LAB66-LAB69 PLA / AND / JSR */
    y = 0x01;                                               /* LAB6C */
    TEMP1 = 0x00;                                           /* LAB6E-LAB70 */
    do {
        TEMP2 = cpu_rd((uint16_t)(INDY_ADDR + y));          /* LAB72-LAB74 */
        x = (uint8_t)(TEMP2 & 0x7F);                        /* LAB76, LAB7B TAX */
        y++;                                                /* LAB78 */
        TEMP3 = y;                                          /* LAB79 */
        vg_put(TEMP1, cpu_rd((uint16_t)(A_VGMSGA + x)));    /* LAB7C-LAB81 */
        vg_put((uint8_t)(TEMP1 + 1), cpu_rd((uint16_t)(A_VGMSGA + 1 + x)));   /* LAB83-LAB87 */
        TEMP1 = (uint8_t)(TEMP1 + 2);                       /* LAB89-LAB8A */
        y = TEMP3;                                          /* LAB8C */
    } while (!(TEMP2 & 0x80));                              /* LAB8E-LAB90 */
    vgadd((uint8_t)(TEMP1 - 1));                            /* LAB92-LAB95 */
}

/* MSGFOL ($AB98): message X at X offset A, no vertical positioning. */
void msgfol(uint8_t a, uint8_t x)
{
    SAVEX = x;                                              /* LAB98 */
    TEMP1 = a;                                              /* LAB9A */
    TEMP2 = 0x00;                                           /* LAB9C-LAB9E */
    msgen2();                                               /* LABA0 BEQ MSGEN2 */
}

/* ======================================================================= */
/* OPTIONS, HIGH SCORES, INITIALS                                          */
/* ======================================================================= */

/* INICHK ($ABA2): new game option setup & check. */
xy6502 inichk(uint8_t x, uint8_t y)
{
    xy6502 r = gamsta(x, y);                                /* LABA2 */
    if ((EABAD & 0x03) == 0) return r;                      /* LABA5-LABAA -> LAC07 */
    return iniini(r.x, r.y);
}

/* INIINI ($ABAC): reinitialise the scores and initials that did not read
 * back from the EAROM (EABAD), set up the game play options. */
xy6502 iniini(uint8_t x, uint8_t y)
{
    int i;
    xy6502 r = gamsta(x, y);                                /* LABAC */
    /* INIIN2 ($ABAF) */
    NGAMES = K_NHISCO;                                      /* LABAF-LABB1 */
    if ((g.ram[A_HSCORL + 21] | g.ram[A_HSCORM + 21] | g.ram[A_HSCORH + 21]) == 0)   /* LABB4-LABBD */
        induce();                                           /* LABBF */
    i = (EABAD & 0x01) ? 23 : 14;                           /* LABC2-LABCB: initials valid? keep top 3 */
    for (; i >= 0; i--)                                     /* LABD3-LABD4 */
        g.ram[A_INITAL + i] = cpu_rd((uint16_t)(ROM_SCOINI + i));   /* LABCD-LABD0 */
    i = (EABAD & 0x02) ? 23 : 14;                           /* LABD6-LABDF */
    for (; i >= 0; i--)                                     /* LABE6-LABE7 */
        g.ram[A_HSCORL + i] = 0x01;                         /* LABE1-LABE3 */
    if (EABAD & 0x03) {                                     /* LABE9-LABEE: reinitialising? */
        GAMOP1 = (uint8_t)(OPTIN2 & 0xF8);                  /* LABF0-LABF4 */
        GAMOP3 = (uint8_t)(OPTIN3 & 0x03);                  /* LABF7-LABFC */
    }
    EABAD = (uint8_t)(EABAD & 0xFC);                        /* LABFF-LAC04: clear bad read flags */
    return xy(0xFF, r.y);                                   /* LAC07 (X from the loops) */
}

/* GAMSTA ($AC20): read the options (INILIT); a changed game play option
 * induces reinitialisation. */
xy6502 gamsta(uint8_t x, uint8_t y)
{
    xy6502 r = inilit(x, y);                                /* LAC20 */
    if ((OPTIN2 & 0xF8) != GAMOP1 || (OPTIN3 & 0x03) != GAMOP3)   /* LAC23-LAC34 */
        induce();                                           /* INDUCE */
    return r;                                               /* LAC3E */
}

/* INDUCE ($AC36): request reinitialisation of scores & initials. */
void induce(void)
{
    EABAD = (uint8_t)(EABAD | 0x03);                        /* LAC36-LAC3B */
}

/* HISCHK ($AC3F): state routine - high score detection: rank each player
 * against the high-score table and the rank table, insert, then INTLDR. */
xy6502 hischk(uint8_t x, uint8_t y)
{
    uint8_t a, t;
    unsigned c;
    QSTATUS = (uint8_t)(QSTATUS & (uint8_t)~K_MGTMOD);      /* LAC3F-LAC43: out of game time mode */
    CK(0xAC45);
    if ((OPTIN1 & 0x43) == 0x40) {                          /* LAC45-LAC4B: sales mode? */
        clrsco();                                           /* LAC4D */
        x = 0xFF;
    }
    wrbook();                                               /* LAC50 */
    y = 0x00;
    g.ram[A_RANKS + 1] = 0x00;                              /* LAC53-LAC55 */
    x = NUMPLA;                                             /* LAC58 */
    if (x != 0) x = 0x03;                                   /* LAC5A-LAC5C: start with player 2 */
    for (;;) {
        TEMP3 = g.ram[A_LSCORH + x];                        /* LAC5E-LAC60 */
        TEMP4 = g.ram[A_LSCORM + x];                        /* LAC62-LAC64 */
        TEMPX = g.ram[A_LSCORL + x];                        /* LAC66-LAC68 */
        SAVEY = (uint8_t)(x & 0x01);                        /* LAC6A-LAC6D */
        TEMP2 = 0x00;                                       /* LAC6F-LAC71 */
        TEMP1 = K_CBLANK;                                   /* LAC73-LAC75 */
        TEMP0 = K_CBLANK;                                   /* LAC77: initials A, blank, blank */
        TIMHIS = 0x00;                                      /* LAC79-LAC7B: rank */
        y = 0xFD;                                           /* LAC7E */
        do {
            /* until the player's score > the table entry */
            a = g.ram[A_HRANKH + y];                        /* LAC80 */
            c = (a >= TEMP3);                               /* LAC83 */
            if (a == TEMP3) {                               /* LAC85 */
                a = g.ram[A_HRANKM + y];                    /* LAC87 */
                c = (a >= TEMP4);                           /* LAC8A */
                if (a == TEMP4) {                           /* LAC8C */
                    if (y >= 0x52) {                        /* LAC8E-LAC90: triple precision? */
                        a = g.ram[A_HRANKL + y];            /* LAC92 */
                        c = (a >= TEMPX);                   /* LAC95-LAC98 */
                    } else {
                        c = 1;                              /* LAC9A SEC: double */
                    }
                }
            }
            if (!c) {                                       /* LAC9B BCS LACEC */
                /* player's score > entry: move the rest of the table down */
                do {
                    if (y >= 0xE8) {                        /* LAC9D-LAC9F: high-score table? */
                        a = TEMP0;                          /* LACA1: move initials down */
                        t = g.ram[A_INITAL - 232 + y];      /* LACA3 */
                        g.ram[A_INITAL - 232 + y] = a;      /* LACA6 */
                        TEMP0 = t;                          /* LACA9 */
                        a = TEMP1;                          /* LACAB */
                        t = g.ram[A_INITAL + 1 - 232 + y];  /* LACAD */
                        g.ram[A_INITAL + 1 - 232 + y] = a;  /* LACB0 */
                        TEMP1 = t;                          /* LACB3 */
                        a = TEMP2;                          /* LACB5 */
                        t = g.ram[A_INITAL + 2 - 232 + y];  /* LACB7 */
                        g.ram[A_INITAL + 2 - 232 + y] = a;  /* LACBA */
                        TEMP2 = t;                          /* LACBD */
                    }
                    a = TEMP4;                              /* LACBF: move scores down */
                    t = g.ram[A_HRANKM + y];                /* LACC1 */
                    g.ram[A_HRANKM + y] = a;                /* LACC4 */
                    TEMP4 = t;                              /* LACC7 */
                    a = TEMP3;                              /* LACC9 */
                    t = g.ram[A_HRANKH + y];                /* LACCB */
                    g.ram[A_HRANKH + y] = a;                /* LACCE */
                    TEMP3 = t;                              /* LACD1 */
                    if (y >= 0x52) {                        /* LACD3-LACD5: triple precision? */
                        a = TEMPX;                          /* LACD7 */
                        t = g.ram[A_HRANKL + y];            /* LACD9 */
                        g.ram[A_HRANKL + y] = a;            /* LACDC */
                        TEMPX = t;                          /* LACDF */
                    }
                    if (y >= 0x55) y--;                     /* LACE1-LACE5 */
                    y = (uint8_t)(y - 2);                   /* LACE6-LACE7 */
                } while (y != 0);                           /* LACE8 */
                y = 0x02;                                   /* LACEA: abort outer loop */
            }
            TIMHIS = (uint8_t)(TIMHIS + 1);                 /* LACEC: update rank */
            if (y >= 0x55) y--;                             /* LACEF-LACF3 */
            y = (uint8_t)(y - 2);                           /* LACF4-LACF5 */
        } while (y != 0);                                   /* LACF6 */
        x = SAVEY;                                          /* LACF8 */
        g.ram[A_RANKS + x] = TIMHIS;                        /* LACFA-LACFD: player's rank */
        x--;                                                /* LAD00 */
        if (x & 0x80) break;                                /* LAD01 */
    }                                                       /* LAD03 JMP LAC5E */
    a = g.ram[A_RANKS + 1];                                 /* LAD06 */
    if (a >= RANKS && a < K_NRANKS)                         /* LAD09-LAD10: both ranks same? */
        g.ram[A_RANKS + 1] = (uint8_t)(g.ram[A_RANKS + 1] + 1);   /* LAD12: player 2 low */
    /* player order of finish: last player up in D0-D1, first to die in D2-D3 */
    a = (uint8_t)(PLAYUP ^ 0x01);                           /* LAD15-LAD17 */
    c = (a & 0x80) ? 1u : 0u; a = (uint8_t)(a << 1);        /* LAD19 ASL */
    c = (a & 0x80) ? 1u : 0u; a = (uint8_t)(a << 1);        /* LAD1A ASL */
    a |= PLAYUP;                                            /* LAD1B */
    FLGNHI = adc6502(a, 0x05, &c, NULL);                    /* LAD1D-LAD1F */
    return intldr(x, y);                                    /* falls into INTLDR */
}

/* INTLDR ($AD22): high-score initials prep - next player (from FLGNHI) who
 * made the table: set up GETINI via the BOOM state; none left: CNOTFOU. */
xy6502 intldr(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c;
    for (;;) {
        y = K_CNOTFOU;                                      /* LAD22: default is failure */
        if (FLGNHI == 0) {                                  /* LAD24-LAD27 */
            QSTATE = y;                                     /* LAD6B */
            return xy(x, y);                                /* LAD6D */
        }
        PLAYUP = (uint8_t)(FLGNHI & 0x03);                  /* LAD29-LAD2B */
        PLAYUP = (uint8_t)(PLAYUP - 1);                     /* LAD2D */
        FLGNHI = (uint8_t)(FLGNHI >> 1);                    /* LAD2F */
        FLGNHI = (uint8_t)(FLGNHI >> 1);                    /* LAD32 */
        x = PLAYUP;                                         /* LAD35 */
        a = g.ram[A_RANKS + x];                             /* LAD37 */
        if (a != 0 && a < 0x09) break;                      /* LAD3A-LAD3E: player got a high score? */
    }                                                       /* LAD68 JMP INTLDR */
    c = 0;                                                  /* LAD41 CLC */
    a = adc6502((uint8_t)(a << 1), g.ram[A_RANKS + x], &c, NULL);   /* LAD40-LAD42 */
    a ^= 0xFF;                                              /* LAD45 */
    c = 1;                                                  /* LAD47 SEC */
    TBLIND = sbc6502(a, 0xE5, &c, NULL);                    /* LAD48-LAD4A: index of the MS letter */
    y = cocfli();                                           /* LAD4D */
    TIMHIS = K_ITIMHI;                                      /* LAD50-LAD52 */
    SWFINA = 0x00;                                          /* LAD55-LAD57: clear switches */
    TBHD = 0x00;                                            /* LAD59 */
    ININDX = 0x02;                                          /* LAD5B-LAD5D */
    x = inboom(x, y).x;                                     /* LAD60 */
    y = K_CBOOM;                                            /* LAD63 */
    QSTATE = y;                                             /* LAD65 */
    return xy(x, y);                                        /* LAD67 */
}

/* GETINI ($AD6E): state routine - get the high-score initials. */
xy6502 getini(uint8_t x, uint8_t y)
{
    uint8_t a;
    QDSTATE = K_CDGETI;                                     /* LAD6E-LAD70 */
    if ((QFRAME & 0x1F) == 0) {                             /* LAD72-LAD76: update timer? */
        TIMHIS = (uint8_t)(TIMHIS - 1);                     /* LAD78 */
        if (TIMHIS == 0) {                                  /* LAD7B: time up? */
            y = K_CNOTFOU;                                  /* LAD7D */
            QSTATE = y;                                     /* LAD7F */
            return xy(x, y);                                /* LAD81 */
        }
    }
    x = TBLIND;                                             /* LAD82 */
    a = ginico(g.ram[A_INITAL + x]);                        /* LAD85-LAD88: change the letter */
    y = a;                                                  /* LAD8B TAY */
    if (a & 0x80) a = 0x1A;                                 /* LAD8C-LAD91 */
    else if (a >= 0x1B) a = 0x00;                           /* LAD93-LAD97 */
    x = TBLIND;                                             /* LAD99 */
    g.ram[A_INITAL + x] = a;                                /* LAD9C */
    y = (uint8_t)(SWFINA & (K_MFIRE | K_MSUZA));            /* LAD9F-LADA3 */
    SWFINA = (uint8_t)(SWFINA & (uint8_t)~(K_MFIRE | K_MSUZA | K_MFAKE));   /* LADA4-LADA8 */
    if (y == 0) return xy(x, y);                            /* LADAA-LADAB -> LADCD */
    TBLIND = (uint8_t)(TBLIND - 1);                         /* LADAD: next initial */
    ININDX = (uint8_t)(ININDX - 1);                         /* LADB0 */
    if (!(ININDX & 0x80)) {                                 /* LADB3 */
        x--;                                                /* LADC7 */
        g.ram[A_INITAL + x] = 0x00;                         /* LADC8-LADCA: start at A */
        return xy(x, y);                                    /* LADCD */
    }
    x = PLAYUP;                                             /* LADB5: all done with player */
    if (g.ram[A_RANKS + x] < 0x04) {                        /* LADB7-LADBC: save in EAROM? */
        wrhiin();                                           /* LADBE */
        y = 0x00;
    }
    return intldr(x, y);                                    /* LADC1, LADC4-LADC5 -> LADCD */
}

/* GINICO ($ADCE): spinner input - CURSPO += TBHD * 8; returns A + direction
 * + the carry of that add; TBHD cleared.  Exits Y = 0, X unchanged. */
uint8_t ginico(uint8_t a)
{
    unsigned c = 0;                                         /* LADD4 CLC */
    CURSPO = adc6502((uint8_t)(TBHD << 3), CURSPO, &c, NULL);   /* LADCE-LADD7 PHA / ASL x3 / ADC */
    if (!(TBHD & 0x80))                                     /* LADD9-LADDC PLA / LDY TBHD / BMI */
        a = adc6502(a, 0x00, &c, NULL);                     /* LADDE: + direction + carry */
    else
        a = adc6502(a, 0xFF, &c, NULL);                     /* LADE3 */
    TBHD = 0x00;                                            /* LADE5-LADE7 */
    return a;                                               /* LADE9 */
}

/* GETDSP ($ADEA): display state - initials entry. */
void getdsp(void)
{
    unsigned c;
    info();                                                 /* LADEA */
    msgen3(0xC0, K_MPLAYR);                                 /* LADED-LADF1 */
    SECUVY = (uint8_t)(SECUVY - 1);                         /* LADF4 */
    dplrx();                                                /* LADF7 */
    msgs(K_MENTER);                                         /* LADFA-LADFC */
    msgen3(0xA6, K_MPRMOV);                                 /* LADFF-LAE03 */
    msgen3(0x9C, K_MPRFIR);                                 /* ZATC3S LAE06-LAE0A */
    msgs(K_MATARI);                                         /* LAE0D-LAE0F */
    c = 1;                                                  /* LAE15 SEC */
    ldrout(sbc6502(TBLIND, ININDX, &c, NULL));              /* LAE12-LAE19: glow code, JMP LDROUT */
}

/* LDRDSP ($AE1C): display state - high-score table (and the POKEY check). */
void ldrdsp(void)
{
    uint8_t a, y;
    info();                                                 /* LAE1C */
    /* ZPONTS: both POKEYs' random numbers - sequential reads have matching
     * nibbles; QT5 = 0 if all is well (SEI/CLI: no IRQ in between) */
    a = hw_random(0);                                       /* LAE1F SEI / LAE20 */
    y = hw_random(0);                                       /* LAE23 */
    TEMP0 = y;                                              /* LAE26 */
    TEMP0 = (uint8_t)((a >> 4) ^ TEMP0);                    /* LAE28-LAE2E */
    a = hw_random(1);                                       /* LAE30 */
    y = hw_random(1);                                       /* LAE33 / LAE36 CLI */
    TEMP0 = (uint8_t)(((a ^ TEMP0) & 0xF0) ^ TEMP0);        /* LAE37-LAE3D */
    QT5 = (uint8_t)((uint8_t)(y << 4) ^ TEMP0);             /* LAE3F-LAE46 */
    rnkdsp();                                               /* LAE49 */
    ldrout(0xFF);                                           /* LAE4C: no glow */
}

/* LDROUT ($AE4E): the high-score ladder; A = index of the initials to glow
 * ($FF none). */
void ldrout(uint8_t a)
{
    uint8_t x, y;
    unsigned c;
    SZL = a;                                                /* LAE4E */
    msgs(K_MHIGHS);                                         /* LAE50-LAE52 */
    SXL = 0x01;                                             /* LAE55-LAE57: initial rank */
    nwsca1(0x01);                                           /* LAE59 */
    TEMP3 = 0x28;                                           /* LAE5C-LAE5E: initial height */
    INDEX1 = (uint8_t)(3 * (K_NHISCO - 1));                 /* LAE60-LAE62 */
    do {
        vgcntr_ab0d();                                      /* LAE64 */
        VGBRIT = 0x00;                                      /* LAE67-LAE69 */
        x = TEMP3;                                          /* LAE6B-LAE6D: Y coordinate */
        c = 1;                                              /* LAE6E SEC */
        TEMP3 = sbc6502(TEMP3, 0x0A, &c, NULL);             /* LAE6F-LAE71 */
        vgvtr1(0xD0, x);                                    /* LAE73-LAE75: position beam */
        y = K_BLULET;                                       /* LAE78 */
        if (SZL == INDEX1) y = K_WHITE;                     /* LAE7A-LAE80: glow */
        nwcolo(y);                                          /* LAE82 */
        digtys(A_SXL, 0x01);                                /* LAE85-LAE89: rank */
        vgdot(0xA0);                                        /* LAE8C-LAE8E */
        VGBRIT = 0x00;                                      /* LAE91-LAE93 */
        vgvtr1(0x08, 0x00);                                 /* LAE95-LAE98: space */
        SXL = (uint8_t)(SXL + 1);                           /* LAE9B */
        outini(INDEX1);                                     /* LAE9D-LAE9F */
        vgvtr1(0x08, 0x00);                                 /* LAEA2-LAEA6 */
        x = INDEX1;                                         /* LAEA9 */
        PXL = g.ram[A_HSCORL + x];                          /* LAEAB-LAEAE */
        PYL = g.ram[A_HSCORM + x];                          /* LAEB0-LAEB3 */
        PZL = g.ram[A_HSCORH + x];                          /* LAEB5-LAEB8 */
        digtys(A_PXL, 0x03);                                /* LAEBA-LAEBE: score */
        INDEX1 = (uint8_t)(INDEX1 - 1);                     /* LAEC1 */
        INDEX1 = (uint8_t)(INDEX1 - 1);                     /* LAEC3 */
        INDEX1 = (uint8_t)(INDEX1 - 1);                     /* LAEC5 */
    } while (!(INDEX1 & 0x80));                             /* LAEC7 */
}

/* BOLOUT ($AECA): "BONUS EVERY nn0000", then the ATARI literal checksum. */
void bolout(void)
{
    uint8_t a;
    int y;
    unsigned c;
    if (BLIFIN != 0) {                                      /* LAECA-LAECD: bonus life? */
        PZL = BLIFIN;                                       /* LAECF */
        msgs(K_MBOLIF);                                     /* LAED1-LAED3 */
        PXL = 0x00;                                         /* LAED6-LAED8 */
        PYL = 0x00;                                         /* LAEDA */
        digtys(A_PXL, 0x03);                                /* LAEDC-LAEE0 */
    }
    /* ZATLIV */
    c = 0;                                                  /* LAEE3 CLC */
    a = 0x85;                                               /* LAEE6 */
    for (y = K_ZATLIC; y >= 0; y--)                         /* LAEE4, LAEEB-LAEEC */
        a = adc6502(a, cpu_rd((uint16_t)(ALLANG_ZATLIS + y)), &c, NULL);   /* LAEE8 */
    QT1 = a;                                                /* LAEEE: verify ATARI literal */
}

/* OUTCUR ($AEF1): the initials being entered (TBLIND - ININDX). */
void outcur(void)
{
    unsigned c = 1;                                         /* LAEF4 SEC */
    outini(sbc6502(TBLIND, ININDX, &c, NULL));              /* LAEF1-LAEF5, falls into OUTINI */
}

/* OUTINI ($AEF8): the 3 initials at INITAL+A..A+2 (highest first) as JSRLs. */
void outini(uint8_t a)
{
    uint8_t x, y;
    unsigned c = 0;                                         /* LAEF8 CLC */
    INDEX2 = adc6502(a, 0x02, &c, NULL);                    /* LAEF9-LAEFB */
    y = 0x00;                                               /* LAEFD */
    INDEX3 = 0x02;                                          /* LAEFF-LAF01 */
    do {
        x = INDEX2;                                         /* LAF03 */
        a = g.ram[A_INITAL + x];                            /* LAF05 */
        if (a >= 0x1E) a = 0x1A;                            /* LAF08-LAF0C: invalid: space */
        x = (uint8_t)(a << 1);                              /* LAF0E-LAF0F */
        vg_put(y, cpu_rd((uint16_t)(A_VGMSGA + 22 + x)));   /* LAF10-LAF13 */
        y++;                                                /* LAF15 */
        vg_put(y, cpu_rd((uint16_t)(A_VGMSGA + 1 + 22 + x)));   /* LAF16-LAF19 */
        y++;                                                /* LAF1B */
        INDEX2 = (uint8_t)(INDEX2 - 1);                     /* LAF1C */
        INDEX3 = (uint8_t)(INDEX3 - 1);                     /* LAF1E */
    } while (!(INDEX3 & 0x80));                             /* LAF20 */
    vgadd((uint8_t)(y - 1));                                /* LAF22-LAF23 */
}

/* RNKDSP ($AF26): "RANKING FROM 1 TO 99" and each player's rank. */
void rnkdsp(void)
{
    if ((RANKS | g.ram[A_RANKS + 1]) == 0) return;          /* LAF26-LAF2C */
    msgs(K_MRANK);                                          /* LAF2E-LAF30 */
    onernk(0x63);                                           /* LAF33-LAF35 */
    pl1rnk(0x00);                                           /* LAF38-LAF3A */
    pl1rnk(0x01);                                           /* LAF3D, falls into PL1RNK */
}

/* PL1RNK ($AF3F): rank of player X (none if 0). */
void pl1rnk(uint8_t x)
{
    uint8_t a = g.ram[A_RANKS + x];                         /* LAF3F */
    if (a == 0) return;                                     /* LAF42 */
    TEMPX = x;                                              /* LAF44 PHA / LAF45 */
    nwcolo(K_RED);                                          /* LAF47-LAF49 */
    vgcntr_ab0d();                                          /* LAF4C */
    vgvtr1(0xD0, cpu_rd((uint16_t)(ROM_HITRNK + TEMPX)));   /* LAF4F-LAF56 */
    onernk(a);                                              /* LAF59-LAF5A PLA / JSR */
    vgdot(0xA0);                                            /* LAF5D-LAF5F */
    msgfol(0x10, K_MPLYR2);                                 /* LAF62-LAF66 */
    dplrxx(TEMPX);                                          /* LAF69-LAF6B */
}

/* ONERNK ($AF71): rank A, max NRANKS. */
void onernk(uint8_t a)
{
    if (a >= K_NRANKS) a = K_NRANKS;                        /* LAF71-LAF75 */
    dsp1hx(a);                                              /* falls into DSP1HX */
}

/* DSP1HX ($AF77): 2 BCD digits of binary A, zero suppressed. */
void dsp1hx(uint8_t a)
{
    (void)hexbcd(a);                                        /* LAF77 */
    digtys(A_TEMP0, 0x01);                                  /* LAF7A-LAF7E JMP DIGTYS */
}

/* RQRDSP ($AF81): display state - player rating request (RATE YOURSELF). */
void rqrdsp(void)
{
    uint8_t a, x, y;
    unsigned c;
    int n, z;
    (void)cocfli();                                         /* LAF81 */
    CK(0xAF84);
    SECUVY = (uint8_t)(SECUVY - 1);                         /* LAF84 */
    nwcolo(K_RED);                                          /* LAF87-LAF89 */
    VGSIZE = 0x01;                                          /* LAF8C-LAF8E */
    vgsca1(0x01);                                           /* LAF90 */
    msgen3(0x60, K_MATARI);                                 /* ZATC2S LAF93-LAF97 */
    dplrno();                                               /* LAF9A */
    INDEX1 = (uint8_t)(ROM_ENDMSG - ROM_MSGTAB - 1);        /* ZATC2E LAF9D-LAF9F */
    do {
        msgs(cpu_rd((uint16_t)(ROM_MSGTAB + INDEX1)));      /* LAFA1-LAFA6 */
        INDEX1 = (uint8_t)(INDEX1 - 1);                     /* LAFA9 */
    } while (!(INDEX1 & 0x80));                             /* LAFAB */
    /* if the cursor is at the edge of the visible screen, try to scroll */
    c = 1;                                                  /* LAFB0 SEC */
    n = sbc_n(CURSL1, LEFSID, c);
    a = sbc6502(CURSL1, LEFSID, &c, &z);                    /* LAFAD-LAFB1 */
    if (n) {                                                /* LAFB3 BPL */
        LEFSID = (uint8_t)(LEFSID - 1);                     /* LAFB5 */
        RITSID = (uint8_t)(RITSID - 1);                     /* LAFB7 */
    } else if (z) {                                         /* LAFBC BNE: at the left side */
        RITSID = (uint8_t)(RITSID - 1);                     /* LAFBE: scroll left */
        LEFSID = (uint8_t)(LEFSID - 1);                     /* LAFC0 */
        if (LEFSID & 0x80) {                                /* LAFC2: valid? */
            LEFSID = (uint8_t)(LEFSID + 1);                 /* LAFC4: no - unscroll */
            RITSID = (uint8_t)(RITSID + 1);                 /* LAFC6 */
        }
    } else {
        a = RITSID;                                         /* LAFCB */
        if (a <= HIRATE) {                                  /* LAFCD-LAFD2: BEQ YES / BCS LAFE1 */
            c = 1;                                          /* YES LAFD4 SEC */
            a = sbc6502(a, CURSL1, &c, &z);                 /* LAFD5 */
            if (z) c = 0;                                   /* LAFD8-LAFDA */
            if (!c) {                                       /* LAFDB: scroll right valid? */
                LEFSID = (uint8_t)(LEFSID + 1);             /* LAFDD */
                RITSID = (uint8_t)(RITSID + 1);             /* LAFDF */
            }
        }
    }
    INDEX4 = RITSID;                                        /* LAFE1-LAFE3 */
    INDEX1 = 0x04;                                          /* LAFE5-LAFE7 */
    do {
        nwcolo(K_GREEN);                                    /* LAFE9-LAFEB */
        VGBRIT = 0x00;                                      /* LAFEE-LAFF0 */
        vgcntr_ab0d();                                      /* LAFF2: level number */
        c = 0;                                              /* LAFFC CLC */
        a = adc6502(cpu_rd((uint16_t)(ROM_XPOTAB + INDEX1)), 0xF8, &c, NULL);   /* LAFF5-LAFFD */
        vgvtr1(a, 0xD8);                                    /* LAFFF */
        x = INDEX4;                                         /* LB002 */
        y = cpu_rd((uint16_t)(ROM_LEVEL + x));              /* LB004 */
        if (y < 0x63) {                                     /* LB007-LB009: in range? */
            dsp1hx((uint8_t)(y + 1));                       /* LB00B-LB00D */
            nwcolo(K_RED);                                  /* LB010-LB012 */
            vgcntr_ab0d();                                  /* LB015: bonus points */
            c = 0;                                          /* LB01F CLC */
            a = adc6502(cpu_rd((uint16_t)(ROM_XPOTAB + INDEX1)), 0xEC, &c, NULL);   /* LB018-LB020 */
            vgvtr1(a, 0xBA);                                /* LB022 */
            bodspl(INDEX4);                                 /* LB025-LB027 */
            vgcntr_ab0d();                                  /* LB02A */
            c = 0;                                          /* LB034 CLC */
            a = adc6502(cpu_rd((uint16_t)(ROM_XPOTAB + INDEX1)), 0x00, &c, NULL);   /* LB02D-LB035 */
            vgvtr1(a, 0xCC);                                /* LB037 */
            x = INDEX4;                                     /* LB03A: hole */
            dsphol(cpu_rd((uint16_t)(ROM_LEVEL + x)));      /* LB03C-LB03F */
        }
        INDEX4 = (uint8_t)(INDEX4 - 1);                     /* LB042 */
        INDEX1 = (uint8_t)(INDEX1 - 1);                     /* LB044 */
    } while (!(INDEX1 & 0x80));                             /* LB046 */
    /* time left */
    VGBRIT = 0x00;                                          /* LB048-LB04A */
    vgcntr_ab0d();                                          /* LB04C */
    msgs(K_MTIME);                                          /* LB04F-LB051 */
    digtys(A_QTMPAUS, 0x01);                                /* LB054-LB058 */
    /* cursor: box around the level */
    nwcolo(K_WHITE);                                        /* LB05B-LB05D */
    vgcntr_ab0d();                                          /* LB060 */
    a = getcur();                                           /* LB063-LB065 (X = $B8) */
    c = 1;                                                  /* LB068 SEC */
    y = sbc6502(a, LEFSID, &c, NULL);                       /* LB069-LB06B: relative */
    c = 1;                                                  /* LB06F SEC */
    a = sbc6502(cpu_rd((uint16_t)(ROM_XPOTAB + y)), 0x16, &c, NULL);   /* LB06C-LB070 */
    vgvtr1(a, 0xB8);                                        /* LB072: upper right corner */
    VGBRIT = 0xE0;                                          /* LB075-LB077: beam on */
    /* DOBOX ($B07B), X = 0 */
    INDEX2 = 0x00;                                          /* LB079-LB07B */
    INDEX1 = 0x03;                                          /* LB07D-LB07F */
    do {
        y = INDEX2;                                         /* LB081 */
        x = cpu_rd((uint16_t)(ROM_BOXTAB + y));             /* LB083-LB086 */
        y++;                                                /* LB087 */
        a = cpu_rd((uint16_t)(ROM_BOXTAB + y));             /* LB088 */
        y++;                                                /* LB08B */
        INDEX2 = y;                                         /* LB08C */
        vgvtr1(a, x);                                       /* LB08E */
        INDEX1 = (uint8_t)(INDEX1 - 1);                     /* LB091 */
    } while (!(INDEX1 & 0x80));                             /* LB093 */
}

/* GETCUR ($B0AB): move the rating cursor CURSL1 by the spinner, limited to
 * 0..HIRATE.  Returns A = Y = CURSL1; X unchanged. */
uint8_t getcur(void)
{
    uint8_t a = ginico(CURSL1);                             /* LB0AB-LB0AE */
    if (a & 0x80) a = 0x00;                                 /* LB0B1-LB0B7: min */
    else if (a >= HIRATE) a = HIRATE;                       /* LB0B9-LB0BE: max */
    CURSL1 = a;                                             /* LB0C1 */
    return a;                                               /* LB0C4-LB0C5 TAY / RTS */
}

/* BODSPL ($B0C6): bonus points for level index X. */
void bodspl(uint8_t x)
{
    (void)bonsco(x, x, 0);                                  /* LB0C6-LB0C7 TXA / JSR BONSCO */
    digtys(A_TEMP0, 0x03);                                  /* LB0CA-LB0CE */
}

/* NWCOLO ($B0D1): colour Y, if it changed. */
void nwcolo(uint8_t y)
{
    if (y == COLOR) return;                                 /* LB0D1-LB0D3 */
    COLOR = y;                                              /* LB0D5 */
    vgstat(0x08, y);                                        /* LB0D7-LB0D9 JMP VGSTAT */
}

/* NWSCA1 ($B0DD): binary scale A, if it changed. */
void nwsca1(uint8_t a)
{
    if (a == VGSIZE) return;                                /* LB0DD-LB0DF */
    VGSIZE = a;                                             /* LB0E1 */
    vgsca1(a);                                              /* LB0E3 JMP VGSCA1 */
}

/* ======================================================================= */
/* LOGO                                                                    */
/* ======================================================================= */

/* LOGINI ($B0E7): state routine - logo initialisation. */
xy6502 logini(uint8_t x, uint8_t y)
{
    QSTATE = K_CPAUSE;                                      /* LB0E7-LB0E9: pause for the entire logo */
    QNXTSTA = K_CNEWGA;                                     /* LB0EB-LB0ED: then go to game */
    QTMPAUS = 0xDF;                                         /* LB0EF-LB0F1 */
    QDSTATE = K_CDBOXP;                                     /* LB0F3-LB0F5: shrinking box rainbow first */
    FARY = 0x19;                                            /* LB0F7-LB0F9: starting close */
    NEARY = 0x18;                                           /* LB0FC-LB0FE */
    return xy(x, y);                                        /* LB101 */
}

/* BOXPRO ($B102): display state - shrinking box rainbow. */
void boxpro(void)
{
    uint8_t a;
    unsigned c;
    scarng((uint8_t)((A_VORBOX + 1) >> 8), (uint8_t)(A_VORBOX & 0xFF));   /* LB102-LB106 */
    a = FARY;                                               /* LB109 */
    if (a < 0xA0) {                                         /* LB10C-LB10E: far point past destination? */
        c = 0;                                              /* (C clear from CMP) */
        a = adc6502(a, 0x14, &c, NULL);                     /* LB110: move it farther */
        FARY = a;                                           /* LB112 */
    }
    if (a < 0x50) return;                                   /* LB115-LB117 */
    c = 0;                                                  /* LB11C CLC */
    a = adc6502(NEARY, K_FARINC, &c, NULL);                 /* LB119-LB11D */
    NEARY = a;                                              /* LB11F */
    if (a < FARY) return;                                   /* LB122-LB125 */
    NEARY = 0xA0;                                           /* LB127-LB129 */
    QDSTATE = K_CDLOGP;                                     /* LB12C-LB12E: now the growing logo */
}

/* LOGPRO ($B131): display state - approaching logo rainbow. */
void logpro(void)
{
    uint8_t a;
    unsigned c;
    scarng((uint8_t)((A_VORLIT + 1) >> 8), (uint8_t)(A_VORLIT & 0xFF));   /* LB131-LB135 */
    a = NEARY;                                              /* LB138 */
    if (a >= 0x30) {                                        /* LB13B-LB13D: near point past destination? */
        c = 1;                                              /* (C set from CMP) */
        a = sbc6502(a, 0x01, &c, NULL);                     /* LB13F: bring it closer */
        NEARY = a;                                          /* LB141 */
    }
    if (a >= 0x80) return;                                  /* LB144-LB146 */
    c = 1;                                                  /* LB14B SEC */
    a = sbc6502(FARY, 0x01, &c, NULL);                      /* LB148-LB14C */
    if (a < NEARY) a = NEARY;                               /* LB14E-LB153 */
    FARY = a;                                               /* LB156 */
}

/* SCARNG ($B15A): logo rainbow builder - the picture at A:X (MSB:LSB) drawn
 * from NEARY to FARY in steps of 2, scale and colour by distance; then the
 * copyright and the beam killer. */
void scarng(uint8_t a, uint8_t x)
{
    uint8_t y;
    unsigned c;
    PYL = a;                                                /* LB15A: pointer to picture */
    PXL = x;                                                /* LB15C */
    INDEX1 = NEARY;                                         /* LB15E-LB161 */
    SECUVY = (uint8_t)(SECUVY - 1);                         /* LB163 */
    do {
        y = (uint8_t)((uint8_t)(INDEX1 << 2) & 0x7F);       /* LB166-LB16C: linear scale */
        vgscal((uint8_t)(INDEX1 >> 5), y);                  /* LB16D-LB174: binary scale */
        a = INDEX1;                                         /* LB177 */
        if (a == NEARY) {                                   /* LB179-LB17C: leading point? */
            a = K_WHITE;                                    /* LB17E */
        } else {
            a = (uint8_t)((a >> 3) & 0x07);                 /* LB183-LB187 (NOP) */
            if (a == 0x07) a = K_RED;                       /* LB189-LB18D: red for black */
        }
        vgstat(0x68, a);                                    /* LB18F-LB192 TAY / LDA #$68 */
        vgjsrl(PYL, PXL);                                   /* LB195-LB199 */
        c = 0;                                              /* ZATC1S LB19E CLC */
        INDEX1 = adc6502(INDEX1, 0x02, &c, NULL);           /* LB19C-LB1A1 */
    } while (INDEX1 < FARY);                                /* LB1A3-LB1A6 */
    msgen3(0xD0, K_MATARI);                                 /* LB1A8-LB1AC */
    vgjsrl((uint8_t)((A_KILLER + 1) >> 8), (uint8_t)(A_KILLER & 0xFF));   /* LB1AF-LB1B3 JMP VGJSRL */
}
