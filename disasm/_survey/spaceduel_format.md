# Space Duel disassembly: format and tooling reference (for the Tempest port)

Source surveyed (read-only): `C:\Source2026\Space Duel Dissasembly\`
(`disasm\` = tools + generated listings, `c_src\` = C port). Everything in
`disasm\*.asm`, `vec_names.py` and `shapes_preview.html` is **generated**; the
Python scripts are the real source. Do not hand-edit listings; fix the tool and
regenerate.

Core idea: **the ROM is the oracle, Atari's MACRO-65 source is the transcript.**
Atari's own `.MAC` sources (github historicalsource/space-duel, project
"ASTERIODS 2") are parsed, each source instruction line is "locked" onto a ROM
address by checking the ROM decodes to the same mnemonic there, and then names
and comments are carried over. Every emitted line is re-encoded and
byte-compared with the ROM.

---------------------------------------------------------------------------

## 0. Environment check on this machine (2026-09-14)

| tool | result |
|---|---|
| `python --version` | **Python 3.14.7** (on PATH as `python`) |
| `where ca65` / `where ld65` / `where da65` | **not found** on PATH |
| Ophis (`where ophis`, `pip show ophis`) | **not installed** |

Consequences: the in-house pipeline (`emit.py` + `verify.py`) needs only
Python, stdlib only (argparse, hashlib, zipfile, re, subprocess). The
independent `--check` round trip needs cc65 (`ca65`, `ld65`) on PATH or
`--cc65 <bindir>`; it is not currently runnable here. The listings are Ophis
syntax but Ophis itself is never invoked by the tools (only claimed in the
header); the actual third-party check is ca65.

Tempest inputs already present: `C:\Source2026\Tempest Dissasembly\roms\tempest*.zip`
(MAME sets tempest, tempest1, tempest1r, tempest2, tempest3),
`tempest-main\tempest-main\*.MAC` (Atari source: ALEXEC, ALDISP, ALSCOR, ALSOUN,
ALWELG, ALVROM, ALVGUT, ALEARO, ALCOIN, ALTEST, ALCOMN, HLL65, VGMC, ASCVG,
COIN65, STATE2 ... plus **`TEMPST.LDA`, `ALEXEC.LDA` link images and `.MAP`
link maps** - the maps should give module bases directly, removing the need for
Space Duel's base-sweeping scripts), and
`disasm\Tempest Commented Source.txt` (1999/2004 McCormick commented dump,
program from $9000, RESET $D93F, IRQ $D704, $C000-$DFFF mirrored to $E000-$FFFF).

---------------------------------------------------------------------------

## 1. Pipeline

### 1.1 Directory layout / paths (`paths.py`)

```
<ROOT>/                      repo root
<ROOT>/roms/                 default ROM dir (gitignored); or --roms DIR / $SD_ROMS
<ROOT>/space-duel-main/      Atari source archive (gitignored); tools skip if absent
<ROOT>/disasm/               tools + listings
<ROOT>/disasm/build/         scratch (gitignored): spacduel_64k.bin, program_ca65.s, program.o, program.bin
<ROOT>/c_src/, c_src/tools/  C port + generators (gen_*.py)
```
`paths.py` constants: `DISASM, ROOT, CSRC, ARCHIVE, BUILD, IMAGE64K,
PROGRAM_ROM, VECTOR_ROM, DEFINES, VEC_NAMES, SHAPES_HTML`, the ROM table
`ROMS = [(socket, filename, cpu_base, size), ...]`, `CODE_LO/CODE_HI`
($4000/$8FFF), `VROM_LO/VROM_HI` ($2800/$3FFF). Helpers: `roms_dir()` (parses
`--roms` from sys.argv itself, so every tool accepts it), `archive_dir()`,
`archive(fn)`, `rom_bytes()` (loose file or zip member via gen_from_roms),
`load_roms()` -> `{addr: byte}` dict, `image64k()` (ROMs + mirror
$9000-$FFFF from $8000 page; writes build/…_64k.bin), `load_image64k()`.
Every script starts with `sys.path.insert(0, os.path.dirname(__file__))` and
imports siblings directly; all run from any cwd.

### 1.2 Driver: `gen_from_roms.py`

```
cd disasm
python gen_from_roms.py <rom-dir-or-zip>                      # build 64K image only
python gen_from_roms.py <rom-dir-or-zip> --listings           # whole chain
python gen_from_roms.py <rom-dir-or-zip> --listings --check   # + ca65/ld65 round trip
      [--cc65 <dir holding ca65/ld65>]
