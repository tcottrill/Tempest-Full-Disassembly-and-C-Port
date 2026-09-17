# Tempest (Atari 1981) - input survey for the disassembly / C port

Survey date 2026-09-14. Everything below was checked against the actual files
(Python scripts over the zips, the .LDA load files, the .MAC source, and the AAE driver),
unless marked "unverified".

Paths (project root `C:\Source2026\Tempest Dissasembly\`):

| Input | Path |
|---|---|
| ROM sets (MAME-style zips, untouched) | `roms\tempest.zip, tempest1.zip, tempest1r.zip, tempest2.zip, tempest3.zip` |
| Extracted ROM files | `disasm\_survey\roms_extracted\<set>\*` |
| Assembled 64K CPU images (0xFF fill) | `disasm\_survey\roms_extracted\<set>_cpu64k.bin` |
| Decoded Atari load files | `disasm\_survey\roms_extracted\ALEXEC.LDA.bin`, `TEMPST.LDA.bin` |
| Third-party commented disassembly | `disasm\Tempest Commented Source.txt` |
| Original Atari source + build files | `tempest-main\tempest-main\*.MAC, *.MAP, *.LDA, *.DOC, *.DAT, *.COM` |
| AAE emulator driver (hardware reference) | `tempest.cpp`, `tempest.h` |

(There is also an existing `c_src\` folder with C files such as `avg.c`, `earom.c`,
`c012294.c`, `app_loop.c`. I did not look at it for this survey.)

---

## 0. Headline findings

1. **The original source matches the ROMs byte for byte.** `ALEXEC.LDA` (the Atari
   linker output) is 100% identical to the `tempest` / `tempest3` CPU image (24574 of 24574
   loaded bytes). `TEMPST.LDA` is 100% identical to `tempest1` (24546 of 24546).
2. **The "2" source variants are the later build.** `ALSCO2, ALDIS2, ALHAR2, ALTES2` +
   common modules = `ALEXEC.LDA` = rev 3 (`tempest`, `tempest3`).
   `ALSCOR, ALDISP, ALHARD, ALTEST` + common modules = `TEMPST.LDA` = rev 1 (`tempest1`).
3. **The commented disassembly is rev 3.** It matches the `tempest3` image at every listed
   byte except one address typo. It covers only `$9000-$DFFF` (no vector ROM) and has no labels.
4. **`ALEXEC.MAP` comes from the Version-1 (rev 1) link**, even though it is named ALEXEC.
   Its `QCHKSx` checksum equates are the ALHARD (rev 1) values, and its high limit is `$DFDC`.
   Its 276 global symbol addresses are still valid for rev 3: none of the code moved, and
   only the three checksum bytes at global symbols differ.
5. **Recommended primary target: `tempest3.zip`** (2K chips) or **`tempest.zip`** (4K chips).
   Both give the identical CPU image. Build the disassembly from the `*2` source variants.

---

## 1. ROM sets and hardware memory map

### 1.1 Zip contents (CRC32 computed from the extracted data)

| File | Size | CRC32 | tempest | tempest1 | tempest1r | tempest2 | tempest3 | CPU addr |
|---|---|---|---|---|---|---|---|---|
| 136002-113.d1 | 2048 | 65d61fe7 | | x | | x | x | 9000-97FF |
| 136002-114.e1 | 2048 | 11077375 | | x | | x | x | 9800-9FFF |
| 136002-115.f1 | 2048 | f3e2827a | | x | | x | x | A000-A7FF |
| 136002-116.h1 | 2048 | 7356896c | | x | | x | | A800-AFFF |
| 136002-316.h1 | 2048 | aeb0f7e9 | | | | | x | A800-AFFF |
| 136002-117.j1 | 2048 | 55952119 | | x | | | | B000-B7FF |
| 136002-217.j1 | 2048 | ef2eb645 | | | | x | x | B000-B7FF |
| 136002-118.k1 | 2048 | beb352ab | | x | | x | x | B800-BFFF |
| 136002-119.lm1 | 2048 | a4de050f | | x | | x | x | C000-C7FF |
| 136002-120.mn1 | 2048 | 35619648 | | x | | x | x | C800-CFFF |
| 136002-121.p1 | 2048 | 73d38e47 | | x | | x | x | D000-D7FF |
| 136002-122.r1 | 2048 | 796a9918 | | x | | | | D800-DFFF |
| 136002-222.r1 | 2048 | 707bd5c3 | | | | x | x | D800-DFFF |
| 136002-123.np3 | 2048 | 29f7e937 | | x | | x | x | 3000-37FF (vector ROM) |
| 136002-124.r3 | 2048 | c16ec351 | | x | | x | x | 3800-3FFF (vector ROM) |
| 136002-133.d1 | 4096 | 1d0cc503 | x | | x | | | 9000-9FFF (=113+114) |
| 136002-134.f1 | 4096 | c88e3524 | x | | x | | | A000-AFFF (=115+316) |
| 136002-135.j1 | 4096 | 1ca27781 | | | x | | | B000-BFFF (=117+118) |
| 136002-235.j1 | 4096 | a4b2ce3f | x | | | | | B000-BFFF (=217+118) |
| 136002-136.lm1 | 4096 | 65a9a9f9 | x | | x | | | C000-CFFF (=119+120) |
| 136002-137.p1 | 4096 | d75fd2ef | | | x | | | D000-DFFF (=121+122) |
| 136002-237.p1 | 4096 | de4e9e34 | x | | | | | D000-DFFF (=121+222) |
| 136002-138.np3 | 4096 | 9995256d | x | | x | | | 3000-3FFF (=123+124) |
| 136002-125.d7 | 256 | 5903af03 | x | x | x | x | x | AVG state PROM (not CPU-visible) |
| 136002-126.a1 | 32 | 8b04f921 | x | x | x | x | x | Mathbox mapping PROM (82S123) |
| 136002-127.e1 | 256 | 276eadd5 | x | x | x | x | x | Mathbox microcode PROM |
| 136002-128.f1 | 256 | 823b61ae | x | x | x | x | x | Mathbox microcode PROM |
| 136002-129.h1 | 256 | 09f5a4d5 | x | x | x | x | x | Mathbox microcode PROM |
| 136002-130.j1 | 256 | 8119b847 | x | x | x | x | x | Mathbox microcode PROM |
| 136002-131.k1 | 256 | b31f6e24 | x | x | x | x | x | Mathbox microcode PROM |
| 136002-132.l1 | 256 | 2af82e87 | x | x | x | x | x | Mathbox microcode PROM |

The PROM roles come from `TEMPST.DOC`: 125 = "V.G. STATE ROM" (source `STATE2.MAC`),
127-132 = mathbox "MICRO. PROM" (source `MBUCOD.V05`), 126 = "MAPPING PROM".
The AAE driver loads only 125.d7. It emulates the mathbox at a high level.

The "=113+114" style equivalences were checked by building both images and comparing them.
The load addresses were also checked: with them, the images reproduce `ALEXEC.LDA` and
`TEMPST.LDA` exactly and give the reset vector `$D93F`.

### 1.2 Revision identity (by CPU image content)

Diffs were done over `$3000-$3FFF` and `$9000-$DFFF`:

| Image class | Sets | Matches | Atari doc name |
|---|---|---|---|
| **R1** | tempest1 | TEMPST.LDA (100%) | "Version 1", 8/26/81 |
| **R2** | tempest2 | neither LDA (differs from R3 by 2 bytes) | (MAME "rev 2") |
| **R3** | tempest3, tempest (identical) | ALEXEC.LDA (100%) | "Version 2" (2716) and "2A (alt)" (2532), 11-25-81 / 12-17-81 |
| R1+patch | tempest1r | R1 plus the R3 A800 patch | "1A (alt)" dummy set, per TEMPST.DOC |

The vector ROM (`$3000-$3FFF`) is identical in every set.

Differences:
- **R1 -> R2** (chips 117->217, 122->222):
  - `$B1F1`: `EOR #$2A` becomes `#$29`. This is the "ATARI on screen" security check in ALDISP vs ALDIS2.
  - `$B497`: CHKSM6 byte.
  - `$D800` block, 1048 bytes:
    - ALTEST becomes ALTES2: the switch test is removed; `WDGTST: JMP WDGTST` becomes `BNE WDGTST` (`$DAF2`/`$DAF7`).
    - `SNDFRQ` moves to `.VCTRS` at `$DFDC-$DFF7`. These bytes are `00` in R1.
    - `$DDDC`: CHKSMB changes from EE to 73.
- **R2 -> R3** (chip 116->316): 2 bytes.
  - `$AA6C JMP $A8B4` (INFO) becomes `JMP $A8E7` (HACKER). This is ALSCOR vs ALSCO2.
  - `$A8AF`: CHKSM5 changes from B2 to E1.
- **tempest1r**: R1 with only `$A8AF` and `$AA6D` changed to the R3 values. The driver notes that
  134.f1 "contains the 316 fix".

Naming caveats:
- The AAE driver titles `tempest3` "Revision 2B" (code comment "rev 2") and `tempest2` "Revision 2A".
  By content, tempest3 == tempest == R3.
- Atari's TEMPST.DOC lists H1 as "136002-216" for Version 2. Its content is what MAME calls 316.
- TEMPST.DOC's 2532 table lists the 4K chips at reversed addresses (237 at 9000 ... 133 at D000).
  The commented-out 4K lines in the driver's `tempest3` block copy that reversed layout.
  **Both are wrong. The correct layout is 133.d1 at 9000 ... 237.p1 at D000**, proven by the LDA match.

### 1.3 Memory map

The AAE driver `tempest.cpp` is authoritative. Each row was cross-checked against the
`ALCOMN.MAC` "HARDWARE DEFINITIONS" section.

| Addr | R/W | Driver handler | ALCOMN symbol | Notes |
|---|---|---|---|---|
| 0000-07FF | RW | MRA/MWA_RAM | RAM `.ASECT` blocks at `.=0`, `.=100`, `.=200`, `.=600`; ALEARO at `.=1C6`; ALSOUN at `.=0BF` | zero page = control/work vars; stack in page 1 |
| 0800-080F | W | `colorram_w` | `COLPORT=800` | 16 color entries, 4 bits, active low: b0 = red low (0x11), b1 = red (0xEE), b2 = blue, b3 = green. Source colors `FRED=0C FBLUE=0B FGREEN=07` agree. |
| 0C00 | R | `TempestIN0read` | `IN1=0C00` | b0 R coin, b1 C coin, b2 L coin, b3 slam, b4 self test, b5 diag step, **b6 VG HALT (1 = done)**, b7 3 kHz clock. Source: `MCOINR=01 MCOINC=02 MCOINL=04 $LMBIT=8 MTEST=10 MDITES=20 MHALT=40 M3KHTI=80`. |
| 0D00 | R | ip_port_3 (DS1 N13) | `INOP0=0D00` | coinage switches |
| 0E00 | R | ip_port_4 (DS2 L12) | `INOP1=0E00` | min credits / language / bonus / lives |
| 2000-2FFF | RW | RAM (`AAE_DRIVER_VECTORRAM(0x2000,0x1000)`) | `VECRAM=2000` | AVG display-list RAM |
| 3000-3FFF | R | ROM | `ROMSTART=3000` | vector ROM (ALVROM .ASECT) |
| 4000 | W | `coin_write` | `OUT0=4000` | `MRCCNT=01 MMCCNT=02 MLCCNT=04 MVINVX=08 MVINVY=10`. The driver uses 0x08 as flip. |
| 4800 | W | `avgdvg_go_w` | `VGSTART=4800` | |
| 5000 | W | `watchdog_reset_w` | `WTCHDG=5000`, `INTACK=WTCHDG` | |
| 5800 | W | `avg_reset_w` | `VGSTOP=5800` | |
| 6000-603F | W | `EaromWrite` | `EADAL=HARDWA` | EAROM address = offset, plus data |
| 6040 | W | `EaromCtrl` | `EACTL=HARDWA+40` | |
| 6040 | R | `MathboxStatusRead` | `MSTAT=HARDWA+40` | D7 = 1 busy |
| 6050 | R | `EaromRead` | `EAIN=HARDWA+50` | |
| 6060 / 6070 | R | Mathbox low / high | `MYLOW` / `MYHIGH` | |
| 6080-609F | W | `MathboxGo` | `MBSTAR=HARDWA+80` (`MAL..MDYPL`) | write = load register / start function |
| 60C0-60CF | RW | POKEY 1 | `POKEY=HARDWA+0C0` | `ALLPOT`/`POTGO` = spinner (4 bits) + cocktail b4; `RANDOM=$60CA` |
| 60D0-60DF | RW | POKEY 2 | `POKEY2=HARDWA+0D0` | `ALLPO2` = b3 zap, b4 fire, b5 start1, b6 start2, b0-2 options |
| 60E0 | W | `tempest_led_w` | `OUTANK=HARDWA+0E0` | `MLED1=2 MLED2=1 MFLIP=4` |
| 9000-DFFF | R | ROM | `PROG=09000` | 20K program |
| F000-FFFF | R | ROM mirror | (`.VCTRS 0DFFA,IRQ,RESET,IRQ`) | 4K sets RELOAD D000 at F000; 2K sets RELOAD D800 at F800 |

CPU vectors (all sets): NMI = `$D704`, RESET = `$D93F`, IRQ = `$D704`. Both NMI and IRQ go to
`IRQ:` in ALHARD, which is placed after the `CHKSMA` byte at `$D703`.

**Disagreements between the driver and the source:**
- The driver's header comment says `60E0 R` for the LEDs and flip. The source (and the driver's own
  handler table) treat it as a **write**.
