/* altes2.c - ALTES2 ($D7E1-$DDDC, + $DFDC tables): RESET (power-on), the
 * self test (RAM / ROM / POKEY / EAROM tests, the diagnostic loop and its seven
 * screens), the SYSTEM configuration state and its display DSPSYS (option
 * switches, bookkeeping, the self-test / zero-EAROM options).
 *
 * M8 translated RESET's power-on path and GETOP3; M9 batch B1 the rest.
 *
 * One function per Atari routine header (21 of 22; NOOPR_DB21 is SSTATE's
 * dispatch RTS, written inline) plus the code labels the ROM jumps to
 * (BRAMREP, HIRBD2, ROMTST) and the two unlabeled RAM tests (sftest_zp $D9A9,
 * sftest_ram $D9D6).  Entry labels reached only by a branch or fall-through are
 * inline with their label in a comment (SFTEST = RESET, JMPHIB, OK1, OK2,
 * TIMEST, WDGTST, SSTATE = SFTJSE, BADBOX, TOOSLO) or small static helpers
 * named after their label (JSRVGJ, NOSOUN).
 *
 * Endless loops: the ROM's self test never returns - RESET falls into the RAM
 * march, BRAMREP JMPs to ROMTST, ROMTST runs into the diagnostic loop at $DA8D,
 * which is left only by the hardware watchdog.  Here reset() returns at the
 * loop head the ROM reaches (MAINLN's first frame wait, or $DA8D) and
 * g.cpu_loop tells the caller which loop to run: mainln_pass() or diag_pass().
 * JMP RESET inside running code (DSPSYS option 0/1) is hw_reset(); the WDGTST
 * spin is hw_watchdog_hang(); both never return on a finished seam (hw.h).
 *
 * Tables: every ROM address comes from tools/gen_altes2.py (altes2_data.h);
 * table bytes are read with cpu_rd() over progrom/vecrom.  RAM tests and the
 * ROM checksum run on g.ram / g.vram / the ROM images through cpu_rd/cpu_wr
 * (genuine hardware: every RAM cell passes, every checksum is 0).
 *
 * Mathbox, POKEY, EAROM, IN1 busy waits (the 3 kHz clock b7, VG HALT b6) and
 * every watchdog kick go through the hw_* seam in the ROM's order.
 * Decimal mode: DBOOKE's SED ... CLD sets / clears g.dflag around its ADCs
 * (adc6502); its first ROL takes the carry VGJSRL / VGVTR1 leave (VGADD's ADC).
 *
 * Unreachable on genuine hardware (translated from the listing, unexercised):
 * HIBAD, BRAMREP, HIRBAD, HIRBD2 (bad RAM), the bad-ROM report and VG-ROM tone,
 * the PK1CND / PK2CND stores (stuck RANDOM), BADBOX / TOOSLO (Mathbox timeout).
 */
#include <string.h>
#include "state.h"
#include "hw.h"
#include "game.h"
#include "altes2_data.h"

#define ROM(a)      cpu_rd((uint16_t)(a))       /* ROM table byte */
#define HI(a)       ((uint8_t)((a) >> 8))       /* LAH: #>[addr] */
#define LO(a)       ((uint8_t)((a) & 0xFF))     /* LXL: #<addr */
#define INDY_ADDR   ((uint16_t)(INDYLO | ((uint16_t)INDYHI << 8)))
#define PTR00_ADDR  ((uint16_t)(g.ram[0x00] | ((uint16_t)g.ram[0x01] << 8)))

/* The self test's condition cells (ALTES2: MBCOND = CBUF1 ...) */
#define MBCOND      (g.ram[K_MBCOND])           /* $78 */
#define RAMCND      (g.ram[K_RAMCND])           /* $79 */
#define PK1CND      (g.ram[K_PK1CND])           /* $7A */
#define PK2CND      (g.ram[K_PK2CND])           /* $7B */
#define EARCND      (g.ram[K_EARCND])           /* $7C */
/* CHKSMS = $7D .. $88 (NROMS bytes): g.ram[K_CHKSMS + x] */

/* Mathbox registers (offsets from MBSTAR $6080) */
#define MB_MNL      ((uint8_t)(A_MNL   - A_MBSTAR))     /* $0C */
#define MB_MZLL     ((uint8_t)(A_MZLL  - A_MBSTAR))     /* $0D */
#define MB_MZLH     ((uint8_t)(A_MZLH  - A_MBSTAR))     /* $0E */
#define MB_MZHL     ((uint8_t)(A_MZHL  - A_MBSTAR))     /* $0F */
#define MB_MZHH     ((uint8_t)(A_MZHH  - A_MBSTAR))     /* $10 */
#define MB_MSZXD    ((uint8_t)(A_MSZXD - A_MBSTAR))     /* $14 */
#define MB_MXPL     ((uint8_t)(A_MXPL  - A_MBSTAR))     /* $15 */
#define MB_MXPH     ((uint8_t)(A_MXPH  - A_MBSTAR))     /* $16 */

/* The C flag VGADD leaves (TYA / SEC / ADC VGLIST / STA VGLIST): set when the
 * low byte of the list pointer wrapped.  VGJSRL and VGVTR1 end in VGADD. */
#define VGADD_CARRY(before) ((unsigned)(VGLIST < (before)))

static xy6502 xy(uint8_t x, uint8_t y) { xy6502 r; r.x = x; r.y = y; return r; }

/* STA AUDF1,Y / STA AUDC1,Y with a SNDTBL offset: CPU $60C0-$60DF = POKEY 1
 * (chip 0) or POKEY 2 (chip 1) register. */
static void pokey_abs(uint16_t addr, uint8_t v)
{
    uint16_t off = (uint16_t)(addr - A_POKEY);
    hw_pokey_write((int)(off >> 4), (uint8_t)(off & 0x0F), v);
}

/* ---- seam hook stand-ins (M9 B1) ------------------------------------------
 * tests\lockstep.c and tests\gate.c implement hw_reset() / hw_watchdog_hang()
 * since M9 B3, app_loop.c (skeleton, tempest_selftest, tempest_win) since
 * M9 B5; every build line defines ALTES2_SEAM_HAS_RESET_HOOKS.  These
 * stand-ins (the software-watchdog restart) are only for a seam without them. */
#ifndef ALTES2_SEAM_HAS_RESET_HOOKS
void hw_reset(void)         { hw_soft_watchdog(); }
void hw_watchdog_hang(void) { hw_soft_watchdog(); }
#endif

/* SYSTEM ($D7E1): system configuration state (ROUTAD, CSYSTM).  Back to the
 * game once the EAROM is idle and the TEST switch is open again.  Returns
 * INIINI's exit X/Y when it runs, else X/Y unchanged.  (Named system_ -
 * stdlib declares system.) */
xy6502 system_(uint8_t x, uint8_t y)
{
    xy6502 r = xy(x, y);
    QSTATUS = 0x00;                                         /* LD7E1-LD7E3 stop game */
    QDSTATE = K_CDSYST;                                     /* LD7E5-LD7E7 system display state */
    if (EAFLG == 0) {                                       /* LD7E9-LD7EC EAROM busy? */
        if (hw_in1() & K_MTEST) {                           /* LD7EE-LD7F3 all done? (switch open) */
            QSTATE = K_CNEWGA;                              /* LD7F5-LD7F7 back to game */
            if (EABAD & 0x03)                               /* LD7F9-LD7FE initialize score stuff? */
                r = iniini(x, y);                           /* LD800 JSR INIINI */
        }
    }
    return r;                                               /* LD803 */
}

