# Tempest (Atari, 1981) - annotated disassembly, revision 3

Byte-exact, fully labelled listings of the Tempest rev-3 CPU ROMs (MAME `tempest` /
`tempest3`), built by re-assembling Atari's own source and checked against the ROM
image twice: by our own read-back verifier and by a ca65/ld65 round trip.
The format follows the
[Space Duel disassembly](https://github.com/tcottrill/Space-Duel-Full-Disassembly-and-C-Port).

[`shapes_preview.html`](shapes_preview.html) draws every vector object the
listings describe and runs the CAM enemy-motion scripts live
([open it](https://tcottrill.github.io/Tempest-Full-Disassembly-and-C-Port/disasm/shapes_preview.html)).

## Regenerate and verify

```
cd disasm
python gen_from_roms.py --check                 # ROMs from ../roms, cc65 from ../cc65-win32/bin
python gen_from_roms.py ../roms/tempest.zip --check --cc65 C:\path\to\cc65\bin
```

`gen_from_roms.py` finds the rev-3 chips by part number + CRC32 (2K set `tempest3.zip`
or 4K set `tempest.zip`, loose files also work), writes
`_survey/roms_extracted/tempest3_cpu64k.bin`, then runs:

1. `macasm.py` when its outputs are missing/stale (or `--macasm`); `commented_src.py` if needed
2. `emit_vrom.py`, `verify_vrom.py`, `emit_shapes.py` (vector ROM track, when present)
3. `emit.py` -> `tempest_program_rom.asm`, `emit_defines.py` -> `tempest_defines.asm`
4. `verify.py` - must print `MISMATCHES : 0`
5. with `--check`: `emit_ca65.py` - must print `ca65 round trip: 20480 bytes, 0 mismatches`

Any failing step prints `!!` and the exit status is 1. The individual scripts can also be
run on their own in that order.

Current result: verify 20480/20480 bytes, 0 mismatches; ca65 round trip 20480 bytes,
0 mismatches; 299 routine headers (271 source descriptions, 67 of them from the comments on
the calls; 2 from the Commented Source; 26 written for this disassembly).

## Files

| File | What |
|---|---|
| `tempest_program_rom.asm` | program ROM `$9000-$DFFF` (+ note on the `$E000-$FFFF` mirror and the vectors at `$DFFA`) - generated |
| `tempest_defines.asm` | memory map, hardware registers, RAM variables, vector RAM/ROM symbols, constants - generated |
| `tempest_vector_rom.asm`, `shapes_preview.html` | vector ROM `$3000-$3FFF` and rendered pictures / CAM scripts (vector ROM track) |
| `gen_from_roms.py` | driver (above) |
| `macasm.py`, `macsrc.py`, `rtexpr.py`, `m6502.py` | MAC65-subset assembler for Atari's source; writes `build/symbols.json`, `build/srclines.json`, `build/macasm_report.md` |
| `emit.py` | program listing generator; also `build/program_refs.json` (identifiers used, routine list) |
| `routine_docs.py` | hand-written routine descriptions (used only where the sources say nothing) |
| `emit_defines.py` | defines generator |
| `verify.py` | independent read-back of the two .asm files, re-encodes every line and compares with the image |
| `emit_ca65.py`, `flat.cfg` | ca65 translation (`build/program_ca65.s`), assemble + link (`build/program.bin`), compare |
| `commented_src.py` | parses `Tempest Commented Source.txt` to `build/commented_src.json` |
| `avg.py`, `vromsrc.py`, `vrender.py`, `vtables.py`, `cam.py`, `emit_vrom.py`, `verify_vrom.py`, `emit_shapes.py` | vector ROM track; `emit_shapes.py` also writes `build/vector_tables.json` and `build/cam_scripts.json`, which `emit.py` uses to annotate tables |
| `_survey/` | input surveys (ROM sets, memory map, source dialect, Space Duel format) |

Inputs, none of them distributed here: a ROM set (`../roms/*.zip`, or the path given on
the command line); Atari's source archive,
[historicalsource/tempest](https://github.com/historicalsource/tempest), unzipped so the
`.MAC` files are at `../tempest-main/tempest-main/`; `Tempest Commented Source.txt` in this
directory (see Credits); [cc65](https://cc65.github.io) for `--check` (`../cc65-win32/bin`,
`--cc65 DIR`, or on PATH). Hardware notes came from the Tempest driver of the
[AAE emulator](https://github.com/tcottrill/AAE).

## Conventions (program listing)

- Syntax as Space Duel's listing (Ophis style): `.org`, `.include`, `.alias NAME $VALUE`,
  `.byte`, `.word`, upper-case mnemonics, `$` hex. `[ ]` groups expressions, `~` is one's
  complement, `|` is OR, `<`/`>` low/high byte. `emit_ca65.py` shows the only syntax
  changes ca65 needs.
- Every code/data line starts with its address label `Lxxxx:`. Named labels sit on their
  own line above it. A label that falls inside a line is an `.alias` right after that line.
- Names are Atari's (6 significant characters in MAC65, so `QDSTATE` = `QDSTAT`),
  with `.` -> `_` and `$` -> `S_` (`CHAR.A` -> `CHAR_A`, `$$CRDT` -> `S_S_CRDT`).
  Local `10$` labels become `SCOPE_10`. A name defined twice with different values keeps
  its name for the main definition; a second ROM/RAM label gets `_ADDR`
  (`VGCNTR_AB0D`, `NOOPR_C7D9`).
- Operands are the source's own expressions with the source's symbols
  (`LDA #CDPLAY`, `STA QDSTATE`, `LDA CAM+1,Y`, `.word JEXIT-1`, `.byte NOJUMP-CAM-1`).
  MAC65 evaluates left to right without precedence; the listing makes that explicit
  with `[ ]`. An expression is used only if it yields exactly the ROM bytes; otherwise the
  operand is hex and the source statement is kept as `(src: ...)` in the comment.
  Numbers-only expressions are printed as hex. Branch targets are always labels.
  The HLL65 `LDAL/LDAH/LXL` byte-pair macros appear as `LDA #<X` / `LDA #>X`.
- Where the ROM uses an absolute address below `$0100` (an assembler would pick zero
  page) the bytes are raw: `.byte $B9, $48, $00 ;LDA LIVES1,Y (forced absolute)`.
- Data keeps the source's statement layout (one line per source statement, at most 8 bytes
  or 4 words per line); macro-generated data carries the macro call as its comment
  (`;VSLOOP 8`, `;MESS GAMOV,GREEN,1,56  GAME OVER`, `;ASCVH <GAME OVER>`).
  Pointer and jump tables are `.word` with names; messages get a decoded-text note;
  CAM scripts are grouped per script with Atari's names and a one-line description;
  table formats (CAM opcodes, message tables, between-point pictures, COLTAB) are noted
  at the table labels (from `build/cam_scripts.json` / `build/vector_tables.json`).
- Module banners (`; MODULE ALWELG ...`) mark each linked source module; `;.SBTTL ...`
  lines carry Atari's section titles where no routine header does.
- Comments: upper case = Atari's source comment (verbatim); for macro statements the
  macro call comes first (`;IFMI`, `;MIEND`, `;CSEND` show the HLL65 structure).
  `[CS]` = remark from *Tempest Commented Source* at that address. Comment-only lines
  (`;`) from Atari's comment blocks are kept above the statement they precede.
- Routine headers (a `;----` block before each routine entry: a code label that is a
  JSR/JMP target, a jump-table entry, starts a module, or follows RTS/RTI/JMP/BRK):
  `; NAME - description`. Description sources, in priority order:
  1. Atari source: the `.SBTTL` title starting there, the label's own comment, the comment
     block above the label; else the comment on the calls to it, marked
     `(comment at the call)`;
  2. `[CS]` Commented Source block header at that address;
  3. `[note]` written for this disassembly from reading the code (`routine_docs.py`).
  Commented Source block headers also appear inside headers that have a source description.
  Labels reached only by falling through (e.g. the `ZATCxS`/`ZATCxE` checksum-range
  markers) get no header.
- `$DFF8-$DFF9` are not assembled by any statement (ROM fill `00 00`).
  `$E000-$FFFF` has no ROM of its own; it mirrors the top program ROM, so the CPU
  vectors are the `.VCTRS` words at `$DFFA` (NMI/IRQ -> `IRQ` `$D704`, RESET -> `RESET` `$D93F`).

## Conventions (defines)

Sections as Space Duel: `Memory map` (descriptive names), `Hardware registers` (Atari I/O
equates with bit notes), `Zero page variables`, `Stack page`, `Game RAM`, `Vector RAM`,
`Vector ROM`, `Constants` (all ALCOMN constants plus every module equate the listing uses,
grouped by file and `.SBTTL`). Format `.alias NAME $VALUE ;comment`; comments are Atari's.
Every identifier the program listing uses is defined here.

## Credits

- **Tempest** program and source code: Atari, Inc., 1981 (Dave Theurer; the vector macros
  VGMC and character set ANVGAN by Ed Logg, ASCVG by Rich Moore, COIN65 by Downend &
  Albaugh, Mathbox microcode by Mike Albaugh), source archive
  [historicalsource/tempest](https://github.com/historicalsource/tempest) (`tempest-main`).
- **Tempest Commented Source**: documentation Copyright 1999 by Arcade Gameshop
  Corporation (OpenContent License), updated 09/17/2004 by Josh McCormick (project lead and
  main documentarian), with Clay Cowgill (EAROM example, vector ROM image list, MAME tips)
  and Ken Lui (data segments, assembler/disassembler package). Its remarks are quoted
  with the `[CS]` tag.
- Hardware notes: the AAE Tempest driver (`tempest.cpp`).
- Listing format and tooling pattern: the Space Duel disassembly project.
