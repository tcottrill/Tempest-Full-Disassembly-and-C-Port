/* alexec.c - ALEXEC ($C7A0-$CB00): the executive - MAINLN frame loop, the
 * ROUTAD state dispatch, credits / start, new game, new life, end of wave,
 * end of life / game, cocktail flip and score update.
 *
 * Register protocol: X and Y leave these routines in xy6502 results and enter
 * as parameters wherever they can reach a store (the sound starters save the
 * caller's X/Y in MTEMP), including through the ROUTAD dispatch and the ROM
 * thunks for routines not translated yet.  The 6502 D flag lives in g.dflag.
 * Verified by tests/lockstep.exe: per call, per ROUTAD entry (the dispatch
 * RTSes into the state routine), and per whole MAINLN pass.
 */
#include "state.h"
#include "hw.h"
#include "game.h"

#define ROUTAD   0xC7DA       /* ROUTAD: state routine addresses - 1 (ROM table) */
#define TUPSCL   0xCAF1       /* TUPSCL: score LSBs per enemy (ROM table, 8)     */
#define TUPSCM   0xCAF9       /* TUPSCM: score middle bytes (ROM table, 8)       */
#define TUPS_N   0x08         /* <[TUPSLE-TUPSCL]                                */

#define LIVES_X(x)  g.ram[A_LIVES1 + (x)]
#define WAVEN_X(x)  g.ram[A_WAVEN1 + (x)]

static xy6502 xy(uint8_t x, uint8_t y) { xy6502 r; r.x = x; r.y = y; return r; }

/* MAINLN ($C7A0): power-on entry (RESET's JMP MAINLN) up to the first frame. */
void mainln(void)
{
    xy6502 r = inisou();                        /* LC7A0 JSR INISOU */
    QSTATE = K_CNEWGA;                          /* LC7A3-LC7A5 */
    g.loop_x = r.x; g.loop_y = r.y;
    hw_wait_frame();                            /* LC7A7-LC7AB: wait FRTIMR >= 9 */
}

/* MAINLN ($C7AD): one pass of the frame loop, from the frame-timer restart
 * through DISPLAY and the wait for the next frame (the loop rotated so a
 * pass starts where tests\refrun.exe captures, $C7AD). */
void mainln_pass(void)
{
    xy6502 r;
    FRTIMR = 0x00;                              /* LC7AD-LC7AF: restart frame timer */
    CK(0xC7B1);
    r = exstat(g.loop_x, g.loop_y);             /* LC7B1 */
    CK(0xC7B4);
    r = nonsta(r.x, r.y);                       /* LC7B4 */
    CK(0xC7B7);
    r = display(r.x, r.y);                      /* LC7B7 */
    g.loop_x = r.x; g.loop_y = r.y;
    g.pass_count++;
    hw_wait_frame();                            /* LC7BA-LC7BB CLC/BCC, LC7A7-LC7AB */
}

/* EXSTAT ($C7BD): execute the state routine QSTATE indexes in ROUTAD.  The
 * ROM pushes the address - 1 and RTSes into it; the C port reads the same
 * table and calls the routine at that address. */