/* DSPSYS ($D804): system configuration display state (DROUTAD, dispatched by
 * ALDIS2's DSTATE with X = QDSTATE, Y = 1).  Option 0/1 (self test) selected
 * -> JMP RESET = hw_reset().  Exits DIGTYS's X/Y (X = TEMP0 - 1, Y = 1), which
 * DISPLAY does not use. */
xy6502 dspsys(uint8_t a, uint8_t x, uint8_t y)
{
    uint8_t v;
    (void)a;
    (void)inilit(x, y);                                     /* LD804 JSR INILIT: language pointer */
    dspcrd();                                               /* LD807 coin mode stuff */
    dopswi();                                               /* LD80A binary option switches */
    dbooke();                                               /* LD80D bookkeeping */
    INDEX1 = LVSGAM;                                        /* LD810-LD813 # lives */
    vgcntr();                                               /* LD815 */
    vgvtr1(0xE8, 0xC0);                                     /* LD818-LD81C position beam */
    do {
        vgjsrl(HI(ROM_LIFEY + 1), LO(ROM_LIFEY));           /* LD81F-LD823 life picture */
        INDEX1 = (uint8_t)(INDEX1 - 1);                     /* LD826 DEC INDEX1 */
    } while (INDEX1 != 0);                                  /* LD828 BNE */
    y = (uint8_t)((OPTIN3 & 0x03) << 1);                    /* LD82A-LD830 easy/med/hard */
    vgjsrl(ROM(ROM_SYSOPT + 9 + y), ROM(ROM_SYSOPT + 8 + y));   /* LD831-LD837 */
    /* special options */
    v = ginico(CURSL1);                                     /* LD83A-LD83D update cursor */
    CURSL1 = v;                                             /* LD840 */
    v = (uint8_t)(v & 0x06);                                /* LD843 AND #6 / LD845 PHA */
    y = v;                                                  /* LD846 TAY */
    vgjsrl(ROM(ROM_SYSOPT + 1 + y), ROM(ROM_SYSOPT + y));   /* LD847-LD84D option literal */
    x = (uint8_t)(v >> 1);                                  /* LD850-LD852 PLA / LSR / TAX */
    if ((SWSTAT & ROM(ALTES2_OPTMSK + x)) == ROM(ALTES2_OPTMSK + x)) {  /* LD853-LD85B option selected? */
        x = (uint8_t)(x - 2);                               /* LD85D-LD85E DEX / DEX */
        if (x & 0x80) {                                     /* LD85F BPL LD864 */
            hw_reset();                                     /* LD861 JMP RESET: self-test option */
            return xy(x, y);
        }
        if (x == 0) {                                       /* LD864 BNE LD86C */
            eazboo();                                       /* LD866 zero times option */
        } else {
            eazhis();                                       /* LD86C zero hi scores option */
            EABAD = (uint8_t)(EABAD | 0x03);                /* LD86F-LD874 induce hi score init */
        }
    }
    if (EAFLG & EAZFLG)                                     /* LD877-LD87D EAROM busy erasing? */
        vgjsrl(HI(ROM_EASING + 1), LO(ROM_EASING));         /* LD87F-LD883 "ERASING" */
    /* mech multipliers */
    vgcntr();                                               /* LD886 */
    x = (uint8_t)((OPTIN1 & 0x1C) >> 2);                    /* LD889-LD88F */
    posdig(ROM(ALTES2_CRMECHT + x), 0x1B, 0xEE);            /* LD890-LD897 LDY #-72./4 / LDX #$1B */
    /* bonus adder */
    x = (uint8_t)(OPTIN1 >> 5);                             /* LD89A-LD8A1 */
    posdig(ROM(ALTES2_BONADR + x), 0xF8, 0x32);             /* LD8A2-LD8A7 LDY #$32 / LDX #-32./4, falls into POSDIG */
    return xy((uint8_t)(A_TEMP0 - 1), 0x01);
}

/* POSDIG ($D8A9): TEMP0 = A; move the beam (Y, X); two BCD digits of TEMP0. */
void posdig(uint8_t a, uint8_t x, uint8_t y)
{
    TEMP0 = a;                                              /* LD8A9 */
    vgvtr1(y, x);                                           /* LD8AB-LD8AC TYA / JSR VGVTR1 */
    digtys((uint8_t)A_TEMP0, 0x01);                         /* LD8AF-LD8B3 LDA #TEMP0 / LDY #1 / JMP DIGTYS */
}

/* HIBAD ($D8CA): bad zero-page RAM.  A = bad bits.  (Unreachable on genuine
 * RAM.) */
void hibad(uint8_t a)
{
    bramrep(0x00, a);                                       /* LD8CA-LD8CB TAY / LDA #0 bad block */
}

/* BRAMREP ($D8CD): report bad RAM block A, bad bits Y by tones and the start
 * LEDs, one short "good" beep per nibble and a long high one for the bad
 * nibble; the stack pointer is the nibble counter.  Then JMP ROMTST.
 * (Unreachable on genuine RAM.) */
void bramrep(uint8_t a, uint8_t y)
{
    uint8_t x, s, v;
    RAMCND = y;                                             /* LD8CD */
    x = (uint8_t)((uint8_t)(a >> 2) << 1);                  /* LD8CF-LD8D2 LSR / LSR / ASL / TAX */
    if ((y & 0x0F) == 0)                                    /* LD8D3-LD8D6 TYA / AND #$0F / BNE */
        x++;                                                /* LD8D8 +1 if MSB nibble bad and LSB good */
    s = x;                                                  /* LD8D9 TXS */
    do {
        hw_pokey_write(0, 0x01, 0xA2);                      /* LD8DA-LD8DC STA AUDC1 */
        if (s == 0) {                                       /* LD8DF-LD8E0 TSX / BNE: bad nibble? */
            a = 0x60;                                       /* LD8E2 bad (hi) tone */
            y = 0x09;                                       /* LD8E4 bad (long) delay */
        } else {
            a = 0xC0;                                       /* LD8E9 good (lo) tone */
            y = 0x01;                                       /* LD8EB good (short) delay */
        }
        hw_pokey_write(0, 0x00, a);                         /* LD8ED STA AUDF1: sound on */
        hw_outank(K_LEDOFF);                                /* LD8F0-LD8F2 LED off */
        x = 0x00;                                           /* LD8F5 */
        do {
            do {
                do v = hw_in1(); while (v & 0x80);          /* LD8F7-LD8FA BIT IN1 / BMI */
                do v = hw_in1(); while (!(v & 0x80));       /* LD8FC-LD8FF BIT IN1 / BPL */
                hw_watchdog();                              /* LD901 */
                x--;                                        /* LD904 */
            } while (x != 0);                               /* LD905 */
            y--;                                            /* LD907 */
        } while (y != 0);                                   /* LD908 */
        hw_pokey_write(0, 0x01, x);                         /* LD90A STX AUDC1: sound off */
        hw_outank(0x00);                                    /* LD90D-LD90F LED on */
        y = 0x09;                                           /* LD912 */
        do {
            do {
                do v = hw_in1(); while (v & 0x80);          /* LD914-LD917 */
                do v = hw_in1(); while (!(v & 0x80));       /* LD919-LD91C */
                hw_watchdog();                              /* LD91E */
                x--;                                        /* LD921 */
            } while (x != 0);                               /* LD922 */
            y--;                                            /* LD924 */
        } while (y != 0);                                   /* LD925 */
        s--;                                                /* LD927-LD929 TSX / DEX / TXS */
    } while (!(s & 0x80));                                  /* LD92A BPL LD8DA */
    romtst();                                               /* LD92C JMP ROMTST */
}

