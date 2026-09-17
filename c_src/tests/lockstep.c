/* lockstep.c - call-by-call and pass-by-pass verification of the translated
 * modules against the real ROM running in tests/refrun.c (build: /DLOCKSTEP,
 * tests\lockstep.exe).
 *
 * How a routine check works
 *  - ENTRY.  When the ROM CPU arrives at a translated routine through JSR or
 *    JMP, or when the IRQ is taken, the whole machine memory (RAM, vector
 *    RAM, colour RAM) and the registers are snapshotted.  Checks nest (M3):
 *    a routine called inside an open check gets its own check, except a
 *    routine re-entering itself at the same stack level (EAUPD's JMP EAUPD).
 *    ROUTAD state routines are also entered by EXSTAT's PHA/PHA/RTS ($C7D9):
 *    for the routines flagged `rts` an RTS arrival opens a check as well
 *    (none of them follows a JSR in the ROM, so no ordinary return lands
 *    there).  The ROM then runs on untouched.
 *  - DURING.  Every hardware read and write the ROM makes (everything that is
 *    not RAM / colour RAM / vector RAM / ROM) is logged in order, plus the SP
 *    value the IRQ's TSX at $D70A sees.
 *  - COMPLETION.  At the instruction boundary where the ROM returns (SP back
 *    above the entry SP after RTS; for the IRQ, RTI back to the entry SP) the
 *    snapshot is loaded into the C port's `g`, the C function runs with the
 *    entry registers as arguments, and its hw_* calls REPLAY the log: reads get
 *    the ROM's values, writes must match address and value (strobes: address
 *    only), in the same order, and all of them must be consumed.
 *  - COMPARE.  C RAM == ROM RAM (except the stack bytes the ROM call itself
 *    pushed, $0100+minSP+1..$0100+entrySP), vector RAM and colour RAM equal,
 *    I/O sequence equal, documented return flag (C) equal, D flag equal.
 *  - A check that an IRQ interrupted where it cannot be placed (see below) is
 *    "tainted"; the IRQ itself is still checked.  Since M4 a tainted call is
 *    still compared with its IRQs moved: the C routine runs with each IRQ's
 *    own I/O range cut out of its replay and C irq() run on that range at the
 *    routine's entry, then (if that differs) after its return.  An exact
 *    match either way passes ("IRQ moved").  Both differing is a failure when
 *    neither run had an I/O mismatch and both differ in a cell the moved IRQ
 *    did not itself change; otherwise it stays tainted ("moved mismatch",
 *    e.g. MODSND stepping a channel a sound starter just set).  This is what
 *    verifies INFO / DSPCRD / LDRDSP, which always contain an IRQ.
 *  - Exit registers: adapters that set ret_a / ret_x / ret_y (M4: INICHK,
 *    HISCHK, GETINI, LOGINI, GAMSTA, INIINI, INTLDR, INILIT, GINICO, GETCUR,
 *    HEXBCD, NWHEXZ) have them compared with the ROM's A/X/Y at the return.
 *  - Calls from translated code into routines not translated yet run the ROM
 *    routine itself on a second ref6502 over `g` (a thunk), with its I/O
 *    replayed from the same log.  Entry X/Y and the D flag go in, exit X/Y
 *    and D come out; stack bytes the thunk pushes are put back afterwards (the
 *    thunk's stack depth differs from the ROM's; those bytes are garbage).
 *
 * How the pass probe works (M3)
 *  - A pass runs from $C7AD (FRTIMR = 0, where refrun captures frame_NNNN)
 *    to the next $C7AD: EXSTAT, NONSTA, DISPLAY, then the frame wait.  The
 *    boot pass runs from MAINLN ($C7A0) to the first $C7AD.  At the start the
 *    machine is snapshotted (the same bytes as tests\ref\frame_NNNN.ram/.vram);
 *    at the end C mainln_pass() (boot: mainln()) runs from the snapshot with
 *    the whole pass's I/O replayed and must reproduce RAM (minus stack
 *    garbage), vector RAM, colour RAM, the D flag and the IRQ count.
 *  - IRQs are placed where the ROM took them: each ROM IRQ is recorded as
 *    "in the frame wait" (the C wait calls irq()), or "in top-level thunk
 *    segment k after n instructions" (the thunk CPU calls irq() at the same
 *    instruction; a segment starts when the ROM, outside any segment, arrives
 *    at a thunk address and ends when SP rises above that level).
 *
 * IRQ placement in translated code (M5)
 *  - The event log holds, besides I/O, CHECKPOINT VISITS: the ROM arriving at
 *    an address of ck_addr[] outside the IRQ.  The C code marks the same
 *    statements with CK(0xLLLL) (game.h; compiled away outside lockstep) and
 *    must consume the visits in order, like I/O.
 *  - An IRQ in translated code is ANCHORED: C fires irq() as soon as its run
 *    has consumed every event logged before the IRQ's own I/O (the last
 *    event may be an I/O operation, a checkpoint visit, the end of the
 *    previous IRQ, or nothing since the run's start).  This is exact when the
 *    ROM instructions between that event and the interrupted instruction
 *    commute with the IRQ, which the ROM side proves from memory access sets
 *    (refrun.c's lk_mem hook): no RAM / colour / vector RAM cell the IRQ
 *    writes was read or written by them, none the IRQ reads was written by
 *    them (the stack page is exempt; they did no I/O by construction).  An
 *    IRQ taken at a checkpoint logs that visit first, so its window is empty.
 *    Checkpoints are needed only where a window does not commute; the report
 *    lists the PCs and cells of unplaceable IRQs to show where.
 *  - Checks and passes whose IRQs all place (thunk segment, frame wait or
 *    anchor) are compared exactly; others are skipped (pass) or fall back to
 *    the M4 "IRQ moved" compare (routine check).
 *
 * Replay trace for Gates 1/N (M6): `--trace-out FILE` also writes the whole
 * run from RESET - every I/O event, checkpoint visit and IRQ marker in ROM
 * order, and the machine image at every $C7AD - for tests\gate.exe, which
 * runs the C modules alone on it.  Format: see "M6 Gates 1/N" below.
 *
 * RESET events, RESET regions and the diag pass probe (M9 B3, ALTES2)
 *  - refrun.c calls lk_note_reset(cause) before the boundary at every RESET
 *    arrival ($D93F: power-on, JMP RESET, hardware watchdog bite, script
 *    reset).  That boundary logs an IO_RESET event (addr = cause) and FINISHES
 *    every open check and the open pass there: their C code must reach
 *    hw_reset() (JMP RESET, DSPSYS $D861) or hw_watchdog_hang() (WDGTST
 *    $DAF7), which consume the event and longjmp back into run_compare /
 *    close_pass; RAM etc. are then compared as usual (exit registers and the
 *    C flag are not: nothing returns).  An IRQ taken at $D93F itself (JMP
 *    RESET with I clear) is logged before the event and placed as usual.  A
 *    pass cut by a script reset before its first instruction (the reset is
 *    applied at the loop head) never ran and is dropped.
 *  - RESET REGION: from the arrival to the next loop head, $C7AD (TEST open:
 *    RESET's power-on path, MAINLN's first wait - the old boot pass) or $DA8D
 *    (TEST closed: RAM march, ROM checksums, POKEY / EAROM tests), C reset()
 *    runs from the snapshot with all its I/O replayed (normal path ~66,000
 *    events, self-test path ~1,300: within IOLOG_MAX).  Stack exemption: the
 *    lowest SP since the arrival, ignoring boundaries at $D93F-$D9D5 (before
 *    the RAM clear S is the old stack, in the zero-page march S holds the
 *    test pattern $11/$22/$44/$88), up to $01FF.
 *  - DIAG PASS PROBE: $DA8D to the next $DA8D (or to the watchdog RESET after
 *    WDGTST), C diag_pass(); the diag loop runs with the IRQ masked, so its
 *    passes have no IRQs.  Routine checks nest inside as elsewhere.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <setjmp.h>
#include "ref6502.h"
#include "state.h"
#include "hw.h"
#include "game.h"

static const uint8_t *R_ram, *R_vram, *R_col;

/* ------------------------------------------------------------------ */
/* I/O log                                                             */
/* ------------------------------------------------------------------ */
#define IOLOG_MAX 262144u
#define IO_SP     0x10000u          /* pseudo address: SP seen by TSX at $D70A */
/* write: 0 read, 1 write, 2 (M5) checkpoint visit - the ROM arrived at a CK()
 * address of ck_addr[] outside the IRQ; addr = that PC */
enum { IO_READ, IO_WRITE, IO_CK, IO_RESET };     /* IO_RESET (M9 B3): RESET arrival, addr = cause */
enum { RST_POWERON, RST_JMP, RST_WATCHDOG, RST_SCRIPT, RST_KINDS };
static const char *const rst_name[RST_KINDS] = { "power-on", "JMP RESET", "watchdog", "script" };
typedef struct { uint32_t addr; uint8_t val, write; } io_ev;
static io_ev    iolog[IOLOG_MAX];
static unsigned io_n;
static unsigned long io_overflow;

static unsigned rp, rp_end;
static int      io_err;
static char     io_msg[160];

static const char *ev_verb(const io_ev *e)
{
    return e->write == IO_RESET ? "arrived at RESET" : e->write == IO_CK ? "visited checkpoint" : e->write ? "wrote" : "read";
}

static int is_strobe(uint32_t a)
{
    return a == 0x4800 || a == 0x5000 || a == 0x5800 || a == 0x60CB || a == 0x60DB;
}

static void try_fire(void);

/* M4, "IRQ moved" comparisons: I/O ranges of the check's IRQs that the
 * routine's own replay skips (skipping = 1) and run_skipped_irqs() replays. */
#define MAX_SKIP 16u
static unsigned skip_lo[MAX_SKIP], skip_hi[MAX_SKIP], n_skip;
static int      skipping;
static uint8_t  touched[0x800], before_irq[0x800];   /* RAM cells the moved IRQs changed */
static unsigned last_outside, last_ioerr;            /* of the last run_compare */

static void skip_io(void)
{
    int moved = skipping;
    while (moved) {
        moved = 0;
        for (unsigned i = 0; i < n_skip; i++)
            if (rp == skip_lo[i] && skip_hi[i] > skip_lo[i]) { rp = skip_hi[i]; moved = 1; }
    }
}

static uint8_t replay_read(uint32_t a)
{
    uint8_t v;
    skip_io();
    if (rp >= rp_end || iolog[rp].write != IO_READ || iolog[rp].addr != a) {
        if (!io_err) {
            io_err = 1;
            if (rp >= rp_end) snprintf(io_msg, sizeof io_msg, "C read $%04X but the ROM did no more I/O", (unsigned)a);
            else snprintf(io_msg, sizeof io_msg, "C read $%04X, ROM %s $%04X (I/O #%u)", (unsigned)a,
                          ev_verb(&iolog[rp]), (unsigned)iolog[rp].addr, rp);
        }
        return 0;
    }
    v = iolog[rp++].val;
    try_fire();
    return v;
}

static void replay_write(uint32_t a, uint8_t v)
{
    skip_io();
    if (rp >= rp_end || iolog[rp].write != IO_WRITE || iolog[rp].addr != a ||
        (!is_strobe(a) && iolog[rp].val != v)) {
        if (!io_err) {
            io_err = 1;
            if (rp >= rp_end) snprintf(io_msg, sizeof io_msg, "C wrote $%02X to $%04X but the ROM did no more I/O", v, (unsigned)a);
            else snprintf(io_msg, sizeof io_msg, "C wrote $%02X to $%04X, ROM %s $%02X at $%04X (I/O #%u)", v, (unsigned)a,
                          ev_verb(&iolog[rp]), iolog[rp].val, (unsigned)iolog[rp].addr, rp);
        }
        return;
    }
    rp++;
    try_fire();
}

/* M5: a checkpoint visit, from C CK() or from the thunk CPU arriving at a
 * checkpoint address.  The ROM's visits must be consumed in order, like I/O. */
static int ck_consume(uint16_t pc)
{
    skip_io();
    if (rp >= rp_end || iolog[rp].write != IO_CK || iolog[rp].addr != pc) {
        if (!io_err) {
            io_err = 1;
            if (rp >= rp_end) snprintf(io_msg, sizeof io_msg, "C visited checkpoint $%04X but the ROM did no more events", pc);
            else snprintf(io_msg, sizeof io_msg, "C visited checkpoint $%04X, ROM %s $%04X (I/O #%u)", pc,
                          ev_verb(&iolog[rp]), (unsigned)iolog[rp].addr, rp);
        }
        return 0;
    }
    rp++;
    return 1;
}

static void set_err(const char *msg)
{
    if (!io_err) { io_err = 1; snprintf(io_msg, sizeof io_msg, "%s", msg); }
}

/* ------------------------------------------------------------------ */
/* IRQ placement (recorded while the ROM runs, like the I/O log)       */
/* ------------------------------------------------------------------ */
enum { EV_START, EV_WAIT, EV_SEG, EV_OTHER };
typedef struct {
    uint8_t kind, s; uint32_t ord, off; uint16_t pc;
    unsigned io_s, io_e; int io_done;       /* M4: the IRQ's own I/O is iolog[io_s .. io_e) */
    uint8_t  anch_ok;                       /* M5: commutes with the ROM instructions since the last event */
    uint16_t conflict;                      /* M5: first cell that did not commute (diagnostics) */
} irq_place;
#define PLACE_MAX 4096u

static irq_place place[PLACE_MAX];
static unsigned  n_place;
static unsigned long place_overflow;

/* How a C run services each of its IRQs (M5):
 *  M_SEG    inside a top-level thunk segment the C run itself calls: the thunk
 *           CPU calls irq() at the same instruction (M3);
 *  M_WAIT   in MAINLN's frame wait (pass probe only);
 *  M_START  at MAINLN's first instruction (boot pass);
 *  M_ANCH   anchored: irq() runs as soon as the C run has consumed every event
 *           of the log before the IRQ's own I/O (the event = an I/O operation,
 *           a checkpoint visit, the end of the previous IRQ, or the start of
 *           the run).  Allowed only when the ROM instructions between the last
 *           event and the interrupted instruction provably commute with the
 *           IRQ: no RAM / colour / vector RAM cell (stack page excepted) that
 *           the IRQ writes was read or written by them, and none that the IRQ
 *           reads was written by them.  They did no I/O by construction.
 *  M_NONE   unplaceable: the pass is skipped, the check tainted. */
enum { M_NONE, M_SEG, M_WAIT, M_START, M_ANCH };
static uint8_t pmode[PLACE_MAX];

/* the C run in progress: its IRQs are place[place_pos .. place_end), thunk
 * ordinals counted from place_base */
static int      placing, wait_ok, c_in_irq;
static unsigned place_pos, place_end;
static uint32_t place_base, thunk_ord;

static void fire_one(void)
{
    place_pos++;
    c_in_irq = 1;
    irq();
    c_in_irq = 0;
}

/* anchored IRQs whose event position has been reached */
static void try_fire(void)
{
    if (c_in_irq) return;
    while (placing && place_pos < place_end && pmode[place_pos] == M_ANCH && place[place_pos].io_s == rp)
        fire_one();
}

#ifdef LK_THUNKS
static void fire_irqs(uint32_t ord, uint32_t off)
{
    while (place_pos < place_end && pmode[place_pos] == M_SEG &&
           place[place_pos].ord == place_base + ord && place[place_pos].off == off) {
        fire_one();
        try_fire();
    }
}
#endif

/* the mode of place[i] for a run whose own thunk segments start at seg_base */
static uint8_t place_mode(unsigned i, uint32_t seg_base, int in_seg, int pass)
{
    if (place[i].kind == EV_SEG && !in_seg && place[i].ord >= seg_base) return M_SEG;
    if (pass && place[i].kind == EV_WAIT) return M_WAIT;
    if (pass && place[i].kind == EV_START) return M_START;
    if (place[i].kind != EV_WAIT && place[i].kind != EV_START && place[i].anch_ok) return M_ANCH;
    return M_NONE;
}