xy6502 exstat(uint8_t x, uint8_t y)
{
    uint16_t target;
    xy6502 r;
    if ((hw_inop0() & 0x83) == 0x82)            /* LC7BD-LC7C4: freeze & free play? */
        return xy(x, y);                        /* NOOPR LC7D9 RTS */
    r = prstar(x, y);                           /* LC7C6 PROCESS STAR FIELD */
    CK(0xC7C9);
    x = QSTATE;                                 /* LC7C9 */
    {
        uint8_t sw = SWFINA;                    /* LC7CB */
        CK(0xC7CD);
        SWFINA = (uint8_t)(sw | K_MFAKE);       /* LC7CD-LC7CF: set must-process flag */
    }
    CK(0xC7D1);
    target = (uint16_t)((cpu_rd((uint16_t)(ROUTAD + x)) |
                         (cpu_rd((uint16_t)(ROUTAD + 1 + x)) << 8)) + 1);  /* LC7D1-LC7D9 PHA/PHA/RTS */
    y = r.y;
    switch (target) {
    case 0xC90C: return newgam(x, y);           /* CNEWGA  NEWGAM */
    case 0xC940: return newlif(x, y);           /* CNEWLI  NEWLIF */
    case 0x970B: return play(x, y);             /* CPLAY   PLAY */
    case 0xC9AF: return endlif(x, y);           /* CENDLI  ENDLIF */
    case 0xC9F1: return endgam(x, y);           /* CENDGA  ENDGAM */
    case 0xC800: return routen(x, y);           /* CPAUSE  PAUSE */
    case 0xC98C: return endwav(x, y);           /* CENDWAV ENDWAV */
    case 0xAC3F: return hischk(x, y);           /* CHISCHK HISCHK */
    case 0xAD6E: return getini(x, y);           /* CGETINI GETINI */
    case 0xCA18: return dladr(x, y);            /* CDLADR  DLADR */
    case 0x9149: return prorat(x, y);           /* CREQRAT PRORAT */
    case 0x904B: return newav2(x, y);           /* CNEWV2  NEWAV2 */
    case 0xB0E7: return logini(x, y);           /* CLOGO   LOGINI */
    case 0x9108: return inirat(x, y);           /* CINIRAT INIRAT */
    case 0xC97B: return newlf2(x, y);           /* CNWLF2  NEWLF2 */
    case 0x9729: return pldrop(x, y);           /* CDROP   PLDROP */
    case 0xD7E1: return system_(x, y);          /* CSYSTM  SYSTEM */
    case 0xA618: return prboom(x, y);           /* CBOOM   PRBOOM */
    default:
        /* CNEWAV's entry is .word $0000 (RTS to $0001) and odd / large QSTATE
         * values index past the table: the ROM runs wild.  Never reached. */
        hw_soft_watchdog();
        return xy(x, y);
    }
}

/* ROUTEN ($C800) = PAUSE: count QTMPAUS down every PSCALE-masked frame, then
 * go to QNXTSTA. */
xy6502 routen(uint8_t x, uint8_t y)
{
    if ((QFRAME & PSCALE) == 0) {               /* LC800-LC805 */
        int z = 1;                              /* LC807-LC809: at 0? stop at 0 */
        if (QTMPAUS != 0) {
            QTMPAUS = (uint8_t)(QTMPAUS - 1);   /* LC80B */
            z = (QTMPAUS == 0);
        }
        if (z) {                                /* LC80D */
            QSTATE = QNXTSTA;                   /* LC80F-LC811 */
            PSCALE = 0x00;                      /* LC813-LC815: standard timer scale */
        }
    }
    return movcur(x, y);                        /* LC818 JMP MOVCUR */
}

/* PROCRE ($C81B): process credits - start buttons, attract "press start". */
xy6502 procre(uint8_t x, uint8_t y)
{
    int two = (S_S_CRDT >= 0x02);               /* LC81B-LC81F: C set if 2 or more */
    uint8_t a = (uint8_t)(SWFINA & (K_MSTRT2 | K_MSTRT1));  /* LC821-LC823 */
    y = 0x00;                                   /* LC81D */
    CK(0xC823);
    SWFINA = y;                                 /* LC825 */
    CK(0xC827);
    if (a != 0) {                               /* LC827: either start pressed? */
        if (!two) {                             /* LC829 */
            a &= K_MSTRT1;                      /* LC82B: 1 credit */
        } else {
            y++;                                /* LC830: 2 or more credits */
            S_S_CRDT = (uint8_t)(S_S_CRDT - 1); /* LC831 */
            a &= K_MSTRT2;                      /* LC833 */
        }
        if (a != 0) {                           /* LC835 */
            S_S_CRDT = (uint8_t)(S_S_CRDT - 1); /* LC837 */
            y++;                                /* LC839 */
        }
        NUMPLA = y;                             /* LC83A-LC83B */
        if (y != 0) {                           /* LC83D: game? */
            unsigned c;
            QSTATUS = (uint8_t)(QSTATUS | K_MATRACT | K_MGTMOD);  /* LC83F-LC843 */
            S_BCCNT = 0x00;                     /* LC845-LC847 */
            S_BC = 0x00;                        /* LC849 */
            QSTATE = K_CNEWGA;                  /* LC84B-LC84D */
            NUMPLA = (uint8_t)(NUMPLA - 1);     /* LC84F */
            x = NUMPLA;                         /* LC851 */
            if (x != 0) x = 0x03;               /* LC853-LC855 */
            g.ram[A_NGAMIL + x] = (uint8_t)(g.ram[A_NGAMIL + x] + 1);          /* LC857 */
            if (g.ram[A_NGAMIL + x] == 0)
                g.ram[A_NGAMIH + x] = (uint8_t)(g.ram[A_NGAMIH + x] + 1);      /* LC85C */
            c = 1;                              /* LC862 SEC */
            a = adc6502(NGAMES, NUMPLA, &c, NULL);                            /* LC85F-LC863 */
            if (a >= K_NRANKS) a = K_NRANKS;    /* LC865-LC869 */
            NGAMES = a;                         /* LC86B */
        }
    } else if (TBHD != 0 && !(QSTATUS & 0x80)) {  /* LC871-LC877: trying to play in attract? */
        QDSTATE = K_CDPRST;                     /* LC879-LC87B */
        QTMPAUS = 0x20;                         /* LC87D-LC87F */
        QSTATE = K_CPAUSE;                      /* LC881-LC883 */
        QNXTSTA = K_CDLADR;                     /* LC885-LC887 */
        TBHD = 0x00;                            /* LC889-LC88B */
        ELICNT = 0x00;                          /* LC88D */
    }
    CK(0xC890);
    return xy(x, y);                            /* LC890 */
}