/* HIRBAD ($D92F): bad non-zero-page RAM, A = test pattern, Y = cell index of
 * the pointer $00/$01.  (Unreachable on genuine RAM.) */
void hirbad(uint8_t a, uint8_t y)
{
    a = (uint8_t)(a ^ cpu_rd((uint16_t)(PTR00_ADDR + y)));  /* LD92F EOR ($00),Y: bad bits */
    hirbd2(a);                                              /* falls into HIRBD2 */
}

/* HIRBD2 ($D931): A = bad bits; block = page of $01 (vector RAM folded down)
 * & $1F -> BRAMREP.  (Unreachable on genuine RAM.) */
void hirbd2(uint8_t a)
{
    uint8_t y = a;                                          /* LD931 TAY */
    unsigned c;
    a = g.ram[0x01];                                        /* LD932 LDA $01 */
    c = a >= HI(A_VECRAM);                                  /* LD934 CMP #VECRAM/$100 */
    if (c)                                                  /* LD936 BCC */
        a = sbc6502(a, HI(A_VECRAM - 0x800), &c, NULL);     /* LD938 SBC #[VECRAM-$800]/$100 */
    a = (uint8_t)(a & 0x1F);                                /* LD93A bad RAM block */
    bramrep(a, y);                                          /* LD93C JMP BRAMREP */
}

/* RESET = SFTEST ($D93F): power-on.  TEST switch open: the M8 power-on path,
 * JMP MAINLN, returning when MAINLN reaches its first frame wait
 * (g.cpu_loop = LOOP_MAINLN).  TEST switch closed: the self test - RAM, ROM,
 * POKEY and EAROM tests - returning at the diagnostic loop head $DA8D
 * (g.cpu_loop = LOOP_DIAG; run diag_pass() from there). */
void reset(void)
{
    xy6502 r;
    /* LD93F SEI: the IRQ stays masked (and pending) until hw_cli() */
    hw_watchdog();                                  /* LD940 STA WTCHDG */
    hw_vgstop();                                    /* LD943 STA VGSTOP */
    /* LD946-LD949 LDX #$FF / TXS / CLD */
    g.dflag = 0;
    /* LD94A-LD964: zero RAM pages $00-$07, then vector RAM pages $20-$2F,
     * through the pointer ($00),Y = (Y=0, X=page); one watchdog kick per
     * page.  LD966 STA $01 then zeroes the pointer's high byte, so $00 and
     * $01 both end 0 (A = 0 throughout). */
    for (int page = 0; page < 24; page++) {
        if (page < 8) memset(g.ram + page * 0x100, 0, 0x100);
        else          memset(g.vram + (page - 8) * 0x100, 0, 0x100);
        hw_watchdog();                              /* LD961 */
    }
    g.ram[0x00] = 0x00;                             /* LD94D STY $00 (last: Y = 0) */
    g.ram[0x01] = 0x00;                             /* LD966 STA $01 */
    hw_outank(0x00);                                /* LD968 STA OUTANK: LEDs on */
    hw_pokey_write(0, 0x0F, 0x00);                  /* LD96B STA AUDF1+$F (SKCTL) */
    hw_pokey_write(1, 0x0F, 0x00);                  /* LD96E STA AUDF2+$F */
    hw_pokey_write(0, 0x0F, 0x07);                  /* LD971-LD973 LDX #7 / STX AUDF1+$F */
    hw_pokey_write(1, 0x0F, 0x07);                  /* LD976 STX AUDF2+$F */
    for (int x = 8; x >= 0; x--) {                  /* LD979 INX, LD980-LD981 DEX / BPL */
        hw_pokey_write(0, (uint8_t)x, 0x00);        /* LD97A STA AUDF1,X */
        hw_pokey_write(1, (uint8_t)x, 0x00);        /* LD97D STA AUDF2,X */
    }
    if (!(hw_in1() & K_MTEST)) {                    /* LD983-LD988 LDA IN1 / AND #MTEST / BEQ LD9A9 */
        sftest_zp();                                /* TEST switch closed: the self test */
        return;                                     /* (at the diag loop head, g.cpu_loop = LOOP_DIAG) */
    }
    for (unsigned i = 0; i < 65536u; i++)           /* LD98A-LD995: delay, DEC $0100 / DEC $0101 */
        hw_watchdog();                              /* LD98A STA WTCHDG ($0100/$0101 end 0) */
    TOUT0 = K_MVINVY;                               /* LD997-LD999: init screen flip */
    r = rehiin(0xFF, 0x00);                         /* LD99B JSR REHIIN (X = $FF from the POKEY loop, Y = 0) */
    r = iniini(r.x, r.y);                           /* LD99E JSR INIINI */
    (void)inidsp(r.x, r.y);                         /* LD9A1 JSR INIDSP */
    hw_cli();                                       /* LD9A4 CLI: the pending IRQ */
    g.cpu_loop = LOOP_MAINLN;                       /* LD9A5 JMP MAINLN */
    mainln();
}

/* ($D9A9, no label) zero-page test.  On entry all RAM is 0.  Each of the
 * patterns $11 $22 $44 $88 (kept in S) goes into every zero-page cell in turn,
 * all 255 other cells must still read 0, the cell must read the pattern back,
 * and is cleared.  Falls into the non-zero-page test.  JMPHIB -> HIBAD. */
void sftest_zp(void)
{
    uint8_t x, s, y, a;
    unsigned c;
    x = 0x11;                                               /* LD9A9 starting pattern */
    do {
        s = x;                                              /* LD9AB TXS */
        y = 0x00;                                           /* LD9AC */
        do {
            x = s;                                          /* LD9AE TSX */
            g.ram[y] = x;                                   /* LD9AF STX $00,Y pattern to test cell */
            x = 0x01;                                       /* LD9B1 */
            do {
                y++;                                        /* LD9B3 */
                a = g.ram[y];                               /* LD9B4 LDA $0000,Y */
                if (a != 0) {                               /* LD9B7 BEQ LD9BC */
                    hibad(a);                               /* LD9B9 JMPHIB: JMP HIBAD */
                    return;
                }
                x++;                                        /* LD9BC */
            } while (x != 0);                               /* LD9BD all other cells */
            x = s;                                          /* LD9BF TSX */
            a = x;                                          /* LD9C0 TXA */
            hw_watchdog();                                  /* LD9C1 kick dog */
            y++;                                            /* LD9C4 back to the test cell */
            a = (uint8_t)(a ^ g.ram[y]);                    /* LD9C5 EOR $0000,Y */
            if (a != 0) {                                   /* LD9C8 BNE JMPHIB */
                hibad(a);
                return;
            }
            g.ram[y] = a;                                   /* LD9CA STA $0000,Y clear test cell */
            y++;                                            /* LD9CD */
        } while (y != 0);                                   /* LD9CE */
        x = s;                                              /* LD9D0 TSX */
        a = x;                                              /* LD9D1 TXA */
        c = (a >> 7) & 1u;                                  /* LD9D2 ASL shift pattern */
        a = (uint8_t)(a << 1);
        x = a;                                              /* LD9D3 TAX */
    } while (!c);                                           /* LD9D4 BCC LD9AB */
    sftest_ram();                                           /* falls into LD9D6 (S = $88) */
}

