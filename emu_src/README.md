# tempest_emu — the emulator twin of the C port

`tempest_emu.exe` runs the **real Tempest program ROM** on a 6502 core behind
the same window, beam renderer, audio path, controls, ini and command line as
[`../c_src/tempest_win.exe`](../c_src/README.md), so the emulated ROM and the
translated C can be run side by side and compared.

It is an emulator twin **of the C port**, not a cut-down AAE. It is built from
the C port's own chip models and board model, compiled from `..\c_src` in
place (nothing is copied, nothing under `c_src` is edited):

| piece | from |
|---|---|
| Windows backend (window, GL beam renderer, XAudio2 stream, raw input, joystick, ini, log, screenshots, command line) | `..\c_src\platform\windows\*`, unchanged |
| AVG display-list walker and its cycle-counted draw time | `..\c_src\avg.c` |
| Mathbox (executes the microcode PROMs) | `..\c_src\mathbox.c` |
| POKEY ×2, ER2055 EAROM | `..\c_src\c012294.c`, `..\c_src\er2055.c` |
| machine state `g` (RAM, vector RAM, colour RAM) | `..\c_src\state.c` — so `win_probe.c` and the backend's log readouts work unchanged |
| memory map, IN1 / 3 kHz bit, VG HALT model, IRQ grid, hardware watchdog | the model of `..\c_src\tests\refrun.c` (the oracle the C port is verified against) |
| input encoding, POKEY set-up, audio mixing, picture timing, EAROM policy, fps lock | the rules of `..\c_src\app_loop.c` |
| addresses, port bits, names | `..\disasm\tempest_defines.asm` (Atari's names), through `..\c_src\state_defs.h`'s generated `A_*` / `K_*` constants |
| **6502 CPU** | **AAE's `cpu_6502` core** (`cpu\`, vendored) — the only thing taken from AAE |

Order of authority: the disassembly and `c_src` first, MAME 0.286 to settle
doubts, the AAE Tempest driver only as a cross-reference (no AAE driver code
or timing approximations are used). Design: [`DESIGN.md`](DESIGN.md).

## Files

| file | |
|---|---|
| `emu_main.cpp` | the seam: memory map (AAE-style handler tables), board devices, POKEY catch-up, IRQ / INTACK, watchdog, VG HALT, the picture event, audio ticks, pacing, and every `tempest_app_*` hook `plat_win.c` calls (`extern "C"`) |
| `emu_roms.c`, `emu_roms.h` | loads a MAME Tempest set from a zip with the backend's miniz; defines the storage behind `progrom`, `vecrom`, `mb_map`, `mb_ucode` (c_src's generated `progrom.c` / `vecrom.c` / `mbprom.c` are not linked) |
| `cpu\cpu_6502.cpp`, `cpu\cpu_6502.h` | vendored from the [AAE emulator](https://github.com/tcottrill/AAE)'s `cpu_code\` (released under The Unlicense); the only edit is `#define USING_AAE_EMU` commented out (the core's documented standalone mode) |
| `cpu\deftypes.h`, `cpu\sys_log.h` | the core's support headers, from the core's standalone Klaus-test tree, unchanged |
| `cpu\cpu_fw.h` | forwards to `deftypes.h` (the Klaus tree's copy redefines deftypes.h's structs and does not compile with it) |
| `cpu\sys_log_shim.cpp` | the `Log::` functions the core calls, forwarded to the backend's log; replaces AAE's `sys_log.cpp` (own worker thread, and `AllocConsole()`) |
| `build_emu.bat` | builds `tempest_emu.exe`; objects in `obj\` |
| `tests\build_tests.bat`, `tests\romcheck.c` | the Klaus functional test on the vendored core, and the ROM loader's byte-identity check |
| `tempest_emu.exe`, `tempest_win.ini` | the prebuilt emulator (64-bit Windows) and its settings |
| `roms\` | **not distributed**: put your own MAME `tempest.zip` here (or in `..\roms\`) |

## Build

    cmd /c ".\build_emu.bat"            (from emu_src; VS2022 x64, the dev prompt call is inside)
    cmd /c ".\tests\build_tests.bat"    (obj\klaus_test.exe, obj\romcheck.exe)

The emulator builds from this repository alone. The tests need two things
that are not in it: a ROM zip (`romcheck`), and Klaus Dormann's functional
test with the core's `test_6502.cpp` runner — set `KLAUS` to that tree before
calling `build_tests.bat`.

Our files and the shared `c_src` machine files compile at `/W4` without
warnings; the vendored framework, chip models and CPU core at `/W3`, as
`c_src\build_win.bat` does.

## Run

`tempest_emu.exe` — keys, ini keys and command line are `tempest_win.exe`'s
([c_src README](../c_src/README.md)): mouse / Left / Right spin, Ctrl / Space /
LMB fire, Alt / RMB zap, 5 / 6 / 7 coin, 1 / 2 start, F2 TEST switch, F1 diag
step, F3 slam, Esc quit.

Files next to the exe keep the backend's fixed names — `tempest_win.ini`,
`tempest_win.log`, `tempest.nv` — but they are `emu_src\`'s own: the two
programs never share settings, log or EAROM image. (DESIGN.md asked for a
separate default name `tempest_emu.nv` "if the backend allows": it does not,
`plat_win.c` hard-codes the names next to the exe; the separate directory does
the job. `--nvram FILE` names another image.)

### ROMs

Default: `roms\tempest.zip` beside the exe, then `..\roms\tempest.zip`.
`--roms FILE` or `[main] roms=FILE` in the ini selects another zip (`--roms`
wins). The set is recognised by the file names in the zip:

| set | program ROMs | vector ROM |
|---|---|---|
| `tempest` (rev 3, 4K) | 133.d1 $9000, 134.f1 $A000, 235.j1 $B000, 136.lm1 $C000, 237.p1 $D000 | 138.np3 |
| `tempest1r` (rev 1, 4K) | as above with 135.j1, 137.p1 | 138.np3 |
| `tempest3` (rev 3, 2K) | 113.d1 114.e1 115.f1 316.h1 217.j1 118.k1 119.lm1 120.mn1 121.p1 222.r1 | 123.np3, 124.r3 |
| `tempest2` (rev 2, 2K) | tempest3 with 116.h1 | 〃 |
| `tempest1` (rev 1, 2K) | tempest2 with 117.j1, 122.r1 | 〃 |

All sets: Mathbox 126.a1 (mapping) and 127.e1 … 132.l1 (microcode nibbles,
assembled exactly as `c_src\tools\gen_roms.py` does). 125.d7 is not needed.
CRC32s from the zip directory are checked against a table (rev-3 values from
`disasm\gen_from_roms.py`, the rest from MAME 0.286's ROM table): a mismatch
is a log warning, a missing file or zip is an error — log, message box (not
with `--hidden` / `--shot` / `--pass-log`) and exit code 3.

The loop heads the stepper needs (MAINLN `LC7AD`, the diag loop `LDA8D`) are
found by their byte signatures: rev 1's diag loop is at `$DA88`, found that way.

### Command line

Everything `tempest_win.exe` takes works: `--test`, `--hidden`, `--nvram FILE`,
`--no-nvram`, `--quit-after-ms N`, `--shot PASS FILE` (repeatable),
`--shot-size WxH`, `--hold SWITCH FROM TO`, `--spin N FROM TO`, `--autoplay`
`[--autoplay-spin N]`, `--pass-log FILE N`. **Only use `--shot` / `--hidden`
from scripts; a plain run opens the window.** Pass numbers mean the same
thing: one `tempest_app_step` = one pass of the loop the CPU is in, MAINLN
(~27/s) or the self test's diag loop (~90–140/s), detected by PC; pass 0 =
power-on. Two options exist only here:

| option | |
|---|---|
| `--roms FILE` | the ROM zip |
| `--ram-dump PASS FILE` | after step PASS (0 = after power-on) write RAM $0000–$07FF + vector RAM $2000–$2FFF (6144 bytes) — loop head PASS+1, comparable with `c_src\tests\ref\frame_NNNN.ram` / `.vram` |

`plat_win.c`'s `parse_args` answers an unknown option with its usage box, and
`c_src` is not edited, so a static initializer in `emu_main.cpp` takes these
two out of `__argv` before `WinMain` runs. (The usage box itself still says
`tempest_win.exe` and does not list them.)

`--pass-log`'s last column is here the pass's **measured** mainline cycles
(outside the frame wait and the IRQ handler — what `tests\passcost.exe`
measures on the oracle), where the C port logs its pass-cost model's charge.

## Differences from `tempest_win.exe`

- The ROM runs; nothing is translated. CPU time is real, so no pass-cost
  model: attract runs 9.15 IRQs per pass here (oracle 9.21, C port 9.02).
- **IRQ acknowledge.** The line is asserted every 6144 cycles and held until
  the ROM writes `$5000` — `WTCHDG` = `INTACK` in Atari's defines, and MAME's
  `wdclr_w` does the same. `refrun.c` holds it "pending until taken". The two
  agree whenever the handler runs (`SOFTOK LD717 STA WTCHDG` is its first act).
- **RANDOM** is the real POKEY's (`c012294.c`), clocked to the exact bus cycle
  of each access (the core's cycle callback + catch-up), not refrun's bare
  poly-17, and the core counts true bus cycles (page crossings, indexed-store
  dummy reads) where refrun uses a base-cycle table. The ROM's protection
  cells stay 0 (`QT5 $011F`, `QT4 $0720`; logged at exit), but RAM at pass N is
  not byte-identical to `tests\ref` — see Verification 6.
- **The picture** follows `app_loop.c`'s `vg_picture` rule, with two
  additions forced by real CPU time: a list that reads HALT (the SWHALT list
  at boot and between attract screens) is looked at again every IRQ period,
  because the ROM patches VECRAM to the looping master list between IRQs and
  refrun's VG HALT model then needs no further VGSTART; and a walk that lands
  inside a half-written list is retried 512 cycles later (up to 8 times)
  before it is treated as a broken list (none seen in any run below).
- Boots (power-on, watchdog bite, `JMP RESET`) run on a synthetic clock,
  unpaced, audio dropped, as `app_loop.c` does; live audio then starts on the
  next whole IRQ tick.
- A step that reaches no loop head within 2 s of machine time returns anyway
  (the window stays alive); logged once, counted at exit.
- The exit log has extra `emu:` lines: ROM set, IRQs raised / serviced / lost,
  watchdog kicks / bites (and bites not at `WDGTST`), RESET arrivals, the
  protection cells, Mathbox starts / runaways, unmapped accesses, audio
  underruns and — after a self test — the ROM's result cells `$78–$88`.

## Verification (all windowless; 2026-09-17)

1. **CPU core.** `obj\klaus_test.exe …\bin_files\6502_functional_test.bin`
   (Klaus's own `test_6502.cpp`, unmodified, against `emu_src\cpu`):
   `SUCCESS! Reached target $3469 in 96241364 cycles.`
2. **ROMs.** `obj\romcheck.exe`: `progrom`, `vecrom`, `mb_map`, `mb_ucode`
   from `roms\tempest.zip` IDENTICAL to c_src's generated arrays (also
   `tempest3.zip`); `tempest1.zip` differs from $A8AF on, as it should.
3. **Boot and pacing.** `--hidden --quit-after-ms 12000`: MAINLN reached after
   977,774 cycles (oracle 977,767), 9 IRQs; 246.10 IRQ/s, machine/wall 1.0000,
   0 starved audio blocks, 0 watchdog bites, 0 lost IRQs, 552 pictures with a
   mean AVG draw time of 21.63 ms (`tempest_win.exe`, same run: 552, 21.68 ms).
   `--hidden --autoplay --quit-after-ms 30000`: 246.10 IRQ/s, ratio 1.0000,
   audio peak 20724, 0 starved, in-game pictures at the 61.5 Hz floor.
4. **Self test.** `--test --hidden --quit-after-ms 9000`: diag loop after
   4,588,244 cycles (oracle 4,588,221), 89.7 diag passes/s on ROMREP (oracle
   bucket 33 = 89.5), `CHKSMS` all 00, `MBCOND RAMCND PK1CND PK2CND EARCND` 00.
   `--hidden --hold test 0 300`: TEST opened in the diag loop → one watchdog
   bite, at `WDGTST`, reboot into the game. EAROM: the C port's own scenario
   (`--nvram` copy of `tests\earom\earom.nv`, `--hold test 200
   2100 --hold fire 260 2000 --hold start2 260 2000 --spin 4 260 2000 --shot
   2600`) zeroes the score and initials groups and keeps the bookkeeping, as
   it does there.
5. **Shots** (not kept in the repository), made with the C port's commands: `attract_0100/
   0300/0700.png`, `attract_logo_1000.png`, `game_0400/0800/1200.png`
   (`--autoplay`), `selftest_*.png`. `selftest_romrep.png` matches
   `c_src\shots` line for line; attract and game differ only in what the
   random demo is doing.
6. **RAM against the oracle** (`--ram-dump`, `c_src\tests\ref`): loop head 1 —
   2 of 2048 RAM bytes differ (`$01FD/$01FE`, the stale stack frame of the last
   IRQ: it interrupted a different instruction of the wait loop), vector RAM
   identical; loop head 2 — 58 RAM / 110 vector-RAM bytes; loop head 16 — 291 /
   760. Expected (different RANDOM, IRQ phase); byte-identity is not a goal.

Not verified: anything that needs a visible window (live keys, mouse,
joystick, F2, fullscreen, vsync modes 1 / 2) — those are the backend's,
shared unchanged with `tempest_win.exe`; the rev 1 / rev 2 sets beyond boot,
attract and (tempest1r) the self test; cocktail flip.
