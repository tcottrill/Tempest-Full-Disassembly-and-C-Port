/* alvgut.c - ALVGUT ($DF09-$DFDB): vector generator utilities.
 *
 * They append real AVG words to the display list through the zero-page
 * pointer VGLIST ($74/$75), exactly as the ROM does (rule 4: the words the
 * ROM wrote are the words we write).  Every store the ROM makes is made,
 * scratch cells included (XCOMP, ZPNLOC, ZPOFFS).
 *
 * Register protocol per Atari's ENTRY/EXIT headers; register values the
 * headers mark as "USES" (clobbered) are not returned.  Verified call by call
 * against the ROM by tests/lockstep.exe.
 */
#include "state.h"
#include "game.h"

/* STA (VGLIST),Y */
static void vg_put(uint8_t y, uint8_t v)
{
    cpu_wr((uint16_t)(VGLIST_ADDR + y), v);
}

/* VGADD ($DF5F): VGLIST += Y+1.  ENTRY (Y). */
void vgadd(uint8_t y)
{
    unsigned sum = (unsigned)y + 1u + VGLIST;     /* LDF5F-LDF61: TYA / SEC / ADC VGLIST */
    VGLIST = (uint8_t)sum;                          /* LDF63 */
    if (sum > 0xFF)                                 /* LDF65 BCC */
        g.ram[A_VGLIST + 1]++;                      /* LDF67 INC VGLIST+1 */
}

/* VGWAI1 ($DFAC, internal): INY / STA (VGLIST),Y / BNE VGADD (always). */
static void vgwai1(uint8_t a, uint8_t y)
{
    y++;                                            /* LDFAC */
    vg_put(y, a);                                   /* LDFAD */
    vgadd(y);                                       /* LDFAF */
}

/* VGHAL1 ($DF12, internal): the RTSL/HALT word, A in both bytes. */
static void vghal1(uint8_t a)
{
    vg_put(0, a);                                   /* LDF12-LDF14 */
    vgwai1(a, 0);                                   /* LDF16 JMP VGWAI1 */
}

/* VGRTSL ($DF09): add RTSL ($C0,$C0). */
void vgrtsl(void)
{
    vghal1(0xC0);                                   /* LDF09-LDF0B */
}

/* VGHALT ($DF0D): centre, then HALT ($20,$20). */
void vghalt(void)
{
    vgcntr();                                       /* LDF0D */
    vghal1(0x20);                                   /* LDF10 */
}

/* VGHEX1 ($DF24, internal): JSRL to character A (0 = blank, n+1 = digit n)
 * from the VGMSGA table in the vector ROM; returns the C it was given. */
static int vghex1(uint8_t a, int c)
{
    uint8_t x = (uint8_t)(a << 1);                  /* LDF25-LDF28: ASL / LDY #0 / TAX */
    vg_put(0, cpu_rd((uint16_t)(A_VGMSGA + x)));    /* LDF29-LDF2C */
    vg_put(1, cpu_rd((uint16_t)(A_VGMSGA + x + 1)));/* LDF2E-LDF32 */
    vgadd(1);                                       /* LDF34 */
    return c;                                       /* LDF24 PHP ... LDF37 PLP */
}

/* VGHEX ($DF1F): display the low nibble of A.  EXIT C clear. */
int vghex(uint8_t a)
{
    return vghex1((uint8_t)((a & 0x0F) + 1), 0);    /* LDF1F-LDF22: AND / CLC / ADC #1 */
}

/* VGHEXZ ($DF19): digit with zero suppression.  ENTRY C set = suppress.
 * EXIT C cleared if a non-zero digit was displayed (a suppressed zero shows
 * the blank character and returns C set). */
int vghexz(uint8_t a, int c)
{
    if (!c) return vghex(a);                        /* LDF19 BCC VGHEX */
    a &= 0x0F;                                      /* LDF1B */
    if (a == 0) return vghex1(0, 1);                /* LDF1D BEQ VGHEX1, C still set */
    return vghex(a);                                /* fall into VGHEX */
}

/* VGJSRL ($DF39): JSRL to the CPU address A:X (MSB:LSB). */
void vgjsrl(uint8_t a_msb, uint8_t x_lsb)
{
    uint8_t carry = a_msb & 1u;                                     /* LDF39 LSR */
    vg_put(1, (uint8_t)(((a_msb >> 1) & 0x0F) | 0xA0));             /* LDF3A-LDF40 */
    vg_put(0, (uint8_t)((x_lsb >> 1) | (carry << 7)));              /* LDF42-LDF45: DEY / TXA / ROR */
    vgadd(1);                                                       /* LDF47-LDF48: INY / BNE VGADD */
}

/* VGSTA1 ($DF4A): VGSTAT with Y = VGBRIT. */
void vgsta1(uint8_t a)
{
    vgstat(a, VGBRIT);                              /* LDF4A */
}

/* VGSTAT ($DF4C): STAT word, A = flag/colour bits, Y = low byte. */
void vgstat(uint8_t a, uint8_t y)
{
    vgadd2(y, (uint8_t)(a | 0x60));                 /* LDF4C-LDF50: ORA #$60 / TAX / TYA / JMP VGADD2 */
}

