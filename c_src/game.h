/* game.h - entry points of the translated program ROM.
 *
 * One C function per original routine, named after Atari's label in lower
 * case ($ -> s_), ROM address in a comment.  Register protocol: A/X/Y/C inputs
 * become parameters, documented outputs become return values (C flag = int).
 * Status: ALVGUT, ALHAR2 (M1), COIN65, ALSOUN (M2), ALEXEC, ALEARO (M3),
 * ALSCO2, ALLANG (M4), ALDIS2 (M5), ALWELG (M6) translated and
 * lockstep-verified; ALTES2 translated (M9 B1, verification at M9 B3/B4).
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>

/* CK(0xLLLL): checkpoint - the C statement of ROM address LLLL (M5).  Under
 * tests\lockstep.exe (/DLOCKSTEP) the ROM's visits to these addresses are
 * events the C run must meet in order, and IRQs are anchored to them; every
 * address must be listed in lockstep.c's ck_addr[].  Anywhere else it
 * compiles to nothing. */
#ifdef LOCKSTEP
void lk_ck(uint16_t pc);
#define CK(pc) lk_ck(pc)
#else
#define CK(pc) ((void)0)
#endif

/* ---- ALVGUT ($DF09-$DFDB) - translated ---------------------------------- */
void vgrtsl(void);                              /* VGRTSL $DF09 */
void vghalt(void);                              /* VGHALT $DF0D */
int  vghexz(uint8_t a, int c);                  /* VGHEXZ $DF19  returns C */
int  vghex(uint8_t a);                          /* VGHEX  $DF1F  returns C (0) */
void vgjsrl(uint8_t a_msb, uint8_t x_lsb);      /* VGJSRL $DF39 */
void vgsta1(uint8_t a);                         /* VGSTA1 $DF4A */
void vgstat(uint8_t a, uint8_t y);              /* VGSTAT $DF4C */
void vgcntr(void);                              /* VGCNTR $DF53 */
void vgadd2(uint8_t a, uint8_t x);              /* VGADD2 $DF57 */
void vgadd3(uint8_t a, uint8_t x, uint8_t y);   /* VGADD3 $DF59 */
void vgadd(uint8_t y);                          /* VGADD  $DF5F */
void vgsca1(uint8_t a);                         /* VGSCA1 $DF6A */
void vgscal(uint8_t a, uint8_t y);              /* VGSCAL $DF6C */
void vgvtr(uint8_t a, uint8_t x, uint8_t y);    /* VGVTR  $DF73 */
void vgvtr1(uint8_t a, uint8_t x);              /* VGVTR1 $DF75 */
void vgvctr(uint8_t x);                         /* VGVCTR $DF92 */
void digtys(uint8_t a, uint8_t y);              /* DIGTYS $DFB1 */
void digits(uint8_t a, uint8_t y, int c);       /* DIGITS $DFB2 */

/* ---- ALHAR2 ($D703-$D7E0) - translated ---------------------------------- */
void irq(void);                                 /* IRQ    $D704 */
void softok(void);                              /* SOFTOK $D717 */

/* ---- ALCOIN / COIN65 ($CF24-$D030) - translated ------------------------- */
void moolah(void);           /* MOOLAH  $CF24  coin routine (IRQ) */
void s_bonus(void);          /* S_BONUS $CFBD  bonus adder        */
void s_extb(void);           /* S_EXTB  $CFE1  coins to credits   */

/* ---- ALSOUN ($CB01-$CDDD) - translated ---------------------------------- */
/* Sound starters take the caller's X and Y (saved in MTEMP by FSNDON). */
void sndon(uint8_t a, uint8_t x, uint8_t y);    /* SNDON  $CCC3 */
void fsndon(uint8_t a, uint8_t x, uint8_t y);   /* FSNDON $CCC7 */
void ipexpl(uint8_t x, uint8_t y);              /* IPEXPL $CCB0 (= CPEXPL) */
#define cpexpl ipexpl
void sboing(uint8_t x, uint8_t y);              /* SBOING $CCB5 */
void sauson(uint8_t x, uint8_t y);              /* SAUSON $CCB9 */
void eslson(uint8_t x, uint8_t y);              /* ESLSON $CCBD */
void ccexpl(uint8_t x, uint8_t y);              /* CCEXPL $CCC1 (= CIEXPL = EXSNON) */
#define ciexpl ccexpl
#define exsnon ccexpl
void slaunc(uint8_t x, uint8_t y);              /* SLAUNC $CCEA */
void souts2(uint8_t x, uint8_t y);              /* SOUTS2 $CCEE */
void souts3(uint8_t x, uint8_t y);              /* SOUTS3 $CCF2 */
void selico(uint8_t x, uint8_t y);              /* SELICO $CCF6 */
void sslams(uint8_t x, uint8_t y);              /* SSLAMS $CCFA */
void s3swar(uint8_t x, uint8_t y);              /* S3SWAR $CCFE */
void pulstr(uint8_t x, uint8_t y);              /* PULSTR $CD02 */
void pulsto(uint8_t x, uint8_t y);              /* PULSTO $CD06 */
void modsnd(void);                              /* MODSND $CD0A (IRQ) */