```
1. `build_image()` - ROMs matched by **part number regex**
   `^(\d{6})[.\-_](\d{3})(?!\d)` + **SHA-1 + length** (table `SHA1{}` joined with
   `paths.ROMS`), loose files before zip members; fills 64K, mirrors, writes
   `build/spacduel_64k.bin`.
2. `--listings` runs each tool as a subprocess (`python script --roms DIR`,
   failures reported with `!!`, chain continues):
   - `emit.py` -> `spaceduel_program_rom.asm` (needs archive)
   - `emit_defines.py` -> `spaceduel_defines.asm` (needs archive)
   - `verify.py` (reads listing back; exit 1 on mismatch)
   - `emit_vrom.py` -> `spaceduel_vector_rom.asm` (needs archive)
   - `emit_shapes.py` -> `vec_names.py`, `shapes_preview.html`
   - `c_src/tools/gen_*.py` (C port generated files; list `PORT_GENERATORS`)
3. `--check`: `emit_ca65.py` -> `build/program_ca65.s` (defines converted to
   `NAME = $XXXX` and prepended; `.org`/`.include` dropped), then
   `ca65 --cpu 6502 -o build/program.o build/program_ca65.s`,
   `ld65 -C disasm/flat.cfg -o build/program.bin build/program.o`,
   compare with image[$4000:$9000]; prints `ca65 round trip: N bytes, M mismatches`.
4. Ends with `git status --short --untracked-files=no -- disasm c_src`
   diff before/after ("checked-in files changed by this run").

`flat.cfg` (verbatim):
```
MEMORY {
    PROG: start = $4000, size = $5000, type = ro, fill = yes, fillval = $00, file = %O;
}
SEGMENTS {
    CODE: load = PROG, type = ro, start = $4000;
}
```
Only the program ROM is ca65-checked; the vector ROM listing uses AVG
pseudo-mnemonics with no macro definitions and is verified inside
`emit_vrom.py` at emit time (encode each decoded op, compare bytes; fall back
to raw `.word`).

### 1.3 Module dependency graph (what each script does)

| script | in | out / role |
|---|---|---|
| `m6502.py` | - | opcode tables `TAB[mn][mode]`, `SIZE`, `BRANCHES`, `decode(mem,a)->(mn,mode,val,len)`. Mode consts IMP ACC IMM ZP ZPX ZPY ABS ABX ABY IND IZX IZY REL. **Generic** |
| `encode.py` | m6502 | `encode(mn,mode,val,pc)->bytes`, `verify(mem,pc,mn,mode,val)`. **Generic** |
| `image.py` | paths | `load()` {addr:byte}; `lda(path)` RT-11 `.LDA` parser; `vectors(mem)` reads $8FFA-$8FFF (**SD-specific addrs**) |
| `trace.py` | m6502,image | recursive-descent tracer from RESET/IRQ/NMI: `starts, covered, instr, xrefs, drefs, indirect`. Stops at JMP/RTS/RTI/BRK; JMP() recorded not followed. **Generic** (range from image.CODE_LO/HI) |
| `rtexpr.py` | - | RT-11 expression evaluator: no precedence, left-to-right, `<>` grouping, radix 16 + `.` decimal suffix, `^H ^D ^O ^B`, `sym6()` truncates symbols to 6 chars. **Generic for Atari MAC** |
| `macsrc.py` | m6502 | line parser for MACRO-65: `Line(n, raw, labels[(name,is_global)], op, operand, comment, kind)` kind in blank/comment/label/assign/directive/insn/macro; `parse_file()` (latin-1, LF-only split); `operand_mode()` for DEC operands `I,v` `X,a` `Y,a` `A,a`(force abs) `Z,a`(force zp). **Generic** |
| `mapper.py` | macsrc,rtexpr,m6502 | `Mapper(mem, lines, start_line, org, extra_syms).run()` -> `addr{line->addr}`, `labels`, `label_at{addr->[(name,is_global,scope)]}`, `desyncs`, `locked`. Handles `.=`, `.CSECT/.PSECT`, `.REPT` (skips `.REPT 0` comment blocks), `.IF/.IFF/.IFT/.IFTF/.IIF/.ENDC`, `.BYTE/.WORD/.ASCII/.BLKB`, `MACRO_LEN` table (byte sizes of VGMC/HLL65F/AS2DEC macros), `MACRO_EXPAND` (DNEGATE etc. decoded by ROM), relock search ±64 bytes on desync. `INCLUDES=["AS2DEC.MAC","VGMC.MAC","HLL65F.MAC"]` for symbols (**SD file names; macro table needs Tempest's macros**) |
| `locate.py`, `modules.py`, `layout.py`, `solve_bases.py`, `sweep_free.py` | mapper | finding `.CSECT` module link bases (anchor by longest unique instruction run, fixed-point, anchor-free sweep). Result hard-coded as `KNOWN_BASE` in build_map. **Probably unneeded for Tempest if the .MAP files give bases** |
| `build_map.py` | all above | `build(srcdir) -> (mem, src{module:[Line]}, out{module:{line:addr}}, info)`. `MAIN=("ASTRD2.MAC", 709, 0x4000)` (file, first body line, org), `CSECTS=[...]`, `KNOWN_BASE{}`, `COIN_OPTS` (conditional-assembly option symbols for COIN65). `coverage()`. **Tables SD-specific, logic generic** |
| `rammap.py` | macsrc,rtexpr,mapper | walks main source lines `[:stop_line]` accumulating a PC over `.=`, `.BLKB/.BLKW/.BYTE/.WORD`, **skipping `.MACRO…ENDM` bodies**; returns `vars_{name:addr}`, `syms`; `storage_labels()` (declared storage outranks equal-valued equates) |
| `naming.py` | macsrc | `.SBTTL` harvest, `harvest()` inline/above comment prose, `name_for()`, `expand_identifier()` morpheme table (see §5) |
| `emit.py` | build_map, trace, rammap, naming, encode | program ROM listing (see §2) |
| `verify.py` | image, encode, rammap, emit.clean_symbols | read-back: regex `^L([0-9A-F]{4}):\s+(\S+)\s*(.*?)\s*(?:;.*)?$`; named labels on own lines attach to the next `Lxxxx` line; re-encode each instruction at its stated address (zp chosen when value<$100 and mode exists), `.byte` compared directly. Prints `verified bytes / instructions / data bytes / MISMATCHES`. **Generic** |
| `emit_ca65.py` | paths | Ophis -> ca65 translation (`.alias X $Y` -> `X = $Y`). **Generic** |
| `emit_defines.py` | rammap, emit, var_docs | defines file (see §2.6). `HARDWARE` table **SD-specific** |
| `var_docs.py` | - | `DOCS{name:(expected_addr, prose)}`, `COLLIDES="@"` marker, `lookup(name, addr)`. Hand-written content |
| `avg.py` | - | AVG decoder `decode(mem,a)->(mn, operand_text, len)`, `s13`, `s5`, `targets()`, `VG_BASE=0x2000`. **Generic Atari colour AVG** (Tempest uses the same AVG; its VG_BASE/ROM ranges differ) |
| `shippix.py`, `vecmap.py` (`ship_table`, `map_as2rom`) | avg, mapper | SD ship-picture format at $2800 + mapping AS2ROM.MAC labels to vector ROM. **SD-specific** |
| `rom_names.py` | build_map, avg | shape names from program-ROM immediates (`LALJSR/LXHJSR/...` macro operand + byte at addr+1), the 32-word `RSOURC` JMPL table (pass2), JSRL word tables copied to vector RAM (pass3), `jsrl_word_run()`, `rom_tail()` (zero-fill + checksum). **SD-specific heuristics, reusable ideas** |
| `vshapes.py` | avg, vecmap | block segmentation of $3000-$3FFF (split at RTSL/HALT/JMPL) + source-order names (suffix `?`) |
| `shapes.py` | vshapes, rom_names | `resolve(mem)->(blks, names{addr:[names]}, secondary, glyphs, desc, (fill,cksum))`, priority: glyph tables > ROM immediates/RSOURC > JSRL word tables > source order `?`. `GLYPH_TABLES=[(0x324A,"CHR"),(0x3458,"CHRF")]` with ASCVG order 0 blank, 1..10 digits, 11..36 A..Z. `xrefs(mem)` |
| `emit_vrom.py` | avg, shapes, shippix, rom_names | vector ROM listing (see §2.5) |
| `emit_shapes.py` | shapes, shippix, avg | `vec_names.py` + `shapes_preview.html` (see §3); `avg_segments()` renderer model, `TERMINATORS=("RTSL","HALT","JMPL")`, `scal_factor()` |

Run order for development without the driver:
`python emit.py --roms R && python emit_defines.py --roms R && python verify.py --roms R && python emit_vrom.py --roms R && python emit_shapes.py --roms R`
(`build_map.py`, `trace.py`, `rammap.py`, `avg.py`, `naming.py` also have
`__main__` diagnostics printing coverage/census.)

---------------------------------------------------------------------------

## 2. Output .asm format

Syntax: **Ophis** (`.org`, `.include`, `.alias NAME $XXXX`, `.byte`, `.word`,
`label:`), upper-case mnemonics, `$` hex. Comments start with `;` with no space
after it. No segments (single `.org`); ld65 places code for the ca65 check.

### 2.1 Program ROM file header (verbatim, `spaceduel_program_rom.asm`)

```
;Space Duel (Atari, 1982) - annotated disassembly of the program ROMs.
;Reconstructed against the MAME 'spacduel' set (rev 2) and cross-checked line
;by line against Atari's own source archive (project 'ASTERIODS 2').
;Every instruction below was re-encoded and byte-compared with the ROM.
;Assembles with Ophis; disasm/gen_from_roms.py --check round-trips it with ca65.

