# Tempest: what the C port proved, and what it found

Written 2026-09-16, at the end of milestone M9 (all eleven modules translated,
Gates V, 1, N, P, S and E passed). The durable record: the claim, the evidence,
and what the port turned up that the disassembly alone could not.

Companion docs: `README.md` (build, run, verify) and `tests\scenarios\*.txt`
(the ROM reasoning behind every input script).

---

## 1. The claim, and the evidence for it

**The C program does what the ROM does, call by call and pass by pass.** Not
"the listing reassembles to the right bytes" (the disassembly already proved
that) but: each of the 299 translated routines, run from the machine state the
real ROM had at its entry, leaves the same machine state as the ROM at its
return.

```
12 lockstep runs (attract x2, coin_start, high_score, two_player, fuseball_pulsar,
superzapper, slam, selftest_boot, selftest_midrun, earom, earom_readback)
  8,823,670 routine calls compared     0 failed, 0 tainted, 0 IRQ moved
     30,586 MAINLN / diag passes       0 failed, 0 skipped
         18 RESET regions               all passed (power-on, watchdog, JMP RESET, script)
    175,062 IRQs placed inside passes   every one with a commutation proof
12 gate.exe traces                      every pass byte-identical, C modules alone
Gate V 168/168 shapes, Gate S 69/69 checks, Gate E 29/29 checks, tempest_selftest 37/37
```

What a "compared call" means: lockstep snapshots RAM, vector RAM,
colour RAM and the D flag at the ROM's JSR, lets the ROM run to the RTS while
logging every hardware access (POKEY, Mathbox, EAROM, IN1, watchdog, AVG), then
runs the C function from the snapshot with those reads replayed and compares
all RAM (minus the stack bytes the call pushed), vector RAM, colour RAM, the
write sequence (address and value), D, and the documented exit registers.
Checks nest, so a routine is checked both on its own and inside every caller.
The pass probe does the same for whole MAINLN passes and diag-loop passes, and
`gate.exe` does it for entire runs with no 6502 linked at all - the C state is
never resynchronised from the ROM, so an error anywhere persists and shows.

**The mutation tests prove the net bites.** Each was caught immediately, at the
exact byte, by lockstep and (from M6 on) by the gate, then reverted: `VGADD +2`,
`SPARE3 +2` (M1); `EABC+2`, `QNXTSTA=CNWLF2+2` (M3); `SECUVY-2`,
`TIMHIS=ITIMHI+2` (M4); `EYLDES+2`, `QT6+2` (M5); `CHACOU+2` (M6); `INVAC2 +1`,
`SUZTIM = 1` (M7); the SHATCH picture pointer, one decimal ADC in DBOOKE made
binary, a RAM-test cell left non-zero, TOUT0 at RESET (M9).

### Why this is stronger than byte verification
A byte-exact reassembly says nothing about whether a label's comment, a table's
meaning or a register convention is right. Here the C is re-derived from the
annotated listing and has to reproduce the machine: every scratch store, every
register a caller relies on, every RANDOM read in the ROM's order (the POKEY
poly counter shifts every CPU cycle, so a read out of order returns a
different number and the protection cells change).

### What is NOT proven
- **Bad-hardware paths** (HIBAD, BRAMREP, HIRBAD, HIRBD2, JMPHIB, BADBOX /
  TOOSLO, the bad-ROM report and VG-ROM tone, the PK1CND/PK2CND stores) are
  translated from the listing and proven *not executed* (Gate S), not compared:
  they need fault injection (batch B6, not done).
- **The protection branches' taken sides** (QT1-QT6, §3) are unreachable on
  genuine hardware; only their not-taken sides are verified, with every side
  condition met (score above 180,000, late waves).
- **Decimal-mode N/V flags**: DBOOKE is the only genuine decimal ADC chain and
  consumes only C; `adc6502/sbc6502`'s decimal N/V follow the shared core but
  nothing checks them.
- **Routines with no direct check**: DBOLOU and EXCESS (branch-only entries;
  their code runs inside every DSPCRD / DSPNYM check), OUTCUR and DIGITS (no
  caller in rev 3).
- **The native seam's timing** is a model: IRQs are serviced in MAINLN's frame
  wait, not mid-pass, so native play diverges from the oracle after pass 1 by
  design (the RANDOM phase, switch timing). Pass 1 matches the oracle except
  SPARE3 (§2). Cocktail player 2's flipped picture has not been seen on screen.

---

## 2. What the verifier caught