/* ($D9D6, no label) non-zero-page RAM test: pages $01-$07 and vector RAM
 * $20-$2F through the pointer $00/$01.  Each cell must read 0, then hold
 * $11 $22 $44 $88, and is cleared; one watchdog kick per page.  Falls into
 * ROMTST.  Errors -> HIRBD2 (cell not 0) / HIRBAD (pattern not held). */
void sftest_ram(void)
{
    uint8_t x, y, a;
    unsigned c;
    y = 0x00;                                               /* LD9D6 start at page 1 */
    x = 0x01;                                               /* LD9D8 */
    do {
        g.ram[0x00] = y;                                    /* LD9DA STY $00 */
        g.ram[0x01] = x;                                    /* LD9DC STX $01 */
        y = 0x00;                                           /* LD9DE */
        do {
            a = cpu_rd((uint16_t)(PTR00_ADDR + y));         /* LD9E0 LDA ($00),Y */
            if (a != 0) {                                   /* LD9E2 BEQ LD9E7 */
                hirbd2(a);                                  /* LD9E4 JMP HIRBD2 */
                return;
            }
            a = 0x11;                                       /* LD9E7 */
            do {
                cpu_wr((uint16_t)(PTR00_ADDR + y), a);      /* LD9E9 STA ($00),Y */
                if (cpu_rd((uint16_t)(PTR00_ADDR + y)) != a) {  /* LD9EB-LD9ED CMP ($00),Y / BEQ */
                    hirbad(a, y);                           /* LD9EF JMP HIRBAD */
                    return;
                }
                c = (a >> 7) & 1u;                          /* LD9F2 ASL next pattern */
                a = (uint8_t)(a << 1);
            } while (!c);                                   /* LD9F3 BCC LD9E9 */
            a = 0x00;                                       /* LD9F5 */
            cpu_wr((uint16_t)(PTR00_ADDR + y), a);          /* LD9F7 STA ($00),Y clear test cell */
            y++;                                            /* LD9F9 */
        } while (y != 0);                                   /* LD9FA */
        hw_watchdog();                                      /* LD9FC */
        x++;                                                /* LD9FF next page */
        if (x == 0x08)                                      /* LDA00-LDA02 */
            x = HI(A_VECRAM);                               /* LDA04 LDX #VECRAM/$100 */
    } while (x < HI(A_VECRAM + 0x1000));                    /* LDA06-LDA08 CPX / BCC LD9DA */
    romtst();                                               /* falls into ROMTST */
}

/* ROMTST ($DA0A): 12 x 2K checksums (seed = ROM index, EOR over $3000-$3FFF
 * then $9000-$DFFF, one kick per page) into CHKSMS; continuous tone on channel
 * 3 if the vector ROM is bad; RANDOM / RANDO2 stuck tests (OK1, OK2); read the
 * EAROM (erase it when bad: QSTATE 0 = BADEAR, else 2 = ROMREP); colour RAM,
 * control and screen flips.  Ends at the diagnostic loop head $DA8D. */
void romtst(void)
{
    uint8_t a, x, y;
    a = 0x00;                                               /* LDA0A */
    y = a;                                                  /* LDA0C TAY index into page */
    x = a;                                                  /* LDA0D TAX checksum index & ROM counter */
    INDYLO = a;                                             /* LDA0E */
    INDYHI = HI(K_ROMSTART);                                /* LDA10-LDA12 first ROM */
    do {
        INDEX2 = 0x08;                                      /* LDA14-LDA16 8 pages per ROM */
        a = x;                                              /* LDA18 TXA seed */
        do {
            do {
                a = (uint8_t)(a ^ cpu_rd((uint16_t)(INDY_ADDR + y)));   /* LDA19 EOR (INDYLO),Y */
                y++;                                        /* LDA1B */
            } while (y != 0);                               /* LDA1C */
            INDYHI = (uint8_t)(INDYHI + 1);                 /* LDA1E next page */
            hw_watchdog();                                  /* LDA20 kick dog */
            INDEX2 = (uint8_t)(INDEX2 - 1);                 /* LDA23 */
        } while (INDEX2 != 0);                              /* LDA25 */
        g.ram[K_CHKSMS + x] = a;                            /* LDA27 STA CHKSMS,X */
        x++;                                                /* LDA29 */
        if (x == 0x02)                                      /* LDA2A-LDA2C program ROM now? */
            INDYHI = ALTES2_PROG_PAGE;                      /* LDA2E-LDA30 LDA #<[PROG/$100] */
    } while (x < K_NROMS);                                  /* LDA32-LDA34 */
    if (g.ram[K_CHKSMS] != 0) {                             /* LDA36-LDA38 bad VG ROM? */
        hw_pokey_write(0, 0x04, 0x40);                      /* LDA3A / LDA3E STA AUDF1+4 */
        hw_pokey_write(0, 0x05, 0xA4);                      /* LDA3C / LDA41 STX AUDC1+4 */
    }
    /* test both POKEYs for random numbers */
    x = 0x05;                                               /* LDA44 */
    a = hw_random(0);                                       /* LDA46 LDA RANDOM */
    for (;;) {
        if (hw_random(0) != a) break;                       /* LDA49-LDA4C CMP RANDOM / BNE OK1 */
        x--;                                                /* LDA4E */
        if (x & 0x80) {                                     /* LDA4F BPL LDA49 */
            PK1CND = a;                                     /* LDA51 bad POKEY 1 */
            break;
        }
    }
    x = 0x05;                                               /* LDA53 OK1 */
    a = hw_random(1);                                       /* LDA55 LDA RANDO2 */
    for (;;) {
        if (hw_random(1) != a) break;                       /* LDA58-LDA5B CMP RANDO2 / BNE OK2 */
        x--;                                                /* LDA5D */
        if (x & 0x80) {                                     /* LDA5E BPL LDA58 */
            PK2CND = a;                                     /* LDA60 bad POKEY 2 */
            break;
        }
    }
    (void)rehiin(x, y);                                     /* LDA62 OK2: JSR REHIIN (Y = 0) */
    y = 0x02;                                               /* LDA65 default good */
    a = EABAD;                                              /* LDA67 */
    if (a != 0) {                                           /* LDA6A bad EAROM? */
        EARCND = a;                                         /* LDA6C */
        eazero();                                           /* LDA6E erase EAROM */
        y = 0x00;                                           /* LDA71 */
        EABAD = y;                                          /* LDA73 */
    }
    QSTATE = y;                                             /* LDA76 */
    for (x = 0x07; !(x & 0x80); x--)                        /* LDA78, LDA80-LDA81 DEX / BPL */
        hw_color(x, ROM(ALTES2_TABCOL + x));                /* LDA7A-LDA7D STA COLPORT,X */
    hw_outank(0x00);                                        /* LDA83-LDA85 init controls flip */
    hw_out0(K_MVINVY);                                      /* LDA88-LDA8A init screen flip */
    g.cpu_loop = LOOP_DIAG;                                 /* -> LDA8D main diag loop */
}