/* X and Y as a routine leaves them, where a later store depends on them
 * (the sound starters save the caller's X/Y in MTEMP). */
typedef struct { uint8_t x, y; } xy6502;

xy6502 inisou(void);                            /* INISOU $CD95  exits X=$FF, Y=first RANDO2 */

/* ---- ALEXEC ($C7A0-$CB00) - translated (M3) ----------------------------- */
void   mainln(void);                            /* MAINLN  $C7A0  init + first frame wait */
void   mainln_pass(void);                       /* MAINLN  $C7AD  one pass, ending in the next frame wait */
xy6502 exstat(uint8_t x, uint8_t y);            /* EXSTAT  $C7BD  (ROUTAD dispatch) */
xy6502 routen(uint8_t x, uint8_t y);            /* ROUTEN  $C800  = PAUSE */
xy6502 procre(uint8_t x, uint8_t y);            /* PROCRE  $C81B */
xy6502 nonsta(uint8_t x, uint8_t y);            /* NONSTA  $C891  (NOSTART $C8D9 inside) */
xy6502 newgam(uint8_t x, uint8_t y);            /* NEWGAM  $C90C */
xy6502 newlif(uint8_t x, uint8_t y);            /* NEWLIF  $C940 */
xy6502 newlf2(uint8_t x, uint8_t y);            /* NEWLF2  $C97B */
xy6502 endwav(uint8_t x, uint8_t y);            /* ENDWAV  $C98C */
xy6502 endlif(uint8_t x, uint8_t y);            /* ENDLIF  $C9AF */
xy6502 endgam(uint8_t x, uint8_t y);            /* ENDGAM  $C9F1 */
xy6502 dladr(uint8_t x, uint8_t y);             /* DLADR   $CA18 */
uint8_t cocfli(void);                           /* COCFLI  $CA48  returns Y */
void   clrsco(void);                            /* CLRSCO  $CA62  exits X=$FF */
xy6502 upscor(uint8_t x, uint8_t y);            /* UPSCOR  $CA6C  (GIVBON $CADC inside) */

/* ---- ALEARO ($DDDD-$DF08) - translated (M3) ----------------------------- */
void   eazboo(void);                            /* EAZBOO  $DDE9 */
void   eazhis(void);                            /* EAZHIS  $DDED */
void   eazero(void);                            /* EAZERO  $DDF1 */
void   wrhiin(void);                            /* WRHIIN  $DDF7 */
void   wrbook(void);                            /* WRBOOK  $DDFB */
xy6502 rehiin(uint8_t x, uint8_t y);            /* REHIIN  $DE11 */
xy6502 eaupd(uint8_t x, uint8_t y);             /* EAUPD   $DE1B */

