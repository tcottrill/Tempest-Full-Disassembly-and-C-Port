# AVG pipeline notes (avg.c / sd_vecrom.c)

What the vector-display pass established, for whoever translates the
mainline next. Companion to DESIGN.md "JMPL address fold (RESOLVED)".

## Memory map: FLAT, no mirror

The AVG program counter is a plain byte offset from CPU $2000; the 13-bit
JSRL/JMPL word operand shifts left once and indexes straight through RAM
into ROM. No fold, no mirror, anywhere.

| AVG word addr | byte offset | CPU | backing in C |
|---|---|---|---|
| $000-$3FF | $0000-$07FF | $2000-$27FF | `g.vram` (display list) |
| $400-$7FF | $0800-$0FFF | $2800-$2FFF | `sd_vecrom` (ship-picture ROM — real bytes, never legitimately executed; fetching sets `AVG_FLAG_SHIPROM`) |
| $800-$FFF | $1000-$1FFF | $3000-$3FFF | `sd_vecrom` (glyphs/shapes) |
| $1000-$1FFF | $2000-$3FFF | $4000-$5FFF | program ROM — not carried; walk stops `AVG_STOP_BADFETCH` |

Evidence (file + line):
- `aae/vidhrdwr/aae_avg.cpp` 558-570:
  `a = (firstwd & 0x1fff) << 1; pc = a` into `vec_mem`, which line 686 sets
  to CPU memory + the driver's vectorram base; `aae/drivers/bwidow.cpp` 788ff:
  `AAE_DRIVER_VECTORRAM(0x2000, 0x800)` for the whole bwidow/spacduel family.
- `mame_late_avgdvg.cpp` 509 (`vectorram[vg->pc ^ 1]`), 780
  (`vg->pc = vg->dvy << 1`), 1535 (base = CPU region + $2000). Same flat walk.
- The ROM's own stored glyph JSRLs confirm the flat fold: table entry 11
  ('A') at $3260 holds word $A86E → operand $086E → CPU $2000+$10DC = $30DC,
  and those targets render as the correct letters (`../disasm/NOTES.md`
  render pass).
- No JSRL/JMPL anywhere in `../disasm/spaceduel_vector_rom.asm`
  targets CPU $2800-$2FFF.

### The $E4 power-on seed is dead code

Poweron ($8090-$8097) writes $2000/1 = $01/$E4 (JMPL word $0401 → CPU $2802,
mid-ship-table) — but no VGGO is strobed on the boot path (the mainline's
only strobe is `STA GOADD` at $4056; $8608/$8AAA/$8D49 are self-test), and
the path ends `JMP Pwron` ($80D1) → Pwron ($68CA) → `JSR LswVectorAddress`
($68D0) → $6C96 rewrites $2000/1 = $01/$E0 (JMPL word $001 → CPU $2002)
and plants HALT at $2003 and $2403. Only then does Start2 strobe VGGO.

### The per-frame buffer swap

Start2 $404E-$4056: `LDA $2001 / EOR #$02 / STA $2001 / STA GOADD` — the
standing word at $2000 toggles $E001/$E201 = JMPL word $001/$201 = CPU
$2002/$2402, the double buffers. The mainline then builds the NEXT frame in
the other buffer (Start2_11: list pointer zp $01/$02 reset to $2002 or
$2402 based on the same bit).

### How the game encodes jump words (for the vgutil translation)

`AddJmplToVector` ($8E7D) / `AddJsrlToVector` ($8E8E): enter with A = MSB,
X = LSB of the target **CPU address**. `LSR A / AND #$0F / ORA #$E0` (or
`#$A0`) makes the high byte, then `TXA / ROR` catches the carry from the LSR
to make the low byte: word = opcode | ((cpu_addr >> 1) & $0FFF). Because of
the `AND #$0F`, game-generated words carry a 12-bit operand — they can only
target CPU $2000-$3FFF, never program ROM. `DisplayDigit` ($8E55) doesn't
encode at all: it copies pre-stored JSRL words from the tables at $324A
(normal) / $3458 (flipped), entries 0=blank, 1..10='0'..'9', 11..36='A'..'Z'.

## Coordinate space (avg.h has the full contract)

Origin = beam center (CNTR target / screen center); +x right, +y UP
(aae negates y only for its raster); unit = 1 LSB of the 13-bit VCTR delta
at scale 1.0. Deltas are 13-bit TWO'S COMPLEMENT (never sign-magnitude).
SCAL: factor = ((~w)&$FF)/256 / 2^((w>>8)&7) — linear part complemented;
scale starts at 0.0 (aae quirk: pre-SCAL vectors are zero-length). STAT
$64xx: color = w&7, intensity register = (w>>4)&$F; vector z==1 means "use
the register", else lum = z<<1. Dark moves are reported with lum 0. No
clipping (the spacduel AVG variant has none; WNDSE in the ROM is all-dark).

## Timing model fidelity