/* ($DA8D-$DAF7) one pass of the main self-test diagnostic loop: wait up to 5
 * tries of 21 x 3 kHz periods for the VG HALT, then TIMEST: new list, pot and
 * switch reads, diag-step debounce, the state's screen (SSTATE), VGHALT,
 * VGSTART, EAUPD every 4th pass.  TEST switch still closed: returns at $DA8D.
 * Open: WDGTST spins until the hardware watchdog resets the CPU =
 * hw_watchdog_hang(). */
void diag_pass(void)
{
    uint8_t x, y, v;
    unsigned c;
    y = 0x04;                                               /* LDA8D */
    for (;;) {
        x = 0x14;                                           /* LDA8F */
        do {                                                /* loop for 7 ms */
            do v = hw_in1(); while (!(v & 0x80));           /* LDA91-LDA94 BIT IN1 / BPL */
            do v = hw_in1(); while (v & 0x80);              /* LDA96-LDA99 BIT IN1 / BMI */
            x--;                                            /* LDA9B */
        } while (!(x & 0x80));                              /* LDA9C BPL */
        y--;                                                /* LDA9E */
        if (y & 0x80) break;                                /* LDA9F BMI TIMEST: abort if too long */
        hw_watchdog();                                      /* LDAA1 */
        if (hw_in1() & 0x40) break;                         /* LDAA4-LDAA7 BIT IN1 / BVC LDA8F (VG HALT) */
    }
    /* TIMEST */
    hw_vgstop();                                            /* LDAA9 */
    VGLIST = LO(A_VECRAM);                                  /* LDAAC-LDAAE top of vector RAM */
    g.ram[A_VGLIST + 1] = HI(A_VECRAM);                     /* LDAB0-LDAB2 */
    hw_pokey_write(0, 0x0B, HI(A_VECRAM));                  /* LDAB4 STA POTGO (A = $20) */
    v = hw_allpot(0);                                       /* LDAB7 LDA ALLPOT */
    OTB = v;                                                /* LDABA */
    TBHD = (uint8_t)(v & 0x0F);                             /* LDABC-LDABE read pot */
    v = (uint8_t)((hw_in1() ^ 0xFF)                         /* LDAC0-LDAC3 */
                  & (K_MCOINL | K_MCOINC | K_MCOINR | K_S_LMBIT | K_MDITES));  /* LDAC5 */
    SWFINA = v;                                             /* LDAC7 */
    if (v & (K_S_LMBIT | K_MDITES)) {                       /* LDAC9-LDACB diagnostic switch pressed? */
        c = (DBSW >> 7) & 1u;                               /* LDACD ASL DBSW */
        DBSW = (uint8_t)(DBSW << 1);
        if (c) {                                            /* LDACF depressed long enough? */
            QSTATE = (uint8_t)(QSTATE + 1);                 /* LDAD1 INC QSTATE */
            QSTATE = (uint8_t)(QSTATE + 1);                 /* LDAD3 INC QSTATE */
        }
    } else {
        DBSW = 0x20;                                        /* LDAD8-LDADA restart pressed timer */
    }
    sstate(y);                                              /* LDADC JSR SSTATE */
    vghalt();                                               /* LDADF JSR VGHALT (exits X = $80, Y = 1) */
    hw_vgstart();                                           /* LDAE2 STA VGSTART */
    QFRAME = (uint8_t)(QFRAME + 1);                         /* LDAE5 */
    if ((QFRAME & 0x03) == 0)                               /* LDAE7-LDAEB */
        (void)eaupd(0x80, 0x01);                            /* LDAED JSR EAUPD */
    if (hw_in1() & K_MTEST) {                               /* LDAF0-LDAF5 BEQ LDA8D while TEST closed */
        hw_watchdog_hang();                                 /* LDAF7 WDGTST: BNE WDGTST */
        return;
    }
}

/* SFTJSE = SSTATE ($DB0F): run self-test state QSTATE (>= 14 -> 2) through
 * the SFTJSR RTS dispatch (PHA / PHA / NOOPR_DB21 RTS).  Y = the diag loop's
 * Y (BADEAR passes it to REHIIN). */
void sstate(uint8_t y)
{
    uint8_t x = QSTATE;                                     /* LDB0F */
    uint8_t lo;
    uint16_t target;
    if (x >= (uint8_t)(ALTES2_SFTJSE - ALTES2_SFTJSR)) {    /* LDB11-LDB13 CPX #SFTJSE-SFTJSR / BCC */
        x = 0x02;                                           /* LDB15 */
        QSTATE = x;                                         /* LDB17 */
    }
    lo = ROM(ALTES2_SFTJSR + x);                            /* LDB1D (A at the RTS) */
    target = (uint16_t)((lo | ((uint16_t)ROM(ALTES2_SFTJSR + 1 + x) << 8)) + 1);  /* LDB19-LDB21 */
    switch (target) {
    case ALTES2_BADEAR:  (void)badear(x, y); break;
    case ALTES2_ROMREP:  romrep(); break;
    case ALTES2_SHATCH:  shatch(); break;
    case ALTES2_SHYSTER: shyster(); break;
    case ALTES2_SINTEN:  sinten(); break;
    case ALTES2_SCHEKR:  schekr(); break;
    case ALTES2_SIGANA:  sigana(); break;
    default:             hw_soft_watchdog(); break;         /* odd QSTATE: the ROM runs wild (never reached) */
    }
}

/* SIGANA ($DB22): signature analysis - strobe every chip select with 0, read
 * the Mathbox / EAROM ports, open the window, start the Mathbox at all 32
 * addresses with a walking one, then the screen boundary box. */
void sigana(void)
{
    uint8_t a, x;
    unsigned c, nc;
    hw_outank(0x00);                                        /* LDB22-LDB24 close signature window */
    hw_mb_write(0x00, 0x00);                                /* LDB27 STA MBSTAR clock it */
    hw_pokey_write(0, 0x00, 0x00);                          /* LDB2A STA POKEY */
    hw_pokey_write(1, 0x00, 0x00);                          /* LDB2D STA POKEY2 */
    hw_earom_write(0x00, 0x00);                             /* LDB30 STA EADAL */
    hw_earom_ctl(0x00);                                     /* LDB33 STA EACTL */
    (void)hw_mb_status();                                   /* LDB36 LDA MSTAT */
    (void)hw_mb_ylow();                                     /* LDB39 LDA MYLOW */
    (void)hw_mb_yhigh();                                    /* LDB3C LDA MYHIGH */
    (void)hw_earom_read();                                  /* LDB3F LDA EAIN */
    hw_outank(0x08);                                        /* LDB42-LDB44 open signature window */
    a = 0x01;                                               /* LDB47 */
    x = 0x1F;                                               /* LDB49 */
    c = 0;                                                  /* LDB4B CLC */
    do {
        hw_mb_write(x, a);                                  /* LDB4C STA MBSTAR,X scan MB mapping PROM */
        nc = (a >> 7) & 1u;                                 /* LDB4F ROL */
        a = (uint8_t)((a << 1) | c);
        c = nc;
        x--;                                                /* LDB50 */
    } while (!(x & 0x80));                                  /* LDB51 BPL */
    vgjsrl(HI(ROM_BONDRY + 1), LO(ROM_BONDRY));             /* LDB53-LDB57 JMP VGJSRL: big box */
}