/* NONSTA ($C891): non-state-dependent processing - self test switch, the
 * 2-game minimum, credits (NOSTART $C8D9), EAROM, slam, protection. */
xy6502 nonsta(uint8_t x, uint8_t y)
{
    xy6502 r;
    if (!(hw_in1() & K_MTEST)) {                /* LC891-LC896: system status display? */
        QSTATE = K_CSYSTM;                      /* LC898-LC89A */
        goto frame;                             /* LC89C-LC89D */
    }
    if (QSTATUS & 0x40) goto frame;             /* LC89F-LC8A1 BIT / BVS */
    if (OPTIN2 & K_OM2GAM) {                    /* LC8A3-LC8A7: 2 game min option? */
        y = S_S_CRDT;                           /* LC8A9 */
        if (y == 0) TCMFLG = 0x80;              /* LC8AB-LC8AF */
        if (TCMFLG & 0x80) {                    /* LC8B1-LC8B3 */
            if (y < 0x02) {                     /* LC8B5-LC8B7 */
                if (y != 0) {                   /* LC8B9-LC8BA: 1 credit? */
                    QDSTATE = K_CD2GAM;         /* LC8BC-LC8BE */
                    QSTATE = K_CPAUSE;          /* LC8C0-LC8C2 */
                }
                goto nostart;                   /* LC8C4 JMP NOSTART (LC8C7-LC8C8 dead) */
            }
            QSTATE = K_CDLADR;                  /* LC8CA-LC8CC */
            TCMFLG = 0x00;                      /* LC8CE-LC8D0: enable start */
        }
    }
    if (S_S_CRDT != 0) {                        /* LC8D2-LC8D4 */
        r = procre(x, y);                       /* LC8D6 */
        x = r.x; y = r.y;
    }
nostart:                                        /* NOSTART ($C8D9) */
    CK(0xC8D9);
    if ((S_CMODE & 0x03) == 0)                  /* LC8D9-LC8DD: free play? */
        S_S_CRDT = 0x02;                        /* LC8DF-LC8E1 */
frame:
    CK(0xC8E3);
    QFRAME = (uint8_t)(QFRAME + 1);             /* LC8E3 */
    if (QFRAME & 0x01) {                        /* LC8E5-LC8E9 */
        r = eaupd(x, y);                        /* LC8EB PROCESS EAROM */
        x = r.x; y = r.y;
    }
    CK(0xC8EE);
    if (S_LMTIM != 0) sslams(x, y);             /* LC8EE-LC8F2 (ZQAT4C) */
    if (QT2 != 0 && CURWAV > 0x13)              /* LC8F5-LC8FE */
        g.dflag = 1;                            /* LC900 SED: protection - decimal mode stays on */
    {
        uint8_t sw = SWFINA;                    /* LC901 */
        CK(0xC903);
        if (sw & K_MFAKE)                       /* LC903-LC905: switch processed this frame? */
            SWFINA = 0x00;                      /* LC907-LC909: no - fake process */
    }
    CK(0xC90B);
    return xy(x, y);                            /* LC90B */
}