/* ---- M5: checkpoints --------------------------------------------------
 * CK(0xLLLL) in the translated modules (game.h; compiled away outside
 * lockstep) marks the C statement of ROM address LLLL.  Every address must be
 * listed here: the ROM logs its visits to them (outside the IRQ) as events,
 * and the C run must consume them in order, like I/O. */
static const uint16_t ck_addr[] = {
    /* ALEXEC: MAINLN pass, EXSTAT (after PRSTAR, after SWFINA), PROCRE
     * (SWFINA, TBHD / credits), NONSTA (after PROCRE, QFRAME, EAUPD, the
     * SWFINA read, the SWFINA store) */
    0xC7B1, 0xC7B4, 0xC7B7, 0xC7C9, 0xC7CD, 0xC7D1, 0xC823, 0xC827, 0xC890, 0xC8D9,
    0xC8E3, 0xC8EE, 0xC903, 0xC90B,
    /* ALEXEC (M6): NEWLIF after COCFLI (TOUT0), ENDWAV after SAUSON (SINDEX) */
    0xC96F, 0xC9AC,
    /* ALWELG (M6): CLRPOT after the TBHD store, PRORAT after S3SWAR (SINDEX),
     * after GETCUR (TBHD) and after the SWFINA store, TIMLAU after CIEXPL
     * (POINT) */
    0x92B1, 0x9164, 0x916C, 0x917E, 0xA69A,
    /* ALWELG (M6 step 1): after each SWFINA / TBHD read and store - PRORAT
     * ($915B store, $9176 read, $91AE read, $91B2 store), MOVCUR ($975B read,
     * $976E store), PROSUZ ($A848 read, $A85D read, $A861 store, $A87C read,
     * $A880 store) */
    0x915D, 0x9178, 0x91B0, 0x91B4, 0x975D, 0x9770,
    0xA84A, 0xA85F, 0xA863, 0xA87E, 0xA882,
    /* ALWELG (M6 step 1): FIREPC after the SWSTAT read, LINER after the
     * SWSTRT read */
    0xA272, 0xA5B7,
    /* ALSOUN (M6 step 1): FSNDON after each channel store MODSND reads
     * (SINDEX, POINT, FRAMES, COUNT, SINDEX) */
    0xCCD5, 0xCCD7, 0xCCDB, 0xCCDD, 0xCCE1,
    /* ALSCO2: HISCHK after the QSTATUS store, RQRDSP after COCFLI (TOUT0) */
    0xAC45, 0xAF84,
    /* ALSCO2 (M7): ZATC4S after the S_CNCT read (with coin mode 0 = free
     * play every IRQ's COIN65 stores S_CNCT at $D002) */
    0xAAE7,
    /* ALDIS2: DISPLAY after the SPARE3 read, EXCESS after the FRTIMR store,
     * INIDSP after the SPARE3 read and store */
    0xB1C4, 0xB565, 0xC180, 0xC18A,
    /* ALTES2 (M9 B3): DBOOKE at its SED, after each counter's three BOOKKS
     * reads (every IRQ's up timer increments SECOUL $0406) */
    0xDDAB,
};
static uint8_t ck_map[0x10000];

void lk_ck(uint16_t pc)
{
    if (!ck_map[pc]) {
        char m[80];
        snprintf(m, sizeof m, "CK($%04X) is not listed in ck_addr[]", pc);
        if (!io_err) { io_err = 1; snprintf(io_msg, sizeof io_msg, "%s", m); }
        return;
    }
    if (ck_consume(pc)) try_fire();
}

/* ------------------------------------------------------------------ */
/* the hardware seam, replayed                                         */
/* ------------------------------------------------------------------ */
uint8_t hw_in1(void)                { return replay_read(0x0C00); }
uint8_t hw_inop0(void)              { return replay_read(0x0D00); }
uint8_t hw_inop1(void)              { return replay_read(0x0E00); }
uint8_t hw_allpot(int chip)         { return replay_read(chip ? 0x60D8u : 0x60C8u); }
uint8_t hw_random(int chip)         { return replay_read(chip ? 0x60DAu : 0x60CAu); }
void    hw_pokey_write(int chip, uint8_t reg, uint8_t val) { replay_write((chip ? 0x60D0u : 0x60C0u) + (reg & 0x0Fu), val); }
void    hw_mb_write(uint8_t offset, uint8_t val) { replay_write(0x6080u + (offset & 0x1Fu), val); }
uint8_t hw_mb_status(void)          { return replay_read(0x6040); }
uint8_t hw_mb_ylow(void)            { return replay_read(0x6060); }
uint8_t hw_mb_yhigh(void)           { return replay_read(0x6070); }
void    hw_earom_write(uint8_t offset, uint8_t val) { replay_write(0x6000u + (offset & 0x3Fu), val); }
void    hw_earom_ctl(uint8_t val)   { replay_write(0x6040, val); }
uint8_t hw_earom_read(void)         { return replay_read(0x6050); }
void    hw_out0(uint8_t v)          { replay_write(0x4000, v); }
void    hw_outank(uint8_t v)        { replay_write(0x60E0, v); }
void    hw_color(uint8_t idx, uint8_t v) { g.colram[idx & 15] = v; }
void    hw_vgstart(void)            { replay_write(0x4800, 0); }
void    hw_vgstop(void)             { replay_write(0x5800, 0); }
void    hw_watchdog(void)           { replay_write(0x5000, 0); }
uint8_t hw_sp(void)                 { return replay_read(IO_SP); }
void    hw_soft_watchdog(void)      { set_err("C tripped the software watchdog"); }

/* M9 B3: RESET's CLI ($D9A4) - the IRQ pending since its SEI, taken at MAINLN's
 * first instruction (placement START), then any anchored IRQ now due. */
void hw_cli(void)
{
    while (placing && place_pos < place_end && pmode[place_pos] == M_START) fire_one();
    try_fire();
}

/* M9 B3: the C run ends where the ROM arrived at RESET.  The next event must
 * be that arrival with the matching cause; either way the C call stack is
 * abandoned (longjmp into run_compare / close_pass), as the CPU abandons its
 * stack. */
static jmp_buf reset_jmp;
static int     reset_armed;
static int     by_reset;                 /* the runs being finished end at a RESET arrival */
static unsigned long c_reset_calls[RST_KINDS];

static void reset_hook(int cause, const char *what)
{
    skip_io();
    if (rp >= rp_end || iolog[rp].write != IO_RESET || iolog[rp].addr != (uint32_t)cause) {
        if (!io_err) {
            io_err = 1;
            if (rp >= rp_end) snprintf(io_msg, sizeof io_msg, "C %s but the ROM did no more events", what);
            else if (iolog[rp].write == IO_RESET)
                snprintf(io_msg, sizeof io_msg, "C %s, ROM arrived at RESET by %s (I/O #%u)", what, rst_name[iolog[rp].addr % RST_KINDS], rp);
            else snprintf(io_msg, sizeof io_msg, "C %s, ROM %s $%04X (I/O #%u)", what, ev_verb(&iolog[rp]), (unsigned)iolog[rp].addr, rp);
        }
    } else {
        rp++;
        c_reset_calls[cause]++;
    }
    if (reset_armed) longjmp(reset_jmp, 1);
    set_err("hw_reset / hw_watchdog_hang outside a lockstep C run");
}

void hw_reset(void)         { reset_hook(RST_JMP, "did JMP RESET (hw_reset)"); }
void hw_watchdog_hang(void) { reset_hook(RST_WATCHDOG, "spun in WDGTST (hw_watchdog_hang)"); }

/* MAINLN's frame wait: under the pass probe, service the IRQs the ROM took
 * in its wait loop; anywhere else it is an error. */
void hw_wait_frame(void)
{
    if (!wait_ok) { set_err("hw_wait_frame called under lockstep"); return; }
    while (FRTIMR < 9) {
        if (place_pos >= place_end || pmode[place_pos] != M_WAIT) {
            set_err("C waits for an IRQ the ROM did not take in its frame wait");
            return;
        }
        fire_one();
        if (io_err) return;
    }
    /* an IRQ taken at $C7A9/$C7AB after LDA FRTIMR already read >= 9: the
     * ROM services it before leaving the loop, with nothing in between */
    while (place_pos < place_end && pmode[place_pos] == M_WAIT) fire_one();
}

/* ------------------------------------------------------------------ */
/* ROM thunks for routines not translated yet                          */
/* ------------------------------------------------------------------ */
static uint8_t thunk_s;           /* SP the ROM had when it made this call's JSR */
#ifdef LK_THUNKS
static cpu tc;

static uint8_t th_read(uint16_t a)
{
    if (a < 0x0800 || (a >= 0x2000 && a < 0x4000) || a >= 0x9000) return cpu_rd(a);
    return replay_read(a);
}

static void th_write(uint16_t a, uint8_t v)
{
    if (a < 0x0810 || (a >= 0x2000 && a < 0x3000)) { cpu_wr(a, v); return; }
    replay_write(a, v);
}

/* Run the ROM routine at addr over `g` with A/X/Y and g.dflag in; returns
 * exit X/Y, g.dflag updated. */
static xy6502 lk_rom_call(uint16_t addr, uint8_t a, uint8_t x, uint8_t y)
{
    unsigned long n;
    uint8_t stack[0x100], low;
    uint32_t ord = thunk_ord++;
    xy6502 r;
    memcpy(stack, g.ram + 0x100, sizeof stack);
    memset(&tc, 0, sizeof tc);
    tc.read = th_read;
    tc.write = th_write;
    tc.p = (uint8_t)(0x24 | (g.dflag ? CPU_D : 0));
    tc.a = a; tc.x = x; tc.y = y;
    tc.s = thunk_s;
    g.ram[0x100 | tc.s] = 0xFF; tc.s--;          /* return address $FFFE -> RTS lands on $FFFF */
    g.ram[0x100 | tc.s] = 0xFE; tc.s--;
    low = tc.s;
    tc.pc = addr;
    for (n = 0; tc.pc != 0xFFFF; n++) {
        if (n > 5000000ul) {
            char m[80];
            snprintf(m, sizeof m, "ROM thunk $%04X did not return", addr);
            set_err(m);
            break;
        }
        if (ck_map[tc.pc] && ck_consume(tc.pc)) try_fire();   /* M5: the ROM logged this visit too */
        if (placing) fire_irqs(ord, (uint32_t)n);
        cpu_step(&tc);
        if (tc.s < low) low = tc.s;
    }
    if (placing && tc.pc == 0xFFFF) fire_irqs(ord, (uint32_t)n);
    for (unsigned s = (unsigned)low + 1u; s <= thunk_s; s++)        /* put the pushed bytes back */
        g.ram[0x100 + s] = stack[s];
    g.dflag = (uint8_t)((tc.p & CPU_D) != 0);
    r.x = tc.x; r.y = tc.y;
    return r;
}

#define THUNK(name, addr) xy6502 name(uint8_t x, uint8_t y) { return lk_rom_call(addr, 0, x, y); }
#define THUNK_A(name, addr) xy6502 name(uint8_t a, uint8_t x, uint8_t y) { return lk_rom_call(addr, a, x, y); }
/* M9 B3: no thunks left (ALTES2 translated: SYSTEM, DSPSYS, GETOP3 removed).
 * The machinery stays for debugging a future module: define LK_THUNKS, add
 * THUNK lines here and their addresses to thunk_addr[]. */
#endif /* LK_THUNKS */

/* every thunk address: where the ROM starts a top-level thunk segment
 * (0 = sentinel, C has no empty arrays) */
static const uint16_t thunk_addr[] = {
    0,
};
static uint8_t thunk_map[0x10000];
static int is_thunk_addr(uint16_t pc) { return thunk_map[pc]; }

/* ROM-side segment tracking (always on): a segment starts when the ROM,
 * outside any segment and outside the IRQ, arrives at a thunk address, and
 * ends when SP rises above its entry level.  seg_ord = ordinal of the next. */
static int      seg_open, in_irq;
static uint8_t  seg_s, irq_s;
static uint32_t seg_ord, seg_n;

/* ------------------------------------------------------------------ */
/* checked routines                                                    */
/* ------------------------------------------------------------------ */
static struct { uint8_t a, x, y, p; } R;
static int ret_c;                               /* -1: no documented flag return */
static int ret_a, ret_x, ret_y;                 /* -1: register not a documented result (M4) */
static void ret_xy(xy6502 r) { ret_x = r.x; ret_y = r.y; }