`avg_draw_time_units()` = Σ max(|dx|,|dy|) in beam units over VCTR/SVEC —
the aae_avg.cpp model (`vector_timer`, charged 1500 ns/unit of AVG BUSY for
frame pacing). It ignores per-word fetch time, CNTR settling and the
normalization shifter. Kept for continuity, but superseded for frame pacing
by the cycle-true model (`avg_frame_cycles()` / `avg_frame_time_ms()`) —
see "Cycle-true timing" below. JSRL stack: 8 deep like aae (hardware is 4;
the game never gets near either).

## Verification (Gate V evidence)

- `sd_vecrom.c`: 6144 bytes from the ROM set's 136006.106+107,
  cross-checked 6144/6144 against `$2800:$4000` of the 64K image
  (`../disasm/build/spacduel_64k.bin`, built by `disasm/gen_from_roms.py`)
  by `c_src/tools/gen_vecrom.py`.
- `c_src/tests/avg_dump.c` (MSVC 2022 `/W4 /WX /std:c11`, compiles clean)
  builds a game-shaped list — JMPL slot at $2000, CNTR, SCAL 1,$00, COLOR
  $64E1, JSRL 'A' (word read from the real $324A table), VCTR(+100,-50,z=1),
  SVEC(-4,+6,z=7), JSRL 'B', HALT — and prints 22 segments + a STOP line.