.org $4000

.include "spaceduel_defines.asm"

```

### 2.2 Line format and labels

- **Every** line of code/data starts with an address label `Lxxxx:` (upper hex)
  then two spaces. Instruction: `"L%04X:  %-4s %-22s ;comment"` (trailing
  whitespace stripped if no comment).
- Named labels sit on their **own line**, preceded by a blank line, directly
  above the `Lxxxx` line: `NAME:`. There is **no separate routine header
  comment block** in Space Duel's program listing - the "header" is just the
  blank line + descriptive name; all prose is inline source comments.
- Global names: CamelCase descriptive (from `.SBTTL` prose / comments /
  morpheme expansion) or Atari's identifier CamelCased (`Poweron`, `Gtoptn`).
- Local labels (`10$` in source) -> `<EnclosingRoutine>_<num>` e.g.
  `Start2_14`, `CheckForStartEnd_65`.
- Trace-only targets with no source name stay `Lxxxx` (no extra label line).
- Duplicates get `_%04X` suffix.
- Operands use labels for code targets, defines names for RAM/IO
  (`LDA HALT`, `STA WTCHDG`, `LDA ENTER,X`), raw `$XXXX` otherwise, `#$XX`
  immediates (always hex, never symbolic).
- Inline comment = Atari's original source comment verbatim (upper case),
  column-aligned after the operand field.