static void run_vgrtsl(void) { vgrtsl(); }
static void run_vghalt(void) { vghalt(); }
static void run_vghexz(void) { ret_c = vghexz(R.a, R.p & 1); }
static void run_vghex(void)  { ret_c = vghex(R.a); }
static void run_vgjsrl(void) { vgjsrl(R.a, R.x); }
static void run_vgsta1(void) { vgsta1(R.a); }
static void run_vgstat(void) { vgstat(R.a, R.y); }
static void run_vgcntr(void) { vgcntr(); }
static void run_vgadd2(void) { vgadd2(R.a, R.x); }
static void run_vgadd3(void) { vgadd3(R.a, R.x, R.y); }
static void run_vgadd(void)  { vgadd(R.y); }
static void run_vgsca1(void) { vgsca1(R.a); }
static void run_vgscal(void) { vgscal(R.a, R.y); }
static void run_vgvtr(void)  { vgvtr(R.a, R.x, R.y); }
static void run_vgvtr1(void) { vgvtr1(R.a, R.x); }
static void run_vgvctr(void) { vgvctr(R.x); }
static void run_digtys(void) { digtys(R.a, R.y); }
static void run_digits(void) { digits(R.a, R.y, R.p & 1); }
static void run_irq(void)    { irq(); }
static void run_inisou(void) { (void)inisou(); }
static void run_ipexpl(void) { ipexpl(R.x, R.y); }
static void run_sboing(void) { sboing(R.x, R.y); }
static void run_sauson(void) { sauson(R.x, R.y); }
static void run_eslson(void) { eslson(R.x, R.y); }
static void run_ccexpl(void) { ccexpl(R.x, R.y); }
static void run_slaunc(void) { slaunc(R.x, R.y); }
static void run_souts2(void) { souts2(R.x, R.y); }
static void run_souts3(void) { souts3(R.x, R.y); }
static void run_selico(void) { selico(R.x, R.y); }
static void run_sslams(void) { sslams(R.x, R.y); }
static void run_s3swar(void) { s3swar(R.x, R.y); }
static void run_pulstr(void) { pulstr(R.x, R.y); }
static void run_pulsto(void) { pulsto(R.x, R.y); }
static void run_moolah(void) { moolah(); }
static void run_modsnd(void) { modsnd(); }
/* ALEXEC */
static void run_exstat(void) { (void)exstat(R.x, R.y); }
static void run_routen(void) { (void)routen(R.x, R.y); }
static void run_procre(void) { (void)procre(R.x, R.y); }
static void run_nonsta(void) { (void)nonsta(R.x, R.y); }
static void run_newgam(void) { (void)newgam(R.x, R.y); }
static void run_newlif(void) { (void)newlif(R.x, R.y); }
static void run_newlf2(void) { (void)newlf2(R.x, R.y); }
static void run_endwav(void) { (void)endwav(R.x, R.y); }
static void run_endlif(void) { (void)endlif(R.x, R.y); }
static void run_endgam(void) { (void)endgam(R.x, R.y); }
static void run_dladr(void)  { (void)dladr(R.x, R.y); }
static void run_cocfli(void) { (void)cocfli(); }
static void run_clrsco(void) { clrsco(); }
static void run_upscor(void) { (void)upscor(R.x, R.y); }
/* ALEARO */
static void run_eazboo(void) { eazboo(); }
static void run_eazhis(void) { eazhis(); }
static void run_eazero(void) { eazero(); }
static void run_wrhiin(void) { wrhiin(); }
static void run_wrbook(void) { wrbook(); }
static void run_rehiin(void) { (void)rehiin(R.x, R.y); }
static void run_eaupd(void)  { (void)eaupd(R.x, R.y); }
/* ALSCO2 / ALLANG (M4): exit registers compared where the C returns them */
static void run_info(void)   { info(); }
static void run_hacker(void) { hacker(); }
static void run_upscli(void) { upscli(R.a, R.y); }
static void run_nwdigs(void) { nwdigs(R.x); }
static void run_nwhexz(void) { int c = R.p & 1; ret_x = nwhexz(R.a, R.x, &c); ret_c = c; }
static void run_initem(void) { initem(); }
static void run_dplpla(void) { dplpla(); }
static void run_dgover(void) { dgover(); }
static void run_dprsta(void) { dprsta(); }
static void run_d2game(void) { d2game(); }
static void run_dplrno(void) { dplrno(); }
static void run_dplrx(void)  { dplrx(); }
static void run_dplrxx(void) { dplrxx(R.x); }
static void run_dspcrd(void) { dspcrd(); }
static void run_dbolou(void) { dbolou(); }
static void run_hexbcd(void) { ret_a = hexbcd(R.a); ret_y = 0xFF; }
static void run_vgcntr_ab0d(void) { vgcntr_ab0d(); }
static void run_msgs(void)   { msgs(R.x); }
static void run_msgen3(void) { msgen3(R.a, R.x); }
static void run_msgfol(void) { msgfol(R.a, R.x); }
static void run_inichk(void) { ret_xy(inichk(R.x, R.y)); }
static void run_iniini(void) { ret_xy(iniini(R.x, R.y)); }
static void run_gamsta(void) { ret_xy(gamsta(R.x, R.y)); }
static void run_induce(void) { induce(); }
static void run_hischk(void) { ret_xy(hischk(R.x, R.y)); }
static void run_intldr(void) { ret_xy(intldr(R.x, R.y)); }
static void run_getini(void) { ret_xy(getini(R.x, R.y)); }
static void run_ginico(void) { ret_a = ginico(R.a); ret_y = 0x00; ret_x = R.x; }
static void run_getdsp(void) { getdsp(); }
static void run_ldrdsp(void) { ldrdsp(); }
static void run_ldrout(void) { ldrout(R.a); }
static void run_bolout(void) { bolout(); }
static void run_outcur(void) { outcur(); }
static void run_outini(void) { outini(R.a); }
static void run_rnkdsp(void) { rnkdsp(); }
static void run_pl1rnk(void) { pl1rnk(R.x); }
static void run_onernk(void) { onernk(R.a); }
static void run_dsp1hx(void) { dsp1hx(R.a); }
static void run_rqrdsp(void) { rqrdsp(); }
static void run_getcur(void) { ret_a = ret_y = getcur(); ret_x = R.x; }
static void run_bodspl(void) { bodspl(R.x); }
static void run_nwcolo(void) { nwcolo(R.y); }
static void run_nwsca1(void) { nwsca1(R.a); }
static void run_logini(void) { ret_xy(logini(R.x, R.y)); }
static void run_boxpro(void) { boxpro(); }
static void run_logpro(void) { logpro(); }
static void run_scarng(void) { scarng(R.a, R.x); }
static void run_inilit(void) { ret_xy(inilit(R.x, R.y)); }
/* ALDIS2 (M5) */
static void run_display(void) { ret_xy(display(R.x, R.y)); }
static void run_dstate(void)  { dstate(R.y); }
static void run_denorm(void)  { ret_xy(denorm()); }
static void run_sbclog(void)  { sbclog(R.a); }
static void run_sbcact(void)  { sbcact(R.a); }
static void run_sbcswi(void)  { ret_xy(sbcswi(R.a)); }
static void run_bigtex(void)  { ret_c = bigtex(); }
static void run_dspwel(void)  { dspwel(); }
static void run_dspnym(void)  { dspnym(); }
static void run_excess(void)  { excess(R.y); }
static void run_vgdot(void)   { vgdot(R.a); ret_x = R.x; ret_y = 0x03; }
static void run_dspcur(void)  { dspcur(); }
static void run_dspinv(void)  { dspinv(); }
static void run_invpic(void)  { invpic(R.a, R.x); }
static void run_flipic(void)  { flipic(R.x); }
static void run_tanpic(void)  { tanpic(R.x); }
static void run_trapic(void)  { trapic(R.x); }
static void run_ijmpds(void)  { ret_y = ijmpds(R.x); ret_x = R.x; }
static void run_fuspic(void)  { fuspic(R.x); }
static void run_delta8(void)  { ret_a = delta8(R.a, R.x); ret_x = R.x; ret_y = R.y; }
static void run_pulpic(void)  { pulpic(R.x); }
static void run_dspchg(void)  { dspchg(); }
static void run_dspexp(void)  { dspexp(); }
static void run_chplki(void)  { chplki(); }
static void run_special(void) { special(R.y); }
static void run_altcol(void)  { altcol(); }
static void run_rotcol(void)  { rotcol(); }
static void run_setshr(void)  { setshr(); }
static void run_shrsca(void)  { shrsca(); }
static void run_dsboom(void)  { dsboom(); }
static void run_swapvg(void)  { ret_xy(swapvg()); }
static void run_calmag(void)  { uint8_t y; ret_a = calmag(&y); ret_y = y; }
static void run_whichb(void)  { uint8_t x; ret_a = whichb(&x); ret_x = x; }
static void run_scapic(void)  { scapic(R.a, R.y); }
static void run_scapi2(void)  { scapi2(); }
static void run_cascal(void)  { ret_y = cascal(); ret_a = (uint8_t)(BFACTR | 0x70); }
static void run_onelin(void)  { onelin(R.a, R.y); }
static void run_oneln2(void)  { oneln2(R.y); }
static void run_worscr(void)  { worscr(); }
static void run_inidsp(void)  { ret_xy(inidsp(R.x, R.y)); }
static void run_inicol(void)  { ret_xy(inicol()); }
static void run_inimat(void)  { inimat(); }
static void run_iniwls(void)  { iniwls(); }
static void run_lvlwel(void)  { uint8_t x; ret_a = lvlwel(R.a, &x); ret_x = x; }
static void run_bldwel(void)  { bldwel(); }
static void run_outlin(void)  { outlin(R.a, R.y); }
static void run_connec(void)  { connec(); }
static void run_spoke(void)   { ret_x = spoke(R.a, R.x); }
static void run_lintos(void)  { lintos(); }
static void run_liftos(void)  { liftos(); }
static void run_chkdep(void)  { chkdep(); }
static void run_calout(void)  { ret_a = calout(R.a, R.x); }
static void run_dsphol(void)  { dsphol(R.a); }
static void run_dstarf(void)  { dstarf(); }
static void run_dspenl(void)  { dspenl(); }
static void run_fixstu(void)  { fixstu(); }
static void run_tipact(void)  { tipact(); }
static void run_fconnec(void) { fconnec(); }
static void run_vgyab1(void)  { vgyab1(R.x); }
static void run_vgyabs(void)  { vgyabs(R.x); }
/* ALWELG (M6) */
static void same_xy(void)     { ret_x = R.x; ret_y = R.y; }
static void run_inewav(void)  { ret_xy(inewav(R.x, R.y)); }
static void run_inewli(void)  { ret_xy(inewli(R.x, R.y)); }
static void run_iniobj(void)  { ret_xy(iniobj(R.x, R.y)); ret_a = 0x00; }
static void run_newav2(void)  { ret_xy(newav2(R.x, R.y)); }
static void run_inira0(void)  { ret_xy(inira0(R.x, R.y)); }
static void run_inirat(void)  { ret_xy(inirat(R.x, R.y)); }
static void run_prorat(void)  { ret_xy(prorat(R.x, R.y)); }
static void run_bonsco(void)  { ret_xy(bonsco(R.a, R.x, R.y)); }
static void run_inicur(void)  { inicur(); same_xy(); ret_a = K_ILINLIY; }
static void run_iniene(void)  { iniene(); ret_x = 0xFF; ret_y = R.y; }
static void run_ininym(void)  { ininym(); ret_x = 0xFF; ret_y = R.y; }
static void run_iniinv(void)  { iniinv(); ret_x = 0xFF; ret_y = R.y; ret_a = 0x00; }
static void run_inicha(void)  { inicha(); ret_x = 0xFF; ret_y = R.y; ret_a = 0x00; }
static void run_iniexp(void)  { iniexp(); ret_x = 0xFF; ret_y = R.y; ret_a = 0x00; }
static void run_clrpot(void)  { clrpot(); same_xy(); ret_a = 0x00; }
static void run_swapen(void)  { ret_xy(swapen(R.x, R.y)); }
static void run_contour(void) { ret_xy(contour()); }
static void run_times8(void)  { uint8_t x, y; ret_a = times8(R.a, &x, &y); ret_x = x; ret_y = y; }
static void run_dotype(void)  { ret_a = dotype(R.y); }
static void run_donext(void)  { ret_y = donext(R.y); }
static void run_dotzan(void)  { ret_a = dotzan(R.y); }
static void run_itmize(void)  { ret_a = itmize(R.y); }
static void run_samall(void)  { ret_a = samall(R.y); ret_x = R.x; ret_y = R.y; }
static void run_twobyt(void)  { ret_y = twobyt(R.y); ret_x = R.x; }
static void run_onebyt(void)  { ret_y = onebyt(R.y); ret_x = R.x; }
static void run_nitmiz(void)  { ret_y = nitmiz(R.y); ret_x = R.x; }
static void run_dotb(void)    { ret_a = dotb(R.y); ret_x = R.x; ret_y = R.y; }
static void run_dota(void)    { ret_a = dota(R.y); }
static void run_ranger(void)  { ret_a = ranger(R.y); same_xy(); }
static void run_dotr(void)    { ret_a = dotr(R.y); ret_x = R.x; }
static void run_prboom(void)  { ret_xy(prboom(R.x, R.y)); }
static void run_timlau(void)  { timlau(R.x, R.y); same_xy(); }
static void run_fixtop(void)  { int n; ret_a = fixtop(R.a, &n); same_xy(); }
static void run_uparpo(void)  { ret_y = uparpo(R.x); ret_x = R.x; }
static void run_decpar(void)  { ret_y = decpar(R.x); ret_x = R.x; }
static void run_decele(void)  { uint8_t y = R.y; ret_a = decele(R.a, &y); ret_y = y; ret_x = R.x; }
static void run_inboom(void)  { ret_xy(inboom(R.x, R.y)); }
static void run_poldel(void)  { ret_a = poldel(R.a, R.y); same_xy(); }
static void run_instar(void)  { instar(); ret_x = 0xFF; ret_y = R.y; }
static void run_prstar(void)  { ret_xy(prstar(R.x, R.y)); }
static void run_inisuz(void)  { inisuz(); same_xy(); ret_a = 0x00; }
static void run_play(void)    { ret_xy(play(R.x, R.y)); }
static void run_pldrop(void)  { ret_xy(pldrop(R.x, R.y)); }
static void run_movcur(void)  { ret_xy(movcur(R.x, R.y)); }
static void run_autocu(void)  { uint8_t x = R.x, y = R.y; ret_a = autocu(&x, &y); ret_x = x; ret_y = y; }
static void run_movcud(void)  { ret_xy(movcud(R.x, R.y)); }
static void run_movnym(void)  { ret_xy(movnym(R.x, R.y)); }
static void run_conymp(void)  { ret_xy(conymp(R.x, R.y)); }
static void run_actinv(void)  { ret_a = actinv(R.x, R.y); same_xy(); }
static void run_nymcha(void)  { ret_y = nymcha(R.x, R.y); }
static void run_newtyp(void)  { uint8_t y = R.y; ret_a = newtyp(R.x, &y); ret_y = y; ret_x = R.x; }
static void run_newty2(void)  { uint8_t y = R.y; ret_a = newty2(R.a, R.x, &y); ret_y = y; ret_x = R.x; }
static void run_newfli(void)  { uint8_t y = R.y; ret_a = newfli(&y); ret_y = y; ret_x = R.x; }
static void run_newpul(void)  { uint8_t y = R.y; ret_a = newpul(&y); ret_y = y; ret_x = R.x; }
static void run_newfus(void)  { uint8_t y = R.y; ret_a = newfus(&y); ret_y = y; ret_x = R.x; }
static void run_newspi(void)  { uint8_t y = R.y; ret_a = newspi(&y); ret_y = y; ret_x = R.x; }
static void run_newtan(void)  { uint8_t y = R.y; ret_a = newtan(R.x, &y); ret_y = y; ret_x = R.x; }
static void run_newgen(void)  { ret_a = newgen(R.y); same_xy(); }
static void run_splcha(void)  { splcha(R.x, R.y); same_xy(); }
static void run_movinv(void)  { ret_xy(movinv(R.x, R.y)); }
static void run_jsrcam(void)  { ret_xy(jsrcam(R.a, R.x)); }
static void run_jexit(void)   { ret_xy(jexit(R.x, R.y)); }
static void run_jnoop(void)   { ret_xy(jnoop(R.x, R.y)); }
static void run_jsloop(void)  { ret_xy(jsloop(R.x, R.y)); }
static void run_jslopb(void)  { ret_xy(jslopb(R.x, R.y)); }
static void run_jskip0(void)  { ret_xy(jskip0(R.x, R.y)); }
static void run_jbr0pc(void)  { ret_xy(jbr0pc(R.x, R.y)); }
static void run_jeloop(void)  { ret_xy(jeloop(R.x, R.y)); }
static void run_jsetpc(void)  { ret_xy(jsetpc(R.x, R.y)); }
static void run_jeltst(void)  { ret_xy(jeltst(R.x, R.y)); }
static void run_jchkpu(void)  { ret_xy(jchkpu(R.x, R.y)); }
static void run_jchrot(void)  { ret_xy(jchrot(R.x, R.y)); }
static void run_jsmove(void)  { ret_xy(jsmove(R.x, R.y)); }
static void run_jsmovu(void)  { ret_xy(jsmovu(R.x, R.y)); }
static void run_jsmovd(void)  { ret_a = jsmovd(R.x, R.y); same_xy(); }
static void run_jpulmo(void)  { ret_xy(jpulmo(R.x, R.y)); }
static void run_chaser(void)  { ret_xy(chaser(R.x, R.y)); }
static void run_jchpla(void)  { ret_xy(jchpla(R.x, R.y)); }
static void run_jjumpm(void)  { ret_xy(jjumpm(R.x, R.y)); }
static void run_jkitst(void)  { ret_xy(jkitst(R.x, R.y)); }
static void run_jfuski(void)  { ret_xy(jfuski(R.x, R.y)); }
static void run_jjumps(void)  { ret_xy(jjumps(R.x, R.y)); }
static void run_oktojm(void)  { oktojm(R.x); same_xy(); }
static void run_calsan(void)  { uint8_t y = R.y; ret_a = calsan(R.a, &y); ret_y = y; ret_x = R.x; }
static void run_jfuseup(void) { ret_xy(jfuseup(R.x, R.y)); }
static void run_mayblr(void)  { ret_xy(mayblr(R.x, R.y)); }
static void run_fuchpl(void)  { ret_xy(fuchpl(R.x, R.y)); }
static void run_lefrit(void)  { ret_xy(lefrit(R.x, R.y)); }
static void run_jstrai(void)  { ret_xy(jstrai(R.x, R.y)); }
static void run_astral(void)  { ret_y = astral(R.x); ret_x = R.x; }
static void run_kilinv(void)  { kilinv(R.x, R.y); same_xy(); }
static void run_movcha(void)  { ret_xy(movcha(R.x, R.y)); }
static void run_chatop(void)  { chatop(R.x, R.y); same_xy(); }
static void run_lifect(void)  { ret_xy(lifect(R.x, R.y)); }
static void run_firepc(void)  { ret_xy(firepc(R.x, R.y)); }
static void run_fireic(void)  { ret_xy(fireic(R.x, R.y)); }
static void run_incfs2(void)  { ret_xy(incfs2(R.x, R.y)); }
static void run_inipsq(void)  { inipsq(R.x, R.y); same_xy(); }
static void run_infpsq(void)  { infpsq(R.x, R.y); same_xy(); }
static void run_inppsq(void)  { inppsq(R.x, R.y); same_xy(); }
static void run_incpsq(void)  { incpsq(R.x, R.y); same_xy(); }
static void run_deadcu(void)  { deadcu(R.a, R.x, R.y); same_xy(); }
static void run_inccsq(void)  { inccsq(R.x, R.y); same_xy(); }
static void run_incis2(void)  { ret_xy(incis2(R.x, R.y)); }
static void run_gexifu(void)  { gexifu(R.a, R.x, R.y); same_xy(); }
static void run_genexp(void)  { genexp(R.a, R.x, R.y); same_xy(); }
static void run_genex2(void)  { genex2(R.x, R.y); same_xy(); }
static void run_proexp(void)  { ret_xy(proexp(R.x, R.y)); }
static void run_collis(void)  { ret_xy(collis(R.x, R.y)); }
static void run_colchk(void)  { ret_xy(colchk(R.a, R.x, R.y)); }
static void run_analyz(void)  { ret_xy(analyz(R.x, R.y)); }
static void run_indrop(void)  { indrop(); ret_x = 0xFF; ret_y = R.y; }
static void run_prosuz(void)  { ret_xy(prosuz(R.x, R.y)); }
static void run_kilene(void)  { ret_xy(kilene(R.x, R.y)); }
static void run_exikil(void)  { ret_xy(exikil(R.x, R.y)); }
/* ALTES2 (M9 B3).  Not rows: RESET and its non-returning parts (sftest_zp,
 * sftest_ram, ROMTST: the RESET region), the diag loop (diag pass probe),
 * HIBAD / BRAMREP / HIRBAD / HIRBD2 (bad RAM: never reached on genuine RAM;
 * they JMP into ROMTST and never return - fault runs, batch B6). */