- The driver header names only right/left coin counters on 4000. The source has **three**
  (right 01, center 02, left 04).
- IRQ rate: the hardware is 12.096 MHz / 4096 / 12 = 246.1 Hz. AAE deliberately runs 240 Hz
  (4 IRQs per 60 Hz frame).
- Mirror: the old commented source says "`$C000-DFFF` duplicated to `$E000-FFFF`". The driver only
  maps `F000-FFFF` (or `F800-FFFF`). The game itself only relies on the vectors.
- The tempest1r set is not in the AAE driver. The tempest3 labels and the commented 4K addresses
  are wrong, as noted in 1.2.

### 1.4 Module link map (ALEXEC.MAP section summary; same layout in R1 and R3)

| Range | Module (R3 file) | Purpose |
|---|---|---|
| 3000-3FFF | ALVROM (.ASECT) + ANVGAN (included) | vector ROM: characters, pictures, self-test patterns |
| 9000-A8AF | ALWELG (.ASECT `.=9000`) | well/game mainline, enemies, CAM scripts, wave tables. First 8 bytes are "MORSE CODE ATARI", then CHKSM2 at 9008 |
| A8B0-B1B5 | ALSCO2 | score, high score, initials, credits/info display |
| B1B6-C79F | ALDIS2 | display: well projection, objects, VG buffer management |
| C7A0-CB00 | ALEXEC | executive: MAINLN, state dispatch |
| CB01-CDDD | ALSOUN | sound tables and driver (POKEY) |
| CDDE-CF23 | ALVROM (.CSECT) | ROM tables (INVERS, SCALOC, PICLO/PICHI JSRL table ...) |
| CF24-D030 | ALCOIN (includes COIN65) | coin routine `MOOLAH` |
| D031-D702 | ALLANG | messages in 4 languages (`ENGMSG D031`, `MSGLBS D121`) |
| D703-D7E0 | ALHAR2 | IRQ handler (+ vectors at DFFA) |
| D7E1-DDDC | ALTES2 | self-test / diagnostics, RESET at D93F |
| DDDD-DF08 | ALEARO | EAROM high-score / stats I/O |
| DF09-DFDB | ALVGUT | VG utilities (VGRTSL, VGHEX, VGVCTR, VGSCAL ...) |
| DFDC-DFF7 | ALTES2 `.VCTRS SNDFRQ...POTYTA` | small tables (R2/R3 only) |
| DFFA-DFFF | ALHAR2 `.VCTRS` | NMI/RESET/IRQ |