/* BADEAR ($DB5A): bad EAROM recovery - once the erase is done, read the EAROM
 * again, record EABAD in EARCND and go to the report state. */
xy6502 badear(uint8_t x, uint8_t y)
{
    xy6502 r = xy(x, y);
    if ((EAFLG | EAREQU) == 0) {                            /* LDB5A-LDB60 done erasing? */
        r = rehiin(x, y);                                   /* LDB62 try to read again */
        EARCND = EABAD;                                     /* LDB65-LDB68 still bad? */
        QSTATE = 0x02;                                      /* LDB6A-LDB6C report status state */
    }
    return r;                                               /* LDB6E */
}

/* NOSOUN ($DB8B): AUDC1..AUDC4 of both POKEYs = 0. */
static void nosoun(void)
{
    uint8_t x = 0x06;                                       /* LDB8B */
    do {                                                    /* LDB8D LDA #0 */
        hw_pokey_write(0, (uint8_t)(0x01 + x), 0x00);       /* LDB8F STA AUDC1,X */
        hw_pokey_write(1, (uint8_t)(0x01 + x), 0x00);       /* LDB92 STA AUDC2,X */
        x = (uint8_t)(x - 2);                               /* LDB95-LDB96 */
    } while (!(x & 0x80));                                  /* LDB97 BPL */
}

/* JSRVGJ ($DB88): JSRL to picture A:X, then NOSOUN. */
static void jsrvgj(uint8_t a, uint8_t x)
{
    vgjsrl(a, x);                                           /* LDB88 */
    nosoun();                                               /* falls into NOSOUN */
}

/* SCHEKR ($DB6F): checker board in the colour TBHD / 2. */
void schekr(void)
{
    vgstat(0x68, (uint8_t)(TBHD >> 1));                     /* LDB6F-LDB75 LDA TBHD / LSR / TAY / LDA #$68 / JSR VGSTAT */
    jsrvgj(HI(ROM_CHEKER + 1), LO(ROM_CHEKER));             /* LDB78-LDB7C BNE JSRVGJ */
}

/* SINTEN ($DB7E): intensity test pattern. */
void sinten(void)
{
    jsrvgj(HI(ROM_INTEST + 1), LO(ROM_INTEST));             /* LDB7E-LDB82 BNE JSRVGJ */
}

/* SHATCH ($DB84): cross hatch and alphabet. */
void shatch(void)
{
    jsrvgj(HI(ROM_HATCH + 1), LO(ROM_HATCH));               /* LDB84-LDB86, falls into JSRVGJ */
}

/* SHYSTER ($DB9A): sound test - every 64 QFRAMEs the next channel of SNDTBL
 * (previous one off via SNDTBL-1,X), the hysteresis picture and the box of
 * ramping linear scale. */
void shyster(void)
{
    uint8_t x, y;
    if ((QFRAME & 0x3F) == 0)                               /* LDB9A-LDB9E */
        INDEX3 = (uint8_t)(INDEX3 + 1);                     /* LDBA0 update timer */
    x = (uint8_t)(INDEX3 & 0x07);                           /* LDBA2-LDBA6 */
    y = ROM(ALTES2_SNDTBL_M1 + x);                          /* LDBA7 LDY SNDTBL-1,X */
    pokey_abs((uint16_t)(A_AUDC1 + y), 0x00);               /* LDBAA-LDBAC turn off old channel */
    y = ROM(ALTES2_SNDTBL + x);                             /* LDBAF */
    pokey_abs((uint16_t)(A_AUDF1 + y), ROM(ALTES2_SNDFRQ + x));    /* LDBB2-LDBB5 turn on new channel */
    pokey_abs((uint16_t)(A_AUDC1 + y), 0xA8);               /* LDBB8-LDBBA */
    vgjsrl(HI(ROM_HYSTER + 1), LO(ROM_HYSTER));             /* LDBBD-LDBC1 hysteresis */
    vgscal(0x01, (uint8_t)(QFRAME & 0x7F));                 /* LDBC4-LDBCB ramp linear scale */
    vgjsrl(HI(ROM_VORBOX + 1), LO(ROM_VORBOX));             /* LDBCE-LDBD2 JMP VGJSRL: vary box size */
}

/* GETOP3 ($DBE0): option switch 3 from the POKEYs' pot lines into A
 * (called by ALLANG's INILIT and DOPSWI).  X/Y unchanged. */
uint8_t getop3(uint8_t a, uint8_t x, uint8_t y)
{
    uint8_t v;
    (void)x; (void)y;
    hw_pokey_write(1, 0x0B, a);                     /* LDBE0 STA POTGO2 */
    v = (uint8_t)(hw_allpot(1) & K_MOPT13);         /* LDBE3-LDBE6 LDA ALLPO2 / AND #MOPT13 */
    INDEX1 = v;                                     /* LDBE8 STA INDEX1 */
    hw_pokey_write(0, 0x0B, v);                     /* LDBEA STA POTGO */
    v = (uint8_t)(hw_allpot(0) & K_MOPTI4);         /* LDBED-LDBF0 LDA ALLPOT / AND #MOPTI4 */
    return (uint8_t)((v >> 2) | INDEX1);            /* LDBF2-LDBF4 LSR / LSR / ORA INDEX1 */
}

/* ROMREP ($DBF7): report state - Mathbox divide test (TEMPX:TEMPY by itself
 * must give 1), switch test with sounds, option switches, cocktail flips, bad
 * ROM checksums, the M R P Q E status letters and the spinner clock hand. */