- `c_src/tools/avg_check.py` (independent Python walker over the same
  bytes, reusing `../disasm/avg.py`'s verified s13/s5) prints the identical
  format: `fc` shows **no differences** (22 segments, STOP HALT words=32
  time=156.3867).
- Geometry is right by inspection: 'A' = up stroke, apex, down stroke,
  dark move, lit crossbar, dark cell-advance; 'B' = spine + two bumps;
  the VCTR lands at exactly (100,-50) x 255/512; z==1 vectors pick up the
  COLOR intensity ($E → lum 14).

## Open items

- The oracle (tools/oracle.py) does not exist yet; once it captures live
  frames, re-run the JSRL/JMPL-target audit over captured vector RAM (the
  static audit covered the ROM only).
- ~~Per-instruction cycle-true timing (mame_late model) not implemented —
  revisit if frame pacing needs it.~~ Done 2026-08-26 — see "Cycle-true
  timing" below.
- Self-test module writes its own display list directly ($8D7B ff) and
  strobes GOADD at $8608/$8AAA/$8D49 — untouched by this pass.

## Captured-frame audit

The "re-audit jump targets over captured vector RAM" item is closed. Walking
the oracle's attract dumps (frame_0001/0002/0016/0064.vram) through the flat
map: every frame HALTs cleanly (456/494/428 segments after frame 1; frame 1's
active buffer is still empty, as on hardware), and every JSRL/JMPL target is
either a vector-RAM slot ($22D0 SPARKB, $2300/$230A/$2314 PL0SET/PL1SET/
CMBSET, $2290-region rocks) or a $3000+ ROM shape. No ship-picture-region
($2800-$2FFF) fetches occur. Flat map confirmed against real frame data.

## Cycle-true timing (avg.c pass 2)

`avg_frame_cycles()` / `avg_frame_time_ms()` now report the draw time of the
last `avg_run` from the **mame_late state machine model**, replacing the AAE
1500 ns/unit approximation for frame pacing. Geometry/semantics untouched —
only the time accumulator changed; the old `avg_draw_time_units()` remains
for continuity with earlier measurements.

### The model and its clock

Reference: `aae/vidhrdwr/mame_late_avgdvg.cpp`.
Space Duel maps to the **plain `avg_default` variant** (`avg_start()`, lines
1532-1541; handlers `avg_strobe2`/`avg_strobe3`, no clipping hardware).

- **Clock**: `MASTER_CLOCK = 12,096,000` Hz (line 45). The 256x4 state PROM
  is stepped every 8 master cycles (`run_state_machine`, line 1271
  `cycles += 8`), i.e. **1.512 MHz PROM tick** (comment block lines
  1228-1245: "clocked with 1.5 MHz"). ms = cycles / 12096.0. (Note: the
  DESIGN.md guess of "likely 1.512 MHz" is the PROM tick; the timer/draw
  durations below count at the 12.096 MHz master clock.)
- **Per-op PROM tick counts**: traced through the real state PROM
  `136002-125.n4` (`aae/drivers/bwidow.cpp` line 568 — the same PROM for
  the whole bwidow/spacduel family; it is not part of the game's ROM set)
  with `avg_state_addr()` + the `avg_default` handler table:

  | op | PROM ticks | cycles (x8) | extra master cycles |
  |---|---|---|---|
  | VCTR | 8 (states 9,8,B,A,C,D,F,0) | 64 | `0x8000 - timer` |
  | HALT | 2 (9,8; halt visible at F) | 16 | — |
  | SVEC | 6 (9,B,C,D,F,0) | 48 | `0x100 - (timer & 0xFF)` |
  | STAT/SCAL | 7 (9,8,E,3,2,1,0) | 56 | — |
  | CNTR | 5 (9,8,C,F,0) | 40 | `0x8000 - timer` |
  | JSRL | 5 (9,8,C,D,E) | 40 | — |
  | RTSL | 4 (9,8,D,E) | 32 | — |
  | JMPL | 3 (9,8,E) | 24 | — |

  plus one idle PROM tick (8 cycles) after VGGO before the first latch.
- **The timer shift register** (draw durations, `avg_common_strobe3` lines
  934-965): starts at 0 each vector; the normalizer (`avg_strobe0`, lines
  660-698) shifts both 13-bit delta registers up until bit12 != bit11 of
  either (16-shift cutoff when dvx=dvy=0), each shift doing
  `timer = (timer>>1) | 0x4000 | (OP1<<6)`; the SCAL binary scale adds
  `bshift` more shifts (`avg_strobe1`, lines 740-756). SVEC (OP1 set) also
  ORs 0x80 per shift and masks the timer to its low byte. Net effect: a
  vector normalized by n total shifts draws for `0x8000 >> n` master cycles
  (SVEC: `0x100 >> n`-ish, exact low-byte arithmetic in `avg_timer_reg`).
  CNTR's low byte seeds dvy for the same normalizer, so `CNTR $8040` (the
  game's CenterBeamInMiddle) settles for 5 shifts -> 0x400 cycles = 84.7 us.

### Verification

- Ground truth: a byte-exact Python port of `run_state_machine` + the
  `avg_default` handlers driven by the **real PROM bytes** (scratchpad
  `avg_cycle_sim.py`). The per-op accounting above reproduces it
  **cycle-for-cycle on all 34 captured attract frames** (and the C
  implementation matches the Python twin's TIME line diff-clean).
- The existing avg_dump.exe vs avg_check.py segment cross-check still
  passes unchanged (`fc`: no differences; STOP line identical to the
  Gate-V baseline `words=32 time=156.3867`). Both sides now also print
  `TIME cycles=<n> ms=<x.xxxx>` and accept a `frame_NNNN.vram` argument;
  frame_0064 diffs clean across 456 segments + STOP + TIME.
- MSVC 2022 `/W4 /WX /std:c11` clean (avg.c, sd_vecrom.c, sd_state.c,
  tests/avg_dump.c).

### Frame measurements and the 45 FPS question

`frame_0064.vram` (attract, 708 instructions): **136,656 master cycles =
11.298 ms**. Across all captures: steady-state attract frames run
11.30-12.67 ms; the heaviest capture, frame_0576 (1106 instructions), is
**15.93 ms** — within 2% of the 16.26 ms frame gate (246.09 Hz IRQ / 4)
but still under it.

So by the cycle-true model **attract frames do NOT block past the frame
gate**: on hardware the attract mode paces at the full 61.5 Hz, and the
computed numbers are *not* consistent with 45 FPS for these lists. That is
not a clock or timer-model error (both are validated against the reference
state machine + real PROM above); the explanation is the third case:

1. **AAE's "45 FPS" is a declared constant, not a measurement** —
   `aae/drivers/bwidow.cpp` line 784 `AAE_DRIVER_VIDEO_CORE(45,0,...)` fixes
   the driver's nominal refresh at 45 fps for spacduel (the `fps` field of
   the driver struct). The observed ~45 FPS in MAME/AAE reflects this
   driver pacing, not an emergent VG-HALT wait.
2. **Play-mode lists plausibly DO exceed the gate**: attract already
   reaches 97.5% of the gate budget (frame_0576), and play adds ships,
   shots, score redraws and fuses on top of comparable backgrounds. A
   frame > 16.26 ms skips one gate tick (instantaneous 30.8 fps), and a mix
   of 1-gate and 2-gate frames averages out near 45 fps. No play-mode
   captures exist yet to confirm; when the oracle can capture in-game
   frames, walk them through `avg_frame_time_ms()` to settle this.

**Consequence for the playable build**: pace each frame as
`max(16.26 ms, avg_frame_time_ms())` quantized to the 4-IRQ gate (the
mainline waits for VG HALT, *then* for the gate tick), rather than assuming
a fixed 45 or 61.5 fps.

### Findings (no semantics changed)

- No geometry/semantics discrepancies against mame_late were found that
  affect Space Duel's lists. Two known-inert differences, for the record:
  - The hardware normalizer shifts the delta registers up before the DAC
    (bits 3-12); avg.c's float path keeps full precision instead. Position
    differences are sub-LSB; the segment cross-checks stay byte-identical.
  - mame_late treats JMPL/JSRL to word 0 as "frame ends" only for the
    Tempest/Quantum endless-loop style (`avg_common_strobe2`); avg.c/aae
    stop the walk there too (`AVG_STOP_JUMP0`) — Space Duel never emits it.