Original IMGFIL split (from `ALEXEC.COM`): `136002.011`=3000, `.012`=3800, `.001`=9000 ...
`.010`=D800, each 2048 bytes.

---

## 2. Which revision each input corresponds to

| Input | Revision | Evidence |
|---|---|---|
| Tempest Commented Source.txt | **R3** | Matches the tempest3 image on 20478 of 20478 listed bytes (after fixing typo `BC1E` -> `B21E`). Shows `AA6C 4C:E7 A8`, `A8AF E1`, `DDDC 73`, `DFDC 10:10`. Its header text ("dump begins at ROM 136002.113", "V1 roms") is misleading. |
| ALEXEC.LDA | R3 | 100% byte match (only DFF8-DFF9 are absent) |
| TEMPST.LDA | R1 | 100% byte match (DFDC-DFF9 absent) |
| `*2.MAC` set (ALSCO2, ALDIS2, ALHAR2, ALTES2) | R3 | TEMPST.DOC V2 link line; `JMP HACKER`, `EOR I,029`, `QCHKS5==0E1 QCHKS6==01D QCHKSB==073`, `BNE WDGTST` |
| non-2 set (ALSCOR, ALDISP, ALHARD, ALTEST) | R1 | `JMP INFO`, `EOR I,02A`, `QCHKS5==0B2 QCHKS6==01E QCHKSB==0EE`, `JMP WDGTST` |
| ALEXEC.MAP | R1 link (27-AUG-81) | lists ALSCOR/ALDISP/ALHARD/ALTEST; QCHKS values are R1; high limit DFDC |
| ALEXEC.COM | R1-era command file | links the non-2 names |

