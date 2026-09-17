/* alearo.c - ALEARO ($DDDD-$DF08): EAROM high scores, initials and
 * bookkeeping - request entry points and the read / erase / write state
 * machine EAUPD (one byte per call for erase/write, a whole batch per call
 * for reads).  EAROM I/O goes through the seam in the ROM's order
 * (EADAL $6000+X, EACTL $6040, EAIN $6050).  Verified by tests/lockstep.exe
 * (the boot read is RESET's JSR REHIIN at $D99B).
 */
#include "state.h"
#include "hw.h"
#include "game.h"

#define TEAX     0xDDDD       /* TEAX:   EAROM offset of the lowest byte per group (ROM, step 2) */
#define TEACNT   0xDDDE       /* TEACNT: EAROM offset of the last byte (checksum) per group      */
#define TEASRL   0xDDE3       /* TEASRL/TEASRH: RAM source address per group                     */
#define TEASRH   0xDDE4

#define EASRCE_ADDR ((uint16_t)(EASRCE | ((uint16_t)g.ram[A_EASRCE + 1] << 8)))

static xy6502 xy(uint8_t x, uint8_t y) { xy6502 r; r.x = x; r.y = y; return r; }

/* GENREQ ($DDFF): EAZFLG = Y; add request bits A to EAREQU and EARWRQ. */
static void genreq(uint8_t a, uint8_t y)
{
    EAZFLG = y;                                 /* LDDFF */
    EAREQU = (uint8_t)(a | EAREQU);             /* LDE02-LDE06 PHA / ORA / STA */
    EARWRQ = (uint8_t)(a | EARWRQ);             /* LDE09-LDE0D PLA / ORA / STA */
}

/* GENZER ($DDF3): request zeroing of the groups in A. */
static void genzer(uint8_t a) { genreq(a, 0xFF); }        /* LDDF3-LDDF5 */

/* NOZERO ($DDFD): request writing of the groups in A. */
static void nozero(uint8_t a) { genreq(a, 0x00); }        /* LDDFD */

void eazboo(void) { genzer(0x04); }             /* EAZBOO $DDE9: zero bookkeeping only */
void eazhis(void) { genzer(0x03); }             /* EAZHIS $DDED: zero hi scores / initials only */
void eazero(void) { genzer(0x07); }             /* EAZERO $DDF1: zero everything */
void wrhiin(void) { nozero(0x03); }             /* WRHIIN $DDF7: write hi scores & initials */
void wrbook(void) { nozero(0x04); }             /* WRBOOK $DDFB: write bookkeeping */

/* REHIIN ($DE11): read in everything, now. */
xy6502 rehiin(uint8_t x, uint8_t y)
{
    EAREQU = 0x07;                              /* LDE11-LDE13 */
    EARWRQ = 0x00;                              /* LDE16-LDE18 */
    return eaupd(x, y);                         /* falls into EAUPD */
}

/* EAUPD ($DE1B): EAROM I/O mainline.  EAFLG: 0 idle, $80 erase, $40 write,
 * $20 read.  EAX = EAROM address, EABC = offset from @EASRCE, EACNT = last
 * address (the checksum byte). */