/* ---- ALSCO2 ($A8B0-$B1B5) - translated (M4) ----------------------------- */
void    info(void);                             /* INFO    $A8B4 */
void    hacker(void);                           /* HACKER  $A8E7  (entry label inside INFO) */
void    upscli(uint8_t a, uint8_t y);           /* UPSCLI  $A97F  A = scale, Y = player */
void    nwdigs(uint8_t x);                      /* NWDIGS  $A9D7  X = SCOBUF index */
uint8_t nwhexz(uint8_t a, uint8_t x, int *c);   /* NWHEXZ  $A9FC  returns X+2, C in/out */
void    initem(void);                           /* INITEM  $AA13 */
void    dplpla(void);                           /* DPLPLA  $AA5A  (display state) */
void    dgover(void);                           /* DGOVER  $AA62  (display state) */
void    dprsta(void);                           /* DPRSTA  $AA6F  (display state) */
void    d2game(void);                           /* D2GAME  $AA79  (display state) */
void    dplrno(void);                           /* DPLRNO  $AA92 */
void    dplrx(void);                            /* DPLRX   $AA97 */
void    dplrxx(uint8_t x);                      /* DPLRXX  $AA9E */
void    dspcrd(void);                           /* DSPCRD  $AAA8 */
void    dbolou(void);                           /* DBOLOU  $AACB */
uint8_t hexbcd(uint8_t a);                      /* HEXBCD  $AAF5  returns A (= TEMP0), exits Y = $FF */
void    vgcntr_ab0d(void);                      /* VGCNTR  $AB0D  (listed VGCNTR_AB0D) */
void    msgs(uint8_t x);                        /* MSGS    $AB14  X = message number */
void    msgen3(uint8_t a, uint8_t x);           /* MSGEN3  $AB17  A = Y position */
void    msgfol(uint8_t a, uint8_t x);           /* MSGFOL  $AB98  A = X offset */
xy6502  inichk(uint8_t x, uint8_t y);           /* INICHK  $ABA2 */
xy6502  iniini(uint8_t x, uint8_t y);           /* INIINI  $ABAC  (INIIN2 $ABAF inside) */
xy6502  gamsta(uint8_t x, uint8_t y);           /* GAMSTA  $AC20 */
void    induce(void);                           /* INDUCE  $AC36 */
xy6502  hischk(uint8_t x, uint8_t y);           /* HISCHK  $AC3F  (state routine) */
xy6502  intldr(uint8_t x, uint8_t y);           /* INTLDR  $AD22 */
xy6502  getini(uint8_t x, uint8_t y);           /* GETINI  $AD6E  (state routine) */
uint8_t ginico(uint8_t a);                      /* GINICO  $ADCE  returns A, exits Y = 0 */
void    getdsp(void);                           /* GETDSP  $ADEA  (display state) */
void    ldrdsp(void);                           /* LDRDSP  $AE1C  (display state; ZPONTS inside) */
void    ldrout(uint8_t a);                      /* LDROUT  $AE4E  A = glow index */
void    bolout(void);                           /* BOLOUT  $AECA */
void    outcur(void);                           /* OUTCUR  $AEF1 */
void    outini(uint8_t a);                      /* OUTINI  $AEF8 */
void    rnkdsp(void);                           /* RNKDSP  $AF26 */
void    pl1rnk(uint8_t x);                      /* PL1RNK  $AF3F */
void    onernk(uint8_t a);                      /* ONERNK  $AF71 */
void    dsp1hx(uint8_t a);                      /* DSP1HX  $AF77 */
void    rqrdsp(void);                           /* RQRDSP  $AF81  (display state) */
uint8_t getcur(void);                           /* GETCUR  $B0AB  returns A = Y = CURSL1 */
void    bodspl(uint8_t x);                      /* BODSPL  $B0C6 */
void    nwcolo(uint8_t y);                      /* NWCOLO  $B0D1 */
void    nwsca1(uint8_t a);                      /* NWSCA1  $B0DD */
xy6502  logini(uint8_t x, uint8_t y);           /* LOGINI  $B0E7  (state routine) */
void    boxpro(void);                           /* BOXPRO  $B102  (display state) */
void    logpro(void);                           /* LOGPRO  $B131  (display state) */
void    scarng(uint8_t a, uint8_t x);           /* SCARNG  $B15A  A:X = picture */

/* ---- ALLANG ($D031-$D702) - translated (M4); tables in allang_data.c ---- */
xy6502  inilit(uint8_t x, uint8_t y);           /* INILIT  $D6BB */