No single source variant gives R2. R2 would need ALDIS2 + ALTES2 with ALSCOR, plus a
checksum mix. There is no reason to target it.

### Recommendation (the primary set)

- **(a) Set matched by the .MAC source.** The linked `*2` variant set
  (ALWELG, ALSCO2, ALDIS2, ALEXEC, ALSOUN, ALVROM, ALCOIN, ALLANG, ALHAR2, ALTES2, ALEARO, ALVGUT)
  = `ALEXEC.LDA` = **tempest3.zip, and equally tempest.zip**.
  - 0 differing bytes over all 24574 loaded bytes.
  - The non-2 variants = `TEMPST.LDA` = tempest1.zip, also 0 differing bytes.
- **(b) Set matched by Commented Source.txt:** **tempest3.zip / tempest.zip**.
  - 0 differing bytes over $9000-$DFFF.
  - One listing typo (address `BC1E` should be `B21E`).
- **(c) Differences from rev 3.** (a) and (b) *are* rev 3, so 0 bytes. For reference, the other sets:

  | Set | Bytes differing from rev 3 | Where |
  |---|---|---|
  | tempest2 (R2) | 2 | `$A8AF`, `$AA6D` |
  | tempest1 (R1) | 1052 | `$A8AF`, `$AA6D`, `$B1F1`, `$B497`, plus 1048 in `$D800-$DFFF` (`$D80B-D80E`, `$D92D`, `$D94A-DBDF`, `$DBF8-DD8C`, `$DDDC`, `$DFDC-DFF7`) |
  | tempest1r | 1050 | same as tempest1 minus the A800 pair |

  Vector ROM $3000-$3FFF: identical in all sets.
- **Which zip is rev 3:** `tempest.zip` (MAME/AAE "tempest", "Revision 3", 4K chips) and
  `tempest3.zip` (2K chips) give byte-identical CPU images. AAE mislabels tempest3 as "Revision 2B".

**Primary: `tempest3.zip`** (2K chips map 1:1 to the original IMGFIL 2K split; tempest.zip is
equivalent). Binary: `disasm\_survey\roms_extracted\tempest3_cpu64k.bin`. Use the `*2.MAC`
files + common modules as the naming source, and the commented source as extra commentary.

Tooling note: cc65 V2.19 is at `C:\Source2026\Tempest Dissasembly\cc65-win32\bin`
(`ca65`, `ld65`, `da65`). A practical verify loop:
1. Emit ca65 source for `$3000-$3FFF` and `$9000-$DFFF`.
2. Assemble and link with a two-segment ld65 config.
3. `fc /b` the result against the tempest3 image.

