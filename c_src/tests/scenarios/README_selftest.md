# Self-test and EAROM scenarios (M9 batch B2)

`tests\refrun.exe` runs them; since batch B3 `tests\lockstep.exe` (same options, plus `--trace-out`)
verifies them too - RESET arrivals, RESET regions and diag passes - and their traces
are `tests\gate\selftest_boot_1800.trc`, `selftest_midrun_2500.trc`, `earom_2600.trc`,
`earom_readback_600.trc` (`earom_readback` with `--earom-in tests\earom\earom.nv`).

| scenario | run | what it covers |
|---|---|---|
| `selftest_boot.txt` | `--frames 1800` | TEST closed at power-on: RAM/ROM/POKEY/EAROM tests, EAZERO + BADEAR (blank EAROM), ROMREP switch test, all diag screens (SHATCH, SHYSTER, SINTEN, SCHEKR by slam, SIGANA, wrap), watchdog reboot, attract, power cycle into the self test with a good EAROM, second watchdog reboot, attract |
| `selftest_midrun.txt` | `--frames 2500` | coined game, TEST in attract: SYSTEM/DSPSYS, EAZHIS, EAZBOO, TEST off while erasing (SYSTEM waits, INIINI), TEST again, option 0 = JMP RESET with TEST closed, diag step, watchdog reboot, attract |
| `earom.txt` | `--frames 2600 --earom-out tests\earom\earom.nv` | high score + initials (WRHIIN), game end (WRBOOK), EAUPD idle, `reset` (power cycle), REHIIN reads it all back |
| `earom_readback.txt` | `--frames 600 --earom-in tests\earom\earom.nv` | power-on from that image: EABAD 0, same tables as `earom`'s post-reset boot |

Each file's header gives the ROM reasoning and the pass numbers the dumps confirmed.

## Script syntax (`--script FILE`)

One event per line, `PASS PORT [VALUE]`, `#` starts a comment, lines in non-decreasing PASS order.
A pass is one loop-head arrival of either kind (below); a line takes effect at the start of pass PASS,
before its first instruction.

- **PASS 0** = power-on: applied before `cpu_reset` (`0 test 1` holds the TEST switch at RESET).
- **Byte ports** (VALUE replaces the whole port; hex `0x..` or decimal):
  `in1` ($0C00 bits 0-5; b6 HALT and b7 3 kHz are synthesized), `inop0` ($0D00), `inop1` ($0E00),
  `allpot` ($60C8), `allpo2` ($60D8).  A whole-byte `in1` also sets the test/diag bits.
- **Bit ports** (VALUE 1 = pressed / closed / on, 0 = released; the polarity is handled for you):

  | port | register | bit | active |
  |---|---|---|---|
  | `coinr` `coinc` `coinl` | IN1 | b0 b1 b2 | low |
  | `slam` | IN1 | b3 | low |
  | `test` | IN1 | b4 (MTEST, self-test switch) | low |
  | `diag` | IN1 | b5 (MDITES, diagnostic step) | low |
  | `zap` `fire` `start1` `start2` | ALLPO2 | b3 b4 b5 b6 | high |
  | `cocktail` | ALLPOT | b4 (cabinet type) | high |

- **`spin N`**: from PASS on, the ALLPOT spinner nibble steps by N (signed byte) at the start of every
  pass, MAINLN or diag (not at PASS 0).  The diag loop runs ~90-140 passes per second against ~27 for
  MAINLN, so the same N turns faster there.
- **`PASS reset`** (no VALUE, PASS >= 1): pulse the RESET line after that pass's dump and its other
  lines - same effect as a watchdog bite.  **`PASS end`**: stop the run there (verdict relaxed to the
  hardware checks).

## Options added to refrun

- `--diag-pc HEX` (default `DA8D`, `0` = off): second loop head, the self test's main diag loop.
  Both loop heads count as passes (`--frames`), apply the script and are dumped.  `frame_sched.txt`
  lines of diag passes carry a fourth column `D` (MAINLN lines are unchanged: `frame irq_count cycle`);
  `ref_index.json` frames carry `"loop": "M"` / `"D"`.  A run that never reaches $DA8D numbers its
  passes exactly as before.
- `--watchdog-cycles N` / `--watchdog-irqs N` (N x 6144): hardware watchdog timeout, default
  1,134,000 cycles (0.75 s, AAE's 4 Hz timer reset on its third expiry; MAME's unconfigured default
  is 3 s; the board's counter is not documented in this repo).  Only WDGTST ($DAF7) should bite; a bite
  elsewhere fails the run.  The value sets the RANDOM / IRQ phase after a reboot and is recorded in
  `ref_index.json` (`model.watchdog_cycles`).
- `--earom-in FILE` / `--earom-out FILE`: 64-byte ER2055 image (blank chip = all $FF when no image).

RESET model (bite or `reset`): `cpu_reset` (I set, S=$FD, PC=$D93F), AVG stopped; RAM, POKEYs, Mathbox,
EAROM, OUT0/OUTANK, the IRQ clock and a pending IRQ keep their state.

## Summary lines and verdict

Runs that reach the diag loop, reset after power-on, bite, or use an EAROM file print extra lines:
passes per loop kind, RESET arrivals by cause (power-on, JMP RESET, watchdog, script), watchdog bites
and the longest kick gap in cycles, the self-test result cells ($78-$88) **as of the last diag pass**,
Mathbox runaways in SIGANA's start scan, and the EAROM image.  Other runs print exactly what they
printed before M9.

Verdict additions: no watchdog bite outside $DAF7; Mathbox runaways allowed only inside SIGANA's
32-start scan (entry $1F does not reach STALL within mathbox.c's step cap; on the board it just runs
until the next start and nothing reads it); a run with diag passes must end its last diag pass with
CHKSMS x 12, MBCOND, RAMCND, PK1CND, PK2CND and EARCND all 0.

## Findings while writing them

- DSPSYS option = (CURSL1 & 6) / 2: options 0 **and** 1 are JMP RESET (FIRE+ZAP), 2 is EAZBOO
  (FIRE+START1), 3 is EAZHIS (FIRE+START2) - `DEX DEX` then BPL/BNE at $D85D.
- `earom`: SECOUL ($0406, game-up seconds) keeps counting in RAM after WRBOOK, so the post-reset
  BOOKKS equal the EAROM image, not RAM just before the reset; EABAD stays 4 in RAM until the next read.
- With a blank EAROM the power-on self test reports EARCND until EAZERO finishes (diag pass ~281).
- Diag passes take ~10,750 cycles (BADEAR) to ~16,900 (ROMREP) on this model, not a flat 10,752.
