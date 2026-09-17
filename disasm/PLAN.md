# Tempest disassembly plan

Primary ROM: **rev 3** (tempest3.zip == tempest.zip CPU image). Matches `ALEXEC.LDA` (the `*2` .MAC variants + common modules) and `Tempest Commented Source.txt` byte-for-byte.
Format: copy Space Duel (`disasm/_survey/spaceduel_format.md`); inputs survey: `disasm/_survey/tempest_inputs.md`.
Tools: Python 3.14; cc65 V2.19 at `../cc65-win32/bin` (ca65/ld65 for `--check`).

## Phase 1 — disassembly (agents)
| Track | Owner files | Output |
|---|---|---|
| A. MAC65-subset assembler + symbol/comment extraction | `disasm/macsrc.py`, `disasm/macasm.py`, `disasm/build/symbols.json` | every label/equate/RAM name with rev-3 address, verified by assembling to the ROM bytes |
| B. Vector ROM + graphics/scripts HTML | `disasm/avg.py`, `emit_vrom.py`, `emit_shapes.py`, `cam.py` | `tempest_vector_rom.asm`, `shapes_preview.html` (vector pictures + CAM motion scripts) |
| C. Program ROM listing + defines (after A) | `emit.py`, `emit_defines.py`, `verify.py`, `emit_ca65.py`, `gen_from_roms.py` | `tempest_program_rom.asm`, `tempest_defines.asm`; function header comments; `--check` via ca65 |

Naming: original source labels first; descriptions from `.SBTTL`, source comments, then Commented Source.txt remarks merged by address.
Function header comment = `; ---` block with name + 1-2 line description (must not break verify.py).

## Phase 2 — C port
Modeled on `C:\Source2026\Space Duel Dissasembly\c_src`; 6502 reference = `C:\Source2026\shared\ref6502`. Add Mathbox.