`da65` with an info file (labels/ranges generated from step 5 below) can produce a first draft.

---

## 3. Commented Source.txt format

- 11006 lines, Latin-1, CRLF.
  - 10586 address lines
  - 379 comment-only lines
  - 40 blank lines
  - 1 stray continuation line
- One line per instruction or data item: `AAAA BB:BB BB   MNE:mode   operand   ; comment`.
  Byte separators are inconsistent (`:` or space). Branch targets are shown as `Branch->$XXXX`.
  Zero page is shown as `Zp RAM 00XX`.
- **No labels at all.** Every reference is a raw `$XXXX`.
- Data regions are mostly disassembled as code: 947 "Illegal Opcode" lines, 476 BRKs,
  and only 212 `DATA` lines.
- Coverage: `$9000-$DFFF` complete (1 typo). **`$3000-$3FFF` not covered.** Vector ROM
  addresses appear only as operands, e.g. `LDA $31E4`.
- Comment density (lines carrying `;`):

  | Region | Commented lines |
  |---|---|
  | 9xxx | 15% |
  | Axxx | 10% |
  | Bxxx | 1% |
  | Cxxx | 7% |
  | Dxxx | 10% |
  | **Overall** | **891/10586 = 8.4%** |

  Plus block headers such as `; SUBROUTINE: Game initialization.`

Verbatim samples:
```
; Power-on code begins at $D93F. (Obtained from vector at $DFFC.)
9000 02:BB      DATA			 ; Intuitive guess: CRC data?
9009 20:C5:92   JSR:abs    $92C5
900C 20:34 92   JSR:abs    $9234	 ; Set number of enemies to appear
9022 85:01      STA:zp     Zp RAM 0001   ; (or attract mode).
905B 10:02      BPL:rel    Branch->$905F
AC08 07:04:01   DATA		; BEH	Initials to place in the the
C7A0 20:95 CD   JSR:abs    $CD95	 ; Pokey Initialization Routine
DFE2 FF:        Illegal Opcode
```
Parse regex that works: `^([0-9A-F]{4}) ([0-9A-F]{2})([: ][0-9A-F]{2})?([: ][0-9A-F]{2})?:?\s+(\S+)(.*)$`.
Known errors: line 4504 `BC1E E9:AD DATA` should be `B21E`; `AC1E 09:04` overlaps the preceding
3-byte DATA line.

Its value is the **English comments** (RAM meanings such as `$0053` = frame timer, POKEY
protection notes at `C5B1`, high-score defaults at `AC08`). The original source supersedes it
for names and structure.

---

## 4. Original Atari source (tempest-main)

### 4.1 Files

The .MAC files are RT-11 text: tab-indented, NUL-padded to 512-byte blocks (strip `\0` before
parsing). Ripgrep treats them as binary, so use Python or strip the NULs first.

| File | Lines | Role |
|---|---|---|
| ALCOMN.MAC | 1131 | constants, hardware equates, **all RAM variables** (`.ASECT` + `.BLKB`). `.INCLUDE`d by every module. Dave Theurer. |
| HLL65.MAC | 117 | structured-programming macros: `IFEQ/IFNE/IFCS/IFMI/IFVS.. ELSE ENDIF`, `BEGIN .. EQEND/NEEND/CSEND/MIEND..`, `LDAL/LDAH` |
| ALWELG.MAC | 3560 | game mainline (DFT), `.ASECT .=9000` |
| ALSCOR / **ALSCO2** | 1397 | score module; differ only by `HACKER:` label + `JMP HACKER` |
| ALDISP / **ALDIS2** | 3284 | display; differ by one constant (`EOR I,02A` vs `029`) |
| ALEXEC.MAC | 603 | executive / main loop |
| ALSOUN.MAC | 385 | sounds |
| ALVROM.MAC | 2499 | vector ROM pictures + ROM tables; includes VGMC, ANVGAN |
| ALCOIN.MAC | 23 | wrapper: `.INCLUDE ALCOMN, COIN65` |
| COIN65.MAC | 664 | generic Atari coin routine (`MOOLAH`) |
| ALLANG.MAC | 292 | messages (English/French/German/Spanish), `.ASCVG`/ASCVG.MAC |
| ALHARD / **ALHAR2** | 187 | IRQ handler + `QCHKSx` checksum equates + vectors; differ only in 3 checksum values |
| ALTEST / **ALTES2** | 902 / 932 | self-test; about 330 diff lines (switch test removed, `INIDSP`/flip init added, `WDGTST`, SNDFRQ/POTYTA as `.VCTRS` at DFDC) |
| ALEARO.MAC | 261 | EAROM |
| ALVGUT.MAC | 395 | VG utilities |
| VGMC.MAC | 156 | AVG macros (Ed Logg) |
| ANVGAN.MAC | 353 | vector character set `CHAR.A`.. (Ed Logg), **`.RADIX 10`** |
| ASCVG.MAC | 29 | `.ASCVG` string macro (Rich Moore) |
| ALDIAG.MAC | 168 | standalone VG diagnostic PROM (`.=0D800`); **not linked** |
| STATE2.MAC/.MAP/.SAV/.COM | 34 | AVG state PROM 136002-125 |
| MBUCOD.V05/.MAP/.COM, MBUDOC.DOC, MBOX.SAV | - | mathbox 2901 microcode (Mike Albaugh) + 17 KB text doc of the register interface and timing |
| ALEXEC.COM | - | build script (MAC65, LINKM, IMGFIL) |
| ALEXEC.LDA / TEMPST.LDA | - | linked absolute load images (R3 / R1) |
| ALEXEC.MAP | - | LINKM map with global symbols (R1 link) |
| TEMPST.DOC | 8 KB text | release documentation for V1, V2, 2A(alt), 1A(alt): part numbers, link lines |
| 002X1.DAT / 002X2.DAT / MABOX.DAT | - | ROM-burner verification control files (part to address) |