static void run_system(void)  { ret_xy(system_(R.x, R.y)); }
static void run_dspsys(void)  { ret_xy(dspsys(R.a, R.x, R.y)); }
static void run_posdig(void)  { posdig(R.a, R.x, R.y); }
static void run_sstate(void)  { sstate(R.y); }
static void run_sigana(void)  { sigana(); }
static void run_badear(void)  { ret_xy(badear(R.x, R.y)); }
static void run_schekr(void)  { schekr(); }
static void run_sinten(void)  { sinten(); }
static void run_shatch(void)  { shatch(); }
static void run_shyster(void) { shyster(); }
static void run_getop3(void)  { ret_a = getop3(R.a, R.x, R.y); same_xy(); }
static void run_romrep(void)  { romrep(); }
static void run_readmb(void)  { uint8_t x = R.x, y = R.y; ret_a = readmb(R.a, &x, &y); ret_x = x; ret_y = y; }
static void run_dopswi(void)  { dopswi(); }
static void run_bits2(void)   { bits2(R.y); }
static void run_bits3(void)   { bits3(R.a, R.y); }
static void run_genopd(void)  { genopd(R.a, R.x, R.y); }
static void run_dbooke(void)  { dbooke(); }

typedef struct {
    const char *name; uint16_t addr; void (*run)(void); int rts;
    unsigned long checked, passed, failed, tainted, irq_placed;
    unsigned long irq_moved, moved_mismatch;    /* M4: see finish() */
} routine;