xy6502 eaupd(uint8_t x, uint8_t y)
{
    for (;;) {
        uint8_t a;
        unsigned c;
        if (EAFLG == 0 && EAREQU != 0) {        /* LDE1B-LDE23: no activity, a request? */
            x = 0x00;                           /* LDE25 */
            EABC = x;                           /* LDE27: zero source index */
            EACS = x;                           /* LDE2A: zero checksum */
            EASEL = x;                          /* LDE2D: zero select bit */
            x = 0x08;                           /* LDE30 */
            a = EAREQU;                         /* (A = EAREQU from LDE20) */
            c = 1;                              /* LDE32 SEC */
            do {
                EASEL = (uint8_t)((EASEL >> 1) | (c << 7));   /* LDE33 ROR EASEL */
                c = (a & 0x80) ? 1u : 0u;       /* LDE36 ASL */
                a = (uint8_t)(a << 1);
                x--;                            /* LDE37 */
            } while (!c);                       /* LDE38: exit when the set bit is found */
            y = K_EAERAS;                       /* LDE3A: default erase/write */
            if ((EASEL & EARWRQ) == 0)          /* LDE3C-LDE42 */
                y = K_EAREAD;                   /* LDE44 */
            EAFLG = y;                          /* LDE46 */
            EAREQU = (uint8_t)(EASEL ^ EAREQU); /* LDE49-LDE4F: turn off request bit */
            x = (uint8_t)(x << 1);              /* LDE52-LDE54 TXA / ASL / TAX */
            EAX = cpu_rd((uint16_t)(TEAX + x));                     /* LDE55-LDE58 */
            EACNT = cpu_rd((uint16_t)(TEACNT + x));                 /* LDE5B-LDE5E */
            EASRCE = cpu_rd((uint16_t)(TEASRL + x));                /* LDE61-LDE64 */
            g.ram[A_EASRCE + 1] = cpu_rd((uint16_t)(TEASRH + x));   /* LDE66-LDE69 */
        }
        y = 0x00;                               /* LDE6B: deselect chip */
        hw_earom_ctl(y);                        /* LDE6D */
        if (EAFLG == 0) return xy(x, y);        /* LDE70-LDE75: any activity? */
        y = EABC;                               /* LDE76 */
        x = EAX;                                /* LDE79 */
        c = (EAFLG & 0x80) ? 1u : 0u;           /* LDE7C ASL */
        a = (uint8_t)(EAFLG << 1);
        if (c) {                                /* LDE7D: erase */
            hw_earom_write(x, a);               /* LDE7F STA EADAL,X: store address */
            EAFLG = K_EAWRIT;                   /* LDE82-LDE84: request write */
            y = K_EAC1 + K_EAC2 + K_EACE;       /* LDE87: erase & select chip */
            goto ctl;                           /* LDE89-LDE8A */
        }
        if (a & 0x80) {                         /* LDE8C: write a byte */
            EAFLG = K_EAERAS;                   /* LDE8E-LDE90: erase for next byte */
            if (EAZFLG != 0)                    /* LDE93-LDE96: zero EAROM? */
                cpu_wr((uint16_t)(EASRCE_ADDR + y), 0x00);           /* LDE98-LDE9A: clear RAM too */
            a = cpu_rd((uint16_t)(EASRCE_ADDR + y));                 /* LDE9C */
            if (x >= EACNT) {                   /* LDE9E-LDEA1 */
                EAFLG = 0x00;                   /* LDEA3-LDEA5: all done */
                a = EACS;                       /* LDEA8: checksum */
            }
            hw_earom_write(x, a);               /* LDEAB STA EADAL,X */
            y = K_EAC1 + K_EACE;                /* LDEAE: write mode & chip select */
            goto sum;                           /* LDEB0-LDEB1 */
        }
        /* read */
        hw_earom_ctl(K_EACE);                   /* LDEB3-LDEB5 */
        hw_earom_write(x, K_EACE);              /* LDEB8: select address */
        hw_earom_ctl(K_EACE + K_EACK);          /* LDEBB-LDEBD: clock */
        hw_earom_ctl(K_EACE);                   /* LDEC0 NOP, LDEC1-LDEC3 */
        c = (x >= EACNT);                       /* LDEC6 CPX EACNT */
        a = hw_earom_read();                    /* LDEC9 EAIN */
        if (c) {                                /* LDECC: checksum? */
            a = (uint8_t)(a ^ EACS);            /* LDECE */
            if (a != 0) {                       /* LDED1: mismatch */
                a = 0x00;                       /* LDED3 */
                y = EABC;                       /* LDED5 */
                do {
                    cpu_wr((uint16_t)(EASRCE_ADDR + y), a);          /* LDED8 */
                    y--;                        /* LDEDA */
                } while (!(y & 0x80));          /* LDEDB */
                EABAD = (uint8_t)(EASEL | EABAD);                    /* LDEDD-LDEE3: bad flag */
            }
            a = 0x00;                           /* LDEE6 */
            EAFLG = a;                          /* LDEE8: all done */
        } else {
            cpu_wr((uint16_t)(EASRCE_ADDR + y), a);                  /* LDEEE: raw data to RAM */
        }
        y = 0x00;                               /* LDEF0: deselect */
    sum:
        c = 0;                                  /* LDEF2 CLC */
        EACS = adc6502(a, EACS, &c, NULL);      /* LDEF3-LDEF6: update checksum */
        EABC = (uint8_t)(EABC + 1);             /* LDEF9 */
        EAX = (uint8_t)(EAX + 1);               /* LDEFC */
    ctl:
        hw_earom_ctl(y);                        /* LDEFF */
        if (y != 0) return xy(x, y);            /* LDF02-LDF03, LDF08 RTS */
        /* LDF05 JMP EAUPD: read - do all reads at once */
    }
}