/* ---- ALDIS2 ($B1B6-$C79F) - translated (M5); addresses in aldis2_data.h - */
xy6502  display(uint8_t x, uint8_t y);          /* DISPLAY $B1B6  returns exit X/Y (SBCSWI's) */
void    dstate(uint8_t y);                      /* DSTATE  $B20D  Y reaches DSPSYS */
xy6502  denorm(void);                           /* DROUTEN = DENORM $B230  (display state) */
#define drouten denorm
void    sbclog(uint8_t a);                      /* SBCLOG  $B2BE */
void    sbcact(uint8_t a);                      /* SBCACT  $B2DE */
xy6502  sbcswi(uint8_t a);                      /* SBCSWI  $B2FE  exits X = JMPL high, Y = 1 */
int     bigtex(void);                           /* BIGTEX  $B332  returns C */
void    dspwel(void);                           /* DSPWEL  $B367 */
void    dspnym(void);                           /* DSPNYM  $B498 */
void    excess(uint8_t y);                      /* EXCESS  $B550 */
void    vgdot(uint8_t a);                       /* VGDOT   $B56A  A = intensity, exits Y = 3 */
void    dspcur(void);                           /* DSPCUR  $B586 */
void    dspinv(void);                           /* DSPINV  $B5AD */
void    invpic(uint8_t a, uint8_t x);           /* INVPIC  $B5D7 */
void    flipic(uint8_t x);                      /* INVPIE = FLIPIC $B5EB */
#define invpie flipic
void    tanpic(uint8_t x);                      /* TANPIC  $B60F */
void    trapic(uint8_t x);                      /* TRAPIC  $B622 */
uint8_t ijmpds(uint8_t x);                      /* IJMPDS  $B634  returns Y = WELLID */
void    fuspic(uint8_t x);                      /* FUSPIC  $B69B */
uint8_t delta8(uint8_t a, uint8_t x);           /* DELTA8  $B6FA  returns A */
void    pulpic(uint8_t x);                      /* PULPIC  $B71B */
void    dspchg(void);                           /* DSPCHG  $B75B */
void    dspexp(void);                           /* DSPEXP  $B79A */
void    chplki(void);                           /* CHPLKI  $B7EB */
void    special(uint8_t y);                     /* SPECIAL $B84E */
void    altcol(void);                           /* ALTCOL  $B85F */
void    rotcol(void);                           /* ROTCOL  $B875 */
void    setshr(void);                           /* SETSHR  $B888 */
void    shrsca(void);                           /* SHRSCA  $B896 */
void    dsboom(void);                           /* DSBOOM  $B8BA  (display state) */
xy6502  swapvg(void);                           /* SWAPVG  $B944  exits X/Y = old VGLIST */
uint8_t calmag(uint8_t *y);                     /* CALMAG  $B955  returns A, *y = Y */
uint8_t whichb(uint8_t *x);                     /* WHICHB  $B967  returns A (high), *x = X (low) */
void    scapic(uint8_t a, uint8_t y);           /* SCAPIC  $BCFD */
void    scapi2(void);                           /* SCAPI2  $BD09 */
uint8_t cascal(void);                           /* CASCAL  $BD3E  returns Y */
void    onelin(uint8_t a, uint8_t y);           /* ONELIN  $BDA0 */
void    oneln2(uint8_t y);                      /* ONELN2  $BDCB */
void    worscr(void);                           /* PULS0E = WORSCR $C098 */
#define puls0e worscr
xy6502  inidsp(uint8_t x, uint8_t y);           /* INIDSP  $C16E  exits X/Y of INICOL */
xy6502  inicol(void);                           /* INICOL  $C196  exits X = index - 8, Y = $FF */
void    inimat(void);                           /* INIMAT  $C1C3 */
void    iniwls(void);                           /* INIWLS  $C235 */
uint8_t lvlwel(uint8_t a, uint8_t *x);          /* LVLWEL  $C2E8  returns A, *x = X */
void    bldwel(void);                           /* BLDWEL  $C30D  (WELPIC $C339 inside) */
void    outlin(uint8_t a, uint8_t y);           /* OUTLIN  $C36E */
void    connec(void);                           /* CONNEC  $C3BA  (UPCURN $C3D6 inside) */
uint8_t spoke(uint8_t a, uint8_t x);            /* SPOKE   $C3EE  returns X */
void    lintos(void);                           /* LINTOS  $C423 */
void    liftos(void);                           /* LIFTOS  $C43C */
void    chkdep(void);                           /* CHKDEP  $C453 */
uint8_t calout(uint8_t a, uint8_t x);           /* CALOUT  $C473  returns A */
void    dsphol(uint8_t a);                      /* DSPHOL  $C4E1  A = level - 1 */
void    dstarf(void);                           /* DSTARF  $C54D */
void    dspenl(void);                           /* DSPENL  $C5C2 */
void    fixstu(void);                           /* FIXSTU  $C66D  (YVGVCT $C6A2 inside) */
void    tipact(void);                           /* TIPACT  $C6C7 */
void    fconnec(void);                          /* FCONNEC $C73C */
void    vgyab1(uint8_t x);                      /* VGYAB1  $C765  X = zero-page screen point */
void    vgyabs(uint8_t x);                      /* VGYABS  $C772 */