/* NEWGAM ($C90C): prepare a new game. */
xy6502 newgam(uint8_t x, uint8_t y)
{
    xy6502 r = inichk(x, y);                    /* LC90C */
    r = inidsp(r.x, r.y);                       /* LC90F */
    x = r.x; y = r.y;
    if (QSTATUS & 0x80) {                       /* LC912-LC914: attract? */
        clrsco();                               /* LC916: no - clear scores */
        x = 0xFF;
    }
    LIVES2 = 0x00;                              /* LC919-LC91B */
    x = NUMPLA;                                 /* LC91D */
    PLAYUP = x;                                 /* LC91F */
    do {
        x = PLAYUP;                             /* LC921 */
        LIVES_X(x) = LVSGAM;                    /* LC923-LC926 */
        WAVEN_X(x) = 0xFF;                      /* LC929-LC92B: force request rate state */
        PLAYUP = (uint8_t)(PLAYUP - 1);         /* LC92E */
    } while (!(PLAYUP & 0x80));                 /* LC930 */
    NEWPLA = 0x00;                              /* LC932-LC934 */
    PLAGRO = 0x00;                              /* LC936: deactivate star field */
    PLAYUP = NUMPLA;                            /* LC939-LC93B */
    return inira0(x, y);                        /* LC93D JMP INIRA0 */
}

/* NEWLIF ($C940): prepare a new life. */
xy6502 newlif(uint8_t x, uint8_t y)
{
    uint8_t a;
    xy6502 r;
    QDSTATE = K_CDPLAY;                         /* LC940-LC942 */
    QSTATE = K_CNWLF2;                          /* LC944-LC946 */
    QNXTSTA = K_CNWLF2;                         /* LC948 */
    a = NEWPLA;                                 /* LC94A */
    if (a != PLAYUP) {                          /* LC94C-LC94E: same player as before? */
        PLAYUP = a;                             /* LC950 */
        if (QSTATUS & 0x80) {                   /* LC952-LC954: attract? */
            QDSTATE = K_CDPLPL;                 /* LC956-LC958 */
            QSTATE = K_CPAUSE;                  /* LC95A-LC95C */
            a = (uint8_t)(4 * K_SECOND);        /* LC95E */
            y = COCTAL;                         /* LC960 */
            if (y != 0) a = (uint8_t)(2 * K_SECOND);  /* LC963-LC965 */
            QTMPAUS = a;                        /* LC967 */
            r = swapen(x, y);                   /* LC969 */
            x = r.x; y = r.y;
        }
    }
    y = cocfli();                               /* LC96C */
    CK(0xC96F);
    x = PLAYUP;                                 /* LC96F */
    CURWAV = WAVEN_X(x);                        /* LC971-LC973 */
    r = inewli(x, y);                           /* LC975 */
    (void)r;
    return inisou();                            /* LC978 JMP INISOU */
}

/* NEWLF2 ($C97B): new life part 2 - pause 1 second, then play. */
xy6502 newlf2(uint8_t x, uint8_t y)
{
    QNXTSTA = K_CPLAY;                          /* LC97B-LC97D */
    QDSTATE = K_CDPLAY;                         /* LC97F-LC981 */
    QSTATE = K_CPAUSE;                          /* LC983-LC985 */
    QTMPAUS = (uint8_t)(1 * K_SECOND);          /* LC987-LC989 */
    return xy(x, y);                            /* LC98B */
}

/* ENDWAV ($C98C): end of wave - next wave number, start-level bonus. */
xy6502 endwav(uint8_t x, uint8_t y)
{
    uint8_t a;
    x = PLAYUP;                                 /* LC98C */
    if (WAVEN_X(x) < 0x62) {                    /* LC98E-LC992: max at 99 */
        WAVEN_X(x) = (uint8_t)(WAVEN_X(x) + 1); /* LC994 */
        CURWAV = (uint8_t)(CURWAV + 1);         /* LC996 */
    }
    QSTATE = K_CNEWV2;                          /* LC998-LC99A */
    a = g.ram[A_BONUS + x];                     /* LC99C */
    if (a != 0) {                               /* LC99F: bonus? */
        xy6502 r = bonsco(a, x, y);             /* LC9A1 */
        r = upscor(0xFF, r.y);                  /* LC9A4-LC9A6: X = $FF: points in TEMP0-2 */
        sauson(r.x, r.y);                       /* LC9A9 */
        x = r.x; y = r.y;
    }
    CK(0xC9AC);
    return inewav(x, y);                        /* LC9AC JMP INEWAV */
}