Verbatim sample (routine with locals, comments):
```

Start2:
L4012:  JSR  Gtoptn
L4015:  LDA  HALT                   ;CHECK FOR SELF TEST
L4018:  AND  #$10
L401A:  BNE  Start2_6
L401C:  JMP  AllStopPlease          ;GO DO SELF TEST

Start2_6:
L401F:  BIT  HALT
L4022:  BVC  Start2_6
L4024:  JSR  DoLowOnesEvery

Start2_8:
L4027:  LSR  SYNC
L4029:  BCC  Start2_8               ;NOT 1/60 SEC YET
```
```

Irq:
L8639:  PHA
L863A:  TYA
```

### 2.3 Data in the program ROM

- Anything not a verified source instruction and not a traced instruction
  start is data: `"L%04X:  .byte $XX, $XX, ..."` **8 bytes per line**, each
  line labelled with its own start address. A named label inside data flushes
  the run and inserts `\nNAME:`.
- No `.word` jump tables, no text strings in the program listing: pointer
  tables and message text are plain `.byte` runs (names label them where the
  source had a label, e.g. `Offset:`, `Checksum4000:`, `Atari:`).
- CPU vectors at the end are just the last `.byte` rows
  (`L8FFB:  .byte $80, $3F, $80, $39, $86`).
- **Forced absolute** (ROM uses 3-byte abs for an address < $100; an
  assembler would shrink it): emitted raw with mnemonic in comment:
  `L6086:  .byte $BD, $DD, $00    ;LDA HSCORE,X (forced absolute)` (append
  ` - <source comment>` when present).