/* ---- ALWELG ($9000-$A8AF) - translated (M6); addresses in alwelg_data.h - */
xy6502  inewav(uint8_t x, uint8_t y);           /* INEWAV  $9009  exits INIDSP's X/Y */
xy6502  inewli(uint8_t x, uint8_t y);           /* INEWLI  $9025 */
xy6502  iniobj(uint8_t x, uint8_t y);           /* INIOBJ  $902B  exits INIDSP's X/Y */
xy6502  newav2(uint8_t x, uint8_t y);           /* NEWAV2  $904B  (state routine) */
xy6502  inira0(uint8_t x, uint8_t y);           /* INIRA0  $90C4 */
xy6502  inirat(uint8_t x, uint8_t y);           /* INIRAT  $9108  (state routine) */
xy6502  prorat(uint8_t x, uint8_t y);           /* PRORAT  $9149  (state routine) */
xy6502  bonsco(uint8_t a, uint8_t x, uint8_t y);/* BONSCO  $91B5  A = bonus index, exits X = A * 2 */
void    inicur(void);                           /* INICUR  $921B */
void    iniene(void);                           /* INIENE  $9234  exits X = $FF */
void    ininym(void);                           /* ININYM  $9246  exits X = $FF */
void    iniinv(void);                           /* INIINV  $926F  exits X = $FF */
void    inicha(void);                           /* INICHA  $928F  exits X = $FF */
void    iniexp(void);                           /* INIEXP  $929F  exits X = $FF */
void    clrpot(void);                           /* CLRPOT  $92AD */
xy6502  swapen(uint8_t x, uint8_t y);           /* SWAPEN  $92B2 */
xy6502  contour(void);                          /* CONTOUR $92C5  (TEXIT $9319 inside) exits TIMES8's X/Y */
uint8_t times8(uint8_t a, uint8_t *x, uint8_t *y);  /* TIMES8 $93E0  returns A, *x = range, *y = high */
uint8_t dotype(uint8_t y);                      /* WTABEND = DOTYPE $9677  returns A */
#define wtabend dotype
uint8_t donext(uint8_t y);                      /* DONEXT  $9683  returns Y */
uint8_t dotzan(uint8_t y);                      /* DOTZAN  $96AB  returns A (ITMIZ2 $96B9 inside) */
uint8_t itmize(uint8_t y);                      /* ITMIZE  $96B7  returns A */
uint8_t samall(uint8_t y);                      /* SAMALL  $96C4  returns A */
uint8_t twobyt(uint8_t y);                      /* TWOBYT  $96C7  returns Y */
uint8_t onebyt(uint8_t y);                      /* ONEBYT  $96C8  returns Y */
uint8_t nitmiz(uint8_t y);                      /* NITMIZ  $96CB  returns Y */
uint8_t dotb(uint8_t y);                        /* DOTB    $96DB  returns A */
uint8_t dota(uint8_t y);                        /* DOTA    $96E2  returns A */
uint8_t ranger(uint8_t y);                      /* RANGER  $96F4  returns A, Y preserved */
uint8_t dotr(uint8_t y);                        /* DOTR    $9700  returns A */
xy6502  play(uint8_t x, uint8_t y);             /* PLAY    $970B  (state routine) */
xy6502  pldrop(uint8_t x, uint8_t y);           /* PLDROP  $9729  (state routine) */
xy6502  movcur(uint8_t x, uint8_t y);           /* MOVCUR  $9749  exits X = WELTYP */
uint8_t autocu(uint8_t *x, uint8_t *y);         /* AUTOCU  $97C5  returns A, *x / *y out */
xy6502  movcud(uint8_t x, uint8_t y);           /* MOVCUD  $97F8 */
xy6502  movnym(uint8_t x, uint8_t y);           /* MOVNYM  $98A2 */
xy6502  conymp(uint8_t x, uint8_t y);           /* CONYMP  $9923  X preserved */
uint8_t actinv(uint8_t x, uint8_t y);           /* ACTINV  $994D  returns A ($10 found / 0); X, Y preserved */
uint8_t nymcha(uint8_t x, uint8_t y);           /* NYMCHA  $99A5  returns Y */
uint8_t newtyp(uint8_t x, uint8_t *y);          /* NEWTYP  $9A87  returns A, *y out; X preserved */
uint8_t newty2(uint8_t a, uint8_t x, uint8_t *y);   /* NEWTY2 $9A88  returns A, *y out */
uint8_t newfli(uint8_t *y);                     /* NEWFLI  $9A9D  (NEWGN3 $9AF6 inside) */
uint8_t newpul(uint8_t *y);                     /* NEWPUL  $9AA9  (NEWGN2 $9AF1 inside) */
uint8_t newfus(uint8_t *y);                     /* NEWFUS  $9AB3 */
uint8_t newspi(uint8_t *y);                     /* NEWSPI  $9AB7 */
uint8_t newtan(uint8_t x, uint8_t *y);          /* NEWTAN  $9ABB  X preserved */
uint8_t newgen(uint8_t y);                      /* NEWGEN  $9AEE  returns A; Y preserved */
void    splcha(uint8_t x, uint8_t y);           /* SPLCHA  $9B07  X, Y preserved */
xy6502  movinv(uint8_t x, uint8_t y);           /* MOVINV  $9B1E  (NEGPUL $9B8C inside) */
xy6502  jsrcam(uint8_t a, uint8_t x);           /* JSRCAM  $9B98  A = CAM code, X = invader */
/* CAM routines (TABJSR, entered by JSRCAM's RTS; Y = the CAM code) */
xy6502  jexit(uint8_t x, uint8_t y);            /* TABJSE = JEXIT $9BCA */
#define tabjse jexit
xy6502  jnoop(uint8_t x, uint8_t y);            /* JNOOP   $9BCF */
xy6502  jsloop(uint8_t x, uint8_t y);           /* JSLOOP  $9BD0 */
xy6502  jslopb(uint8_t x, uint8_t y);           /* JSLOPB  $9BDD */
xy6502  jskip0(uint8_t x, uint8_t y);           /* JSKIP0  $9BEE */
xy6502  jbr0pc(uint8_t x, uint8_t y);           /* JBR0PC  $9BFA */
xy6502  jeloop(uint8_t x, uint8_t y);           /* JELOOP  $9C0C */
xy6502  jsetpc(uint8_t x, uint8_t y);           /* JSETPC  $9C17 */
xy6502  jeltst(uint8_t x, uint8_t y);           /* JELTST  $9C21 */
xy6502  jchkpu(uint8_t x, uint8_t y);           /* JCHKPU  $9C3B */
xy6502  jchrot(uint8_t x, uint8_t y);           /* JCHROT  $9C4F */
xy6502  jsmove(uint8_t x, uint8_t y);           /* JSMOVE  $9C58 */
xy6502  jsmovu(uint8_t x, uint8_t y);           /* JSMOVU  $9C63  (ATOP $9C7D inside) Y = type */
uint8_t jsmovd(uint8_t x, uint8_t y);           /* JSMOVD  $9C99  returns A = Y position; X, Y preserved */
xy6502  jpulmo(uint8_t x, uint8_t y);           /* JPULMO  $9CB6 */
xy6502  chaser(uint8_t x, uint8_t y);           /* CHASER  $9D06  (GOTCHA $9D54 inside) */
xy6502  jchpla(uint8_t x, uint8_t y);           /* JCHPLA  $9D67  exits Y = INVAL1(X) */
xy6502  jjumpm(uint8_t x, uint8_t y);           /* JJUMPM  $9D82 */
xy6502  jkitst(uint8_t x, uint8_t y);           /* JKITST  $9E2F */
xy6502  jfuski(uint8_t x, uint8_t y);           /* JFUSKI  $9E48 */
xy6502  jjumps(uint8_t x, uint8_t y);           /* JJUMPS  $9E5C  (JUMPSD $9E5F inside) */
void    oktojm(uint8_t x);                      /* OKTOJM  $9EAB */
uint8_t calsan(uint8_t a, uint8_t *y);          /* CALSAN  $9ED7  returns A, *y = base leg in/out */
xy6502  jfuseup(uint8_t x, uint8_t y);          /* JFUSEUP $9EF1 */
xy6502  mayblr(uint8_t x, uint8_t y);           /* MAYBLR  $9F5F */
xy6502  fuchpl(uint8_t x, uint8_t y);           /* FUCHPL  $9F81 */
xy6502  lefrit(uint8_t x, uint8_t y);           /* LEFRIT  $9F8A */
xy6502  gotjum(uint8_t x, uint8_t y);           /* GOTJUM  $9F99  (entry label; REVFLP inside) */
xy6502  jstrai(uint8_t x, uint8_t y);           /* JSTRAI  $9FC4 */
uint8_t astral(uint8_t x);                      /* ASTRAL  $A028  returns Y (SKIPIT inside) */
void    kilinv(uint8_t x, uint8_t y);           /* KILINV  $A06F  Y = invader; X, Y preserved (MOVER inside) */
xy6502  movcha(uint8_t x, uint8_t y);           /* MOVCHA  $A18F */
void    chatop(uint8_t x, uint8_t y);           /* CHATOP  $A1E4 */
xy6502  lifect(uint8_t x, uint8_t y);           /* LIFECT  $A1FA */
xy6502  firepc(uint8_t x, uint8_t y);           /* FIREPC  $A23F */
xy6502  fireic(uint8_t x, uint8_t y);           /* FIREIC  $A2A6 */
xy6502  incfs2(uint8_t x, uint8_t y);           /* INCFS2  $A309 */
void    inipsq(uint8_t x, uint8_t y);           /* INIPSQ  $A33A  X, Y preserved */
void    infpsq(uint8_t x, uint8_t y);           /* INFPSQ  $A343 */
void    inppsq(uint8_t x, uint8_t y);           /* INPPSQ  $A347 */
void    incpsq(uint8_t x, uint8_t y);           /* INCPSQ  $A34B  (INCP2 $A34D inside) */
void    deadcu(uint8_t a, uint8_t x, uint8_t y);/* DEADCU  $A352  A = explosion type */
void    inccsq(uint8_t x, uint8_t y);           /* INCCSQ  $A36F */
xy6502  incis2(uint8_t x, uint8_t y);           /* INCIS2  $A38E  (INCISQ $A398 inside) exits UPSCOR's X/Y */
void    gexifu(uint8_t a, uint8_t x, uint8_t y);/* GEXIFU  $A3CA */
void    genexp(uint8_t a, uint8_t x, uint8_t y);/* GENEXP  $A3D4 */
void    genex2(uint8_t x, uint8_t y);           /* GENEX2  $A3D6  (GOTEXP inside) X, Y preserved */
xy6502  proexp(uint8_t x, uint8_t y);           /* PROEXP  $A416 */
xy6502  collis(uint8_t x, uint8_t y);           /* COLLIS  $A454 */
xy6502  colchk(uint8_t a, uint8_t x, uint8_t y);/* COLCHK  $A463  A = charge depth (OKATOP/YESCOL/NOCOL inside) */
xy6502  analyz(uint8_t x, uint8_t y);           /* ANALYZ  $A504  (ZQVAVG, LINER inside) */
void    indrop(void);                           /* INDROP  $A5CB  exits X = $FF */
xy6502  prboom(uint8_t x, uint8_t y);           /* PRBOOM  $A618  (state routine) */
void    timlau(uint8_t x, uint8_t y);           /* TIMLAU  $A65B  X = particle; X/Y preserved */
uint8_t fixtop(uint8_t a, int *n);              /* FIXTOP  $A69B  returns A, *n = N flag */
uint8_t uparpo(uint8_t x);                      /* UPARPO  $A6A9  returns Y */
uint8_t decpar(uint8_t x);                      /* DECPAR  $A721  returns Y */
uint8_t decele(uint8_t a, uint8_t *y);          /* DECELE  $A75D  returns A, *y in/out */
xy6502  inboom(uint8_t x, uint8_t y);           /* INBOOM  $A789  exits X = $FF */
uint8_t poldel(uint8_t a, uint8_t y);           /* POLDEL  $A7A6  returns A, Y preserved */
void    instar(void);                           /* INSTAR  $A7BD  exits X = $FF */
xy6502  prstar(uint8_t x, uint8_t y);           /* PRSTAR  $A7D2 */
void    inisuz(void);                           /* INISUZ  $A831 */
xy6502  prosuz(uint8_t x, uint8_t y);           /* PROSUZ  $A83A */
xy6502  kilene(uint8_t x, uint8_t y);           /* KILENE  $A888 */
xy6502  exikil(uint8_t x, uint8_t y);           /* EXIKIL  $A8A4  (JMP INCISQ) */