/* ENDLIF ($C9AF): a base was lost. */
xy6502 endlif(uint8_t x, uint8_t y)
{
    uint8_t a;
    QTMPAUS = (uint8_t)(0 * K_SECOND);          /* LC9AF-LC9B1: normally no pause */
    x = PLAYUP;                                 /* LC9B3 */
    LIVES_X(x) = (uint8_t)(LIVES_X(x) - 1);     /* LC9B5 */
    if ((LIVES1 | LIVES2) == 0)                 /* LC9B7-LC9BB: both dead? */
        return endgam(x, y);                    /* LC9BD, LC9C0-LC9C1 -> LC9F0 RTS */
    x = PLAYUP;                                 /* LC9C3 */
    if (LIVES_X(x) == 0) {                      /* LC9C5-LC9C7: current player dead? */
        QDSTATE = K_CDGOVR;                     /* LC9C9-LC9CB */
        QTMPAUS = (uint8_t)(2 * K_SECOND);      /* LC9CD-LC9CF */
    }
    do {
        if (NUMPLA != 0)                        /* LC9D1-LC9D3: 2 players? */
            NEWPLA = (uint8_t)(NEWPLA ^ 0x01);  /* LC9D5-LC9D9 */
        x = NEWPLA;                             /* LC9DB */
    } while (LIVES_X(x) == 0);                  /* LC9DD-LC9DF */
    a = K_CNEWLI;                               /* LC9E1 */
    y = WAVEN_X(x);                             /* LC9E3 */
    y++;                                        /* LC9E5 */
    if (y == 0) a = K_CINIRAT;                  /* LC9E6-LC9E8: new game for next player? */
    QNXTSTA = a;                                /* LC9EA */
    QSTATE = K_CPAUSE;                          /* LC9EC-LC9EE */
    return xy(x, y);                            /* LC9F0 */
}

/* ENDGAM ($C9F1): end of game - highest wave reached, hi-score check. */
xy6502 endgam(uint8_t x, uint8_t y)
{
    HIWAVE = 0x00;                              /* LC9F1-LC9F3 */
    x = NUMPLA;                                 /* LC9F6 */
    do {
        if (WAVEN_X(x) >= HIWAVE)               /* LC9F8-LC9FD */
            HIWAVE = WAVEN_X(x);                /* LC9FF */
        x--;                                    /* LCA02 */
    } while (!(x & 0x80));                      /* LCA03 */
    y = HIWAVE;                                 /* LCA05 */
    if (y != 0) HIWAVE = (uint8_t)(HIWAVE - 1); /* LCA08-LCA0A */
    QSTATE = (QSTATUS & 0x80) ? K_CHISCHK : K_CDLADR;  /* LCA0D-LCA15 */
    return xy(x, y);                            /* LCA17 */
}

/* DLADR ($CA18): no high score to enter - back to attract via the
 * high-score table and the logo. */
xy6502 dladr(uint8_t x, uint8_t y)
{
    QSTATUS = (uint8_t)(QSTATUS & (uint8_t)~(K_MATRACT | K_MGTMOD));  /* LCA18-LCA1C */
    NUMPLA = 0x00;                              /* LCA1E-LCA20 */
    QNXTSTA = K_CLOGO;                          /* LCA22-LCA24 */
    QSTATE = K_CPAUSE;                          /* LCA26-LCA28 */
    QTMPAUS = 0xA0;                             /* LCA2A-LCA2C */
    PSCALE = 0x01;                              /* LCA2E-LCA30: double time */
    QDSTATE = K_CDHITB;                         /* LCA33-LCA35 */
    return xy(x, y);                            /* LCA37 */
}

/* COCFLI ($CA48): cocktail flip - FLIP for player 2 of a cocktail game.
 * Returns Y (the TOUT0 value). */
uint8_t cocfli(void)
{
    uint8_t y = K_MVINVY;                       /* LCA48 */
    uint8_t a = COCTAL;                         /* LCA4A */
    if (a != 0) {                               /* LCA4D */
        a = PLAYUP;                             /* LCA4F */
        if (a != 0) {                           /* LCA51: player 2? */
            a = K_MFLIP;                        /* LCA53 */
            y = K_MVINVX;                       /* LCA55 */
        }
    }
    TNKOUT = (uint8_t)(((a ^ TNKOUT) & K_MFLIP) ^ TNKOUT);  /* LCA57-LCA5D */
    TOUT0 = y;                                  /* LCA5F */
    return y;                                   /* LCA61 */
}