Sample:
```
Checksum4000:
L4000:  .byte $42

Atari:
L4001:  .byte $02, $BB, $5A, $30, $5F, $EE, $7D, $A8
L4009:  JMP  Poweron
```

### 2.4 Which instruction lines are "from source"

`emit.verified_insn`: a source line mapped to address a is used only if
`m6502.decode(mem,a)` mnemonic == source op and `encode.verify` reproduces the
ROM bytes. Otherwise, if the tracer reached `a`, a plain disassembly line
without comment is emitted; otherwise data. Stats printed:
from_source / from_trace / data / labels / lines.

### 2.5 Vector ROM file (`spaceduel_vector_rom.asm`)

Header (verbatim excerpt):
```
;Space Duel (Atari, 1982) - vector ROMs, byte-exact listing.
;
;  $2800-$2FFF  136006-106  ship PICTURE data - NOT AVG. A 34-entry
;  ...
;AVG deltas are 13-bit TWO'S COMPLEMENT (see aae_avg.cpp twos_comp_val
;and VGMC.MAC's .WORD DY&^H1FFF) - not the sign-magnitude of the DVG.
;Space Duel is the colour XY board, so COLOR replaces STAT.
;Every line below was re-encoded and byte-compared with the ROM.
;Shapes are labelled with Atari's own names; see vec_names.py for the
;name table and shapes_preview.html for rendered previews.

.org $2800
```
- Address labels are `Vxxxx:` (not `L`).
- Section banners: `;------------------------[ ship picture pointer table ]------------------------`,
  `[ ship pictures ]`, `[ AVG display lists ]`, `[ ROM tail ]`.
- Pointer tables: `V2800:  .word $2844              ;[ 0] -> SHPA0`.
- Non-AVG record data: `V2846:  .byte $F0, $18          ;dy=-16   dx=24    ship`.
- AVG ops, one per line, pseudo-mnemonic + decoded operands + raw word(s) hex:
  `"V%04X:  %-6s %-22s ;%s"`:
```
BOXES:           ;JSRL operand $800
;  refs: vector ROM $3AD5
;  refs: main ROM $4F50
;  refs: ... and 1 more
V3000:  JSRL   CNTSCL                 ;AF61
V3002:  VCTR   -510, 248, 0           ;00F8 1E02
V3006:  VCTR   1022, 0, 1             ;0000 23FE
V3026:  RTSL                          ;C000

CHR_A:           ;JSRL operand $86E
V30DC:  SVEC   0, 16, 1               ;4820
V30EA:  RTSL                          ;C000
```
  Mnemonics/operands: `VCTR dx, dy, z` (4 bytes), `SVEC dx, dy, z`, `SCAL b, $ll`,
  `COLOR $c, lum` (colour board: high byte $64), `STAT $xxxx`, `CNTR`, `HALT`,
  `RTSL`, `JSRL name|$addr`, `JMPL name|$addr` (target = VG_BASE + 2*(w&$1FFF)).
- Shape label line: `NAME:` padded to col 17 then `;JSRL operand $XXX`
  ((addr-$2000)/2), optionally `-- name from AS2ROM source order only, UNVERIFIED`;
  then comment lines: `;  <description>`, `;  secondary entry points: ...`,
  `;  no lit vectors - beam positioning / clip / advance list,` (+ `;  not a drawable shape; renders blank by design`),
  `;  JSRL word table (N entries) - the 6502 copies one entry ...`,
  `;  refs: main ROM $XXXX` (max 6, then `;  refs: ... and N more`).
- Ops that don't round-trip through `emit_vrom.encode()`: `V30AC:  .word $C09B              ;RTSL `.
- Glyph tables are just runs of `JSRL CHR_x` lines.
- Tail: zero fill as `.byte` 16/line, then `CKUM1:` / `V3FFF:  .byte $25                    ;ROM checksum`.
- Encoding (emit_vrom.encode): VCTR -> `[dy&$1FFF, (z<<13)|(dx&$1FFF)]`;
  SVEC -> `$4000|(z<<5)|((dx>>1)&$1F)|(((dy>>1)&$1F)<<8)`; SCAL -> `$7000|(s<<8)|ll`;
  COLOR -> `$6400|(lum<<4)|col`; JSRL/JMPL -> `$A000/$E000 | ((t-$2000)/2)`.

### 2.6 Defines file (`spaceduel_defines.asm`)