/* ---- ALTES2 ($D7E1-$DDDC, $DFDC-$DFF7) - translated (M9 B1); addresses in
 * altes2_data.h.  Verified by tests\lockstep.c (routine checks, RESET regions,
 * diag pass probe) and tests\gate.c since M9 B3.  Routines that never return on the ROM (the power-on self
 * test, BRAMREP's JMP ROMTST, the diag loop) return here at the loop head the
 * ROM reaches; g.cpu_loop says which loop that is. ------------------------- */
void    reset(void);                            /* RESET = SFTEST $D93F  TEST open: returns at MAINLN's first frame
                                                   wait (LOOP_MAINLN); TEST closed: at the diag loop head (LOOP_DIAG) */
void    sftest_zp(void);                        /* ($D9A9, no label) zero-page march; falls into sftest_ram */
void    sftest_ram(void);                       /* ($D9D6, no label) pages $01-$07, $20-$2F; falls into ROMTST */
void    romtst(void);                           /* ROMTST  $DA0A  (OK1, OK2 inside) ROMs, POKEYs, EAROM, colours; ends at $DA8D */
void    diag_pass(void);                        /* ($DA8D-$DAF7) one diag loop pass (TIMEST inside); TEST open -> WDGTST ->
                                                   hw_watchdog_hang() */