/* CLRSCO ($CA62): clear both scores (exits X = $FF). */
void clrsco(void)
{
    int x;
    for (x = 5; x >= 0; x--)                    /* LCA62-LCA69 */
        g.ram[A_LSCORL + x] = 0x00;             /* LCA66 */
}

/* UPSCOR ($CA6C): add points (X < 8: TUPSCL/TUPSCM entry; else TEMP0-2) to
 * the player's BCD score and award bonus lives (GIVBON $CADC).  Decimal
 * mode; the carry out of the middle byte is the "passed a 10K boundary"
 * flag (PHP/PLP around the high byte). */
xy6502 upscor(uint8_t x, uint8_t y)
{
    uint8_t a;
    unsigned c, c_mid;
    int z;
    g.dflag = 1;                                /* LCA6C SED */
    if (!(QSTATUS & 0x80)) goto done;           /* LCA6D-LCA6F: attract? */
    y = PLAYUP;                                 /* LCA71 */
    if (y != 0) y = 0x03;                       /* LCA73-LCA75: player 2 */
    if (x >= TUPS_N) {                          /* LCA77-LCA79: bonus in table? */
        c = 0;                                  /* LCA7D CLC */
        g.ram[A_LSCORL + y] = adc6502(TEMP0, g.ram[A_LSCORL + y], &c, NULL);  /* LCA7B-LCA81 */
        g.ram[A_LSCORM + y] = adc6502(TEMP1, g.ram[A_LSCORM + y], &c, NULL);  /* LCA84-LCA89 */
        a = TEMP2;                              /* LCA8C */
        z = (a == 0);                           /* (LCA8E CLV / LCA8F BVC keep Z) */
    } else {
        c = 0;                                  /* LCA94 CLC */
        g.ram[A_LSCORL + y] = adc6502(cpu_rd((uint16_t)(TUPSCL + x)), g.ram[A_LSCORL + y], &c, NULL); /* LCA91-LCA98 */
        g.ram[A_LSCORM + y] = adc6502(cpu_rd((uint16_t)(TUPSCM + x)), g.ram[A_LSCORM + y], &c, NULL); /* LCA9B-LCAA1 */
        a = 0x00;                               /* LCAA4 */
        z = 1;
    }
    c_mid = c;                                  /* LCAA6 PHP (C and Z) */
    a = adc6502(a, g.ram[A_LSCORH + y], &c, NULL);  /* LCAA7 */
    g.ram[A_LSCORH + y] = a;                    /* LCAAA */
    c = c_mid;                                  /* LCAAD PLP */
    if (!z) {                                   /* LCAAE: big bonus? */
        x = BLIFIN;                             /* LCAB0 */
        if (x != 0) {                           /* LCAB3: bonus allowed? */
            if (x == TEMP2) goto givbon;        /* LCAB5-LCAB7 */
            c = (x >= TEMP2);
            if (!c) goto givbon;                /* LCAB9: bonus >= interval */
        }
    }
    if (!c) goto done;                          /* LCABB: passed a 10K boundary? */
    x = BLIFIN;                                 /* LCABD: bonus life interval (10K units) */
    if (x == 0) goto sec_done;                  /* LCAC0 */
    if (x >= 0x03) {                            /* LCAC2-LCAC4: over 20K interval? */
        do {
            c = 1;                              /* LCAC6 SEC */
            a = sbc6502(a, BLIFIN, &c, &z);     /* LCAC7 */
            if (z) goto givbon;                 /* LCACA: no remainder */
        } while (c);                            /* LCACC */
        goto sec_done;                          /* LCACE-LCACF */
    }
    if (x != 0x02) goto givbon;                 /* LCAD1-LCAD3: 20K interval? */
    if ((a & 0x01) == 0) goto givbon;           /* LCAD5-LCAD7 */
    goto sec_done;                              /* LCAD9-LCADA */

givbon:                                         /* GIVBON ($CADC): give a bonus life */
    x = PLAYUP;                                 /* LCADC */
    if (LIVES_X(x) < 0x06) {                    /* LCADE-LCAE2: max at 6 */
        LIVES_X(x) = (uint8_t)(LIVES_X(x) + 1); /* LCAE4 */
        sauson(x, y);                           /* LCAE6 */
        BOFLASH = 0x20;                         /* LCAE9-LCAEB */
    }
sec_done:
    /* LCAEE SEC: carry out, not a documented result */
done:
    g.dflag = 0;                                /* LCAEF CLD */
    return xy(x, y);                            /* LCAF0 */
}