/* VGCNTR ($DF53): CNTR word ($40,$80). */
void vgcntr(void)
{
    vgadd2(0x40, 0x80);                             /* LDF53-LDF55 */
}

/* VGADD2 ($DF57): append bytes A, X. */
void vgadd2(uint8_t a, uint8_t x)
{
    vgadd3(a, x, 0);                                /* LDF57 LDY #0 */
}

/* VGADD3 ($DF59): store A at (VGLIST),Y and X at (VGLIST),Y+1; VGLIST += Y+2. */
void vgadd3(uint8_t a, uint8_t x, uint8_t y)
{
    vg_put(y, a);                                   /* LDF59 */
    y++;                                            /* LDF5B */
    vg_put(y, x);                                   /* LDF5C-LDF5D */
    vgadd(y);                                       /* fall into VGADD */
}

/* VGSCA1 ($DF6A): VGSCAL with linear scale 0. */
void vgsca1(uint8_t a)
{
    vgscal(a, 0);                                   /* LDF6A */
}

/* VGSCAL ($DF6C): SCAL word, A = binary scale, Y = linear scale. */
void vgscal(uint8_t a, uint8_t y)
{
    vgadd2(y, (uint8_t)(a | 0x70));                 /* LDF6C-LDF70 */
}

/* VGVTR ($DF73): VGBRIT = Y, then VGVTR1. */
void vgvtr(uint8_t a, uint8_t x, uint8_t y)
{
    VGBRIT = y;                                     /* LDF73 */
    vgvtr1(a, x);
}

/* VGVTR1 ($DF75): vector (A*4, X*4), signed, via XCOMP..XCOMP+3. */
void vgvtr1(uint8_t a, uint8_t x)
{
    uint8_t hi;
    hi = (a & 0x80) ? 0xFF : 0x00;                  /* LDF75-LDF7A: LDY #0 / ASL / BCC / DEY */
    g.ram[A_XCOMP + 1] = hi;                        /* LDF7B STY XCOMP+1 */
    g.ram[A_XCOMP + 1] = (uint8_t)((hi << 1) | ((a >> 6) & 1u)); /* LDF7D-LDF7E: ASL / ROL XCOMP+1 */
    g.ram[A_XCOMP] = (uint8_t)(a << 2);             /* LDF80 */
    hi = (x & 0x80) ? 0xFF : 0x00;                  /* LDF82-LDF88 */
    g.ram[A_XCOMP + 3] = hi;                        /* LDF89 */
    g.ram[A_XCOMP + 3] = (uint8_t)((hi << 1) | ((x >> 6) & 1u)); /* LDF8B-LDF8C */
    g.ram[A_XCOMP + 2] = (uint8_t)(x << 2);         /* LDF8E */
    vgvctr((uint8_t)A_XCOMP);                       /* LDF90 VGVTR2: LDX #XCOMP */
}

/* VGVCTR ($DF92): long vector from the 4 zero-page cells at X
 * (X lsb, X msb, Y lsb, Y msb), top 3 bits of VGBRIT as intensity. */
void vgvctr(uint8_t x)
{
    uint8_t a;
    vg_put(0, g.ram[(uint8_t)(x + 2)]);                     /* LDF92-LDF96 */
    vg_put(1, (uint8_t)(g.ram[(uint8_t)(x + 3)] & 0x1F));   /* LDF98-LDF9D */
    vg_put(2, g.ram[x]);                                    /* LDF9F-LDFA2 */
    a = g.ram[(uint8_t)(x + 1)];                            /* LDFA4 */
    a = (uint8_t)(((a ^ VGBRIT) & 0x1F) ^ VGBRIT);          /* LDFA6-LDFAA */
    vgwai1(a, 2);                                           /* VGWAI1 */
}

/* DIGTYS ($DFB1): DIGITS with zero suppression. */
void digtys(uint8_t a, uint8_t y)
{
    digits(a, y, 1);                                /* LDFB1 SEC */
}

/* DIGITS ($DFB2): display 2*Y BCD digits from the Y zero-page cells at A
 * (LSB first), most significant first.  ENTRY C set = zero suppression; the
 * last digit is always shown. */
void digits(uint8_t a, uint8_t y, int c)
{
    uint8_t x;
    y = (uint8_t)(y - 1);                           /* LDFB2-LDFB3: PHP / DEY */
    ZPNLOC = y;                                     /* LDFB4 */
    x = (uint8_t)(a + ZPNLOC);                      /* LDFB6-LDFBA: CLC / ADC / PLP / TAX */
    do {
        ZPOFFS = x;                                 /* LDFBB-LDFBC */
        c = vghexz((uint8_t)(g.ram[x] >> 4), c);    /* LDFBE-LDFC5 */
        if (ZPNLOC == 0) c = 0;                     /* LDFC8-LDFCC */
        c = vghexz(g.ram[ZPOFFS], c);               /* LDFCD-LDFD1 */
        x = (uint8_t)(ZPOFFS - 1);                  /* LDFD4-LDFD6 */
        ZPNLOC = (uint8_t)(ZPNLOC - 1);             /* LDFD7 */
    } while (!(ZPNLOC & 0x80));                     /* LDFD9 BPL */
}