LDA format: records `01 00 lenL lenH addrL addrH data... cksum`, where len includes the 6-byte
header. A record with len=6 is the start address.

### 4.2 Dialect (MAC65, a DEC MACRO-11 lookalike)

- Directives: `.TITLE .SBTTL .PAGE .RADIX 16 .ENABL AMA .NLIST/.LIST .INCLUDE .ASECT .CSECT`,
  `.=addr`, `.BLKB .BYTE .WORD .GLOBL .MACRO/.ENDM .REPT/.ENDR .IRPC .IF/.IFF/.ENDC .IIF .NARG .NCHR .MEXIT`,
  and `.VCTRS addr,w1,w2..` (words at an absolute address).
- **Default radix is hex** (`.RADIX 16`). A trailing `.` means decimal (`SECOND=20.`).
  Hex constants starting with a letter get a leading 0 (`0FA`). ANVGAN switches to radix 10.
- Labels: `NAME:` is local, `NAME::` is global. Symbols use up to 6 significant characters in
  the MAP (`INEWAV`, `ZATVG2`, `$$CRDT`). `NAME==val` is a global equate.
- Addressing syntax: `LDA I,0FA` = immediate; `LDA X,TAB` / `Y,TAB` = indexed;
  `STA NY,VGLIST` = `(zp),Y`; `STA A,POTGO2` = forced absolute; plain operand = zp/abs by value.
- Operators: `<...>` groups, `!` = OR, `&` = AND, `^C` = complement, `^H` = hex.
- Structured code: `IFNE ... ENDIF`, `BEGIN ... MIEND`. These expand to conditional branches
  with **no labels**. A disassembly should render them as local labels or keep the original
  structure in comments.

Sample (ALWELG.MAC top, maps to $9000):
```
	.ASECT
	.=9000
	.BYTE 02,0BB,5A,30	;MORSE CODE ATARI
	.BYTE 50,0EE,3D,0A8
CHKSM2::	.BYTE QCHKS2
	.SBTTL INITIALIZE - MAINLINE
INEWAV:				;NEW WAVE
	JSR CONTOUR
	JSR INIENE		;INITIALIZE NYMPHS, ENEMY LINES
```
Source vs ROM at MAINLN ($C7A0):
```
MAINLN:	JSR INISOU	;INITIALIZE SOUNDS      C7A0 20 95 CD  (INISOU=CD95 in MAP)
	LDA I,CNEWGA                            C7A3 A9 00
	STA QSTATE                              C7A5 85 00
	BEGIN
	LDA FRTIMR                              C7A7 A5 53     (FRTIMR = $53)
	CMP I,9                                 C7A9 C9 09
	CSEND                                   C7AB 90 FA
```

### 4.3 Symbols from ALEXEC.MAP

The MAP has 276 global symbols:
- about 159 in `$9000-$DFFF`
- 22 in the vector ROM
- about 92 small values (message numbers `M*`, picture codes `PT*`, checksum values `QCHKS*`, `$` coin vars)

Sample:
```
 Name   Value   Name   Value   Name   Value   Name   Value   Name   Value
BCCURS  0007   BUFBSL  CE7A   COCFLI  CA48   EASING  346E   HISCHK  AC3F
CHKSM2  9008   DISPLA  B1B6   INEWAV  9009   INEWLI  9025   INFO    A8B4
MAINLN  C7A0   PLAY    970B   RESET   D93F   VGRTSL  DF09   ZPONTS  AE1F
```
Caveats:
- The MAP does **not** contain local labels (for example `CONTOUR`, `IRQ`, `HACKER`, `JSRCAM`, `TABJSR`).
- It does not contain RAM variables (defined in ALCOMN by `.ASECT` + `.BLKB`).
- It shows R1 checksum values.

Label counts (col-0 `NAME:`) in the linked R3 modules:

| Module | Labels |
|---|---|
| ALWELG | 219 |
| ALSCO2 | 76 |
| ALDIS2 | 146 |
| ALEXEC | 27 |
| ALSOUN | 60 |
| ALVROM | 204 |
| ALLANG | 126 |
| ALHAR2 | 4 |
| ALTES2 | 44 |
| ALEARO | 23 |
| ALVGUT | 22 |
| COIN65 | 22 |
| ANVGAN | 37 |
| **Total** | **~1010** |