Header comment block explaining provenance, then sections with banner
`";" + "-"*34 + "[ Title ]" + "-"*34`:
`Memory map`, `Inputs`, `Outputs` (hand table `HARDWARE` in emit_defines.py,
descriptive CamelCase names), then auto sections from the recovered symbol
table by address range: `Zero page variables` ($00-$FF), `Stack page`,
`Game RAM`, `Hardware registers` ($0400-$1FFF, Atari equate names like
`WTCHDG`, `INTACK`), `Vector RAM`, `Vector ROM`. **Every symbol the program
listing references must be defined here** (ca65 caught 7 missing ones).
```
;----------------------------------[ Memory map ]----------------------------------
.alias ZeroPageRam      $0000    ;Through $00FF.
.alias VgGo             $0C80    ;Write starts the AVG on the display list.

;----------------------------------[ Zero page variables ]----------------------------------
.alias VGBRIT           $00      ;Vector brightness used by VGSTAT: 0 = off, $F0 = maximum, in steps of $20.
.alias EAC2             $02      ;VGLIST+1, the high byte of the display-list write pointer. EAC2 is AS2DEC's EAROM C2 control bit.
.alias SCORE            $3A      ;Player scores in BCD, 3 bytes each (low, middle, high) for players 0 and 1, so $3A-$3F.
```
Format: `".alias %-16s $%0*X"` (2 hex digits below $100, else 4), comment
padded to col 32/33. RAM names stay **Atari's 6-char identifiers** (not
CamelCased). Where an equate with the same value outranks the real variable,
the comment starts with the true identity (`VGLIST+1. ...`) - `COLLIDES` in
var_docs. The C port's `gen_state.py` parses this file keyed on the section
headings, so keep the headings identical.

---------------------------------------------------------------------------

## 3. `shapes_preview.html`

Generated by `emit_shapes.emit_preview()` (run `python emit_shapes.py`).
Single self-contained static page, no external resources:
- `<title>Space Duel vector objects</title>`, black background, green monospace
  CSS; `<h3>` count line ("34 ship pictures ($2800-$2FFF) + 98 AVG shapes ($3000-$3FFF)").
- One card per object (186 total): `<div class='s'><canvas id='cV30DC' width='104' height='104'></canvas><br>CHR_A<br>30DC</div>`
  (+ `<br><span class='d'>no lit vectors</span>` if all dark). Key `P%04X` for
  ship pictures, `V%04X` for AVG blocks and named entries inside blocks
  (glyphs), sorted by address.
- `<script>var S={}; S['V30DC']=[[x0,y0,x1,y1,lit],...]; ...` then a loop that
  bounding-boxes **lit** segments only, scales to fit 92px, flips Y, strokes
  green lines.
- Segments come from a Python AVG model `avg_segments()`: follows JSRL/JMPL
  (depth<6, into $3000-$3FFF), VCTR/SVEC accumulate scaled deltas
  (floats, formatted `%g` to 3 dp - never int()), CNTR resets to 0,0, SCAL sets
  `((~lin)&0xFF)/256/2^b`, initial scale = SCAL 1,$00 (≈0.498), lit = z>0,
  stop at RTSL/HALT/JMPL. Colour is ignored.
- Lessons recorded (NOTES.md): two's complement not sign-magnitude; no Python
  True/False into JS; don't int() coordinates; ambient scale; bbox over lit
  only; follow JSRL. Byte round-trip does not catch decoder semantics bugs -
  the preview is the semantic check.

`vec_names.py` (also generated): `VEC_NAMES = { 0x30DC: 'CHR_A',  # vec=7 refs=4 ...}`,
placeholder `SHAPE_xxxx` for unnamed.

---------------------------------------------------------------------------

## 4. Space-Duel-specific vs reusable

Copy nearly verbatim (change strings/constants only):
- `m6502.py`, `encode.py`, `trace.py`, `verify.py`, `emit_ca65.py`,
  `rtexpr.py`, `macsrc.py`, `avg.py` (same Atari AVG; Tempest is colour AVG too),
  the `gen_from_roms.py` framework (index/zip/SHA-1/run/check/git-status),
  `paths.py` structure, `naming.py` algorithm, `emit.py` main loop,
  `emit_defines.py` layout, `emit_shapes.py` preview HTML + `avg_segments()`.

