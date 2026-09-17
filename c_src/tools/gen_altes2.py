#!/usr/bin/env python3
"""Generate altes2_data.h: the addresses of every ALTES2 label ($D7E1-$DDDC and the
tables at $DFDC-$DFF7), the self test's dispatch table size, and the vector-ROM
pictures / tables ALTES2 draws or indexes - never hand-typed.  The C port reads
the table bytes (OPTMSK, CRMECHT, BONADR, TABCOL, SFTJSR, SNDTBL, BADNWS, SNDFRQ,
POTYTA, POTXTA, SYSOPT, VGMSGA) with cpu_rd() over the generated progrom[] /
vecrom[] images at these addresses, as the ROM does.

Inputs
  ../disasm/_survey/roms_extracted/tempest3_cpu64k.bin   64K CPU image (the dump)
  ../disasm/build/program.bin                            ca65 build of tempest_program_rom.asm
  ../disasm/tempest_program_rom.asm                      labels ("NAME:" then "Lxxxx:"), .alias lines
  ../disasm/tempest_vector_rom.asm                       vector-ROM labels, VGMSGA entries, SYSOPT words
  ../disasm/tempest_defines.asm                          constants (QCHKSB, MFIRE, ZWHITE, ...), vector-ROM .alias
  ../disasm/build/symbols.json                           cross-check of the label addresses
  ../disasm/build/program_refs.json                      the 22 ALTES2 routine headers
  progrom.c, vecrom.c, state_defs.h                      the port's generated images / defines

Checks (the script stops on any failure)
  * the ALTES2 ranges of the dump equal the assembled listing byte for byte
    ($D7E1-$DDDC and $DFDC-$DFF7; $DFF8-$DFF9 are unassembled fill)
  * the listing has the 47 labels symbols.json has (36 code + 11 data; symbols.json
    names are cut to 6 characters: BRAMRE, CRMECH, SHYSTE; NOOPR_DB21 is NOOPR),
    at the same addresses; every program_refs.json header is one of them
  * SFTJSR holds label-1 of BADEAR ROMREP SHATCH SHYSTER SINTEN SCHEKR SIGANA
    (QSTATE 0, 2 .. 12), none of which follows a JSR, and SFTJSE - SFTJSR = 14
    (SSTATE's CPX operand at $DB12)
  * BADNWS indices land on the VGMSGA letter entries M R P Q E
  * SNDTBL-1 (the underflow byte read with X = 0) equals SNDTBL+7
  * OPTMSK = FIRE|ZAP, FIRE|ZAP, FIRE|START1, FIRE|START2; TABCOL = ZWHITE ZYELLO
    ZPURPL ZRED ZTURQOI ZGREEN ZBLUE ZBLUE; CHKSMB = QCHKSB
  * POTXTA = POTYTA + 4 (the tables overlap) and both 16-entry reads end before $DFF8
  * SYSOPT words: options 0-3 = ENTEST ENTEST ZTIMES ZHISCO (cursor & 6 -> DSPSYS
    option; 0 and 1 both select the self test), 4-7 = ZMED ZEASY ZHARD ZMED
  * ROMTST's program-ROM page operand ($DA2F) is $90
  * the 12 self-test ROM checksums (seed = ROM index, EOR over each 2K of $3000,
    $3800, $9000..$D800) are all $00, computed from the dump AND from the port's
    generated progrom.c / vecrom.c
  * every ROM_<NAME> (defines .alias) is that label in the vector-ROM listing and
    equals state_defs.h's A_<NAME>

Output (c_src/)
  altes2_data.h  ALTES2_<LABEL> (all 47 labels, full listing names), ALTES2_SNDTBL_M1,
                 ALTES2_ZPTEST / _RAMTST / _DIAGLP (unlabeled loop heads), ALTES2_SFTJSR_N,
                 ALTES2_PROG_PAGE, ROM_<NAME> (vector ROM)

Rerun:  python c_src/tools/gen_altes2.py
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CSRC = os.path.normpath(os.path.join(HERE, ".."))
ROOT = os.path.normpath(os.path.join(CSRC, ".."))
IMAGE = os.path.join(ROOT, "disasm", "_survey", "roms_extracted", "tempest3_cpu64k.bin")
PROGBIN = os.path.join(ROOT, "disasm", "build", "program.bin")
ASM = os.path.join(ROOT, "disasm", "tempest_program_rom.asm")
VASM = os.path.join(ROOT, "disasm", "tempest_vector_rom.asm")
DEFS = os.path.join(ROOT, "disasm", "tempest_defines.asm")
SYMS = os.path.join(ROOT, "disasm", "build", "symbols.json")
REFS = os.path.join(ROOT, "disasm", "build", "program_refs.json")
PROGROM_C = os.path.join(CSRC, "progrom.c")
VECROM_C = os.path.join(CSRC, "vecrom.c")
STATE_DEFS = os.path.join(CSRC, "state_defs.h")

RANGES = [(0xD7E1, 0xDDDD), (0xDFDC, 0xDFF8)]

# vector-ROM pictures and tables ALTES2 draws (LAH/LXL + VGJSRL) or indexes
EXTRA = ["SYSOPT", "LIFEY", "EASING", "COCMSG", "ROMRPI", "BOKLIT", "HATCH", "INTEST",
         "CHEKER", "HYSTER", "VORBOX", "BONDRY", "VGMSGA"]
SFTJSR = ["BADEAR", "ROMREP", "SHATCH", "SHYSTER", "SINTEN", "SCHEKR", "SIGANA"]
BADNWS_LETTERS = "MRPQE"
SYSOPT_WORDS = ["ENTEST", "ENTEST", "ZTIMES", "ZHISCO", "ZMED", "ZEASY", "ZHARD", "ZMED"]
LOOP_HEADS = [("ZPTEST", 0xD9A9, "zero-page march (RESET, TEST closed)"),
              ("RAMTST", 0xD9D6, "pages $01-$07, $20-$2F"),
              ("DIAGLP", 0xDA8D, "diagnostic loop head (one diag_pass() per visit)")]
N_CODE, N_DATA = 36, 11


def fail(msg):
    sys.exit("gen_altes2: " + msg)


def in_ranges(a):
    return any(lo <= a < hi for lo, hi in RANGES)


def c_array(path):
    txt = open(path, encoding="utf-8").read()
    body = txt[txt.index("{") + 1:txt.rindex("}")]
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    return bytes(int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]{2})", body))


def checksums(read):
    """ROMTST $DA0A: 12 x (seed = index, EOR over 8 pages)."""
    out = []
    page = 0x30
    for rom in range(12):
        if rom == 2:
            page = 0x90
        a = rom
        for _ in range(8):
            for y in range(256):
                a ^= read((page << 8) | y)
            page += 1
        out.append(a)
    return out


def main():
    img = open(IMAGE, "rb").read()
    prog = open(PROGBIN, "rb").read()
    if len(img) != 0x10000 or len(prog) != 0x5000:
        fail("image sizes wrong")
    for lo, hi in RANGES:
        if img[lo:hi] != prog[lo - 0x9000:hi - 0x9000]:
            fail("ALTES2 bytes $%04X-$%04X of the dump differ from the assembled listing" % (lo, hi - 1))

    # labels from the listing: "NAME:" lines bind to the next "Lxxxx:" address
    labels = {}
    pending = []
    for line in open(ASM, encoding="utf-8", errors="replace"):
        m = re.match(r"^([A-Z_][A-Z0-9_]*):", line)
        if m:
            name = m.group(1)
            a = re.match(r"^L([0-9A-F]{4}):", line)
            if a and name == "L" + a.group(1):
                for n in pending:
                    labels.setdefault(n, int(a.group(1), 16))
                pending = []
            else:
                pending.append(name)
            continue
        m = re.match(r"^\.alias\s+([A-Z_][A-Z0-9_]*)\s+\$([0-9A-Fa-f]{4})\b", line)
        if m:
            labels.setdefault(m.group(1), int(m.group(2), 16))

    defs = {}
    for line in open(DEFS, encoding="utf-8", errors="replace"):
        m = re.match(r"^\.alias\s+([A-Z_][A-Z0-9_]*)\s+\$([0-9A-Fa-f]+)\b", line)
        if m:
            defs.setdefault(m.group(1), int(m.group(2), 16))

    mod = sorted((a, n) for n, a in labels.items() if in_ranges(a) and not re.match(r"^L[0-9A-F]{4}$", n))

    # symbols.json: the same 47 labels (names cut to 6 characters)
    syms = json.load(open(SYMS))
    sym = sorted((s["addr"], s["name"], s["kind"]) for s in syms
                 if s["kind"] in ("code", "data") and in_ranges(s["addr"]))
    n_code = sum(1 for s in sym if s[2] == "code")
    n_data = sum(1 for s in sym if s[2] == "data")
    if (n_code, n_data) != (N_CODE, N_DATA):
        fail("symbols.json has %d code / %d data labels in ALTES2, expected %d / %d" % (n_code, n_data, N_CODE, N_DATA))
    if len(mod) != len(sym):
        fail("listing has %d labels in ALTES2, symbols.json %d" % (len(mod), len(sym)))
    short = lambda n: n.split("_")[0][:6]
    if sorted((a, short(n)) for a, n in mod) != sorted((a, n) for a, n, _ in sym):
        fail("listing labels differ from symbols.json:\n  %s\n  %s" % (mod, sym))
    kinds = {short(n): k for _, n, k in sym}

    refs = json.load(open(REFS))["routines"]
    heads = sorted((int(v["addr"].lstrip("$"), 16), k) for k, v in refs.items()
                   if v.get("module") == "ALTES2")
    if len(heads) != 22:
        fail("program_refs.json has %d ALTES2 headers, expected 22" % len(heads))
    for a, n in heads:
        if labels.get(n) != a:
            fail("routine header %s $%04X is not a listing label" % (n, a))

    rd16 = lambda a: img[a] | (img[a + 1] << 8)

    # vector-ROM names: the defines' .alias values (the program listing's operands)
    for n in EXTRA:
        if n not in defs:
            fail("no .alias for %s in the defines" % n)
        labels[n] = defs[n]

    base = labels["SFTJSR"]
    for i, t in enumerate(SFTJSR):
        if rd16(base + 2 * i) + 1 != labels[t]:
            fail("SFTJSR[%d] = $%04X, expected %s-1 ($%04X)" % (i, rd16(base + 2 * i), t, labels[t] - 1))
        if img[labels[t] - 3] == 0x20:
            fail("%s ($%04X) follows a JSR: an RTS arrival there is not only the dispatch" % (t, labels[t]))
    if labels["SFTJSE"] - labels["SFTJSR"] != 2 * len(SFTJSR) or img[0xDB12] != 2 * len(SFTJSR):
        fail("SSTATE's CPX #SFTJSE-SFTJSR ($DB12 = $%02X) is not %d" % (img[0xDB12], 2 * len(SFTJSR)))

    # VGMSGA letter entries from the vector-ROM listing
    vjsrl = {}
    vword = {}
    vlabels = {}
    vpending = []
    for line in open(VASM, encoding="utf-8", errors="replace"):
        m = re.match(r"^([A-Z_][A-Z0-9_]*):", line)
        if m:
            vpending.append(m.group(1))
        m = re.match(r"^V([0-9A-F]{4}):", line)
        if m:
            for n in vpending:
                vlabels.setdefault(n, int(m.group(1), 16))
            vpending = []
        m = re.match(r"^V([0-9A-F]{4}):\s+JSRL\s+(\S+)", line)
        if m:
            vjsrl[int(m.group(1), 16)] = m.group(2)
        m = re.match(r"^V([0-9A-F]{4}):\s+\.word\s+(\S+)", line)
        if m:
            vword[int(m.group(1), 16)] = m.group(2)
    for i, letter in enumerate(BADNWS_LETTERS):
        b = img[labels["BADNWS"] + i]
        got = vjsrl.get(labels["VGMSGA"] + b)
        if got != "CHAR_" + letter:
            fail("BADNWS[%d] = $%02X -> VGMSGA entry %s, expected CHAR_%s" % (i, b, got, letter))
    for i, w in enumerate(SYSOPT_WORDS):
        got = vword.get(labels["SYSOPT"] + 2 * i)
        if got != w:
            fail("SYSOPT[%d] = %s, expected %s" % (i, got, w))

    if img[labels["SNDTBL"] - 1] != img[labels["SNDTBL"] + 7]:
        fail("SNDTBL-1 ($%02X) is not SNDTBL+7 ($%02X)" % (img[labels["SNDTBL"] - 1], img[labels["SNDTBL"] + 7]))
    for i in range(-1, 8):
        v = img[labels["SNDTBL"] + i]
        if v & 1 or v > 0x16:
            fail("SNDTBL[%d] = $%02X is not a channel register offset" % (i, v))

    optmsk = [defs["MFIRE"] | defs["MSUZA"], defs["MFIRE"] | defs["MSUZA"],
              defs["MFIRE"] | defs["MSTRT1"], defs["MFIRE"] | defs["MSTRT2"]]
    if list(img[labels["OPTMSK"]:labels["OPTMSK"] + 4]) != optmsk:
        fail("OPTMSK bytes differ from the defines")
    tabcol = [defs[n] for n in ("ZWHITE", "ZYELLO", "ZPURPL", "ZRED", "ZTURQOI", "ZGREEN", "ZBLUE", "ZBLUE")]
    if list(img[labels["TABCOL"]:labels["TABCOL"] + 8]) != tabcol:
        fail("TABCOL bytes differ from the defines")
    if img[labels["CHKSMB"]] != defs["QCHKSB"]:
        fail("CHKSMB $%02X != QCHKSB $%02X" % (img[labels["CHKSMB"]], defs["QCHKSB"]))

    if labels["POTXTA"] != labels["POTYTA"] + 4:
        fail("POTXTA is not POTYTA+4")
    if labels["POTXTA"] + 15 >= 0xDFF8 or labels["SNDFRQ"] + 8 != labels["POTYTA"]:
        fail("SNDFRQ / POTYTA / POTXTA do not fit $DFDC-$DFF7")

    if img[0xDA2E] != 0xA9 or img[0xDA2F] != 0x90:
        fail("ROMTST $DA2E is not LDA #$90")
    prog_page = img[0xDA2F]

    sums_dump = checksums(lambda a: img[a])
    pr, vr = c_array(PROGROM_C), c_array(VECROM_C)
    if len(pr) != 0x5000 or len(vr) != 0x1000:
        fail("progrom.c / vecrom.c sizes %d / %d" % (len(pr), len(vr)))
    sums_port = checksums(lambda a: vr[a - 0x3000] if a < 0x4000 else pr[a - 0x9000])
    print("ROM checksums (dump): " + " ".join("%02X" % s for s in sums_dump))
    print("ROM checksums (port): " + " ".join("%02X" % s for s in sums_port))
    if sums_dump != sums_port:
        fail("progrom.c / vecrom.c checksums differ from the dump")
    if any(sums_dump):
        fail("a self-test ROM checksum is not 0")

    sdefs = {}
    for line in open(STATE_DEFS, encoding="utf-8"):
        m = re.match(r"^#define A_([A-Z0-9_]+)\s+0x([0-9A-Fa-f]+)", line)
        if m:
            sdefs[m.group(1)] = int(m.group(2), 16)
    for n in EXTRA:
        if vlabels.get(n) != labels[n]:
            fail("ROM_%s $%04X is not the vector-ROM listing label (%s)" % (n, labels[n], vlabels.get(n)))
        if sdefs.get(n) != labels[n]:
            fail("ROM_%s $%04X != state_defs.h A_%s %s" % (n, labels[n], n, sdefs.get(n)))

    h = os.path.join(CSRC, "altes2_data.h")
    with open(h, "w", encoding="utf-8", newline="\n") as f:
        f.write("/* altes2_data.h - GENERATED by c_src/tools/gen_altes2.py from the ROM image\n"
                " * (cross-checked with the assembled listing, symbols.json, program_refs.json,\n"
                " * the vector-ROM listing and the defines) - do not edit.\n"
                " * ALTES2 ($D7E1-$DDDC, $DFDC-$DFF7) label addresses; table bytes are read\n"
                " * with cpu_rd().  Self-test ROM checksums of rev 3 (dump and progrom/vecrom):\n"
                " * %s (all 0). */\n" % " ".join("%02X" % s for s in sums_dump))
        f.write("#ifndef ALTES2_DATA_H\n#define ALTES2_DATA_H\n\n")
        f.write("/* ---- ALTES2 labels (code and data; * = program_refs.json routine header) ---- */\n")
        head_addrs = {(a, n) for a, n in heads}
        for a, n in mod:
            f.write("#define ALTES2_%-10s 0x%04X  /* %s%s */\n"
                    % (n, a, kinds[short(n)], " *" if (a, n) in head_addrs else ""))
        f.write("#define ALTES2_%-10s 0x%04X  /* SNDTBL-1: the underflow byte read with X = 0 */\n"
                % ("SNDTBL_M1", labels["SNDTBL"] - 1))
        f.write("\n/* ---- unlabeled loop heads ---- */\n")
        for n, a, c in LOOP_HEADS:
            f.write("#define ALTES2_%-10s 0x%04X  /* %s */\n" % (n, a, c))
        f.write("\n/* SFTJSR entries (QSTATE / 2): %s */\n" % " ".join(SFTJSR))
        f.write("#define ALTES2_SFTJSR_N %d\n" % len(SFTJSR))
        f.write("/* ROMTST's LDA #<[PROG/$100] operand ($DA2F): first program-ROM page */\n")
        f.write("#define ALTES2_PROG_PAGE 0x%02X\n" % prog_page)
        f.write("\n/* ---- vector-ROM pictures / tables ALTES2 draws or indexes ---- */\n")
        for n in EXTRA:
            f.write("#define ROM_%-8s 0x%04X\n" % (n, labels[n]))
        f.write("\n#endif\n")
    print("wrote altes2_data.h (%d ALTES2 labels: %d code, %d data; %d headers; %d vector-ROM labels)"
          % (len(mod), n_code, n_data, len(heads), len(EXTRA)))


if __name__ == "__main__":
    main()
