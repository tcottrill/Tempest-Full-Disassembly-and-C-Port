# Tempest — C port

A faithful C11 translation of the game program in
[`../disasm/tempest_program_rom.asm`](../disasm/tempest_program_rom.asm)
(Atari Tempest, 1981, program ROM revision 3).

**Every routine is translated, and it plays in a window with sound, the
spinner and persistent high scores.** All eleven original modules are in —
ALEXEC's mainline and state machine, ALWELG's wells, enemies and player,
ALDIS2's display and the Mathbox transforms, ALSCO2's scores, messages and
high-score entry, ALLANG's four languages, ALSOUN's sound scripts, COIN65's
coin logic, ALHAR2's 246 Hz interrupt, ALEARO's EAROM, ALVGUT's vector
utilities and ALTES2's RESET, operator options and self test — one C
function per Atari label (299 routine headers), each with its ROM address
and a `Lxxxx` comment per statement. It is not an emulator: there is no 6502
core at runtime.

The display pipeline is the real one. ALVGUT and ALDIS2 write actual AVG
words into a modelled 4 KB vector RAM through the zero-page list pointer
`VGLIST`, and `avg.c` — a C transcription of the AVG state machine — walks
that list and the vector ROM it calls into and emits coloured line segments.
The Mathbox is `mathbox.c`, which executes the real bit-slice microcode PROMs
(136002-126..132). Two real POKEYs (`c012294.c`) stand behind RANDOM, the
spinner and buttons on their pot lines and all of the game's audio. Behind
`$6000`/`$6040`/`$6050` sits a real ER2055 model (`er2055.c`), persisted as
`tempest.nv` beside the exe. The self test is the ROM's own, on F2.

How the listing, the names and the comments were recovered is in
[`../disasm/`](../disasm/); what the port proved and found is in
[`FINDINGS.md`](FINDINGS.md).

## Building and running

```bat
build_win.bat
tempest_win.exe
```