Must adapt for Tempest:
- `paths.py`: `ROMS` table (Tempest program $9000-$DFFF, mirrored to $FFFF;
  vector ROM ranges; different part numbers 136002-xxx), `CODE_LO/HI`,
  `VROM_LO/HI`, file names (`tempest_program_rom.asm` etc.), `IMAGE64K`,
  `ARCHIVE` (-> `tempest-main\tempest-main`), env var `SD_ROMS`.
  Mirroring logic in `build_image`/`image64k` (SD mirrors $8000 page up; Tempest
  mirrors $C000-$DFFF to $E000-$FFFF per the commented source).
- `gen_from_roms.py`: `SHA1` table, `ROM_NAME_RE` (Tempest MAME names, which set:
  tempest = rev 3?), `PORT_GENERATORS`, ld65 slice `[0x4000:0x9000]`.
- `image.vectors()` hard-codes $8FFA-$8FFF -> use $FFFA-$FFFF.
- `flat.cfg`: start/size.
- `build_map.py`: `MAIN`, `CSECTS`, `KNOWN_BASE`, `COIN_OPTS`; Tempest likely
  gets bases from `ALEXEC.MAP`/`TEMPST.LDA` (`image.lda()` already parses LDA).
- `mapper.py`: `INCLUDES` (Tempest: ALCOMN? HLL65.MAC, VGMC.MAC), `MACRO_LEN`/
  `MACRO_EXPAND` for Tempest's macros, CODE range in `relock`.
- `rammap.py`: main file + `stop_line` (line where RAM declarations end).
- `naming.py`: `build()` file list; `ABBREV` and `MORPHEMES` are SD vocabulary
  (Comet, Saucer, Rock...) - replace with Tempest vocabulary (Well, Flipper,
  Tanker, Spiker, Fuseball, Pulsar, Zap, Cursor, Nymph, Superzapper ...).
- `emit.py` header strings, `.include` name; `emit.clean_symbols` generic.
- `emit_defines.py` `HARDWARE` table and group ranges (Tempest RAM $0000-$07FF,
  colour RAM, Mathbox, two POKEYs, EAROM, vector RAM/ROM - verify from MAME /
  source before writing).
- `var_docs.py`: all content.
- `shippix.py`, `vecmap.py`, `rom_names.py` passes 2/3, `shapes.GLYPH_TABLES`,
  `emit_vrom.py` section layout (SD ship pictures at $2800): SD-specific. For
  Tempest write equivalent resolvers from ALVROM.MAC / ASCVG.MAC labels and
  program-ROM JSRL immediates. `rom_names.pass1` idea (macro operand -> byte at
  mapped address) is reusable. Keep `rom_tail()` idea for padding/checksum.
- `emit_vrom.encode` + `avg.decode`: check Tempest's AVG word variants
  (STAT vs COLOR high byte, SCAL) against VGMC.MAC in tempest-main.
- `locate/modules/layout/solve_bases/sweep_free.py`: only needed if link maps
  don't give bases.

---------------------------------------------------------------------------

## 5. How original-source names/comments were merged

1. **Parse** each `.MAC` with `macsrc.parse_file` (Line objects with labels,
   `is_global` = `::`, comments split at `;`).
2. **Lock** with `mapper.Mapper` (absolute body at `.=4000` maps directly; each
   `.CSECT` module mapped from its solved link base). Result
   `out[module][line] = addr` and `label_at[addr]`. ASTRD2: 5,065/5,065
   locked; COIN65 has desyncs from conditional assembly (option symbols fed in
   via `extra_syms`).
3. **Comments**: `emit.source_comment(line)` = the source line's own `;`
   comment, stripped, placed only on instructions whose bytes verify.
   Full-line source comments and block comments are *not* copied into the
   listing (only used for naming prose).
4. **Label names** (`emit.build_labels`), address-keyed:
   - pass 1 globals via `naming.name_for(name, True, ...)`, priority:
     (a) `.SBTTL NAME-DESCRIPTION` prose -> `camel()` with `ABBREV`, drop
     leading Do/Go/The, max 4 words, `trim_filler` trailing filler words;
     (b) label's inline comment or up to 3 full-line comments directly above
     (`naming.harvest`), STOP words removed, max 4 words;
     (c) `expand_identifier()` strict full decomposition into `MORPHEMES`
     (+ trailing L/H -> Lo/Hi, digits kept), refuse partial matches;
     (d) CamelCased Atari identifier.
   - pass 2 locals `10$` -> `<scope global name>_10`.
   - pass 3 tracer-only targets -> `Lxxxx`. Uniqueness via numeric suffix / `_ADDR`.
