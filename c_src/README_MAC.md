# Tempest C port - macOS (Apple Silicon)

Native SDL2 backend: `platform/macos/plat_mac.c` (implements `platform/tempest_platform.h`).
The game core (`*.c` outside `platform/`) is the verified translation and is built unchanged.
Built and run on macOS (arm64), Apple clang, SDL2 2.32.70 (Homebrew `sdl2`, which is the
`sdl2-compat` shim over SDL3).

## Prerequisites
```sh
xcode-select --install        # clang, make, macOS SDK (skip if installed)
brew install sdl2             # only for the playable build; the headless checks need no SDL
```

## Build and run
```sh
cd c_src
make -f Makefile.mac              # -> build/mac/tempest_mac
./build/mac/tempest_mac
make -f Makefile.mac app          # -> build/mac/Tempest.app (bundles libSDL2 + libSDL3)
open build/mac/Tempest.app
```
The ROM images are compiled in (`progrom.c`, `vecrom.c`, `mbprom.c`), so no ROM files are
needed to build or play.

The `.app` is ad-hoc signed and not notarized. If Gatekeeper complains after copying it to
another Mac: right-click > Open, or `xattr -dr com.apple.quarantine Tempest.app`.

## Controls
| Input | Action |
|---|---|
| Mouse / trackpad X, Left / Right | spinner |
| Space, Ctrl, left click | fire |
| Option (Alt), Z, right click | superzapper |
| 1 / 2 | start 1 / 2 |
| 5 / 6 / 7 | coin left / centre / right |
| F2 or T | TEST switch (latching, like the board's) |
| F1 or D | diagnostic step (hold) |
| F3 or S | slam (also steps the self-test screens) |
| `[` / `]` | mouse speed -10% / +10% |
| Shift+`[` / Shift+`]` | key spin rate -10% / +10% |
| F8 | toggle mouse capture |
| Cmd+F, Alt+Return | fullscreen |
| Esc, Cmd+Q | quit |

The mouse is captured (hidden, relative motion) while the window has focus; F8 releases it.
Mouse right makes the spinner counter count down, as in the Windows backend
(`invert_spin=1` reverses it). On a Mac keyboard the F-keys need fn, hence the T / D / S
duplicates. Game controller: left stick X = spinner, A = fire, B or X = zap, Back = coin
left, Start = start 1.

TEST is a latching switch: closed at power-on (`--test`, or `test_switch=1`) boots the ROM's
self test; toggled while a game runs it enters the options / bookkeeping screens.

## Command line
```
tempest_mac [--fullscreen] [--mute] [--stats] [--test] [--dip KEY=WORD]...
            [--demo] [--demo-spin N] [--quit-after-ms MS] [--no-nvram] [--nvram FILE]
            [--hold NAME FROM TO]... [--spin N FROM TO]...
            [--shot PASS FILE.bmp]... [--shot-size WxH]     headless, turbo clock
            [--shot FILE.bmp SECONDS]                        real time, saved after SECONDS
```
- `--demo` (alias `--autoplay`): scripted coin, start, rate, autofire and a steady spin;
  after game over it coins again.
- `--hold` names: `test diag slam fire zap start1 start2 coinl coinc coinr`. FROM / TO are
  pass numbers (1-based, TO exclusive, 0 = for ever); `--spin N` adds N to the spinner each
  pass. Scripted runs read no keys.
- `--shot PASS FILE` runs on a fast simulated clock with a hidden window and no audio, saves
  the newest complete picture after pass PASS, and quits after the last shot (up to 8 shots).
  Output is always BMP (no PNG). `sips -s format png a.bmp --out a.png` converts it.
- `--shot FILE SECONDS` runs in real time in the live window and saves at the window's size.
- `--demo` and `--shot PASS FILE` never read or write your saved EAROM unless `--nvram FILE`
  is given.

## Settings
`~/Library/Application Support/Tempest/tempest.ini` is optional: `key=value` lines using the
names in `tempest_win.ini` (sections are ignored, `;` and `#` start comments). `--dip KEY=WORD`
overrides it. Keys read: the dips below, `mouse_sensitivity` (0.5), `key_spin_rate` (180),
`joy_spin_rate` (180), `invert_spin`, `test_switch`, `fps_lock` (0 = the board's 246.09 Hz),
`vg_window` (`cycles` or `free`), `watchdog_cycles` (1134000), `honour_flip`, `rotate_sign`.

Option switches (default = first word; all four bytes 0 = the AAE / refrun defaults):

| Key | Words |
|---|---|
| `coinage` | 1_coin_1_play, 2_coins_1_play, 1_coin_2_plays, free_play |
| `right_coin` | x1, x4, x5, x6 |
| `left_coin` | x1, x2 |
| `bonus_coins` | none, 1_each_5, 1_each_4, 1_each_3, 2_each_4, 1_each_2, freeze |
| `minimum_credits` | 1, 2 |
| `language` | english, french, german, spanish |
| `bonus_life` | 20000, 10000, 30000, 40000, 50000, 60000, 70000, none |
| `lives` | 3, 4, 5, 2 |
| `difficulty` | medium1, easy, medium2, hard |
| `rating` | 1_3_5_7_9, tied_to_high_score |
| `cabinet` | upright, cocktail |
| `special_option` | off, on |

The raw bytes can also be forced with `TEMPEST_DSW_N13`, `TEMPEST_DSW_L12`,
`TEMPEST_DSW_POKEY1`, `TEMPEST_DSW_POKEY2` (hex). An unknown word falls back to the default
with a message on stderr.

## Files
- `~/Library/Application Support/Tempest/tempest.nv`: the ER2055 EAROM image (64 bytes,
  high scores and bookkeeping). Written whole to a temp file, then renamed. Delete it to
  reset. No file = a blank part, every cell `$FF`.

## Checks (no window, no SDL needed)
```sh
make -f Makefile.mac check      # skeleton + self-test + lockstep
make -f Makefile.mac lockstep   # real ROM vs. translated C, call by call
```
`check` was run from a clean tree and passes:
- `skeleton`: 100 passes of the real boot and loop, no watchdog trips.
- `selftest`: 38 of 38 seam checks (boot, attract, a coined game with fire / zap / spin, EAROM
  round trip, watchdog reboots, the self-test screens against the ROM oracle's dumps).
- `lockstep`: twelve runs, all PASS, 0 calls failed; the two attract runs (600 / 5000 frames:
  222026 / 1564454 calls) and the six gameplay ones (`coin_start`,
  `high_score`, `two_player`, `superzapper`, `fuseball_pulsar`, `slam`) compare exactly the
  call counts the upstream `README.md` records (as do the attract runs).
  The self-test / EAROM four: `selftest_boot` 167338 calls compared
  (4 RESET regions, 348 MAINLN and 1450 DIAG passes); `selftest_midrun` 708398; `earom` 808812;
  `earom_readback` 221042. The image `earom_readback` writes back equals the one it read.

Porting notes (flags only, no core changes):
- The core uses `NULL` without including a header for it (MSVC allows that); the Makefile
  passes `-include stddef.h`.
- The self-test opens literal Windows paths (`tests\scenarios\...`, `tests\ref\...`). The
  `selftest` target runs it from `build/mac/run/` containing symlinks with those exact names.
- The oracle dumps those paths refer to are not checked in. The Makefile generates them with
  the real ROM run (`lockstep` is `refrun` plus the verifier) into `build/mac/refdumps/`.

## More checks: gate, passcost, AVG walker
```sh
make -f Makefile.mac tools      # build gate, passcost, avgshapes, avgframe, avgtime
make -f Makefile.mac gate       # record the 12 lockstep traces, replay each through gate
make -f Makefile.mac passcost   # pass-cost observer + refit of app_loop.c's table
make -f Makefile.mac avg        # avgtime + avgframe, and the Python AVG model comparison
```
Run from a clean tree and passing:
- `gate`: all 12 traces replay with every pass byte-identical (the C modules alone against the
  ROM's recorded state), and the attract trace's images equal the oracle dumps at 34 frames.
  The gate's own mutation self-test fails when it should (a flipped RAM bit, and inverted IN1
  reads, are caught); inverted RANDOM reads in some attract passes are not caught because they
  never reach a checked state.
- `passcost`: the five-scenario fit (19,994 passes) reproduces all 15 rows of the pass-cost
  table committed in `app_loop.c`.
- `avg`: `avgframe` over the 34 attract dumps - the two walkers inside it agree on all 34, and
  its output is byte-identical to the disassembly's Python AVG model (`tools/avg_ref.py frame`)
  on all 34. `avgtime` reports the draw time of the 32 looping lists (mean 19.755 ms, 50.62 Hz).
  The Python comparison needs `disasm/_survey/roms_extracted/tempest3_cpu64k.bin`, built from
  your own ROM set with `python3 disasm/gen_from_roms.py "<dir or tempest.zip>" --image-only`
  (gitignored; the target skips the comparison if it is absent).

The tools open literal Windows paths, so `avg` runs them from a scratch directory of symlinks
named like those paths (as `selftest` does). The traces and dumps are generated into
`build/mac/` (about 57 MB of traces) and are not distributed.

## Verification against MAME
Compared with `mame tempest3` (rev 3, ROM set verified good) on the same scripted inputs:
the attract screen's layout, orientation, colours and high-score line match. In a game the
state sequence matches (`$16` rate yourself, `$0A`, `$04` play, player on segment 15, wave 1) and
colour bounding boxes of the picture agree within about 1% of its size at the same game
moment (resolutions differ; the C picture's tube is about 1% taller). Enemy positions differ,
which is expected from timing.

## Not verified / known gaps
- Spinner direction against MAME, and all interactive input (keys, trackpad, controller,
  mouse capture), were not driven by a person or script on this Mac. Mouse right should turn
  the claw clockwise; if it does not, set `invert_spin=1`.
- Audio was checked by numbers (about 247 pushes a second, non-silent in a game), not by ear.
  A few underruns occur while the ring buffer primes at start.
- Cocktail flip, non-default dips, fullscreen and window resize were not exercised.
- **Gate V (`avgshapes`) is built but not run.** Its reference (`tools/avg_ref.py shapes`) is
  generated from Atari's own source archive (`ALVROM.MAC`, github.com/historicalsource/tempest),
  which this repo does not ship and which is not on this machine. `make -f Makefile.mac gatev`
  runs it if you unpack the archive to `../tempest-main/tempest-main/`.
- Gates S and E (`tools/gate_s.py`, `tools/gate_e.py`) were not run or ported.
- No phosphor afterglow, no vsync control (each picture is presented as the core finishes
  it), BMP-only screenshots.
- The `.app` was run from the command line, not launched from Finder.