void romrep(void)
{
    uint8_t a, x, y;
    int bad;
    /* math box test */
    a = TEMPX;                                              /* LDBF7 */
    if (a != 0) {                                           /* LDBF9 no divide by 0 please */
        hw_mb_write(MB_MXPL, a);                            /* LDBFB */
        hw_mb_write(MB_MZLL, a);                            /* LDBFE */
        a = TEMPY;                                          /* LDC01 */
        hw_mb_write(MB_MXPH, a);                            /* LDC03 */
        x = 0x00;                                           /* LDC06 */
        a = readmb(a, &x, &y);                              /* LDC08 do divide */
        if (a != 0x01) {                                    /* LDC0B-LDC0D */
            bad = 1;
        } else {
            a = y;                                          /* LDC0F TYA */
            if (a != 0) {                                   /* LDC10 */
                bad = 1;
            } else {
                a = x;                                      /* LDC12 TXA */
                bad = (a & 0x80) != 0;                      /* LDC13 BPL: timed out? */
            }
        }
        if (bad) {                                          /* BADBOX */
            a = 0xFF;                                       /* LDC15 */
            MBCOND = a;                                     /* LDC17 bad mathbox */
        }
    }
    x = 0x00;                                               /* LDC19 */
    VGBRIT = x;                                             /* LDC1B */
    TEMPX = (uint8_t)(TEMPX + 1);                           /* LDC1D update divisor & dividend */
    if (TEMPX == 0) {                                       /* LDC1F */
        TEMPY = (uint8_t)(TEMPY + 1);                       /* LDC21 */
        if (TEMPY & 0x80)                                   /* LDC23 BPL */
            TEMPY = x;                                      /* LDC25 */
    }
    /* switch test */
    hw_pokey_write(1, 0x0B, a);                             /* LDC27 STA POTGO2 */
    a = (uint8_t)(hw_allpot(1) & (K_MSTRT1 | K_MSTRT2 | K_MSUZA | K_MFIRE));   /* LDC2A-LDC2D */
    SWSTAT = a;                                             /* LDC2F */
    if (a != 0) {                                           /* LDC31 any switches pressed? */
        hw_pokey_write(0, 0x00, a);                         /* LDC33 STA AUDF1: make sound */
        x = 0xA4;                                           /* LDC36 */
    }
    hw_pokey_write(0, 0x01, x);                             /* LDC38 STX AUDC1 */
    x = 0x00;                                               /* LDC3B */
    a = SWFINA;                                             /* LDC3D */
    if (a != 0) {                                           /* LDC3F switches pressed? */
        a = (uint8_t)(a << 1);                              /* LDC41 ASL */
        hw_pokey_write(0, 0x02, a);                         /* LDC42 STA AUDF1+2 */
        x = 0xA4;                                           /* LDC45 */
    }
    hw_pokey_write(0, 0x03, x);                             /* LDC47 STX AUDC1+2 */
    dopswi();                                               /* LDC4A display option switches */
    genopd(0xD0, 0xF0, SWSTAT);                             /* LDC4D-LDC53 display switches */
    bits2(SWFINA);                                          /* LDC56-LDC58 */
    if (OTB & K_COCKTA) {                                   /* LDC5B-LDC5F cocktail? */
        vgjsrl(HI(ROM_COCMSG + 1), LO(ROM_COCMSG));         /* LDC61-LDC65 a C in the corner */
        y = K_MVINVY;                                       /* LDC68 default no XY flip */
        a = (uint8_t)(SWSTAT & (K_MSTRT1 | K_MSTRT2));      /* LDC6A-LDC6C */
        if (a != 0) {                                       /* LDC6E either start pressed? */
            a = (uint8_t)(a ^ K_MSTRT1);                    /* LDC70 */
            if (a != 0) {                                   /* LDC72 start 2? */
                a = K_MFLIP;                                /* LDC74 flip screen to player 2 */
                y = K_MVINVX;                               /* LDC76 */
            }
            hw_outank(a);                                   /* LDC78 */
            hw_out0(y);                                     /* LDC7B */
        }
    }
    vgjsrl(HI(ROM_ROMRPI + 1), LO(ROM_ROMRPI));             /* LDC7E-LDC82 horiz line & position */
    x = (uint8_t)(K_NROMS - 1);                             /* LDC85 */
    do {                                                    /* loop for each ROM */
        a = g.ram[K_CHKSMS + x];                            /* LDC87 LDA CHKSMS,X */
        if (a != 0) {                                       /* LDC89 bad checksum? */
            SAVEX = a;                                      /* LDC8B */
            INDEX2 = x;                                     /* LDC8D */
            (void)vghex(x);                                 /* LDC8F-LDC90 TXA / JSR VGHEX: ROM # */
            posdig(SAVEX, 0xF4, 0xF4);                      /* LDC93-LDC99 LDY / LDX #-48./4 / LDA SAVEX / JSR POSDIG */
            vgvtr1(0x0C, 0x0C);                             /* LDC9C-LDC9F LDA #48./4 / TAX: space back & down */
            x = INDEX2;                                     /* LDCA2 */
        }
        x--;                                                /* LDCA4 */
    } while (!(x & 0x80));                                  /* LDCA5 BPL */
    vgcntr();                                               /* LDCA7 */
    vgvtr1(0x00, 0x16);                                     /* LDCAA-LDCAE LDA #0 / LDX #88./4 */
    x = 0x04;                                               /* LDCB1 */
    INDEX1 = x;                                             /* LDCB3 */
    do {                                                    /* loop for each status */
        x = INDEX1;                                         /* LDCB5 */
        y = 0x00;                                           /* LDCB7 default good (blank) */
        if (g.ram[K_MBCOND + x] != 0)                       /* LDCB9-LDCBB LDA MBCOND,X */
            y = ROM(ALTES2_BADNWS + x);                     /* LDCBD bad: letter */
        vgadd2(ROM(ROM_VGMSGA + y), ROM(ROM_VGMSGA + 1 + y));   /* LDCC0-LDCC6 */
        INDEX1 = (uint8_t)(INDEX1 - 1);                     /* LDCC9 */
    } while (!(INDEX1 & 0x80));                             /* LDCCB BPL */
    vgvtr1(0x30, 0xAC);                                     /* LDCCD-LDCD1 vector to clock position */
    y = TBHD;                                               /* LDCD4 */
    vgvtr(ROM(ALTES2_POTXTA + y), ROM(ALTES2_POTYTA + y), 0xC0);   /* LDCD6-LDCDE JMP VGVTR */
}

/* READMB ($DCE6): start a Mathbox divide (MZLH = A, MZHL = X, MZHH = 0,
 * MNL = MSZXD = $10) and poll MSTAT up to 16 times.  Returns A = MYLOW,
 * *y = MYHIGH, *x = polls left; timed out (TOOSLO): *x = $FF, A = the last
 * MSTAT, *y = 0.  VGBRIT = NGAVGZ = 0 (the latter for DBOOKE's squeeze). */
uint8_t readmb(uint8_t a, uint8_t *x, uint8_t *y)
{
    uint8_t yy = 0x00, xx;
    VGBRIT = yy;                                            /* LDCE6-LDCE8 */
    NGAVGZ = yy;                                            /* LDCEA (useful for DBOOKE only) */
    hw_mb_write(MB_MZLH, a);                                /* LDCED */
    hw_mb_write(MB_MZHL, *x);                               /* LDCF0 */
    hw_mb_write(MB_MZHH, yy);                               /* LDCF3 */
    xx = 0x10;                                              /* LDCF6 */
    hw_mb_write(MB_MNL, xx);                                /* LDCF8 */
    hw_mb_write(MB_MSZXD, xx);                              /* LDCFB */
    for (;;) {
        xx--;                                               /* LDCFE */
        if (xx & 0x80) {                                    /* LDCFF BMI TOOSLO */
            *x = xx;
            *y = yy;
            return a;                                       /* LDD0C TOOSLO: RTS */
        }
        a = hw_mb_status();                                 /* LDD01 LDA MSTAT */
        if (!(a & 0x80)) break;                             /* LDD04 BMI LDCFE */
    }
    a = hw_mb_ylow();                                       /* LDD06 LDA MYLOW */
    yy = hw_mb_yhigh();                                     /* LDD09 LDY MYHIGH */
    *x = xx;
    *y = yy;
    return a;                                               /* LDD0C */
}

/* DOPSWI ($DD0D): option switch display - INOP0, INOP1 and option switch 3 as
 * binary, big digits; falls into BITS2 with GETOP3's value. */
void dopswi(void)
{
    uint8_t v;
    vgcntr();                                               /* LDD0D */
    vgsca1(0x00);                                           /* LDD10-LDD12 big digits */
    bits3(0xE8, hw_inop0());                                /* LDD15-LDD1A LDA #-96./4 / LDY INOP0 / JSR BITS3 */
    v = hw_inop1();                                         /* LDD1D LDY INOP1 */
    bits2(v);                                               /* LDD20 JSR BITS2 */
    /* LDD23 JSR GETOP3: A = VGLIST (VGHEX's VGADD left the new low byte in A),
     * X = VGHEX1's TAX of the last digit (INOP1 bit 0), Y = 1 */
    v = getop3(VGLIST, (uint8_t)(((v & 0x01) + 1) << 1), 0x01);
    bits2(v);                                               /* LDD26 TAY, falls into BITS2 */
}