5. **RAM names**: `rammap.build` walks `.BLKB` declarations -> Atari
   identifiers (not expanded); `emit.clean_symbols` picks one name per address
   (declared storage > equate, then shortest), unique.
   `var_docs.DOCS` supplies hand-edited prose from the declaration comments.
6. **Vector shape names**: `shapes.resolve` priority list in §1.3; weakest
   (source-order walk) marked `?` / `UNVERIFIED`.
7. Principle: "no invented data" - names are Atari's or strict expansions;
   ROM wins over source whenever they disagree; findings documented in NOTES.md
   (e.g. rev differences: source is rev 1, ROM rev 2, one byte differs).

For Tempest there are two name sources: the Atari `.MAC` archive (primary,
lock like SD) and `Tempest Commented Source.txt` (a 1999-2004 commented
disassembly dump, not assembler source) - the latter has no SD equivalent; it
could supply descriptions/routine comments keyed by address where the Atari
source is silent. The Goal also asks for a brief description comment per
function, which SD's program listing does **not** have (SD relies on
descriptive names + inline comments only) - Tempest will need a small
extension: e.g. a `;` comment block between the blank line and `NAME:` taken
from `.SBTTL` prose / above-label comment block, verified not to break
`verify.py` (it ignores `;` lines, so it is safe).

---------------------------------------------------------------------------

## 6. C port (`c_src\`) overview

1. Plain C11, VS2022 (`build_win.bat` -> `sd_win.exe`; `build_all.bat` also
   headless probes in `tests\`), /W4 clean, no 6502 core at runtime.
2. One C function per named listing routine, snake_case
   (`CheckForStartEnd` -> `check_for_start_end()`), comment with label + ROM
   address (`/* CenterBeamInMiddle ($8EA1): ... */`); locals become control
   flow; unnamed `Lxxxx` -> `sub_xxxx()`. Rules in `CONVENTIONS.md`, design in `DESIGN.md`.
3. Modules mirror Atari source modules: `mainline.c`, `objects.c`, `score.c`,
   `msgs.c`, `lowones.c`, `display.c`, `vgutil.c` (VGUTR2), `sound.c`,
   `coins.c`, `earom.c`, `irq.c`, `selftest.c`, each with `NOTES_<module>.md`.
4. All machine state in one global `sd_state g` (`ram[0x400]`, `vram[0x800]`);
   `sd_state_defs.h` generated from `spaceduel_defines.asm` by
   `tools/gen_state.py` (`#define A_NAME 0xADDR`, `#define NAME (g.ram[A_NAME])`
   for variable sections only).
5. Const tables never hand-typed: `tools/gen_*_data.py` extract them from
   `disasm/build/spacduel_64k.bin` -> `*_data.c`; `gen_progrom.py`/`gen_vecrom.py`
   emit full ROM arrays (`sd_progrom.c`, `sd_vecrom.c`). Run by `gen_from_roms.py --listings`.
6. Display: translated VG utils write real AVG words into `g.vram`; `avg.c` is a
   C AVG state machine producing line segments.
7. Hardware only through the `sd_hw_*` seam (`sd_hw.h`); implementations in
   `app_loop.c` (game) and `platform/headless` (probes); `platform/windows`
   = Win32/OpenGL beam renderer, XAudio2 mixer, ini, joystick (vendored framework).
8. Chips: `c012294.c` POKEY (shared verbatim with AAE/Atari800 projects),
   `er2055.c` EAROM (copied from Asteroids Deluxe port).
9. Verification: `tools/oracle.py` (a Python 6502 sim running the real 64K
   image, snapshotting RAM+vector RAM at every VgGo) + probes that byte-diff
   the C port per frame (`tests/ref*` dumps). For Tempest, per the user's
   CLAUDE.md, the oracle must reuse `C:\Source2026\shared\ref6502\` (or the AAE
   cpu_6502.cpp), not a new 6502 core.
10. Tempest additionally has a Mathbox co-processor (see `MBUCOD.*`/`MBUDOC.DOC`
    in tempest-main) that the C port will need to model or translate - no SD
    counterpart. Verify its role from the source before planning.