Needs Visual Studio 2022 (the scripts call its developer command prompt) and
nothing else. `build_win.bat` builds `tempest_win.exe` into `c_src` (objects in
`obj\win\`) and prints `WIN BUILD OK`; the game code and the seam compile at
`/W4 /std:c11` warning-free, the vendored framework and chip models at `/W3`.

### Controls

| action | keys |
|---|---|
| spinner | mouse X; Left / Right arrows; joystick X |
| fire | Left Ctrl, Space, left mouse button, joystick button 1 |
| superzapper | Left Alt, right mouse button, joystick button 2 |
| start 1 / 2 | 1 / 2 (joystick button 8 = start 1) |
| coins | 5 left, 6 centre, 7 right mech (joystick button 7 = left) |
| TEST switch | **F2**, a latched toggle like the board's: in the game it opens SYSTEM, the options and bookkeeping screen; held at power-on (`--test` or `test_switch=1`) it gives the ROM's power-on self test; switched off inside the self test, the watchdog reboots into the game (~0.75 s) |
| diagnostic step | **F1** (hold ~3 diag passes = one screen: ROMREP switch test, cross hatch, sound, intensity bars, checker board, signature analysis) |
| slam | **F3** (the tilt switch; also steps the self-test screens, as on the board) |
| spinner tuning | `[` / `]` mouse sensitivity -/+ 10 %, Shift+`[` / Shift+`]` key spin rate -/+ 10 % (live, shown in the title bar, saved on exit) |
| other | F8 mouse capture on/off, Alt+Enter fullscreen, Esc quit |

In SYSTEM (TEST on during the game) the spinner moves the cursor; FIRE+ZAP
starts the self test (the ROM's `JMP RESET` with the switch still closed),
FIRE+START1 zeroes the bookkeeping times, FIRE+START2 zeroes the high scores.
The user checked F2 and the self-test screens live on 2026-09-16; F1 and F3
are verified through `--hold` and the headless self-test.

A positive spinner count (the ALLPOT counter counting up) turns the claw
counter-clockwise; mouse / key right turns it clockwise (MAME's reversed
dial; `invert_spin=1` swaps it).

### `tempest_win.ini`

Beside the exe; every key is commented in the file, read at start and written
back with the value that took effect.

| section | keys |
|---|---|
| `[dips]` | the option switches by MAME/AAE name: N13 `coinage`, `right_coin`, `left_coin`, `bonus_coins`; L12 `minimum_credits`, `language`, `bonus_life`, `lives`; POKEY 2 pot lines `difficulty`, `rating`; POKEY 1 pot lines `cabinet` (upright / cocktail), `special_option`. Defaults = all switches off = the oracle's settings |
| `[main]` | `vsync` (0 default: each ~27 Hz frame swapped when the game finishes it; 1: only at a refresh that cannot delay the next IRQ), `refresh_hz`, `fps_lock` (0 = the board's 246.09 Hz IRQ; 240 = AAE's rate, 2.5 % slower, game speed and sound scale together), `test_switch` (TEST at power-on), `watchdog_cycles` (hardware watchdog timeout, 1134000 = 0.75 s) |
| `[vector]` | `phosphor_ms`, `linewidth`, `gain`, `line_smoothing`, `corner_strength`, `fire_point_size` (unused) |
| `[video]` | `rotate_sign` (1 upright, -1 turned 180 degrees), `honour_flip` (apply the ROM's OUT0 invert bits, cocktail player 2) |
| `[input]` | `mouse_sensitivity` (0.5), `key_spin_rate` (180/s), `joy_spin_rate`, `invert_spin`, `mouse_capture` |
| `[joystick]` | `deadzone` |
| `[sound]` | `pokey_volume` (0..100), `latency_blocks` (1..8, ~4 ms each) |

A missing `tempest.nv` is a blank chip ($FF): the ROM re-induces the 010101
high-score table, exactly as on a new board.

### Command line (`tempest_win.exe`)

| option | |
|---|---|
| `--test` | TEST switch closed at power-on (the self test) |
| `--hidden` | never show the window: real clock, audio at volume 0, no NVRAM unless `--nvram` |
| `--nvram FILE` / `--no-nvram` | use FILE instead of `tempest.nv` / no EAROM persistence |
| `--quit-after-ms N` | timed run; pacing numbers in `tempest_win.log` |
| `--shot PASS FILE` (repeatable), `--shot-size WxH` | hidden window on a turbo clock: renders the frame of that pass through the real GL beam renderer into `.png` / `.bmp`, exits after the last shot |
| `--hold SWITCH FROM TO` | hold `test`, `diag`, `slam`, `fire`, `zap`, `start1`, `start2`, `coinl`, `coinc`, `coinr` over passes FROM..TO-1 (0 = for ever; pass 0 = power-on) |
| `--spin N FROM TO` | spinner N counts per pass over those passes |
| `--autoplay [--autoplay-spin N]` | scripted game: coin, START, rating, autofire + spin, initials, re-coin |
| `--pass-log FILE N` | per pass `qstate qdstate irqs io_r io_w ops work` (pass-cost model data) |

Only use `--shot` / `--hidden` from scripts; a plain run opens the window.

## Pacing

The ROM's game logic runs a MAINLN pass when its IRQ counter reaches 9
(`CMP #$09` at `LC7A9`): **about 27 passes per second** at the board's 246.09
Hz IRQ (12.096 MHz / 4096 / 12). The seam keeps one cycle timeline (1.512 MHz,
both POKEYs clocked to it) and services IRQs on a fixed 6144-cycle grid in
MAINLN's frame wait, while a pass-cost model fitted on 19,994 oracle passes
charges each pass the CPU time the 6502 would have spent
(`tools\fit_passcost.py`). Measured over 30 s: 246.09 IRQ/s, attract 9.21 IRQs per pass
(the ROM on the oracle: 9.214), machine/wall 1.0000, audio never starved. The
self test runs with the IRQ masked; its diagnostic loop is paced on machine
time (~90-140 passes per second, within 0.06 % of the oracle). The host shows
the newest frame; a per-vsync redraw of the display list was discussed and
left for later.

## Verification

Three instruments, all headless, all run from `c_src`:

- **`tests\refrun.exe` — the oracle.** The real rev-3 ROM (compiled in from
  `progrom.c`, so no ROM files are needed) on a plain NMOS 6502 interpreter
  (`tests\ref6502`), with the board modelled around it:
  IRQ every 6144 cycles, the AVG HALT line from walking the list, POKEY RANDOM
  as a per-cycle poly counter, the Mathbox microcode, the ER2055, the 3 kHz
  clock, the hardware watchdog. Scripted inputs (`--script`,
  `tests\scenarios\README_selftest.md`), dumps of RAM / vector RAM / colour RAM
  at every MAINLN or diag-loop pass.
- **`tests\lockstep.exe` — call-by-call verification.** The same run; at every
  entry into a translated routine (and every IRQ, RESET arrival, MAINLN pass
  and diag-loop pass) it snapshots the machine, and at the return runs the C
  function from the snapshot with the ROM's hardware I/O replayed, comparing
  all RAM, vector RAM, colour RAM, the I/O sequence, the D flag and the
  documented return registers. IRQs that land inside translated code are
  placed at the exact C statement with a commutation proof.
- **`tests\gate.exe` — the C modules alone.** Replays a trace recorded by
  `lockstep --trace-out` (hardware reads, IRQ markers, RESET causes) with no
  6502 and no oracle linked, on its own state from RESET, and requires every
  pass byte-identical to the ROM's.

Everything the gates compare against — the oracle dumps (`.ram`, `.vram`,
`.col`), the lockstep traces, the EAROM image — is recorded from the running
ROM and is **not distributed**. `record_refs.bat` records all of it again
(about a minute, about 55 MB), and recording is deterministic: the same bytes
every time. It records the traces by running the 12 lockstep runs below, and
stops with `RECORD FAILED` if any of them does not end `LOCKSTEP: PASS`, so
`RECORD OK` is the call-by-call verification as well. Gate V's reference and Gates S / E also read `..\disasm\build`,
which `..\disasm\gen_from_roms.py` creates from a ROM set and Atari's source
archive; the builds, the oracle, lockstep, `gate.exe` and
`tempest_selftest.exe` need neither.

```bat
build_all.bat
record_refs.bat
tests\refrun.exe --frames 600
tests\lockstep.exe --frames 5000 --no-dumps
tests\lockstep.exe --frames 3000 --no-dumps --script tests\scenarios\coin_start.txt
   ... (the 12 runs below)
tests\gate.exe tests\gate\attract_5000.trc --ref tests\ref
tests\gate.exe tests\gate\coin_start_3000.trc          (every tests\gate\*.trc: 12 traces)
tests\tempest_selftest.exe
tests\avgshapes.exe
python tools\gate_s.py
python tools\gate_e.py
```

`build_all.bat` prints `ALL BUILDS OK` (`/W4` clean). Every lockstep run ends
`LOCKSTEP: PASS` and `REFRUN: PASS`; in all of them **0 failed, 0 tainted, 0 IRQ
moved, 0 moved mismatch, 0 skipped passes**, every RESET region passed:

| scenario (`tests\scenarios\`) | passes | calls compared | RESET regions | MAINLN / diag passes compared |
|---|---|---|---|---|
| attract (no script) | 5000 | 1,564,454 | 1 | 4,999 / 0 |
| attract | 600 | 222,026 | 1 | 599 / 0 |
| `coin_start.txt` | 3000 | 915,281 | 1 | 2,999 / 0 |
| `high_score.txt` | 3000 | 889,466 | 1 | 2,999 / 0 |
| `two_player.txt` | 3000 | 849,858 | 1 | 2,999 / 0 |
| `fuseball_pulsar.txt` | 4000 | 1,159,389 | 1 | 3,999 / 0 |
| `superzapper.txt` (Gate P) | 3000 | 868,887 | 1 | 2,999 / 0 |
| `slam.txt` | 1500 | 448,719 | 1 | 1,499 / 0 |
| `selftest_boot.txt` | 1800 | 167,338 | 4 (power-on, 2 watchdog, script) | 348 / 1,450 |
| `selftest_midrun.txt` | 2500 | 708,398 | 3 (power-on, JMP RESET, watchdog) | 2,319 / 180 |
| `earom.txt` (`--earom-out`) | 2600 | 808,812 | 2 (power-on, script) | 2,598 / 0 |
| `earom_readback.txt` (`--earom-in tests\earom\earom.nv`) | 600 | 221,042 | 1 | 599 / 0 |

`gate.exe` passes all 12 traces with every pass byte-identical (and, with
`--ref tests\ref`, the trace images equal the 34 attract dumps).

The gates:

| gate | what | evidence |
|---|---|---|
| V | `avg.c` draws every vector-ROM shape as the reference AVG | `tests\avgshapes.exe`: 168 of 168 shapes, `GATE V: PASS` |
| 1 / N | first and 600 attract passes byte-identical from the C side | `gate.exe` on `attract_600` / `attract_5000` |
| P | a scripted coined game | `superzapper` lockstep + gate PASS |
| S | self test | `python tools\gate_s.py` — 69 checks, `GATE S: PASS` |
| E | EAROM round trip | `python tools\gate_e.py` — 29 checks, `GATE E: PASS` |

**Gate S** runs lockstep, refrun and gate on `selftest_boot` and
`selftest_midrun` plus `tempest_selftest`, and checks: 0 failed / tainted /
skipped with every RESET arrival compared as a region and every MAINLN and
diag pass compared; a coverage table of all 22 ALTES2 routine headers (every
reachable one with checked calls, RESET through its regions, and the
bad-hardware paths HIBAD / BRAMREP / HIRBAD / HIRBD2 / JMPHIB / BADBOX, the
VG-ROM tone, the PK1CND/PK2CND stores and the bad-ROM report proven never
executed, with the self test's result cells showing why); gate PASS on both
traces; refrun's 12 ROM checksums all 0, all condition cells 0 and exactly one
watchdog bite per TEST switch-off in the diag loop; the 15 native self-test
checks.
**Gate E**: E1 — the power-cycled boot in `earom` reads EABAD 0 and the
initials, scores and bookkeeping cells equal to the chip image (the image, not
RAM before the reset: the game-up timer `$0406` keeps counting); E2 — an image
written by the ROM (refrun) and one written by the native build
(`tempest_selftest --earom-write`) are each read by both sides with identical
boot passes, and neither side rewrites them; E3 — EAZERO on a blank chip,
EAZHIS and EAZBOO each leave the expected image, and INIINI re-induces the
defaults. Both write their logs and dumps under `obj\gate_s` / `obj\gate_e`
(`--work DIR` to change).

**`tests\tempest_selftest.exe`** (37 checks, `SELFTEST: PASS`) drives the
native seam — the one `tempest_win.exe` runs — headless on a synthetic clock:
pass 1 byte-identical to the oracle's dump, attract, the POKEY protection
probe, a scripted game to initials, audio rate lock, EAROM persistence across
sessions, watchdog and real-clock pacing, and the self test: the power-on test
lands on the oracle's cycle, 83 diag frames equal the oracle's dumps, the
screens step as on the oracle, SYSTEM options, EAZHIS, JMP RESET, watchdog
exits.

**Regenerating the evidence.** `record_refs.bat` runs all of the following.
`tests\ref` (34 attract dumps):
`tests\refrun.exe --frames 600`. `tests\ref_selftest`: `tests\refrun.exe
--frames 1361 --capture-every 20 --outdir tests\ref_selftest --script
tests\scenarios\selftest_boot.txt`. A trace: the lockstep command plus
`--trace-out tests\gate\NAME.trc` (a trace records the ROM, so only an oracle
model or scenario change makes it stale; Gate S / E fail if the stored traces
or `tests\earom\earom.nv` differ from a fresh run). `earom.nv`: the `earom`
run with `--earom-out tests\earom\earom.nv`. Regenerating twice gives identical
bytes.

## What is here

| file | |
|---|---|
| `state.h`, `state.c` | the machine state `machine_state g`: `ram[0x800]` ($0000-$07FF), `vram[0x1000]` ($2000-$2FFF), `colram[16]`, D flag, loop kind; `cpu_rd` / `cpu_wr` for computed addresses |
| `state_defs.h` | **generated** (`tools\gen_state.py` from `../disasm/tempest_defines.asm`): every named cell as `A_NAME` address + `NAME` lvalue, `K_NAME` constants |
| `hw.h` | the hardware seam the game code calls: IN1 / option switches, POKEY pots and RANDOM, POKEY writes, Mathbox, EAROM, OUT0/OUTANK, colour, VGSTART/VGSTOP, watchdog, frame wait, `hw_reset` / `hw_watchdog_hang` |
| `game.h` | prototypes of every translated routine; `CK()` checkpoints (no-ops outside lockstep) |
| `alexec.c` `alwelg.c` `aldis2.c` `alsco2.c` `allang.c` `alsoun.c` `alcoin.c` `alhar2.c` `alearo.c` `alvgut.c` `altes2.c` | the game, one file per Atari module (ALEXEC, ALWELG, ALDIS2, ALSCO2, ALLANG, ALSOUN, COIN65, ALHAR2, ALEARO, ALVGUT, ALTES2) |
| `allang_data.c/.h`, `aldis2_data.h`, `alwelg_data.h`, `altes2_data.h` | **generated** tables and label addresses (`tools\gen_allang.py`, `gen_aldis2.py`, `gen_alwelg.py`, `gen_altes2.py`), each generator checking dump == listing == `symbols.json` |
| `progrom.c/.h`, `vecrom.c/.h`, `mbprom.c/.h` | **generated** (`tools\gen_roms.py`): program ROM $9000-$DFFF, vector ROM $3000-$3FFF, the Mathbox microcode PROMs |
| `avg.c/.h` | the AVG state machine (Tempest: colour STAT, 13-bit deltas, 4K vector RAM + 4K ROM flat map) |
| `mathbox.c/.h` | the Mathbox: executes the microcode PROMs (`refrun --mbtest` checks it against MBUDOC's formulas) |
| `c012294.c/.h`, `er2055.c/.h` | the POKEY and ER2055 chip models (harvested verbatim) |
| `samples.c/.h` | harvested, unused (Tempest has no samples) |
| `app_loop.c` | the native seam over the platform: cycle timeline, IRQ grid, pass-cost model, frame boundary, EAROM persistence, restarts, diag-loop time; also `tests\skeleton.exe` and `tests\tempest_selftest.exe` |
| `platform\tempest_platform.h` | the platform contract |
| `platform\windows\` | the Windows backend (`plat_win.c`: window, GL beam renderer, XAudio2 mixer, raw input, joystick, ini, log, screenshots; `win_probe.c` read-only peeks) |
| `platform\headless\` | the quiet backend for the self-test |
| `build_all.bat` | refrun, passcost, lockstep, skeleton, tempest_selftest, gate, avgshapes, avgframe |
| `build_win.bat`, `tempest_win.ini` | the window build and its settings |
| `tests\refrun.c`, `tests\lockstep.c`, `tests\gate.c` | oracle, call-by-call verifier, trace-replay gate |
| `tests\avgshapes.c`, `tests\avgframe.c` | Gate V and the frame walker |
| `tests\scenarios\*.txt` | the 11 input scripts, each with its ROM reasoning in the header; syntax in `README_selftest.md` |
| `tests\ref6502\` | the oracle's 6502 interpreter: all documented opcodes, NMOS decimal mode, callback memory (a copy of the shared `ref6502` core, which passes Klaus Dormann's functional test) |
| `record_refs.bat` | records everything in the next three rows |
| `tests\ref`, `tests\ref_selftest` | oracle dumps (attract 600; self test) — not distributed |
| `tests\gate\*.trc` | the 12 lockstep traces gate.exe replays — not distributed |
| `tests\earom\earom.nv` | the EAROM image the `earom` scenario writes — not distributed |
| `tools\gate_s.py`, `tools\gate_e.py`, `tools\gatelib.py` | Gates S and E |
| `tools\fit_passcost.py`, `tools\avg_ref.py` | pass-cost fit; AVG reference data |
| `shots\` | verification screenshots (attract, game, the self-test screens, DSPSYS) |
| `harvest\` | Space Duel files the Tempest versions were derived from |
| `FINDINGS.md` | what the port proved and what it found |

## The memory model is generated, not typed

`tools\gen_state.py` builds `state_defs.h` from the alias block of
[`../disasm/tempest_defines.asm`](../disasm/tempest_defines.asm), whose names
are Atari's own (from the `.MAC` sources). Every alias becomes an address
constant and an lvalue on the state array:

```c
#define A_EABAD        0x01C9
#define EABAD          (g.ram[A_EABAD])
```

so a renamed or moved cell reaches the port by regenerating. The state is
deliberately raw — the machine's own `ram`, `vram` and colour RAM rather than
a struct of fields — which is what makes all of it diffable against the
oracle after every call, and lets the 6502 idioms translate literally:
indexed runs over several symbols, `(zp),Y` walks, the display list pointer,
and the page-1 cells (EAROM state, protection checksums) that share the stack
page. ROM tables are read at their ROM addresses from generated data, never
retyped.

## Why the display list is modelled

The port builds real AVG words in `g.vram` instead of calling a draw API.
Tempest's pictures are assembled at run time — the wells from Mathbox
projections, enemies from per-type picture routines, text from the language
tables — so an abstraction over shapes would have to re-derive what the ROM
computes, and the generated vector RAM is directly comparable with the
oracle's: the display list is where almost every behavioural error shows
first. It also keeps the self test honest: its cross hatch, intensity bars and
signature box are the ROM's own lists.