static routine routines[] = {
    { "VGRTSL", 0xDF09, run_vgrtsl, 0, 0,0,0,0 }, { "VGHALT", 0xDF0D, run_vghalt, 0, 0,0,0,0 },
    { "VGHEXZ", 0xDF19, run_vghexz, 0, 0,0,0,0 }, { "VGHEX",  0xDF1F, run_vghex,  0, 0,0,0,0 },
    { "VGJSRL", 0xDF39, run_vgjsrl, 0, 0,0,0,0 }, { "VGSTA1", 0xDF4A, run_vgsta1, 0, 0,0,0,0 },
    { "VGSTAT", 0xDF4C, run_vgstat, 0, 0,0,0,0 }, { "VGCNTR", 0xDF53, run_vgcntr, 0, 0,0,0,0 },
    { "VGADD2", 0xDF57, run_vgadd2, 0, 0,0,0,0 }, { "VGADD3", 0xDF59, run_vgadd3, 0, 0,0,0,0 },
    { "VGADD",  0xDF5F, run_vgadd,  0, 0,0,0,0 }, { "VGSCA1", 0xDF6A, run_vgsca1, 0, 0,0,0,0 },
    { "VGSCAL", 0xDF6C, run_vgscal, 0, 0,0,0,0 }, { "VGVTR",  0xDF73, run_vgvtr,  0, 0,0,0,0 },
    { "VGVTR1", 0xDF75, run_vgvtr1, 0, 0,0,0,0 }, { "VGVCTR", 0xDF92, run_vgvctr, 0, 0,0,0,0 },
    { "DIGTYS", 0xDFB1, run_digtys, 0, 0,0,0,0 }, { "DIGITS", 0xDFB2, run_digits, 0, 0,0,0,0 },
    /* COIN65 and MODSND are entered from the IRQ (nested checks since M3) */
    { "MOOLAH", 0xCF24, run_moolah, 0, 0,0,0,0 }, { "MODSND", 0xCD0A, run_modsnd, 0, 0,0,0,0 },
    { "INISOU", 0xCD95, run_inisou, 0, 0,0,0,0 },
    { "IPEXPL", 0xCCB0, run_ipexpl, 0, 0,0,0,0 }, { "SBOING", 0xCCB5, run_sboing, 0, 0,0,0,0 },
    { "SAUSON", 0xCCB9, run_sauson, 0, 0,0,0,0 }, { "ESLSON", 0xCCBD, run_eslson, 0, 0,0,0,0 },
    { "CCEXPL", 0xCCC1, run_ccexpl, 0, 0,0,0,0 }, { "SLAUNC", 0xCCEA, run_slaunc, 0, 0,0,0,0 },
    { "SOUTS2", 0xCCEE, run_souts2, 0, 0,0,0,0 }, { "SOUTS3", 0xCCF2, run_souts3, 0, 0,0,0,0 },
    { "SELICO", 0xCCF6, run_selico, 0, 0,0,0,0 }, { "SSLAMS", 0xCCFA, run_sslams, 0, 0,0,0,0 },
    { "S3SWAR", 0xCCFE, run_s3swar, 0, 0,0,0,0 }, { "PULSTR", 0xCD02, run_pulstr, 0, 0,0,0,0 },
    { "PULSTO", 0xCD06, run_pulsto, 0, 0,0,0,0 },
    /* ALEXEC (rts = 1: also entered by the ROUTAD dispatch's RTS) */
    { "EXSTAT", 0xC7BD, run_exstat, 0, 0,0,0,0 }, { "ROUTEN", 0xC800, run_routen, 1, 0,0,0,0 },
    { "PROCRE", 0xC81B, run_procre, 0, 0,0,0,0 }, { "NONSTA", 0xC891, run_nonsta, 0, 0,0,0,0 },
    { "NEWGAM", 0xC90C, run_newgam, 1, 0,0,0,0 }, { "NEWLIF", 0xC940, run_newlif, 1, 0,0,0,0 },
    { "NEWLF2", 0xC97B, run_newlf2, 1, 0,0,0,0 }, { "ENDWAV", 0xC98C, run_endwav, 1, 0,0,0,0 },
    { "ENDLIF", 0xC9AF, run_endlif, 1, 0,0,0,0 }, { "ENDGAM", 0xC9F1, run_endgam, 1, 0,0,0,0 },
    { "DLADR",  0xCA18, run_dladr,  1, 0,0,0,0 }, { "COCFLI", 0xCA48, run_cocfli, 0, 0,0,0,0 },
    { "CLRSCO", 0xCA62, run_clrsco, 0, 0,0,0,0 }, { "UPSCOR", 0xCA6C, run_upscor, 0, 0,0,0,0 },
    /* ALEARO */
    { "EAZBOO", 0xDDE9, run_eazboo, 0, 0,0,0,0 }, { "EAZHIS", 0xDDED, run_eazhis, 0, 0,0,0,0 },
    { "EAZERO", 0xDDF1, run_eazero, 0, 0,0,0,0 }, { "WRHIIN", 0xDDF7, run_wrhiin, 0, 0,0,0,0 },
    { "WRBOOK", 0xDDFB, run_wrbook, 0, 0,0,0,0 }, { "REHIIN", 0xDE11, run_rehiin, 0, 0,0,0,0 },
    { "EAUPD",  0xDE1B, run_eaupd,  0, 0,0,0,0 },
    /* ALSCO2 (rts = 1: ROUTAD state routines and ALDIS2's DROUTAD display
     * states, entered by an RTS dispatch; none of them follows a JSR) */
    { "INFO",   0xA8B4, run_info,   0, 0,0,0,0 }, { "HACKER", 0xA8E7, run_hacker, 0, 0,0,0,0 },
    { "UPSCLI", 0xA97F, run_upscli, 0, 0,0,0,0 }, { "NWDIGS", 0xA9D7, run_nwdigs, 0, 0,0,0,0 },
    { "NWHEXZ", 0xA9FC, run_nwhexz, 0, 0,0,0,0 }, { "INITEM", 0xAA13, run_initem, 0, 0,0,0,0 },
    { "DPLPLA", 0xAA5A, run_dplpla, 1, 0,0,0,0 }, { "DGOVER", 0xAA62, run_dgover, 1, 0,0,0,0 },
    { "DPRSTA", 0xAA6F, run_dprsta, 1, 0,0,0,0 }, { "D2GAME", 0xAA79, run_d2game, 1, 0,0,0,0 },
    { "DPLRNO", 0xAA92, run_dplrno, 0, 0,0,0,0 }, { "DPLRX",  0xAA97, run_dplrx,  0, 0,0,0,0 },
    { "DPLRXX", 0xAA9E, run_dplrxx, 0, 0,0,0,0 }, { "DSPCRD", 0xAAA8, run_dspcrd, 0, 0,0,0,0 },
    { "DBOLOU", 0xAACB, run_dbolou, 0, 0,0,0,0 }, { "HEXBCD", 0xAAF5, run_hexbcd, 0, 0,0,0,0 },
    { "VGCNTR_AB0D", 0xAB0D, run_vgcntr_ab0d, 0, 0,0,0,0 },
    { "MSGS",   0xAB14, run_msgs,   0, 0,0,0,0 }, { "MSGEN3", 0xAB17, run_msgen3, 0, 0,0,0,0 },
    { "MSGFOL", 0xAB98, run_msgfol, 0, 0,0,0,0 }, { "INICHK", 0xABA2, run_inichk, 0, 0,0,0,0 },
    { "INIINI", 0xABAC, run_iniini, 0, 0,0,0,0 }, { "GAMSTA", 0xAC20, run_gamsta, 0, 0,0,0,0 },
    { "INDUCE", 0xAC36, run_induce, 0, 0,0,0,0 }, { "HISCHK", 0xAC3F, run_hischk, 1, 0,0,0,0 },
    { "INTLDR", 0xAD22, run_intldr, 0, 0,0,0,0 }, { "GETINI", 0xAD6E, run_getini, 1, 0,0,0,0 },
    { "GINICO", 0xADCE, run_ginico, 0, 0,0,0,0 }, { "GETDSP", 0xADEA, run_getdsp, 1, 0,0,0,0 },
    { "LDRDSP", 0xAE1C, run_ldrdsp, 1, 0,0,0,0 }, { "LDROUT", 0xAE4E, run_ldrout, 0, 0,0,0,0 },
    { "BOLOUT", 0xAECA, run_bolout, 0, 0,0,0,0 }, { "OUTCUR", 0xAEF1, run_outcur, 0, 0,0,0,0 },
    { "OUTINI", 0xAEF8, run_outini, 0, 0,0,0,0 }, { "RNKDSP", 0xAF26, run_rnkdsp, 0, 0,0,0,0 },
    { "PL1RNK", 0xAF3F, run_pl1rnk, 0, 0,0,0,0 }, { "ONERNK", 0xAF71, run_onernk, 0, 0,0,0,0 },
    { "DSP1HX", 0xAF77, run_dsp1hx, 0, 0,0,0,0 }, { "RQRDSP", 0xAF81, run_rqrdsp, 1, 0,0,0,0 },
    { "GETCUR", 0xB0AB, run_getcur, 0, 0,0,0,0 }, { "BODSPL", 0xB0C6, run_bodspl, 0, 0,0,0,0 },
    { "NWCOLO", 0xB0D1, run_nwcolo, 0, 0,0,0,0 }, { "NWSCA1", 0xB0DD, run_nwsca1, 0, 0,0,0,0 },
    { "LOGINI", 0xB0E7, run_logini, 1, 0,0,0,0 }, { "BOXPRO", 0xB102, run_boxpro, 1, 0,0,0,0 },
    { "LOGPRO", 0xB131, run_logpro, 1, 0,0,0,0 }, { "SCARNG", 0xB15A, run_scarng, 0, 0,0,0,0 },
    /* ALLANG */
    { "INILIT", 0xD6BB, run_inilit, 0, 0,0,0,0 },
    /* ALDIS2 (M5; rts = 1: DROUTAD display states, INVPIT pictures and XSUBR
     * functions, entered by an RTS dispatch; none of them follows a JSR) */
    { "DISPLAY", 0xB1B6, run_display, 0, 0,0,0,0 }, { "DSTATE", 0xB20D, run_dstate, 0, 0,0,0,0 },
    { "DENORM", 0xB230, run_denorm, 1, 0,0,0,0 }, { "SBCLOG", 0xB2BE, run_sbclog, 0, 0,0,0,0 },
    { "SBCACT", 0xB2DE, run_sbcact, 0, 0,0,0,0 }, { "SBCSWI", 0xB2FE, run_sbcswi, 0, 0,0,0,0 },
    { "BIGTEX", 0xB332, run_bigtex, 0, 0,0,0,0 }, { "DSPWEL", 0xB367, run_dspwel, 0, 0,0,0,0 },
    { "DSPNYM", 0xB498, run_dspnym, 0, 0,0,0,0 }, { "EXCESS", 0xB550, run_excess, 0, 0,0,0,0 },
    { "VGDOT",  0xB56A, run_vgdot,  0, 0,0,0,0 }, { "DSPCUR", 0xB586, run_dspcur, 0, 0,0,0,0 },
    { "DSPINV", 0xB5AD, run_dspinv, 0, 0,0,0,0 }, { "INVPIC", 0xB5D7, run_invpic, 0, 0,0,0,0 },
    { "FLIPIC", 0xB5EB, run_flipic, 1, 0,0,0,0 }, { "TANPIC", 0xB60F, run_tanpic, 1, 0,0,0,0 },
    { "TRAPIC", 0xB622, run_trapic, 1, 0,0,0,0 }, { "IJMPDS", 0xB634, run_ijmpds, 0, 0,0,0,0 },
    { "FUSPIC", 0xB69B, run_fuspic, 1, 0,0,0,0 }, { "DELTA8", 0xB6FA, run_delta8, 0, 0,0,0,0 },
    { "PULPIC", 0xB71B, run_pulpic, 1, 0,0,0,0 }, { "DSPCHG", 0xB75B, run_dspchg, 0, 0,0,0,0 },
    { "DSPEXP", 0xB79A, run_dspexp, 0, 0,0,0,0 }, { "CHPLKI", 0xB7EB, run_chplki, 0, 0,0,0,0 },
    { "SPECIAL", 0xB84E, run_special, 0, 0,0,0,0 }, { "ALTCOL", 0xB85F, run_altcol, 1, 0,0,0,0 },
    { "ROTCOL", 0xB875, run_rotcol, 1, 0,0,0,0 }, { "SETSHR", 0xB888, run_setshr, 1, 0,0,0,0 },
    { "SHRSCA", 0xB896, run_shrsca, 1, 0,0,0,0 }, { "DSBOOM", 0xB8BA, run_dsboom, 1, 0,0,0,0 },
    { "SWAPVG", 0xB944, run_swapvg, 0, 0,0,0,0 }, { "CALMAG", 0xB955, run_calmag, 0, 0,0,0,0 },
    { "WHICHB", 0xB967, run_whichb, 0, 0,0,0,0 }, { "SCAPIC", 0xBCFD, run_scapic, 0, 0,0,0,0 },
    { "SCAPI2", 0xBD09, run_scapi2, 0, 0,0,0,0 }, { "CASCAL", 0xBD3E, run_cascal, 0, 0,0,0,0 },
    { "ONELIN", 0xBDA0, run_onelin, 0, 0,0,0,0 }, { "ONELN2", 0xBDCB, run_oneln2, 0, 0,0,0,0 },
    { "WORSCR", 0xC098, run_worscr, 0, 0,0,0,0 }, { "INIDSP", 0xC16E, run_inidsp, 0, 0,0,0,0 },
    { "INICOL", 0xC196, run_inicol, 0, 0,0,0,0 }, { "INIMAT", 0xC1C3, run_inimat, 0, 0,0,0,0 },
    { "INIWLS", 0xC235, run_iniwls, 0, 0,0,0,0 }, { "LVLWEL", 0xC2E8, run_lvlwel, 0, 0,0,0,0 },
    { "BLDWEL", 0xC30D, run_bldwel, 0, 0,0,0,0 }, { "OUTLIN", 0xC36E, run_outlin, 0, 0,0,0,0 },
    { "CONNEC", 0xC3BA, run_connec, 0, 0,0,0,0 }, { "SPOKE",  0xC3EE, run_spoke,  0, 0,0,0,0 },
    { "LINTOS", 0xC423, run_lintos, 0, 0,0,0,0 }, { "LIFTOS", 0xC43C, run_liftos, 0, 0,0,0,0 },
    { "CHKDEP", 0xC453, run_chkdep, 0, 0,0,0,0 }, { "CALOUT", 0xC473, run_calout, 0, 0,0,0,0 },
    { "DSPHOL", 0xC4E1, run_dsphol, 0, 0,0,0,0 }, { "DSTARF", 0xC54D, run_dstarf, 0, 0,0,0,0 },
    { "DSPENL", 0xC5C2, run_dspenl, 0, 0,0,0,0 }, { "FIXSTU", 0xC66D, run_fixstu, 0, 0,0,0,0 },
    { "TIPACT", 0xC6C7, run_tipact, 0, 0,0,0,0 }, { "FCONNEC", 0xC73C, run_fconnec, 0, 0,0,0,0 },
    { "VGYAB1", 0xC765, run_vgyab1, 0, 0,0,0,0 }, { "VGYABS", 0xC772, run_vgyabs, 0, 0,0,0,0 },
    /* ALWELG (M6; rts = 1: ROUTAD state routines and the SPARAD / NPARAD /
     * NYMTAD / TABJSR dispatch targets; PRORAT also follows INIRAT's JSR
     * CLRPOT, whose RTS arrival is a valid entry into the fall-through) */
    { "INEWAV", 0x9009, run_inewav, 0, 0,0,0,0 }, { "INEWLI", 0x9025, run_inewli, 0, 0,0,0,0 },
    { "INIOBJ", 0x902B, run_iniobj, 0, 0,0,0,0 }, { "NEWAV2", 0x904B, run_newav2, 1, 0,0,0,0 },
    { "INIRA0", 0x90C4, run_inira0, 0, 0,0,0,0 }, { "INIRAT", 0x9108, run_inirat, 1, 0,0,0,0 },
    { "PRORAT", 0x9149, run_prorat, 1, 0,0,0,0 }, { "BONSCO", 0x91B5, run_bonsco, 0, 0,0,0,0 },
    { "INICUR", 0x921B, run_inicur, 0, 0,0,0,0 }, { "INIENE", 0x9234, run_iniene, 0, 0,0,0,0 },
    { "ININYM", 0x9246, run_ininym, 0, 0,0,0,0 }, { "INIINV", 0x926F, run_iniinv, 0, 0,0,0,0 },
    { "INICHA", 0x928F, run_inicha, 0, 0,0,0,0 }, { "INIEXP", 0x929F, run_iniexp, 0, 0,0,0,0 },
    { "CLRPOT", 0x92AD, run_clrpot, 0, 0,0,0,0 }, { "SWAPEN", 0x92B2, run_swapen, 0, 0,0,0,0 },
    { "CONTOUR", 0x92C5, run_contour, 0, 0,0,0,0 }, { "TIMES8", 0x93E0, run_times8, 0, 0,0,0,0 },
    { "DOTYPE", 0x9677, run_dotype, 0, 0,0,0,0 }, { "DONEXT", 0x9683, run_donext, 0, 0,0,0,0 },
    { "DOTZAN", 0x96AB, run_dotzan, 1, 0,0,0,0 }, { "ITMIZE", 0x96B7, run_itmize, 1, 0,0,0,0 },
    { "SAMALL", 0x96C4, run_samall, 1, 0,0,0,0 }, { "TWOBYT", 0x96C7, run_twobyt, 1, 0,0,0,0 },
    { "ONEBYT", 0x96C8, run_onebyt, 1, 0,0,0,0 }, { "NITMIZ", 0x96CB, run_nitmiz, 1, 0,0,0,0 },
    { "DOTB",   0x96DB, run_dotb,   1, 0,0,0,0 }, { "DOTA",   0x96E2, run_dota,   1, 0,0,0,0 },
    { "RANGER", 0x96F4, run_ranger, 0, 0,0,0,0 }, { "DOTR",   0x9700, run_dotr,   1, 0,0,0,0 },
    { "PRBOOM", 0xA618, run_prboom, 1, 0,0,0,0 }, { "TIMLAU", 0xA65B, run_timlau, 0, 0,0,0,0 },
    { "FIXTOP", 0xA69B, run_fixtop, 0, 0,0,0,0 }, { "UPARPO", 0xA6A9, run_uparpo, 0, 0,0,0,0 },
    { "DECPAR", 0xA721, run_decpar, 0, 0,0,0,0 }, { "DECELE", 0xA75D, run_decele, 0, 0,0,0,0 },
    { "INBOOM", 0xA789, run_inboom, 0, 0,0,0,0 }, { "POLDEL", 0xA7A6, run_poldel, 0, 0,0,0,0 },
    { "INSTAR", 0xA7BD, run_instar, 0, 0,0,0,0 }, { "PRSTAR", 0xA7D2, run_prstar, 0, 0,0,0,0 },
    { "INISUZ", 0xA831, run_inisuz, 0, 0,0,0,0 },
    { "PLAY",   0x970B, run_play,   1, 0,0,0,0 }, { "PLDROP", 0x9729, run_pldrop, 1, 0,0,0,0 },
    { "MOVCUR", 0x9749, run_movcur, 0, 0,0,0,0 }, { "AUTOCU", 0x97C5, run_autocu, 0, 0,0,0,0 },
    { "MOVCUD", 0x97F8, run_movcud, 0, 0,0,0,0 }, { "MOVNYM", 0x98A2, run_movnym, 0, 0,0,0,0 },
    { "CONYMP", 0x9923, run_conymp, 0, 0,0,0,0 }, { "ACTINV", 0x994D, run_actinv, 0, 0,0,0,0 },
    { "NYMCHA", 0x99A5, run_nymcha, 0, 0,0,0,0 }, { "NEWTYP", 0x9A87, run_newtyp, 0, 0,0,0,0 },
    { "NEWTY2", 0x9A88, run_newty2, 0, 0,0,0,0 }, { "NEWFLI", 0x9A9D, run_newfli, 1, 0,0,0,0 },
    { "NEWPUL", 0x9AA9, run_newpul, 1, 0,0,0,0 }, { "NEWFUS", 0x9AB3, run_newfus, 1, 0,0,0,0 },
    { "NEWSPI", 0x9AB7, run_newspi, 1, 0,0,0,0 }, { "NEWTAN", 0x9ABB, run_newtan, 1, 0,0,0,0 },
    { "NEWGEN", 0x9AEE, run_newgen, 0, 0,0,0,0 }, { "SPLCHA", 0x9B07, run_splcha, 0, 0,0,0,0 },
    { "MOVINV", 0x9B1E, run_movinv, 0, 0,0,0,0 }, { "JSRCAM", 0x9B98, run_jsrcam, 0, 0,0,0,0 },
    { "JEXIT",  0x9BCA, run_jexit,  1, 0,0,0,0 }, { "JNOOP",  0x9BCF, run_jnoop,  1, 0,0,0,0 },
    { "JSLOOP", 0x9BD0, run_jsloop, 1, 0,0,0,0 }, { "JSLOPB", 0x9BDD, run_jslopb, 1, 0,0,0,0 },
    { "JSKIP0", 0x9BEE, run_jskip0, 1, 0,0,0,0 }, { "JBR0PC", 0x9BFA, run_jbr0pc, 1, 0,0,0,0 },
    { "JELOOP", 0x9C0C, run_jeloop, 1, 0,0,0,0 }, { "JSETPC", 0x9C17, run_jsetpc, 1, 0,0,0,0 },
    { "JELTST", 0x9C21, run_jeltst, 1, 0,0,0,0 }, { "JCHKPU", 0x9C3B, run_jchkpu, 1, 0,0,0,0 },
    { "JCHROT", 0x9C4F, run_jchrot, 1, 0,0,0,0 }, { "JSMOVE", 0x9C58, run_jsmove, 1, 0,0,0,0 },
    { "JSMOVU", 0x9C63, run_jsmovu, 0, 0,0,0,0 }, { "JSMOVD", 0x9C99, run_jsmovd, 0, 0,0,0,0 },
    { "JPULMO", 0x9CB6, run_jpulmo, 1, 0,0,0,0 }, { "CHASER", 0x9D06, run_chaser, 0, 0,0,0,0 },
    { "JCHPLA", 0x9D67, run_jchpla, 1, 0,0,0,0 }, { "JJUMPM", 0x9D82, run_jjumpm, 1, 0,0,0,0 },
    { "JKITST", 0x9E2F, run_jkitst, 1, 0,0,0,0 }, { "JFUSKI", 0x9E48, run_jfuski, 1, 0,0,0,0 },
    { "JJUMPS", 0x9E5C, run_jjumps, 1, 0,0,0,0 }, { "OKTOJM", 0x9EAB, run_oktojm, 0, 0,0,0,0 },
    { "CALSAN", 0x9ED7, run_calsan, 0, 0,0,0,0 }, { "JFUSEUP", 0x9EF1, run_jfuseup, 1, 0,0,0,0 },
    { "MAYBLR", 0x9F5F, run_mayblr, 0, 0,0,0,0 }, { "FUCHPL", 0x9F81, run_fuchpl, 0, 0,0,0,0 },
    { "LEFRIT", 0x9F8A, run_lefrit, 0, 0,0,0,0 }, { "JSTRAI", 0x9FC4, run_jstrai, 1, 0,0,0,0 },
    { "ASTRAL", 0xA028, run_astral, 0, 0,0,0,0 }, { "KILINV", 0xA06F, run_kilinv, 0, 0,0,0,0 },
    { "MOVCHA", 0xA18F, run_movcha, 0, 0,0,0,0 }, { "CHATOP", 0xA1E4, run_chatop, 0, 0,0,0,0 },
    { "LIFECT", 0xA1FA, run_lifect, 0, 0,0,0,0 }, { "FIREPC", 0xA23F, run_firepc, 0, 0,0,0,0 },
    { "FIREIC", 0xA2A6, run_fireic, 0, 0,0,0,0 }, { "INCFS2", 0xA309, run_incfs2, 0, 0,0,0,0 },
    { "INIPSQ", 0xA33A, run_inipsq, 0, 0,0,0,0 }, { "INFPSQ", 0xA343, run_infpsq, 0, 0,0,0,0 },
    { "INPPSQ", 0xA347, run_inppsq, 0, 0,0,0,0 }, { "INCPSQ", 0xA34B, run_incpsq, 0, 0,0,0,0 },
    { "DEADCU", 0xA352, run_deadcu, 0, 0,0,0,0 }, { "INCCSQ", 0xA36F, run_inccsq, 0, 0,0,0,0 },
    { "INCIS2", 0xA38E, run_incis2, 0, 0,0,0,0 }, { "GEXIFU", 0xA3CA, run_gexifu, 0, 0,0,0,0 },
    { "GENEXP", 0xA3D4, run_genexp, 0, 0,0,0,0 }, { "GENEX2", 0xA3D6, run_genex2, 0, 0,0,0,0 },
    { "PROEXP", 0xA416, run_proexp, 0, 0,0,0,0 }, { "COLLIS", 0xA454, run_collis, 0, 0,0,0,0 },
    { "COLCHK", 0xA463, run_colchk, 0, 0,0,0,0 }, { "ANALYZ", 0xA504, run_analyz, 0, 0,0,0,0 },
    { "INDROP", 0xA5CB, run_indrop, 0, 0,0,0,0 }, { "PROSUZ", 0xA83A, run_prosuz, 0, 0,0,0,0 },
    { "KILENE", 0xA888, run_kilene, 0, 0,0,0,0 }, { "EXIKIL", 0xA8A4, run_exikil, 0, 0,0,0,0 },
    /* ALTES2 (M9 B3; rts = 1: SYSTEM by the ROUTAD dispatch, DSPSYS by DROUTAD,
     * the seven self-test states by SSTATE's SFTJSR RTS - none follows a JSR) */
    { "SYSTEM", 0xD7E1, run_system, 1, 0,0,0,0 }, { "DSPSYS", 0xD804, run_dspsys, 1, 0,0,0,0 },
    { "POSDIG", 0xD8A9, run_posdig, 0, 0,0,0,0 }, { "SSTATE", 0xDB0F, run_sstate, 0, 0,0,0,0 },
    { "SIGANA", 0xDB22, run_sigana, 1, 0,0,0,0 }, { "BADEAR", 0xDB5A, run_badear, 1, 0,0,0,0 },
    { "SCHEKR", 0xDB6F, run_schekr, 1, 0,0,0,0 }, { "SINTEN", 0xDB7E, run_sinten, 1, 0,0,0,0 },
    { "SHATCH", 0xDB84, run_shatch, 1, 0,0,0,0 }, { "SHYSTER", 0xDB9A, run_shyster, 1, 0,0,0,0 },
    { "GETOP3", 0xDBE0, run_getop3, 0, 0,0,0,0 }, { "ROMREP", 0xDBF7, run_romrep, 1, 0,0,0,0 },
    { "READMB", 0xDCE6, run_readmb, 0, 0,0,0,0 }, { "DOPSWI", 0xDD0D, run_dopswi, 0, 0,0,0,0 },
    { "BITS2",  0xDD27, run_bits2,  0, 0,0,0,0 }, { "BITS3",  0xDD29, run_bits3,  0, 0,0,0,0 },
    { "GENOPD", 0xDD2B, run_genopd, 0, 0,0,0,0 }, { "DBOOKE", 0xDD41, run_dbooke, 0, 0,0,0,0 },
};
#define N_ROUTINES (sizeof routines / sizeof routines[0])
static routine irq_routine = { "IRQ", 0xD704, run_irq, 0, 0,0,0,0 };

/* ------------------------------------------------------------------ */
/* open checks                                                         */
/* ------------------------------------------------------------------ */
typedef struct {
    routine *r;
    int      is_irq, tainted;
    uint8_t  entry_s, min_s, a, x, y, p;
    uint16_t caller;
    unsigned io_start;
    unsigned ev_start;              /* its IRQs: place[ev_start .. ) */
    uint32_t seg_base;              /* ordinal of the first thunk segment it can own */
    int      in_seg;                /* opened inside a thunk segment: IRQs cannot be placed */
    int      place_idx;             /* IRQ checks: its place[] entry (-1 none) */
    uint8_t  ram[0x800], vram[0x1000], col[16];
} check;