The milestone records keep each batch's verified end state rather than every
failure fixed on the way (translation slips were fixed inside a batch before it
closed). The catches that changed the port, its harness or its plans:

| catch | effect | how it surfaced |
|---|---|---|
| **IRQ-placement window hole** (M9 B3): the commutation window of an anchored IRQ restarted at the previous IRQ's RTI | a second IRQ that increments SECOUL ($0406) was anchored before DBOOKE's reads of that cell, i.e. at a place where it does not commute; the proof accepted a wrong placement | 3 DBOOKE failures in `selftest_midrun` (bookkeeping digits in vector RAM). Fix: a window runs from the last real event across intervening IRQs; the old scenarios' numbers did not change; checkpoint $DDAB places the three passes |
| the M9 survey's DSPSYS option mapping (1 = EAZBOO, 2/3 = EAZHIS) | scenarios would have pressed the wrong option | refrun dumps: option = (CURSL1 & 6) / 2, and `DEX DEX / BPL` makes options 0 **and** 1 JMP RESET, 2 EAZBOO, 3 EAZHIS |
| the M7 plan's "score >= 16000" protection target | a scenario aimed at 16,000 would never reach ZQPONS | reading `CPX #$15` on the BCD high byte: 150,000 (the source comment is wrong, §3) |
| sound starters vs MODSND (M4-M5 "moved mismatch") | 7 calls could not be compared: genuine order dependence on where the IRQ lands | resolved by checkpoints after each FSNDON channel store (M6): 0 moved mismatch since |
| unplaceable IRQs in DISPLAY / EXSTAT / PLAY (M4-M6) | 43 skipped passes in attract, 1,047 after ALWELG batch 2 | the report's `unplaceable IRQs` histogram (PC + first non-commuting cell) -> 47 checkpoints -> 0 skipped |
| SPARE3 ($0133) on the native boot pass (M9 B4) | native 09, ROM 08: one more IRQ sees the boot list halted, because native IRQs run in the frame wait | Gate E2's boot-pass compare; the M8 self-test check exempts the whole stack page, where it hid. Timing model, not translation; exempted by name and reported |
| picture orientation (M8) | the first window build was rotated and mirrored | screenshots (`--shot`): avg.c space with TOUT0 = $10 already is the upright picture |
| pass-cost under-charge (M8) | the game ran slightly fast | measured IRQs per pass against the oracle per state: a linear fit loses passes that cross the 9-IRQ gate; residual-spread correction, 9.21 vs 9.214 |

---

## 3. Findings about the ROM (rev 3)

**Copy protection: six checks, none can fire on genuine hardware.** All six cells
are cleared by RESET's RAM clear and set only by an altered ROM or a POKEY that
does not behave like the real chip:

| cell | set by | effect when set |
|---|---|---|
| QT1 $B5 | ZATLIV $AEE3: sum over the copyright literal $D575-$D585 | ZQATLI $B557 (WAVEN1 >= 10): FRTIMR $7A, watchdog reset |
| QT2 $016C | ZATC4V $A91C: $A7 xor ROM $AACE-$AAD8 | NONSTA $C8F5 `SED` at CURWAV >= $14 (decimal mode into DISPLAY) |
| QT3 $0455 / QT6 $011B | ZATVG2 $B1DA / ZATVG1 $B27D: vector RAM of the copyright list | ZQVAVG $A581: `INC $00,X` mangles zero page once LSCORH >= $18 |
| QT4 $0720 | INISOU / ZPOKST $CDB2: RANDOM changing while SKCTL = 0 | ZQPOKS $B7D6 (CURWAV >= 13): `STA $01FF` |
| QT5 $011F | ZPONTS $AE1F: two RANDOM reads 4 cycles apart need hi(r1) = lo(r2) | ZQPONS $C5B1: `INC $0200,X` once LSCORH >= $15 |

The `fuseball_pulsar` scenario plays to wave 24 and 194,152 points with all six
cells 0 on every pass.