Some of these sit inside `.IF NE,SPACG` blocks (SPACG=0), which are dead code left over from the
Space-Invaders-like "Aliens" ancestry. ALCOMN adds about 449 RAM/constant labels.

### 4.4 Vector data representation

- **VGMC.MAC** defines the AVG opcodes as `.WORD` (little-endian in ROM):

  | Macro | Encoding |
  |---|---|
  | `VCTR dx,dy,z` | long form: 2 words `DY&1FFF`, `z*2000+DX&1FFF`; short form `4000+...` when it fits |
  | `CNTR` | `8040` |
  | `HALT` | `2000` |
  | `RTSL` | `C000` |
  | `SCAL s,ls` | `7000+s*100+ls` |
  | `STAT` | `6000..` |
  | `JSRL L` | `A000+(L&1FFF)/2` |
  | `JMPL L` | `E000+(L&1FFF)/2` |
  | `COLOR col,lum` | `.BYTE lum*10+col,64` |

  So VG address = (CPU addr & 1FFF)/2, and CPU address = $2000 + 2 x (VG address & 0FFF)
  for the RAM/ROM window.
- **ALVROM.MAC** adds `CSTAT c` (`.WORD 68C0+c`) and relative-vector helpers
  (`ICVEC/CVEC/SCVEC/MVEC`, `CIRC16`, `SPOK16`, `CALVEC`, `MSTAR1-4`).
  - The checksum bytes are hidden in RTSL words: `CHKSM0:: .BYTE QCHKS0,0C0`.
  - `PITAB` builds the JSRL picture table `PICLO` (`$CDxx` CSECT) and assigns global indices
    `PTEXP1, PTCURS, PTSTR1, PTSPI1, PTTANK, PTSPAR, PTESHO`...
  - `TDOT/TLABS/TVCTR/TJSR/TSCAL/OBJEND` define a byte-coded object-drawing table format.
    Most uses are inside SPACG-disabled blocks, so check before decoding.
- **ANVGAN.MAC**: `CHAR.A`..`CHAR.Z`, digits, etc. as VCTR lists at the start of `$3000`.
- **ALLANG.MAC**: messages via the `MESS` macro and `.ASCVG`, with per-language origin
  (`.=...ENG/...FRE/...GER/...SPA`).
- **STATE2.MAC**: AVG state PROM contents (`VCTR/HALT/SVEC/STAT/WAIT/JSR/RTS/JMP` rows).

### 4.5 Scripts: the "CAM" enemy-motion bytecode (ALWELG)

`JSRCAM` dispatches through `TABJSR`. The table is built with `CAMAC/CAMA2I/CAMA2F`, which also
define the opcode macros `VEXIT=0 VSLOOP=2(+arg) VSKIP0=4 VSETPC=6(+rel) VELOOP=8(+rel) VNOOP=0A`
`VSMOVE=0C VSTRAI=0E VSLOPB=10(+zp) VJUMPS=12 VJUMPM=14 VCHROT=16 VKITST=18 VBR0PC=1A(+rel)`
`VELTST=1C VSFUSE=1E VFUSKI=20 VSPUMO=22 VCHPLA=24 VCHKPU=26`.

Branch operands are `target-CAM-1`, i.e. offsets into the `CAM` script area. Sample script:
```
MOVJMP:	VSLOOP 8
MJLOP1:	VSMOVE			;MOVE UP N FRAMES
	VEXIT
	VELOOP MJLOP1
	VJUMPS			;START JUMP
```
ALWELG also holds the per-wave parameter tables. The record format is documented at
ALWELG ~line 399 ("PARAMETER TABLES DATA STRUCTURE", `T1=2`, `TZ=4`).
ALSOUN holds the sound tables (`PNTRS`, 6 bytes per sound, `OFFSET` macro).

---

## 5. Label-naming strategy

**Goal:** original names on (nearly) every ROM address. Three layers, best first.

1. **Assemble the source (recommended, about 100% achievable).** The R3 source + ALEXEC.LDA
   match the ROM exactly, so a small Python MAC65-subset assembler can be written and verified
   against `tempest3_cpu64k.bin`. It needs:
   - hex radix and `.` decimals
   - HLL65 IF/BEGIN macros
   - VGMC macros
   - `.IF/.IIF/.REPT/.IRPC/.NARG`
   - zp vs abs sizing (value < 100 and defined before use, else abs; `A,` forces abs)
   - `.ASECT/.CSECT`, with CSECT bases taken from the MAP section table

   Every local and global label then gets an exact address, and each module can be verified
   section by section. RAM names come from assembling ALCOMN's `.ASECT` blocks
   (ALEARO/ALSOUN add their own). The equates give I/O names.
   *Also a correctness oracle:* any byte mismatch means a parser bug.
2. **Anchor-and-walk (cheaper fallback, about 95%).** Start from the 159 MAP globals in ROM and
   the section bases. Walk each module's statements, summing instruction sizes, and
   resynchronize at every global. Confirm with opcode bytes from the ROM. HLL65 macros have
   fixed sizes (2-byte branch; `ELSE` = JMP/branch), which makes this tractable.