#define MAX_DEPTH 16                    /* M4: display states nest INFO/MSGS/VG* inside DISPLAY */
static check stk[MAX_DEPTH];
static int   depth;
static unsigned long failures_printed, depth_overflow, moved_printed;

/* ---- pass probe state ---- */
/* M9 B3: pass kinds - the RESET region (C reset(), from a RESET arrival to the
 * first loop head), a MAINLN pass ($C7AD, mainln_pass()), a diag pass ($DA8D,
 * diag_pass()) */
enum { PK_RESET, PK_MAINLN, PK_DIAG, PK_KINDS };
static const char *const pk_name[PK_KINDS] = { "RESET region", "MAINLN pass", "DIAG pass" };
static struct {
    int      open, kind, cause, skipped;
    char     why[80];
    unsigned pass;                  /* frame number the pass starts at (region: the frame before it) */
    unsigned long open_bnd;         /* boundary ordinal it opened at */
    uint8_t  entry_s, min_s, x, y, p;
    unsigned io_start, ev_start;
    uint32_t seg_base;
    unsigned long rom_irqs;
    uint8_t  ram[0x800], vram[0x1000], col[16];
} P;
static unsigned frame_no;
static unsigned long n_bnd, pending_reset;             /* pending_reset: cause + 1 */
static unsigned long pk_checked[PK_KINDS], pk_passed[PK_KINDS], pk_failed[PK_KINDS], pk_skipped[PK_KINDS];
static unsigned long pk_by_reset[PK_KINDS];            /* ended by a RESET arrival (C reached hw_reset / hw_watchdog_hang) */
static unsigned long rst_arrivals[RST_KINDS], rst_region_passed[RST_KINDS], rst_region_failed[RST_KINDS];
static unsigned long passes_cut_empty;                 /* script reset at the loop head: the pass never ran */
static unsigned long checks_by_reset;                  /* routine checks finished by a RESET arrival */
static unsigned long skip_irq_translated, skip_other;
/* M5: where unplaceable IRQs were taken (ROM PC of the interrupted instruction):
 * in skipped passes, and in routine checks that needed the IRQ-moved device */
static unsigned long hist_pass[0x10000], hist_check[0x10000], hist_cell[0x10000];
static uint16_t hist_pc_cell[0x10000];                /* last non-commuting cell per PC */
static unsigned long pass_anch_irqs, check_anch_calls;

static void print_hist(const char *what, const unsigned long *h)
{
    unsigned long total = 0;
    int shown = 0;
    for (unsigned a = 0; a < 0x10000; a++) total += h[a];
    printf("LOCKSTEP: unplaceable IRQs %s: %lu", what, total);
    while (total && shown < 16) {
        unsigned best = 0;
        for (unsigned a = 1; a < 0x10000; a++) if (h[a] > h[best]) best = a;
        if (!h[best]) break;
        if (h != hist_cell) printf("%s$%04X x%lu [$%04X]", shown ? ", " : " (top: ", best, h[best], hist_pc_cell[best]);
        else printf("%s$%04X x%lu", shown ? ", " : " (top: ", best, h[best]);
        ((unsigned long *)h)[best] = 0;    /* printed last: destructive is fine */
        shown++;
    }
    printf("%s\n", shown ? ")" : "");
}

void lk_init(const uint8_t *ram, const uint8_t *vram, const uint8_t *colram)
{
    R_ram = ram; R_vram = vram; R_col = colram;
    for (size_t i = 0; i < sizeof thunk_addr / sizeof thunk_addr[0]; i++) if (thunk_addr[i]) thunk_map[thunk_addr[i]] = 1;
    for (size_t i = 0; i < sizeof ck_addr / sizeof ck_addr[0]; i++) if (ck_addr[i]) ck_map[ck_addr[i]] = 1;
}

/* ---- M5: ROM-side commutation tracking ----------------------------------
 * Cells: RAM $0000-$07FF and colour RAM $0800-$080F as themselves, vector RAM
 * $2000-$2FFF at $0810+.  The stack page is not tracked (the IRQ's pushes go
 * below SP; the C port has no stack).  win_rd/win_wr hold the window number
 * of the last main-line read / write; a window ends at every event (I/O,
 * checkpoint visit, the end of an IRQ). */
#define CELLS 0x1810u
#define IRQ_ACC_MAX 1024u
static uint32_t win_epoch = 1, irq_win;
static uint32_t win_rd[CELLS], win_wr[CELLS];
static uint32_t irq_stamp[CELLS], irq_epoch;
static uint8_t  irq_flags[CELLS];
static uint16_t irq_acc[IRQ_ACC_MAX];
static unsigned n_irq_acc;
static int      irq_acc_over, cur_irq_place = -1, visit_logged_at_irq;

static unsigned cell_of(uint16_t a) { return a < 0x2000 ? a : (unsigned)(a - 0x2000u + 0x810u); }

void lk_mem(uint16_t a, int write)
{
    unsigned i;
    if (a >= 0x100 && a < 0x200) return;
    i = cell_of(a);
    if (in_irq) {
        if (irq_stamp[i] != irq_epoch) {
            irq_stamp[i] = irq_epoch; irq_flags[i] = 0;
            if (n_irq_acc < IRQ_ACC_MAX) irq_acc[n_irq_acc++] = (uint16_t)i; else irq_acc_over = 1;
        }
        irq_flags[i] |= write ? 2 : 1;
    } else if (write) {
        win_wr[i] = win_epoch;
    } else {
        win_rd[i] = win_epoch;
    }
}

/* at the IRQ's RTI: does it commute with the window it interrupted? */
static void judge_irq(void)
{
    if (cur_irq_place >= 0 && (unsigned)cur_irq_place < n_place) {
        irq_place *pl = &place[cur_irq_place];
        pl->anch_ok = !irq_acc_over;
        pl->conflict = irq_acc_over ? 0xFFFF : 0;
        for (unsigned j = 0; j < n_irq_acc && pl->anch_ok; j++) {
            unsigned i = irq_acc[j];
            if (((irq_flags[i] & 1) && win_wr[i] == irq_win) ||
                ((irq_flags[i] & 2) && (win_rd[i] == irq_win || win_wr[i] == irq_win))) {
                pl->anch_ok = 0;
                pl->conflict = (uint16_t)(i < 0x810 ? i : i - 0x810 + 0x2000);
            }
        }
    }
    cur_irq_place = -1;
    /* M9 B3: NO new window at the RTI.  The C run fires the next anchored IRQ
     * right after this one, so if this one was itself anchored (moved back
     * to the last event) the next moves back over this window too: its
     * commutation must be proven against everything since the last real
     * event.  (Until B3 the window restarted here; DBOOKE showed the hole:
     * IRQ 1 anchored before the BOOKKS reads, IRQ 2 - the one that
     * increments SECOUL - anchored after IRQ 1, i.e. also before the reads.) */
}

static void log_event(uint32_t addr, uint8_t val, uint8_t kind)
{
    if (io_n >= IOLOG_MAX) {
        io_overflow++;
        for (int i = 0; i < depth; i++) stk[i].tainted = 1;
        if (P.open && !P.skipped) { P.skipped = 1; snprintf(P.why, sizeof P.why, "I/O log overflow"); }
        return;
    }
    iolog[io_n].addr = addr; iolog[io_n].val = val; iolog[io_n].write = kind;
    io_n++;
}

/* ------------------------------------------------------------------ */
/* M6 Gates 1/N: the whole run as a replay trace (--trace-out FILE)    */
/* ------------------------------------------------------------------ */
/* tests\gate.exe replays the C modules alone against this file (no ROM, no
 * ref6502).  Unlike iolog[] (per check / per pass, reset at $C7AD), the trace
 * holds EVERY event from RESET, in ROM order.  Binary, little-endian:
 *
 *   header  "TEMPTRC1" (8), u32 version = 2, u32 watchdog timeout (cycles, refrun's model),
 *           u16 n + n bytes script path ("" = attract)
 *   'R' u16 addr, u8 val    hardware read (the value the ROM got)
 *   'r' u16 addr, u8 val, u16 count
 *                           (v2) count >= 2 identical consecutive reads (IN1 busy waits)
 *   'W' u16 addr, u8 val    hardware write (strobes $4800 $5000 $5800 $60CB $60DB: value unchecked)
 *   'S' u8 sp               the SP the IRQ's TSX at $D70A saw (IO_SP)
 *   'C' u16 pc              checkpoint visit (ck_addr[], outside the IRQ, as iolog)
 *   'I' u8 kind, u8 flags, u16 pc
 *                           the ROM took the IRQ here: after every event above it and before
 *                           its own events (which follow up to the IRQ's RTI).  kind: 0 START
 *                           (at MAINLN $C7A0), 1 WAIT (frame wait $C7A7/9/B), 2 SEG (in a ROM
 *                           thunk segment: GETOP3 in attract), 3 OTHER (translated code),
 *                           $FF no placement record.  flags b0: anchorable (lockstep's
 *                           commutation proof, place[].anch_ok).  pc = interrupted PC.
 *   'P' u32 frame, u32 irqs, u8 loop, u8 dflag, u8 min_sp, u8 x, u8 y, u16 nruns, nruns x {u16 off, u16 len, len bytes}
 *                           the ROM arrived at a loop head for the frame-th time (= tests\ref\frame_NNNN):
 *                           loop (v2) 0 MAINLN $C7AD, 1 DIAG $DA8D; irqs = IRQs taken since power-on,
 *                           D flag, the lowest SP since the last RESET arrival (boundaries at
 *                           $D93F-$D9D5 ignored: old stack / the ZP march's pattern in S), X/Y;
 *                           the machine image as runs changed since the previous 'P' / 'X'
 *                           (image: RAM $0000-$07FF at 0, colour RAM at $0800, vector RAM at $0810).
 *   'X' u8 cause, u32 irqs, u8 dflag, u8 min_sp, u16 nruns, runs
 *                           (v2) RESET arrival at $D93F: cause 0 power-on, 1 JMP RESET, 2 watchdog
 *                           bite, 3 script reset; the image at the arrival (before RESET's first
 *                           instruction), min_sp as in 'P' for the code before it.  The first
 *                           record of every trace is 'X' cause 0.
 *   'E' u32 passes, u32 events
 */
#define TR_IMG 0x1810u
typedef struct { uint32_t addr; int32_t place; uint8_t tag, val; } tr_rec;
static FILE    *tr_f;
static tr_rec  *tr_buf;
static size_t   tr_n, tr_cap;
static uint8_t  tr_prev[TR_IMG], tr_cur[TR_IMG], tr_runs[TR_IMG * 5u];
static uint8_t  tr_min_s = 0xFF;
static unsigned long tr_irqs, tr_events, tr_passes, tr_unanch, tr_resets, tr_read_runs;
static int      tr_oom;

static void tr_put(const uint8_t *b, size_t n) { if (tr_f) fwrite(b, 1, n, tr_f); }
static void tr_u16(uint32_t v) { uint8_t b[2] = { (uint8_t)v, (uint8_t)(v >> 8) }; tr_put(b, 2); }
static void tr_u32(uint32_t v) { uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) }; tr_put(b, 4); }

int lk_trace_open(const char *path, const char *script, uint64_t watchdog_cycles)
{
    size_t n = script ? strlen(script) : 0;
    tr_f = fopen(path, "wb");
    if (!tr_f) { fprintf(stderr, "LOCKSTEP: cannot write trace %s\n", path); return 1; }
    tr_put((const uint8_t *)"TEMPTRC1", 8);
    tr_u32(2);
    tr_u32((uint32_t)watchdog_cycles);
    tr_u16((uint32_t)n);
    if (n) tr_put((const uint8_t *)script, n);
    return 0;
}

static void tr_add(uint8_t tag, uint32_t addr, uint8_t val, int32_t pl)
{
    if (tr_n >= tr_cap) {
        size_t cap = tr_cap ? tr_cap * 2 : 65536u;
        tr_rec *nb = (tr_rec *)realloc(tr_buf, cap * sizeof *nb);
        if (!nb) { tr_oom = 1; return; }
        tr_buf = nb; tr_cap = cap;
    }
    tr_buf[tr_n].tag = tag; tr_buf[tr_n].addr = addr; tr_buf[tr_n].val = val; tr_buf[tr_n].place = pl;
    tr_n++;
}

/* at a loop head / RESET arrival: write the events since the last record (IRQ
 * markers resolved from place[], judged at their RTIs); identical consecutive
 * reads as one 'r' record (v2) */
static void tr_flush(void)
{
    uint8_t b[7];
    for (size_t k = 0; k < tr_n; k++) {
        const tr_rec *e = &tr_buf[k];
        b[0] = e->tag;
        switch (e->tag) {
        case 'R': {
            size_t m = k + 1;
            while (m < tr_n && m - k < 65535u && tr_buf[m].tag == 'R' && tr_buf[m].addr == e->addr && tr_buf[m].val == e->val) m++;
            b[1] = (uint8_t)e->addr; b[2] = (uint8_t)(e->addr >> 8); b[3] = e->val;
            if (m - k >= 2) {
                b[0] = 'r'; b[4] = (uint8_t)(m - k); b[5] = (uint8_t)((m - k) >> 8);
                tr_put(b, 6);
                tr_read_runs++;
                k = m - 1;
            } else {
                tr_put(b, 4);
            }
            break;
        }
        case 'W':
            b[1] = (uint8_t)e->addr; b[2] = (uint8_t)(e->addr >> 8); b[3] = e->val; tr_put(b, 4); break;
        case 'S':
            b[1] = e->val; tr_put(b, 2); break;
        case 'C':
            b[1] = (uint8_t)e->addr; b[2] = (uint8_t)(e->addr >> 8); tr_put(b, 3); break;
        default: {                                  /* 'I' */
            int ok = e->place >= 0 && (unsigned)e->place < n_place;
            b[1] = ok ? place[e->place].kind : 0xFF;
            b[2] = (uint8_t)(ok && place[e->place].anch_ok ? 1 : 0);
            if (!b[2] && b[1] != EV_START && b[1] != EV_WAIT) tr_unanch++;
            b[3] = (uint8_t)e->addr; b[4] = (uint8_t)(e->addr >> 8);
            tr_put(b, 5);
            break;
        }
        }
    }
    tr_events += (unsigned long)tr_n;
    tr_n = 0;
}

/* the machine image as changed runs since the previous record */
static void tr_image(void)
{
    size_t rl = 0;
    unsigned nruns = 0, i = 0;
    memcpy(tr_cur, R_ram, 0x800);
    memcpy(tr_cur + 0x800, R_col, 16);
    memcpy(tr_cur + 0x810, R_vram, 0x1000);
    while (i < TR_IMG) {
        unsigned s, e, k;
        if (tr_cur[i] == tr_prev[i]) { i++; continue; }
        s = i; e = i + 1;
        for (k = i + 1; k < TR_IMG && k < e + 8u; k++) if (tr_cur[k] != tr_prev[k]) e = k + 1;
        tr_runs[rl++] = (uint8_t)s; tr_runs[rl++] = (uint8_t)(s >> 8);
        tr_runs[rl++] = (uint8_t)(e - s); tr_runs[rl++] = (uint8_t)((e - s) >> 8);
        memcpy(tr_runs + rl, tr_cur + s, e - s); rl += e - s;
        nruns++;
        i = e;
    }
    memcpy(tr_prev, tr_cur, TR_IMG);
    tr_u16(nruns);
    tr_put(tr_runs, rl);
}

/* at a loop head: the 'P' record (loop 0 MAINLN, 1 DIAG) */
static void tr_pass(const cpu *c, uint8_t loop)
{
    uint8_t b[5];
    tr_flush();
    b[0] = 'P'; tr_put(b, 1);
    tr_u32(frame_no); tr_u32((uint32_t)tr_irqs);
    b[0] = loop; b[1] = (uint8_t)((c->p & CPU_D) != 0); b[2] = tr_min_s; b[3] = c->x; b[4] = c->y; tr_put(b, 5);
    tr_image();
    tr_passes++;
}