**Wrong comments in the commented source.** ZQPONS's `CPX #$15` is commented
"16,000"; on the BCD high byte it is **150,000**. ZQVAVG's `LDA #$17 / CMP LSCORH /
BCS` is commented "170,000"; it fires from **180,000**. ALTES2's "IT IS
IMPERATIVE THAT GETOP3 BE AT OFFSET 3FF" does not hold in rev 3 (GETOP3 is at
$DBE0, offset $3E0; probably an older ROM split). BADBOX's "YES" / "NO" comments
are on the wrong branches.

**MOVNYM `LDA NEOFLI / ORA NEOFLI`** ($98E0): ORs a cell with itself; almost
certainly meant `OLOFLI`. Reproduced, not "fixed".

**DIGITS has no caller** in rev 3: it is reached only by falling through from
DIGTYS's `SEC`.

**DSPSYS options.** Option = (CURSL1 & 6) / 2; FIRE+ZAP on option 0 or 1 is
`JMP RESET` with the TEST switch still closed (the power-on self test over live
RAM), FIRE+START1 on 2 is EAZBOO (zero times), FIRE+START2 on 3 is EAZHIS plus
`EABAD |= 3`. The self test is left only through the hardware watchdog (WDGTST
$DAF7 spins without a kick). The slam switch steps the diagnostic screens exactly
like the diag-step switch.

**DBOOKE's missing CLC.** The hex-to-BCD conversion's first `ROL PXL` takes the
carry VGJSRL / VGVTR1 leave (VGADD's ADC on the list pointer). The stray bit is
rotated 24 times and ends in PXL+2's top bit, so it never reaches the displayed
number - but it is in RAM, so the port reproduces it (`VGADD_CARRY`). DBOOKE also
reuses `LDA #BOOKKS/100` (= 4) as its count of five numbers (the source's
`LDA I,4` is commented out), and READMB's `STY NGAVGZ` exists only for that
fifth number.

**All twelve self-test ROM checksums of rev 3 are $00** (seed = ROM index, EOR over
each 2K of $3000-$3FFF and $9000-$DFFF; balance byte CHKSMB = $73), so the
bad-ROM report never runs on a genuine board. Also: POTYTA and POTXTA overlap
(POTXTA = POTYTA + 4); SHYSTER reads SNDTBL-1 (the byte before the table) with
X = 0.

**Attract never sets VG HALT**: the master lists end in `JMPL VECRAM`, so the AVG
loops and the IRQ's SPARE3 restart path runs only after a HALT-terminated list
(boot, SWHALT, the self test).

