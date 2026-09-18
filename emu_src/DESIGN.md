# tempest_emu - a mini Tempest emulator beside the C port

Design, 2026-09-17: lives in `emu_src\`, builds against
`..\c_src` in place (no copies of shared files), ROMs loaded from
`roms\tempest.zip`.

## Purpose

`tempest_emu.exe` runs the real Tempest program ROM on AAE's `cpu_6502` core
behind the SAME window, beam renderer, audio path, controls, ini and command
line as `c_src\tempest_win.exe`, so the emulated ROM and the translated C can
be run side by side and compared. It is deliberately small: one seam file, one
ROM loader, a vendored CPU core, a build script.

It is the emulator TWIN OF THE C PORT, not a cut-down AAE: a standalone
alternative to AAE whose behaviour must match the C port. It is built from the
C port's own chip models and board model; the ONLY thing taken from AAE is the
6502 core (`cpu_6502.cpp/.h`).

### Order of authority

1. The disassembly and `c_src`, together:
   - `disasm\tempest_defines.asm` (Atari's own names: IN1, INOP0/INOP1, OUT0,
     VGSTART/VGSTOP, WTCHDG/INTACK, EADAL/EACTL/EAIN, MSTAT/MYLOW/MYHIGH/MBSTAR,
     POKEY/POKEY2 cells ...) and `disasm\tempest_program_rom.asm` (how the ROM
     actually uses a port) for hardware addresses, port bit meanings and names.
     The emulator's handlers, constants and comments are named after those
     labels - through `c_src\state_defs.h`'s generated `A_*` / `K_*` constants,
     included rather than retyped - and cite the label or Lxxxx address where
     a hardware fact comes from the ROM's usage;
   - `c_src\tests\refrun.c` and `c_src\app_loop.c` for the memory map, device
     behaviour, input encoding, the IRQ / watchdog / VG-halt model, POKEY
     set-up, audio mixing, picture timing and EAROM policy.
2. MAME 0.286 (`src\mame\atari\tempest.cpp`) to settle
   doubts.
3. The AAE Tempest driver (`tempest.cpp` / `tempest.h`): a cross-reference only.
   No AAE driver code and no AAE timing approximations (e.g. its beam-length
   timer) are ported. Where it differs from `c_src`, `c_src` wins.

## What is reused, untouched, from `..\c_src`

| piece | files |
|---|---|
| Windows backend (window, GL beam renderer, XAudio2 mixer, raw input, joystick, ini, log, screenshots, command line) | `platform\windows\*` incl. `plat_win.c`, `win_probe.c` |
| platform contract | `platform\tempest_platform.h` |
| AVG | `avg.c/.h` |
| Mathbox (executes the microcode PROMs) | `mathbox.c/.h` |
| POKEY, ER2055 | `c012294.c/.h`, `er2055.c/.h` |
| machine state `g` (`ram`, `vram`, `colram`) | `state.c/.h`, `state_defs.h` - the emulated RAM / vector RAM / colour RAM live in `g`, so `win_probe.c` and the backend's log readouts work unchanged |

Nothing under `c_src` is edited. If a change there turns out to be
unavoidable it must be minimal, must not change `tempest_win.exe`'s behaviour,
and is reported.

## What is new, in `emu_src\`

| file | |
|---|---|
| `cpu\cpu_6502.cpp`, `cpu\cpu_6502.h` | vendored from the [AAE emulator](https://github.com/tcottrill/AAE)'s `cpu_code\`, the only edit being `#define USING_AAE_EMU` disabled (the core's documented standalone mode) |
| `cpu\cpu_fw.h`, `cpu\deftypes.h`, `cpu\sys_log.*` | the standalone support files from the core's Klaus-test tree (a small shim instead of `sys_log.cpp` is fine if it collides with `platform\windows\log.c`) |
| `emu_roms.c` | loads a MAME Tempest set from a zip with the backend's `miniz`; defines the storage behind `progrom`, `vecrom`, `mb_map`, `mb_ucode` (the symbols `avg.c`, `mathbox.c`, `state.c` link against; `c_src`'s generated `progrom.c / vecrom.c / mbprom.c` are NOT linked) |
| `emu_main.cpp` | the seam: memory map, board devices, POKEY catch-up, IRQ, watchdog, picture event, audio ticks, pacing; implements every `tempest_app_*` hook `plat_win.c` calls (`extern "C"`) |
| `build_emu.bat` | modelled on `c_src\build_win.bat`; objects in `emu_src\obj\`, output `emu_src\tempest_emu.exe` |
| `README.md` | what it is, build, run, ROM sets, differences from `tempest_win.exe` |

No new 6502 core is written. No AAE framework pieces (Machine, mixer, timers,
`c012294_interface.cpp` itself) are pulled in.

## ROM loading

- Default zip: `roms\tempest.zip` beside the exe, then `..\roms\tempest.zip`.
  `--roms FILE` (and an ini key if the backend's ini layer makes that easy)
  selects another zip.
- The set is identified by the file names in the zip. Layouts (MAME):
  - `tempest` (rev 3, 4K): 136002-133.d1 $9000, -134.f1 $A000, -235.j1 $B000,
    -136.lm1 $C000, -237.p1 $D000; vector -138.np3 $3000 (4K)
  - `tempest1r`: as above with -135.j1 $B000, -137.p1 $D000
  - `tempest3` (2K): 113.d1 $9000, 114.e1 $9800, 115.f1 $A000, 316.h1 $A800,
    217.j1 $B000, 118.k1 $B800, 119.lm1 $C000, 120.mn1 $C800, 121.p1 $D000,
    222.r1 $D800; vector 123.np3 $3000, 124.r3 $3800
  - `tempest2`: tempest3 with 116.h1 at $A800
  - `tempest1`: 116.h1 $A800, 117.j1 $B000, 122.r1 $D800
  - Mathbox, all sets: 136002-126.a1 (32 bytes) = `mb_map`; 127.e1 .. 132.l1
    nibble PROMs assembled into the 24-bit `mb_ucode` words EXACTLY as
    `c_src\tools\gen_roms.py` does it. 125.d7 (AVG state PROM) is not needed.
  - $E000-$FFFF mirrors $C000-$DFFF (vectors), as `tests\refrun.c` maps it.
- CRC32s from the zip directory are compared with a table whose rev-3 values
  come from `disasm\gen_from_roms.py` (`c_src\tools\gen_roms.py` carries no
  names or CRCs of its own: it reads the image that script extracts) and whose
  other values (rev 1 / rev 2 parts, the Mathbox PROMs) come from MAME 0.286's
  ROM table; the AAE driver's table is a cross-reference for file names only.
  A mismatch is a log warning, a missing
  file or missing zip is a clear error (log + message box unless `--hidden`)
  and a nonzero exit.
- Self-check: loading `roms\tempest.zip` must give byte-identical `progrom`,
  `vecrom`, `mb_map`, `mb_ucode` to `c_src`'s generated arrays (a one-off test
  program or a debug option; report the result).

## The board

Take the memory map and device behaviour from `c_src\tests\refrun.c` (the
oracle: the same ROM on a plain 6502 with the board modelled around it), with
the addresses and names from `disasm\tempest_defines.asm` (via
`c_src\state_defs.h`); MAME 0.286's
`src\mame\atari\tempest.cpp` settles doubts; the AAE Tempest driver
(`tempest.cpp` / `tempest.h`, not in this repository) is a cross-reference only (see
"Order of authority").
Take the hardware truth for inputs (IN1 polarity, 3 kHz bit, AVG halt bit,
spinner counter on POKEY 1 pot lines with at most +-7 per IRQ, buttons and
option switches on the pot lines, DIPs) from `c_src\app_loop.c`, which already
encodes `plat_inputs` and `plat_dsw_*` onto the ports.

- RAM $0000-$07FF -> `g.ram`; colour RAM $0800-$080F -> `g.colram`; vector
  RAM $2000-$2FFF -> `g.vram`; vector ROM $3000-$3FFF; program ROM $9000-$DFFF
  (+ mirror). AAE-style `MemoryReadByte` / `MemoryWriteByte` tables.
- POKEYs at $60C0 / $60D0 on `c012294.c`. Catch-up clocking, the idea of AAE's
  `c012294_interface.cpp`: before every register read or write, and at every
  tick end, advance both chips by the CPU cycles elapsed since their last
  catch-up. POKEY clock = CPU clock = 1.512 MHz, so it is 1:1. RANDOM deltas
  are then cycle-true, which the ROM's protection checks depend on. Use the
  same audio set-up calls `app_loop.c` makes (rates, gain, mixing to one mono
  44100 Hz stream pushed per IRQ tick with `plat_audio_push`).
- Mathbox via `mathbox.c`: MBSTAR $6080-$609F (write: register load / start),
  MSTAT $6040, MYLOW $6060, MYHIGH $6070 (read) - corrected from the first
  draft's "$6060-$607F" against `refrun.c` / `tempest_defines.asm`; EAROM
  EADAL $6000-$603F, EACTL $6040 (write), EAIN $6050 (read) via `er2055.c`, persisted through `plat_nvram_read/write` with
  the same policy as `app_loop.c` (note: shares `tempest.nv` semantics - use a
  separate default file name `tempest_emu.nv` if the backend allows, else
  document it).
- IRQ every 6144 CPU cycles (246.09 Hz); watchdog on $5000, 1,134,000 cycles,
  bite = CPU reset; VGGO $4800 / VG reset $5800; OUT0 $4000 (flip bits to
  `plat_video_begin`, coin counters ignored).
- Picture: the same rule as `app_loop.c vg_picture` - walk the list from `g`
  with `avg.c`, present it, next picture when the AVG's counted draw time has
  elapsed, never less than 4 IRQs (`TP_VGW_CYCLES`), no floor in
  `TP_VGW_FREE`; the AVG halt bit reads from the same model.

## Time

One machine timeline in CPU cycles. `tempest_app_step` runs the machine
forward and paces it against `plat_now_ms` (sleep through `plat_sleep_ms`,
honour `tempest_app_idle_ms`), keeps the window's message pump and input
polling live, honours `tempest_app_set_fps_lock`. The step granularity (one
IRQ tick vs. one MAINLN pass detected by PC, as refrun detects passes) is
chosen after reading `plat_win.c`'s main loop so that `--shot PASS`, `--hold`,
`--spin`, `--autoplay`, `--quit-after-ms`, `--hidden` keep working; any option
that cannot be supported is listed in the README.

## Verification (all headless / hidden - never open a visible window)

1. Vendored CPU passes Klaus's 6502 functional test, built with the core's own
   runner (`test_6502.cpp`) against `emu_src\cpu`.
2. ROM self-check above.
3. `tempest_emu.exe --hidden --quit-after-ms ...`: boots through the ROM's
   power-on test, reaches attract; log shows ~246.09 IRQ/s, machine/wall ~1.0,
   no audio starvation, no watchdog bites.
4. `--test` run: the ROM's self test reports no bad RAM/ROM/POKEY/Mathbox.
5. `--shot` frames of attract (and a coined game via `--autoplay` or `--hold`)
   written to `emu_src\shots\`, compared by eye with `c_src\shots\`.
6. Report honestly how RAM at MAINLN pass N compares with `c_src\tests\ref`
   dumps if that comparison is cheap; byte-identity is NOT a goal (different
   RANDOM model than refrun's bare poly counter).

## As built (2026-09-17) - where the code was followed instead of this text

Results of the verification are in `README.md`.

- **Step granularity: one loop-head pass** (MAINLN `LC7AD` / diag loop
  `LDA8D`, by PC, found by byte signature so the rev 1 / rev 2 sets work).
  `plat_win.c` counts `tempest_app_step` calls as passes for `--shot`,
  `--hold`, `--spin`, `--autoplay` and `--pass-log`, with one
  `plat_input_poll` per pass. Pacing, audio ticks and pictures happen inside
  the step, per IRQ tick, as in `app_loop.c`. Boots run on a synthetic clock,
  as there. A step with no loop head for 2 s of machine time returns.
- **NVRAM file name.** `plat_win.c` hard-codes `tempest_win.ini`,
  `tempest_win.log`, `tempest.nv` next to the exe; no separate default name is
  possible without editing it. `emu_src\` being its own directory keeps the
  files apart.
- **`--roms`.** `parse_args` rejects unknown options (usage box), so
  `emu_main.cpp` removes `--roms FILE` (and `--ram-dump PASS FILE`) from
  `__argv` in a static initializer before `WinMain`. `[main] roms` is read
  through the backend's ini layer.
- **IRQ line.** Held until the `$5000` write (WTCHDG = INTACK in
  `tempest_defines.asm`; MAME the same) rather than refrun's "pending until
  taken"; the core's level-sensitive pin API with its cycle-exact polling.
- **Cycle-exact device time.** The core's per-bus-cycle callback is the
  machine clock, so POKEY catch-up, the 3 kHz bit and the IRQ assertion are
  exact to the cycle within an instruction, not only per instruction.
- **Picture.** `vg_picture`'s rule plus: a HALTed list is re-examined every
  IRQ period, and a walk into a half-written list is retried (see
  `emu_main.cpp vg_picture`) - both consequences of the ROM running in real
  CPU time instead of at the loop head.
- **Support files.** The Klaus tree's `cpu_fw.h` does not compile together
  with its `deftypes.h` (duplicate structs); `cpu\cpu_fw.h` forwards to
  `deftypes.h`. `sys_log.cpp` is replaced by `cpu\sys_log_shim.cpp` (it
  allocates a console). The core itself is byte-identical to AAE's apart from
  the `USING_AAE_EMU` line.
- **Nothing under `c_src\` was edited.**