/* at a RESET arrival: the 'X' record */
static void tr_reset(const cpu *c, uint8_t cause)
{
    uint8_t b[3];
    tr_flush();
    b[0] = 'X'; b[1] = cause; tr_put(b, 2);
    tr_u32((uint32_t)tr_irqs);
    b[0] = (uint8_t)((c->p & CPU_D) != 0); b[1] = tr_min_s; tr_put(b, 2);
    tr_image();
    tr_resets++;
}

static int tr_close(void)
{
    if (!tr_f) return 0;
    tr_put((const uint8_t *)"E", 1);
    tr_u32((uint32_t)tr_passes); tr_u32((uint32_t)tr_events);
    fclose(tr_f); tr_f = NULL;
    printf("LOCKSTEP: trace: %lu passes, %lu RESET records, %lu events (%lu read runs), %lu IRQs, lowest SP $%02X, "
           "%lu IRQs outside START/WAIT without an anchor proof%s\n",
           tr_passes, tr_resets, tr_events, tr_read_runs, tr_irqs, tr_min_s, tr_unanch, tr_oom || tr_n ? " - INCOMPLETE" : "");
    return tr_oom ? 1 : 0;
}

static void log_visit(uint16_t pc)
{
    if (tr_f) tr_add('C', pc, 0, -1);
    win_epoch++;
    if (depth == 0 && !P.open) return;
    log_event(pc, 0, IO_CK);
}

void lk_io(uint32_t addr, uint8_t val, int write)
{
    if (tr_f) tr_add(addr == IO_SP ? 'S' : write ? 'W' : 'R', addr, val, -1);
    if (!in_irq) win_epoch++;
    if (depth == 0 && !P.open) return;
    log_event(addr, val, write ? IO_WRITE : IO_READ);
}

static void open_check(routine *r, int is_irq, const cpu *c, uint16_t caller)
{
    check *k;
    if (depth >= MAX_DEPTH) { depth_overflow++; return; }
    k = &stk[depth++];
    k->r = r; k->is_irq = is_irq; k->tainted = 0;
    k->entry_s = k->min_s = c->s;
    k->a = c->a; k->x = c->x; k->y = c->y; k->p = c->p;
    k->caller = caller;
    k->io_start = io_n;
    k->ev_start = n_place;
    k->seg_base = seg_ord;
    k->in_seg = seg_open;
    k->place_idx = -1;
    memcpy(k->ram, R_ram, sizeof k->ram);
    memcpy(k->vram, R_vram, sizeof k->vram);
    memcpy(k->col, R_col, sizeof k->col);
}

/* ---- pass probe ---- */
static void open_pass(const cpu *c, int kind)
{
    P.open = 1; P.kind = kind; P.skipped = 0; P.why[0] = 0;
    P.pass = frame_no;
    P.open_bnd = n_bnd;
    /* the RESET region: RESET's TXS makes S $FF; S before it is the old stack */
    P.entry_s = P.min_s = kind == PK_RESET ? 0xFF : c->s;
    P.x = c->x; P.y = c->y; P.p = c->p;
    P.io_start = io_n;
    P.ev_start = n_place;
    P.seg_base = seg_ord;
    P.rom_irqs = 0;
    memcpy(P.ram, R_ram, sizeof P.ram);
    memcpy(P.vram, R_vram, sizeof P.vram);
    memcpy(P.col, R_col, sizeof P.col);
}

static void add_place(uint8_t kind, uint32_t ord, uint32_t off, uint16_t pc)
{
    if (n_place >= PLACE_MAX) {
        place_overflow++;
        for (int i = 0; i < depth; i++) stk[i].tainted = 1;
        if (P.open && !P.skipped) { P.skipped = 1; snprintf(P.why, sizeof P.why, "IRQ placement list overflow"); }
        return;
    }
    place[n_place].kind = kind; place[n_place].ord = ord; place[n_place].off = off; place[n_place].pc = pc;
    place[n_place].s = irq_s;
    place[n_place].anch_ok = 0; place[n_place].conflict = 0xFFFF;     /* $FFFF: not judged / access list overflow */
    n_place++;
}

/* modes of place[start .. n_place) for a run; returns 1 if all are placeable */
static int set_modes(unsigned start, uint32_t base, int in_seg, int pass)
{
    int ok = 1;
    for (unsigned i = start; i < n_place; i++) {
        pmode[i] = place_mode(i, base, in_seg, pass);
        if (pmode[i] == M_NONE) ok = 0;
    }
    return ok;
}

/* set up the C run of a check / pass: its IRQs are place[start .. n_place)
 * (modes from set_modes); anchored IRQs at the very start fire at once */
static void begin_placing(unsigned start, uint32_t base, int wait)
{
    place_pos = start; place_end = n_place; place_base = base;
    thunk_ord = 0; placing = 1; wait_ok = wait; c_in_irq = 0;
}

static void close_pass(const cpu *c)
{
    unsigned lo, hi, nbad = 0;
    uint32_t irq0;
    char diffs[400];
    size_t dl = 0;

    P.open = 0;
    if (!set_modes(P.ev_start, P.seg_base, 0, 1))
        for (unsigned i = P.ev_start; i < n_place; i++)
            if (pmode[i] == M_NONE) {
                hist_pass[place[i].pc]++;
                hist_cell[place[i].conflict]++;
                hist_pc_cell[place[i].pc] = place[i].conflict;
                if (!P.skipped) {
                    P.skipped = 1;
                    snprintf(P.why, sizeof P.why, "IRQ at $%04X in translated code", place[i].pc);
                }
            }
    for (unsigned i = P.ev_start; i < n_place && !P.skipped; i++)
        if (pmode[i] == M_ANCH) pass_anch_irqs++;
    if (P.skipped) {
        pk_skipped[P.kind]++;
        if (strncmp(P.why, "IRQ at", 6) == 0) skip_irq_translated++; else skip_other++;
        return;
    }
    pk_checked[P.kind]++;
    if (by_reset) pk_by_reset[P.kind]++;

    memcpy(g.ram, P.ram, sizeof g.ram);
    memcpy(g.vram, P.vram, sizeof g.vram);
    memcpy(g.colram, P.col, sizeof g.colram);
    g.stray_writes = 0;
    g.dflag = (uint8_t)((P.p & CPU_D) != 0);
    g.loop_x = P.x; g.loop_y = P.y;
    irq0 = g.irq_count;
    rp = P.io_start; rp_end = io_n; io_err = 0; io_msg[0] = 0;
    thunk_s = P.entry_s;
    begin_placing(P.ev_start, P.seg_base, 1);
    try_fire();
    reset_armed = 1;
    if (setjmp(reset_jmp) == 0) {
        if (P.kind == PK_RESET) {
            reset();                            /* altes2.c: to $C7AD's first wait or $DA8D */
            if (!io_err && !by_reset && g.cpu_loop != (c->pc == 0xDA8D ? LOOP_DIAG : LOOP_MAINLN))
                set_err("reset() returned in the wrong loop (g.cpu_loop)");
        } else if (P.kind == PK_DIAG) {
            diag_pass();
        } else {
            mainln_pass();
        }
    }
    reset_armed = 0;
    placing = 0; wait_ok = 0;

    diffs[0] = 0;
    lo = 0x100u + P.min_s + 1u;
    hi = 0x100u + P.entry_s;
    for (unsigned a = 0; a < 0x800; a++) {
        if (a >= lo && a <= hi) continue;
        if (g.ram[a] != R_ram[a]) {
            if (nbad < 6) dl += (size_t)snprintf(diffs + dl, sizeof diffs - dl, " $%04X C=%02X ROM=%02X", a, g.ram[a], R_ram[a]);
            nbad++;
        }
    }
    for (unsigned a = 0; a < 0x1000; a++)
        if (g.vram[a] != R_vram[a]) {
            if (nbad < 6) dl += (size_t)snprintf(diffs + dl, sizeof diffs - dl, " $%04X C=%02X ROM=%02X", a + 0x2000, g.vram[a], R_vram[a]);
            nbad++;
        }
    for (unsigned a = 0; a < 16; a++)
        if (g.colram[a] != R_col[a]) nbad++;
    if (!io_err && rp != rp_end) {
        io_err = 1;
        snprintf(io_msg, sizeof io_msg, "C did %u of the ROM's %u I/O operations", rp - P.io_start, rp_end - P.io_start);
    }
    if (!io_err && place_pos != place_end) {
        io_err = 1;
        snprintf(io_msg, sizeof io_msg, "C placed %u of the ROM's %u IRQs", place_pos - P.ev_start, place_end - P.ev_start);
    }
    int d_bad = g.dflag != ((c->p & CPU_D) != 0);
    int irq_bad = (g.irq_count - irq0) != P.rom_irqs;
    if (nbad || io_err || d_bad || irq_bad || g.stray_writes) {
        pk_failed[P.kind]++;
        if (P.kind == PK_RESET) rst_region_failed[P.cause]++;
        if (failures_printed++ < 20) {
            printf("LOCKSTEP FAIL %s%s%s from frame %u to frame %u%s (IRQs ROM %lu C %lu): %u byte(s)%s%s%s%s%s\n",
                   pk_name[P.kind], P.kind == PK_RESET ? " after " : "", P.kind == PK_RESET ? rst_name[P.cause] : "",
                   P.pass, frame_no, by_reset ? " (ended by a RESET arrival)" : "",
                   P.rom_irqs, (unsigned long)(g.irq_count - irq0), nbad, diffs,
                   io_err ? "; I/O: " : "", io_err ? io_msg : "",
                   d_bad ? "; D flag differs" : "", g.stray_writes ? "; stray writes" : "");
        }
    } else {
        pk_passed[P.kind]++;
        if (P.kind == PK_RESET) rst_region_passed[P.cause]++;
    }
}

void lk_irq(const cpu *c)
{
    unsigned before = n_place;
    int d0 = depth;
    /* M5: the interrupted instruction's checkpoint visit is logged before the
     * IRQ (the boundary after the RTI does not log it again), so an IRQ taken
     * at a checkpoint is anchored right there.  Not inside a thunk segment:
     * an IRQ at a thunk's return address still belongs to the segment (the
     * thunk CPU fires it at its end, before the caller's checkpoint). */
    visit_logged_at_irq = 0;
    if (ck_map[c->pc] && !seg_open) { log_visit(c->pc); visit_logged_at_irq = 1; }
    irq_win = win_epoch;
    irq_epoch++; n_irq_acc = 0; irq_acc_over = 0; cur_irq_place = -1;
    in_irq = 1; irq_s = c->s;
    if (P.open && P.kind == PK_RESET && depth == 0 && c->pc == 0xC7A0) {
        add_place(EV_START, 0, 0, c->pc);             /* RESET's CLI: the IRQ at MAINLN's first instruction */
    } else if (depth > 0 || P.open) {
        /* where did the ROM take it?  (no IRQ nests inside the IRQ) */
        if (seg_open) add_place(EV_SEG, seg_ord - 1, seg_n + 1, c->pc);
        else if (is_thunk_addr(c->pc)) add_place(EV_SEG, seg_ord, 0, c->pc);
        else if (c->pc == 0xC7A7 || c->pc == 0xC7A9 || c->pc == 0xC7AB) add_place(EV_WAIT, 0, 0, c->pc);
        else add_place(EV_OTHER, 0, 0, c->pc);
    }
    if (n_place > before) {
        place[n_place - 1].io_s = place[n_place - 1].io_e = io_n;
        place[n_place - 1].io_done = 0;
        cur_irq_place = (int)(n_place - 1);
    }
    tr_irqs++;
    if (tr_f) tr_add('I', c->pc, 0, n_place > before ? (int32_t)(n_place - 1) : -1);
    if (P.open) P.rom_irqs++;

    open_check(&irq_routine, 1, c, c->pc);
    if (depth > d0) stk[depth - 1].place_idx = n_place > before ? (int)(n_place - 1) : -1;
}

/* Run C irq() once per skipped range, each replaying exactly its own I/O. */
static void run_skipped_irqs(void)
{
    int save = skipping;
    skipping = 0;
    for (unsigned i = 0; i < n_skip; i++) {
        rp = skip_lo[i]; rp_end = skip_hi[i];
        irq();
        if (!io_err && rp != rp_end) {
            io_err = 1;
            snprintf(io_msg, sizeof io_msg, "moved IRQ did %u of its %u I/O operations", rp - skip_lo[i], rp_end - skip_lo[i]);
        }
    }
    skipping = save;
}

/* The IRQs inside check k as a list of I/O ranges: usable for the "IRQ moved
 * to entry / exit" comparison when every one of them completed. */
static int irq_ranges_complete(const check *k)
{
    unsigned n = n_place - k->ev_start;
    if (n == 0 || n > MAX_SKIP) return 0;
    for (unsigned i = k->ev_start; i < n_place; i++)
        if (!place[i].io_done) return 0;
    return 1;
}

/* Run the C function of check k from its snapshot and compare with the ROM.
 * mode 0: IRQs placed where the ROM took them (inside thunks) or none;
 * mode 1 / 2 (M4): the check's IRQs run as C irq() at the routine's entry /
 * after its return, each replaying its own I/O range, while the routine's
 * replay skips those ranges.  Returns 1 if anything differs; on a mode-0
 * difference (or when report is set) it prints. */