/* BITS2 ($DD27): GENOPD with A = -192./4. */
void bits2(uint8_t y)
{
    bits3(0xD0, y);                                         /* LDD27 */
}

/* BITS3 ($DD29): GENOPD with X = $F8. */
void bits3(uint8_t a, uint8_t y)
{
    genopd(a, 0xF8, y);                                     /* LDD29 */
}

/* GENOPD ($DD2B): move the beam (A, X), then the 8 bits of Y as 0/1 digits,
 * MSB first. */
void genopd(uint8_t a, uint8_t x, uint8_t y)
{
    unsigned c;
    SAVEX = y;                                              /* LDD2B */
    vgvtr1(a, x);                                           /* LDD2D */
    INDEX1 = 0x07;                                          /* LDD30-LDD32 */
    do {                                                    /* loop for each bit in byte */
        c = (SAVEX >> 7) & 1u;                              /* LDD34 ASL SAVEX */
        SAVEX = (uint8_t)(SAVEX << 1);
        (void)vghex((uint8_t)c);                            /* LDD36-LDD39 LDA #0 / ROL / JSR VGHEX */
        INDEX1 = (uint8_t)(INDEX1 - 1);                     /* LDD3C */
    } while (!(INDEX1 & 0x80));                             /* LDD3E BPL */
}

/* DBOOKE ($DD41): bookkeeping - average play time (seconds on / games, games
 * = 1-player + 2 x 2-player games, via the Mathbox divide) into NGAVGL/H,
 * the literals, then the five triple-byte counters from BOOKKS (seconds,
 * games, average) converted to BCD with decimal-mode ADCs and displayed
 * (the TPLIST loop, fallen into). */
void dbooke(void)
{
    uint8_t a, x, y, lo;
    unsigned c, nc;
    a = NGAM2L;                                             /* LDD41 */
    c = (a >> 7) & 1u;                                      /* LDD44 ASL */
    a = (uint8_t)(a << 1);
    TEMP0 = a;                                              /* LDD45 */
    a = NGAM2H;                                             /* LDD47 */
    nc = (a >> 7) & 1u;                                     /* LDD4A ROL */
    a = (uint8_t)((a << 1) | c);
    c = nc;
    TEMP1 = a;                                              /* LDD4B */
    a = NGAMIL;                                             /* LDD4D */
    c = 0;                                                  /* LDD50 CLC */
    a = adc6502(a, TEMP0, &c, NULL);                        /* LDD51 ADC TEMP0 */
    hw_mb_write(MB_MXPL, a);                                /* LDD53 STA MXPL */
    TEMP0 = a;                                              /* LDD56 */
    a = adc6502(NGAMIH, TEMP1, &c, NULL);                   /* LDD58-LDD5B */
    hw_mb_write(MB_MXPH, a);                                /* LDD5D */
    if ((uint8_t)(a | TEMP0) == 0)                          /* LDD60-LDD62 divide by 0? */
        hw_mb_write(MB_MXPL, 0x01);                         /* LDD64-LDD66 make it 1 */
    hw_mb_write(MB_MZLL, SECOPL);                           /* LDD69-LDD6C */
    x = SECOPH;                                             /* LDD6F-LDD72 LDA SECOPM / LDX SECOPH */
    a = readmb(SECOPM, &x, &y);                             /* LDD75 do divide */
    NGAVGL = a;                                             /* LDD78 results */
    NGAVGH = y;                                             /* LDD7B */
    lo = VGLIST;
    vgjsrl(HI(ROM_BOKLIT + 1), LO(ROM_BOKLIT));             /* LDD7E-LDD82 bookkeeping literals */
    c = VGADD_CARRY(lo);                                    /* C as VGJSRL leaves it: into the first ROL PXL */
    INDYLO = LO(A_BOOKKS);                                  /* LDD85-LDD87 point to 1st # */
    a = HI(A_BOOKKS);                                       /* LDD89 */
    INDYHI = a;                                             /* LDD8B */
    INDEX1 = a;                                             /* LDD8D A = 4: five numbers (the source's LDA I,4 is commented out) */
    do {
        y = 0x00;                                           /* LDD8F */
        g.ram[A_MTEMP] = y;                                 /* LDD91 */
        g.ram[A_MTEMP + 1] = y;                             /* LDD93 */
        g.ram[A_MTEMP + 2] = y;                             /* LDD95 */
        g.ram[A_MTEMP + 3] = y;                             /* LDD97 */
        PXL = cpu_rd((uint16_t)(INDY_ADDR + y));            /* LDD99-LDD9B */
        INDYLO = (uint8_t)(INDYLO + 1);                     /* LDD9D */
        g.ram[A_PXL + 1] = cpu_rd((uint16_t)(INDY_ADDR + y));   /* LDD9F-LDDA1 */
        INDYLO = (uint8_t)(INDYLO + 1);                     /* LDDA3 */
        g.ram[A_PXL + 2] = cpu_rd((uint16_t)(INDY_ADDR + y));   /* LDDA5-LDDA7 */
        INDYLO = (uint8_t)(INDYLO + 1);                     /* LDDA9 */
        /* convert PXL(3) from hex to BCD in MTEMP(4) */
        CK(0xDDAB);                                         /* M9 B3: after the counter reads (the IRQ counts SECOUL $0406) */
        g.dflag = 1;                                        /* LDDAB SED */
        INDEX2 = 0x17;                                      /* LDDAC-LDDAE */
        do {                                                /* loop for 3*8 bits */
            for (int i = 0; i < 3; i++) {                   /* LDDB0-LDDB4 ROL PXL / PXL+1 / PXL+2 */
                a = g.ram[A_PXL + i];
                nc = (a >> 7) & 1u;
                g.ram[A_PXL + i] = (uint8_t)((a << 1) | c);
                c = nc;
            }
            y = 0x03;                                       /* LDDB6 */
            x = 0x00;                                       /* LDDB8 */
            do {                                            /* loop for each BCD byte */
                a = g.ram[A_MTEMP + x];                     /* LDDBA */
                a = adc6502(a, g.ram[A_MTEMP + x], &c, NULL);   /* LDDBC ADC MTEMP,X (decimal) */
                g.ram[A_MTEMP + x] = a;                     /* LDDBE */
                x++;                                        /* LDDC0 */
                y--;                                        /* LDDC1 */
            } while (!(y & 0x80));                          /* LDDC2 BPL */
            INDEX2 = (uint8_t)(INDEX2 - 1);                 /* LDDC4 */
        } while (!(INDEX2 & 0x80));                         /* LDDC6 BPL */
        g.dflag = 0;                                        /* LDDC8 CLD */
        digtys((uint8_t)A_MTEMP, 0x04);                     /* LDDC9-LDDCD output # */
        lo = VGLIST;
        vgvtr1(0xD0, 0xF8);                                 /* LDDD0-LDDD4 position for next # */
        c = VGADD_CARRY(lo);                                /* C as VGVTR1 leaves it */
        INDEX1 = (uint8_t)(INDEX1 - 1);                     /* LDDD7 */
    } while (!(INDEX1 & 0x80));                             /* LDDD9 BPL */
}                                                           /* LDDDB RTS */