3. **Commented source as a comment feed.** It has no labels, but after step 1 or 2 its
   per-address comments can be merged in by address (verified identical bytes, so no
   realignment is needed). Treat its interpretations as secondary to the Atari comments.

Expected coverage:

| Label kind | Coverage |
|---|---|
| Globals (MAP only) | 181 ROM addresses, about 18% of source labels |
| Code/data labels after step 1 | ~1000 original names (minus SPACG-dead ones) |
| HLL65 branch targets | no names in source; synthesize (`INEWAV_1` or `L_9061`) |
| Vector ROM | ALVROM/ANVGAN labels (CHAR.x, LIFE1, INTEST, ...) cover all JSRL targets |

Naming rules:
- Keep 6-character originals.
- Map `.` and `$` to legal characters for the target assembler (`CHAR.A` -> `CHAR_A`,
  `$$CRDT` -> `S_CRDT`), keeping the original in a comment.
- Use the original `.SBTTL` titles and `;` comments as the function descriptions.

---

## 6. Driver facts that matter for the C port

- **CPU / IRQ**
  - 6502 at 1.512 MHz.
  - IRQ = 3 kHz / 12, about 246 Hz. AAE uses 240 Hz, 4 per 60 Hz frame. NMI is unused (same handler).
- **Main loop timing (ALEXEC/ALHAR2)**
  - Each IRQ increments `FRTIMR` ($53).
  - `MAINLN` waits until `FRTIMR >= 9`, clears it, then runs `EXSTAT` (state), `NONSTA`, `DISPLA`.
    The game logic therefore runs at about 27 Hz.
  - `$INTCT` wrapping (256 IRQs, about 1 s) drives the second counters.
- **Watchdog**
  - Hardware watchdog is written at `$5000` (`STA WTCHDG`) on every IRQ, but only if SP >= $D0
    and `FRTIMR` < $80 (software watchdog).
  - Otherwise the IRQ handler does `BRK` / `JMP RESET`.
  - A port can drop the hardware dog but should keep the logic.
- **Video start**
  - At the end of each IRQ: `BIT IN1` / `IFVS` (b6 = VG halted) -> `INC SPARE3`,
    `STA VGSTOP` ($5800), `STA VGSTART` ($4800).
  - The VG restarts at vector-RAM $2000. ALDIS2 swaps buffers by writing a JMPL into
    `VECRAM`/`VECRAM+1`.
  - AAE: vector RAM 2000-2FFF, `ORIENTATION_ROTATE_270`, visible area 0-580 x 0-570.
    The driver sets IN0 b6 when `avgdvg_done()`.
- **Inputs**
  - IN0 $0C00: coins/slam/test/diag, b6 VG halt, b7 3 kHz square wave (~256-cycle half period).
    The self-test edge-waits on b7.
  - Spinner: POKEY1 `ALLPOT` b0-3. The IRQ applies `EOR #$0F` and takes a signed 4-bit delta
    into `TBHD` (driver `IPF_REVERSE`). b4 = cocktail.
  - Buttons: POKEY2 `ALLPO2`, active high in the driver: b3 zap, b4 fire, b5 start1, b6 start2,
    b0-1 difficulty, b2 rating.
  - The IRQ debounces into `SWSTAT/SWFINA`. The driver's DIP maps for $0D00/$0E00 are in
    `tempest.cpp` lines 770-816.
- **Mathbox**
  - Write operands/commands to $6080-$609F. Poll $6040 D7 (busy). Read $6060/$6070.
  - AAE emulates it at a high level (`mathbox.h`). `MBUDOC.DOC` documents each register and its
    cycle timing, and `MBUCOD.V05` is the microcode.
  - A C port can implement the functions directly.
- **POKEY protection**
  - The ROM reads `RANDOM` ($60CA/$60DA) at `$AE1F` (global `ZPONTS`), `$CD95` (`INISOU`), and
    around `$DA44`. The result goes to `$011F` (checked at `ZQPONS $C5B1`; `$0720` is also watched).
  - The driver's comments say the pair at `$AE1F` needs hi-nibble(r1) == lo-nibble(r2).
    A port must make this check pass, or patch it.
- **EAROM**
  - $6000-603F write (address = offset), $6040 control, $6050 read.
  - ALEARO RAM at `$1C6..` and `$BD..`. AAE uses `atari_vg_earom_handler` NVRAM.
- **Colors and LEDs**
  - $0800-080F 4-bit active-low RGB as in 1.3.
  - LEDs at $60E0 are active low (`~data & 2` / `& 1`). The IRQ blinks them in attract mode
    from `$INTCT`.
- **Driver hack option**
  - `config.hack` patches `$9001=$D1`, `$90CD/$90CE=NOP` (level-select hack). Ignore it for the
    disassembly.
- **Reuse**
  - Per the user's global instructions, the port should use `C:\Source2026\shared\ref6502\` (or the
    AAE `cpu_6502.cpp`) rather than a new 6502 core.