static int run_compare(check *k, const cpu *c, int mode, int report)
{
    routine *r = k->r;
    unsigned lo, hi, nbad = 0;
    char diffs[400];
    size_t dl = 0;

    memcpy(g.ram, k->ram, sizeof g.ram);
    memcpy(g.vram, k->vram, sizeof g.vram);
    memcpy(g.colram, k->col, sizeof g.colram);
    g.stray_writes = 0;
    g.dflag = (uint8_t)((k->p & CPU_D) != 0);
    io_err = 0; io_msg[0] = 0;
    R.a = k->a; R.x = k->x; R.y = k->y; R.p = k->p;
    ret_c = -1;
    ret_a = ret_x = ret_y = -1;
    thunk_s = k->is_irq ? (uint8_t)(k->entry_s - 6) : k->entry_s;   /* IRQ: 3 bytes pushed by the interrupt + PHA x3 */
    n_skip = 0;
    if (mode) {
        for (unsigned i = k->ev_start; i < n_place; i++) {
            skip_lo[n_skip] = place[i].io_s; skip_hi[n_skip] = place[i].io_e; n_skip++;
        }
    }
    memset(touched, 0, sizeof touched);
    if (mode == 1) {
        memcpy(before_irq, g.ram, sizeof before_irq);
        run_skipped_irqs();
        for (unsigned a = 0; a < 0x800; a++) touched[a] = g.ram[a] != before_irq[a];
    }
    rp = k->io_start; rp_end = io_n;
    skipping = (mode != 0);
    if (mode == 0) { begin_placing(k->ev_start, k->seg_base, 0); try_fire(); }
    else { placing = 0; place_pos = place_end = n_place; }
    reset_armed = 1;
    if (setjmp(reset_jmp) == 0) r->run();     /* M9 B3: hw_reset / hw_watchdog_hang land here */
    reset_armed = 0;
    if (by_reset) { ret_c = -1; ret_a = ret_x = ret_y = -1; }   /* nothing returns at a RESET */
    /* an IRQ taken at the return address after the routine's last RTS: the
     * ROM logged the caller's checkpoint visit there before the IRQ (M5) */
    skip_io();
    if (rp < rp_end && iolog[rp].write == IO_CK && iolog[rp].addr == c->pc) { rp++; try_fire(); }
    placing = 0;
    skip_io();
    skipping = 0;
    if (!io_err && mode == 0 && place_pos != place_end) {
        io_err = 1;
        snprintf(io_msg, sizeof io_msg, "C placed %u of the ROM's %u IRQs", place_pos - k->ev_start, place_end - k->ev_start);
    }
    if (!io_err && rp != rp_end) {
        io_err = 1;
        snprintf(io_msg, sizeof io_msg, "C did %u of the ROM's %u I/O operations", rp - k->io_start, rp_end - k->io_start);
    }
    if (mode == 2) {
        memcpy(before_irq, g.ram, sizeof before_irq);
        run_skipped_irqs();
        for (unsigned a = 0; a < 0x800; a++) touched[a] = g.ram[a] != before_irq[a];
    }
    n_skip = 0;
    last_outside = 0;

    diffs[0] = 0;
    lo = 0x100u + k->min_s + 1u;
    hi = 0x100u + k->entry_s;
    for (unsigned a = 0; a < 0x800; a++) {
        int garbage = 0;
        if (a >= lo && a <= hi) continue;
        /* a placed IRQ taken after the routine's last RTS pushes PC/P and
         * A/X/Y above the entry SP: stack garbage the C call cannot make */
        for (unsigned i = k->ev_start; i < n_place; i++)
            if (a >= 0x100u + place[i].s - 5u && a <= 0x100u + place[i].s) garbage = 1;
        if (garbage) continue;
        if (g.ram[a] != R_ram[a]) {
            if (nbad < 6) dl += (size_t)snprintf(diffs + dl, sizeof diffs - dl, " $%04X C=%02X ROM=%02X", a, g.ram[a], R_ram[a]);
            nbad++;
            if (!touched[a]) last_outside++;
        }
    }
    for (unsigned a = 0; a < 0x1000; a++)
        if (g.vram[a] != R_vram[a]) {
            if (nbad < 6) dl += (size_t)snprintf(diffs + dl, sizeof diffs - dl, " $%04X C=%02X ROM=%02X", a + 0x2000, g.vram[a], R_vram[a]);
            nbad++;
            last_outside++;
        }
    for (unsigned a = 0; a < 16; a++)
        if (g.colram[a] != R_col[a]) { nbad++; last_outside++; }
    int flag_bad = (ret_c >= 0 && ret_c != (c->p & 1));
    int d_bad = g.dflag != ((c->p & CPU_D) != 0);
    char regs[80] = "";
    if ((ret_a >= 0 && ret_a != c->a) || (ret_x >= 0 && ret_x != c->x) || (ret_y >= 0 && ret_y != c->y))
        snprintf(regs, sizeof regs, "; exit registers C A=%d X=%d Y=%d, ROM A=%d X=%d Y=%d (-1 = unchecked)",
                 ret_a, ret_x, ret_y, c->a, c->x, c->y);
    if (flag_bad || d_bad || g.stray_writes || regs[0]) last_outside++;
    last_ioerr = io_err;
    int bad = nbad || io_err || flag_bad || d_bad || g.stray_writes || regs[0];
    if (bad && (report == 2 || (report == 1 && failures_printed++ < 20))) {
        printf("%s %s%s%s #%lu (from $%04X, A=%02X X=%02X Y=%02X P=%02X): %u byte(s)%s%s%s%s%s%s%s\n",
               report == 2 ? "LOCKSTEP NOTE (counted tainted)" : "LOCKSTEP FAIL",
               r->name, mode ? " (IRQ moved to entry and to exit: both differ; exit run shown)" : "",
               by_reset ? " (ended by a RESET arrival)" : "", r->checked, k->caller,
               k->a, k->x, k->y, k->p, nbad, diffs,
               io_err ? "; I/O: " : "", io_err ? io_msg : "",
               flag_bad ? "; C flag differs" : "", d_bad ? "; D flag differs" : "",
               g.stray_writes ? "; stray writes" : "", regs);
    }
    return bad;
}

static void finish(check *k, const cpu *c)
{
    routine *r = k->r;

    /* IRQs inside the call: placeable inside the thunk segments the C function
     * itself calls (M3) or anchored (M5); otherwise the call is tainted */
    if (!set_modes(k->ev_start, k->seg_base, k->in_seg, 0)) {
        for (unsigned i = k->ev_start; i < n_place; i++)
            if (pmode[i] == M_NONE && !k->is_irq) { hist_check[place[i].pc]++; hist_pc_cell[place[i].pc] = place[i].conflict; }
        k->tainted = 1;
    } else if (!k->tainted) {
        for (unsigned i = k->ev_start; i < n_place; i++)
            if (pmode[i] == M_ANCH) { check_anch_calls++; break; }
    }
    if (k->tainted) {
        /* M4: routines that always see an IRQ (INFO, DSPCRD, LDRDSP inside
         * DISPLAY) are still compared with the IRQ's effect applied at the
         * routine's entry or after its return.  Either exact match counts as
         * a pass ("IRQ moved").  If both differ:
         *  - a FAILURE when neither run had an I/O mismatch and both differ in
         *    a cell the moved IRQ itself did not change (vector RAM, colour,
         *    flags and exit registers always count as such): the IRQ cannot
         *    explain that difference;
         *  - otherwise the call may genuinely depend on where the IRQ fell
         *    (e.g. MODSND stepping a channel a sound starter just set): it
         *    stays tainted, counted as a "moved mismatch" and noted. */
        if (!k->is_irq && irq_ranges_complete(k)) {
            unsigned out1, io1;
            if (!run_compare(k, c, 1, 0)) { r->checked++; r->passed++; r->irq_moved++; return; }
            out1 = last_outside; io1 = last_ioerr;
            if (!run_compare(k, c, 2, 0)) { r->checked++; r->passed++; r->irq_moved++; return; }
            if (!io1 && !last_ioerr && out1 && last_outside) {
                r->checked++; r->failed++; r->irq_moved++;
                (void)run_compare(k, c, 2, 1);
                return;
            }
            r->moved_mismatch++;
            if (moved_printed++ < 5) (void)run_compare(k, c, 2, 2);
        }
        r->tainted++;
        return;
    }
    r->checked++;
    if (n_place > k->ev_start) r->irq_placed++;
    if (run_compare(k, c, 0, 1)) r->failed++;
    else r->passed++;
}

static void seg_boundary(const cpu *c, uint8_t last_op)
{
    if (in_irq) {
        if (last_op == 0x40 && c->s == irq_s) in_irq = 0;
        else return;
    }
    if (seg_open) {
        if (c->s > seg_s) seg_open = 0;
        else seg_n++;
    }
    if (!seg_open && is_thunk_addr(c->pc)) {
        seg_open = 1; seg_s = c->s; seg_n = 0; seg_ord++;
    }
}

void lk_note_reset(int cause)
{
    pending_reset = (unsigned long)(cause >= 0 && cause < RST_KINDS ? cause : RST_JMP) + 1u;
}

/* M9 B3: the ROM arrived at RESET ($D93F).  Log the event, finish every open
 * check (innermost first) and the open pass there, then open the RESET region. */
static void on_reset(const cpu *c, int cause)
{
    rst_arrivals[cause]++;
    if (tr_f) tr_reset(c, (uint8_t)cause);
    tr_min_s = 0xFF;
    if (depth > 0 || P.open) log_event((uint32_t)cause, 0, IO_RESET);
    by_reset = 1;
    while (depth > 0) {
        check *k = &stk[depth - 1];
        if (k->is_irq && k->place_idx >= 0 && (unsigned)k->place_idx < n_place) {
            place[k->place_idx].io_e = io_n;
            place[k->place_idx].io_done = 1;
        }
        finish(k, c);
        depth--;
        checks_by_reset++;
    }
    if (P.open) {
        if (n_bnd == P.open_bnd + 1u && P.kind != PK_RESET) {
            P.open = 0;                         /* script reset at the loop head: the pass never ran */
            passes_cut_empty++;
        } else {
            close_pass(c);
        }
    }
    by_reset = 0;
    in_irq = 0; seg_open = 0;
    io_n = 0; n_place = 0; win_epoch++;
    open_pass(c, PK_RESET);
    P.cause = cause;
}

void lk_boundary(const cpu *c, uint8_t last_op)
{
    int rti_now = in_irq && last_op == 0x40 && c->s == irq_s;
    int reset_now = pending_reset && c->pc == 0xD93F;
    /* M9 B3: at $D93F-$D9D5 S is not a stack (the old one before RESET's RAM
     * clear, the test pattern in the zero-page march) */
    int s_is_stack = !(c->pc >= 0xD93F && c->pc < 0xD9D6);
    n_bnd++;
    if (s_is_stack) {
        if (c->s < tr_min_s) tr_min_s = c->s;
        for (int i = 0; i < depth; i++)
            if (c->s < stk[i].min_s) stk[i].min_s = c->s;
        if (P.open && c->s < P.min_s) P.min_s = c->s;
    }
    if (rti_now) judge_irq();                 /* M5: before any check using this IRQ finishes */

    while (depth > 0) {
        check *k = &stk[depth - 1];
        int done = k->is_irq ? (last_op == 0x40 && c->s == k->entry_s) : (c->s > k->entry_s);
        if (!done || (reset_now && !k->is_irq)) break;     /* after a RESET, S says nothing */
        if (k->is_irq && k->place_idx >= 0 && (unsigned)k->place_idx < n_place) {
            place[k->place_idx].io_e = io_n;
            place[k->place_idx].io_done = 1;
        }
        finish(k, c);
        depth--;
    }

    seg_boundary(c, last_op);

    if (reset_now) {
        int cause = (int)pending_reset - 1;
        pending_reset = 0;
        on_reset(c, cause);
        return;                               /* $D93F: no loop head, routine or checkpoint */
    }

    /* pass probe: $C7AD / $DA8D start a pass (and end the previous one or the
     * RESET region) */
    if (!in_irq && (c->pc == 0xC7AD || c->pc == 0xDA8D)) {
        int diag = c->pc == 0xDA8D;
        frame_no++;
        if (tr_f) tr_pass(c, (uint8_t)diag);
        if (P.open) close_pass(c);
        if (depth == 0) { io_n = 0; n_place = 0; win_epoch++; }   /* M5: the pass's C run starts here */
        open_pass(c, diag ? PK_DIAG : PK_MAINLN);
    }

    if (depth == 0 && !P.open) { io_n = 0; n_place = 0; }

    if (depth > 0 && c->pc == 0xD70A) lk_io(IO_SP, c->s, 0);      /* IRQ's TSX */

    if (last_op == 0x20 || last_op == 0x4C || last_op == 0x60) {
        for (size_t i = 0; i < N_ROUTINES; i++) {
            if (routines[i].addr == c->pc) {
                int again = 0;
                if (last_op == 0x60 && !routines[i].rts) break;
                for (int j = 0; j < depth; j++)
                    if (stk[j].r == &routines[i] && stk[j].entry_s == c->s) again = 1;
                if (again) break;
                uint16_t ret = (uint16_t)(R_ram[0x100 | ((c->s + 1) & 0xFF)] |
                                          (R_ram[0x100 | ((c->s + 2) & 0xFF)] << 8));
                open_check(&routines[i], 0, c, last_op == 0x20 ? (uint16_t)(ret - 2) : 0);
                break;
            }
        }
    }

    /* M5: checkpoint visits (after a check opening here, so the visit is its first event) */
    if (!in_irq && ck_map[c->pc]) {
        if (rti_now && visit_logged_at_irq) visit_logged_at_irq = 0;
        else log_visit(c->pc);
    }
}

int lk_report(void)
{
    unsigned long checked = 0, failed = 0, tainted = 0;
    int ok;
    unsigned long moved = 0, mismatch = 0;
    printf("LOCKSTEP  routine      addr   checked   passed   failed  tainted  (IRQs placed) (IRQ moved) (moved mismatch)\n");
    for (size_t i = 0; i <= N_ROUTINES; i++) {
        routine *r = i < N_ROUTINES ? &routines[i] : &irq_routine;
        printf("LOCKSTEP  %-11s $%04X %8lu %8lu %8lu %8lu  %8lu  %8lu  %8lu%s\n", r->name, r->addr, r->checked, r->passed,
               r->failed, r->tainted, r->irq_placed, r->irq_moved, r->moved_mismatch, r->checked ? "" : "   (not exercised)");
        checked += r->checked; failed += r->failed; tainted += r->tainted;
        moved += r->irq_moved; mismatch += r->moved_mismatch;
    }
    printf("LOCKSTEP: %lu calls compared, %lu failed, %lu tainted by an IRQ, I/O log overflows %lu, "
           "IRQ list overflows %lu, depth overflows %lu\n",
           checked, failed, tainted, io_overflow, place_overflow, depth_overflow);
    printf("LOCKSTEP: IRQ moved to entry/exit: %lu calls compared that way (a failure among them is in 'failed'); "
           "%lu tainted calls matched neither with a possible IRQ dependence (moved mismatch)\n", moved, mismatch);
    {
        unsigned long arr = 0, rp_ok = 0, rp_bad = 0;
        for (int i = 0; i < RST_KINDS; i++) { arr += rst_arrivals[i]; rp_ok += rst_region_passed[i]; rp_bad += rst_region_failed[i]; }
        printf("LOCKSTEP: RESET arrivals %lu (power-on %lu, JMP RESET %lu, watchdog %lu, script %lu); "
               "RESET regions %lu compared, %lu passed, %lu failed, %lu skipped\n",
               arr, rst_arrivals[RST_POWERON], rst_arrivals[RST_JMP], rst_arrivals[RST_WATCHDOG], rst_arrivals[RST_SCRIPT],
               pk_checked[PK_RESET], rp_ok, rp_bad, pk_skipped[PK_RESET]);
        printf("LOCKSTEP: runs ended by a RESET arrival: %lu routine checks, %lu MAINLN / %lu DIAG passes; "
               "%lu empty passes dropped (script reset at the loop head); C hw_reset %lu, hw_watchdog_hang %lu\n",
               checks_by_reset, pk_by_reset[PK_MAINLN], pk_by_reset[PK_DIAG], passes_cut_empty,
               c_reset_calls[RST_JMP], c_reset_calls[RST_WATCHDOG]);
        printf("LOCKSTEP: pass probe: boot pass %s; MAINLN passes %lu compared, %lu passed, %lu failed, %lu skipped; "
               "DIAG passes %lu compared, %lu passed, %lu failed, %lu skipped (%lu IRQ inside translated code, %lu other)\n",
               arr == 0 ? "not compared" : rp_bad ? "FAIL" : "PASS",
               pk_checked[PK_MAINLN], pk_passed[PK_MAINLN], pk_failed[PK_MAINLN], pk_skipped[PK_MAINLN],
               pk_checked[PK_DIAG], pk_passed[PK_DIAG], pk_failed[PK_DIAG], pk_skipped[PK_DIAG],
               skip_irq_translated, skip_other);
    }
    printf("LOCKSTEP: IRQ placement (M5): %lu IRQs anchored in compared passes; %lu calls compared with an anchored IRQ\n",
           pass_anch_irqs, check_anch_calls);
    print_hist("in skipped passes", hist_pass);
    print_hist("in routine checks", hist_check);
    print_hist("- first non-commuting cell ($FFFF: access list overflow)", hist_cell);
    if (tr_close() != 0) failed++;
    {
        /* M9 B3: every RESET arrival opened a region; all regions compared passed
         * (the last one may still be open at the end of the run) */
        unsigned long arr = 0, rp_ok = 0;
        for (int i = 0; i < RST_KINDS; i++) { arr += rst_arrivals[i]; rp_ok += rst_region_passed[i]; }
        ok = failed == 0 && checked > 0 && io_overflow == 0 && place_overflow == 0 &&
             pk_failed[PK_RESET] == 0 && pk_failed[PK_MAINLN] == 0 && pk_failed[PK_DIAG] == 0 &&
             pk_skipped[PK_RESET] == 0 && rp_ok >= 1 && rp_ok + (P.open && P.kind == PK_RESET ? 1u : 0u) == arr &&
             pk_passed[PK_MAINLN] + pk_passed[PK_DIAG] > 0;
    }
    printf("LOCKSTEP: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