xy6502  system_(uint8_t x, uint8_t y);          /* SYSTEM  $D7E1  (state routine; '_': stdlib's system) */
xy6502  dspsys(uint8_t a, uint8_t x, uint8_t y);/* DSPSYS  $D804  (display state) option 0/1 -> hw_reset(); exits DIGTYS's X/Y */
void    posdig(uint8_t a, uint8_t x, uint8_t y);/* POSDIG  $D8A9  A = BCD byte, Y:X = beam move */
void    hibad(uint8_t a);                       /* HIBAD   $D8CA  A = bad bits (JMP BRAMREP) */
void    bramrep(uint8_t a, uint8_t y);          /* BRAMREP $D8CD  A = bad block, Y = bad bits (JMP ROMTST) */
void    hirbad(uint8_t a, uint8_t y);           /* HIRBAD  $D92F  A = pattern, Y = cell (HIRBD2 inside) */
void    hirbd2(uint8_t a);                      /* HIRBD2  $D931  A = bad bits */
void    sstate(uint8_t y);                      /* SFTJSE = SSTATE $DB0F  SFTJSR RTS dispatch (NOOPR_DB21 = its RTS) */
#define sftjse sstate
void    sigana(void);                           /* SIGANA  $DB22  (self-test state 12) */
xy6502  badear(uint8_t x, uint8_t y);           /* BADEAR  $DB5A  (state 0) */
void    schekr(void);                           /* SCHEKR  $DB6F  (state 10; JSRVGJ, NOSOUN inside) */
void    sinten(void);                           /* SINTEN  $DB7E  (state 8) */
void    shatch(void);                           /* SHATCH  $DB84  (state 4) */
void    shyster(void);                          /* SHYSTER $DB9A  (state 6) */
uint8_t getop3(uint8_t a, uint8_t x, uint8_t y);/* GETOP3  $DBE0  returns A; X/Y unchanged */
void    romrep(void);                           /* ROMREP  $DBF7  (state 2; BADBOX inside) */
uint8_t readmb(uint8_t a, uint8_t *x, uint8_t *y);  /* READMB $DCE6  returns A, *x in/out (X), *y out; X = $FF timed out (TOOSLO) */
void    dopswi(void);                           /* DOPSWI  $DD0D */
void    bits2(uint8_t y);                       /* BITS2   $DD27 */
void    bits3(uint8_t a, uint8_t y);            /* BITS3   $DD29 */
void    genopd(uint8_t a, uint8_t x, uint8_t y);/* GENOPD  $DD2B  Y = byte, A/X = beam move */
void    dbooke(void);                           /* DBOOKE  $DD41  (TPLIST loop inside) */

#endif /* GAME_H */