**EAROM.** Three groups (ALEARO's TEAX/TEACNT/TEASRL): top-3 initials, top-3
scores plus two game-option bytes, twelve bookkeeping bytes; checksum = sum mod
256. A blank chip ($FF) fails every checksum: the game re-induces 010101 and the
ROM initials; the power-on self test reports EARCND and runs EAZERO until the
erase finishes (diag pass ~281). SECOUL ($0406, game-up seconds) keeps counting
in RAM after WRBOOK, so a power-cycled boot equals the chip image, not RAM
before the reset. In SYSTEM, DSPSYS re-requests an erase while the buttons are
held, so erases pressed close together overlap.

---

## 4. Hardware and timing findings

- **27 Hz game loop.** MAINLN runs a pass when FRTIMR >= 9 (`CMP #$09` at LC7A9):
  246.09 / 9 = 27.3 Hz at most; the ROM on the oracle averages **9.214 IRQs per
  attract pass**, the native build 9.21 (30 s, machine/wall 1.0000). AAE's 240 Hz
  IRQ (`fps_lock=240`) is 2.5 % slow.
- **IRQ 246.09 Hz** = 12.096 MHz / 4096 / 12 = one per 6144 CPU cycles at 1.512 MHz.
- **RANDOM is a per-cycle poly counter.** The protection pair at $AE1F reads it 4
  CPU cycles apart and needs hi(r1) = lo(r2); INISOU's reads after SKCTL = 0 need
  >= 9 clocks of the held $FF. The native seam charges 4 cycles per read and 8 per
  write, which reproduces both; the built-in probe shows a read cost of 5 sets QT5
  in 5,908 of 6,000 start phases and a write cost of 4 sets QT4 in 4,490.
- **Hardware watchdog.** Nothing in the ROM or `tempest.cpp` fixes the timeout.
  The model uses 1,134,000 cycles (0.75 s: AAE `cpu_control.cpp`'s 4 Hz timer,
  reset on its third expiry; MAME's unconfigured default is 3 s). Every legitimate
  gap between kicks is <= 25,130 cycles, so any value above that gives the same
  ROM path; the value sets the RANDOM / IRQ phase after a reboot and is recorded in
  `ref_index.json` and the trace header.
- **Self-test timing** on this model: the power-on RAM/ROM/POKEY/EAROM test takes
  4,588,221 cycles (~3 s, IRQ masked); diag passes take ~10,750 (BADEAR) to
  ~16,900 cycles (ROMREP) - 90 to 140 per second, driven by the 3 kHz clock busy
  waits and the VG HALT line.
- **Mathbox divide** (READMB, used by ROMREP and DBOOKE) and SIGANA's 32 arbitrary
  microcode starts are exercised for the first time by the self test; mathbox.c
  runs them from the real PROMs (entry $1F does not reach STALL within the step
  cap - harmless, nothing reads it).
- **Orientation**: avg.c's space (+x right, +y up) with the normal TOUT0 = $10 is
  the cabinet's upright picture; COCFLI writes $08 for cocktail player 2 (turned
  180 degrees).

---

## 5. Verification techniques worth reusing

- **An oracle with the board around the CPU** (`refrun`): IRQ grid, HALT from
  walking the list, per-cycle RANDOM, microcode Mathbox, EAROM, watchdog, scripted
  switches - deterministic, dumpable, instrumentable.
- **Call-level lockstep with replayed I/O** instead of frame diffs: a failure names
  the routine, the byte and the I/O event; nested checks localise it further.
- **IRQ anchoring with a commutation proof**: an IRQ inside translated code fires
  in C after the last logged event before it, accepted only when the ROM's memory
  accesses in between provably commute with the IRQ's; checkpoints (`CK()`) where
  they do not. No masking, no "tainted" leftovers.
- **Trace-replay gates**: when the native seam cannot reproduce cycle-level inputs,
  record them once from the oracle and replay them to the C modules alone.
- **RESET as an event**: checks and passes finished by a RESET arrival, a RESET
  region from the arrival to the next loop head, and a second loop kind for the
  self test.
- **Generated everything**: state names, ROM images, tables and label addresses,
  each generator checking dump == listing == symbols.
- **Runnable gates** (`tools\gate_s.py`, `gate_e.py`): PASS/FAIL checkers that run
  the tools themselves and check that the stored traces and images are current.
- **Mutation tests** after every batch, caught by both lockstep and the gate.

---

## 6. Process findings

- **Agents in batches, one owner per file set.** Surveys first (read-only), then
  translation batches with a hard exit criterion (0 failed, 0 skipped), then
  review agents comparing code with the listing statement by statement (M6: no bugs).
- **Stopped agents are re-verified from artifacts, never from their notes.** M6
  batch 2 and M7 were stopped mid-work by the user; the main session rebuilt and
  re-ran everything before recording their state.
- **Surveys are wrong in details** (the DSPSYS option mapping, the "16000" target):
  confirm on oracle dumps before writing a scenario, and write the reasoning into
  the scenario's header.
- **Parallel regression** (all lockstep runs, all gate traces, refrun, the
  self-test, Gates S and E) keeps a full check to minutes; logs go to scratch.
- **Live checks by the user complement the headless ones**: the user ran
  `tempest_win.exe` on 2026-09-16 and confirmed the F2 TEST switch and the
  self-test screens; F1 / F3 are verified through `--hold` and the headless
  self-test only.

---

## 7. State of play

**It plays.** `tempest_win.exe` boots through the ROM's RESET, runs attract, takes
coins, plays with the spinner, fire and superzapper at the board's 246.09 Hz IRQ
rate, keeps high scores in `tempest.nv`, and runs the ROM's own operator options
and self test on F2 (user-verified live, 2026-09-16). Every module is translated
and verified; Gates V, 1, N, P, S and E pass.

Open:
1. Per-vsync redraw of the display list (the AVG redraws the list continuously;
   the host shows the newest ~27 Hz frame) - discussed with the user, deferred.
2. Boots run on the synthetic clock: the self test's ~3 s dark RAM/ROM test and the
   0.56 s power-on delay take no wall time; the TEST-open boot is ~77k cycles short
   of the oracle (RAM clear not charged).
3. Cocktail player 2's flip not verified on screen; `[dips]` other than the
   defaults not checked on screen; F1 / F3 not pressed on a live window.
4. Fault injection (B6) for the bad-hardware paths; tamper tests for the
   protection branches' taken sides.
5. Native IRQs are not placed mid-pass (SPARE3 on the boot pass; divergence after
   pass 1 by design).
6. Not exercised: an IRQ taken at $D93F after JMP RESET with I clear, a RESET region
   ended by another RESET, the software-watchdog `JMP RESET` at $D714.
7. A statement-by-statement review of `altes2.c` against the listing (planned for
   B7) has not been run.
