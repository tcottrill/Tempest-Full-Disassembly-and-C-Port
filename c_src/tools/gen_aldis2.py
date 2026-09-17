#!/usr/bin/env python3
"""Generate aldis2_data.h: the addresses of every ALDIS2 label ($B1B6-$C79F) and
of the ALVROM / other-module ROM tables and routines ALDIS2 reads or dispatches
to - never hand-typed.  The C port reads the table bytes with cpu_rd() over the
generated progrom[] / vecrom[] images at these addresses (as the ROM does).

Inputs
  ../disasm/_survey/roms_extracted/tempest3_cpu64k.bin   64K CPU image (the dump)
  ../disasm/build/program.bin                            ca65 build of tempest_program_rom.asm
  ../disasm/tempest_program_rom.asm                      labels ("NAME:" then "Lxxxx:"), .alias lines
  ../disasm/build/symbols.json                           cross-check of the addresses it lists

Checks (the script stops on any failure)
  * the ALDIS2 range of the dump equals the assembled listing byte for byte
  * every label address agrees with symbols.json where symbols.json has it
  * the RTS-dispatch tables hold label-1 of the routines named in their source
    comments: DROUTAD (12 display states), INVPIT (5 invader pictures), XSUBR
    (4 special explosion functions)
  * the high-byte aliases are their low label + 1 (BUFASH, BUFBSH, BUFSWH,
    JMPAHI, JMPBHI, JMPMAH, PICHI)

Output (c_src/)
  aldis2_data.h  ALDIS2_<LABEL> (all labels in the module), ROM_<LABEL> (other
                 modules), ALDIS2_DROUTAD_N / _INVPIT_N / _XSUBR_N

Rerun:  python c_src/tools/gen_aldis2.py
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
SYMS = os.path.join(ROOT, "disasm", "build", "symbols.json")

LO, HI = 0xB1B6, 0xC7A0

# ROM tables / routines outside ALDIS2 that it reads or dispatches to
EXTRA = ["BUFASL", "BUFASH", "BUFBSL", "BUFBSH", "BFASTA", "BFBSTA", "BUFSWL", "BUFSWH",
         "JMPALO", "JMPAHI", "JMPBLO", "JMPBHI", "JMPMAL", "JMPMAH", "PICLO", "PICHI",
         "GETDSP", "RQRDSP", "LDRDSP", "DGOVER", "DPLPLA", "DPRSTA", "BOXPRO", "LOGPRO",
         "D2GAME", "DSPSYS"]
HIGH_ALIASES = [("BUFASH", "BUFASL"), ("BUFBSH", "BUFBSL"), ("BUFSWH", "BUFSWL"),
                ("JMPAHI", "JMPALO"), ("JMPBHI", "JMPBLO"), ("JMPMAH", "JMPMAL"), ("PICHI", "PICLO")]
DISPATCH = [
    ("DROUTAD", ["DENORM", "DSPSYS", "DSBOOM", "GETDSP", "RQRDSP", "LDRDSP", "DGOVER", "DPLPLA",
                 "DPRSTA", "BOXPRO", "LOGPRO", "D2GAME"]),
    ("INVPIT", ["FLIPIC", "PULPIC", "TANPIC", "TRAPIC", "FUSPIC"]),
    ("XSUBR", ["ALTCOL", "ROTCOL", "SETSHR", "SHRSCA"]),
]


def fail(msg):
    sys.exit("gen_aldis2: " + msg)


def main():
    img = open(IMAGE, "rb").read()
    prog = open(PROGBIN, "rb").read()
    if len(img) != 0x10000 or len(prog) != 0x5000:
        fail("image sizes wrong")
    if img[LO:HI] != prog[LO - 0x9000:HI - 0x9000]:
        fail("ALDIS2 bytes of the dump differ from the assembled listing")

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
    syms = json.load(open(SYMS))
    seen = {}
    for s in syms:
        if s["kind"] in ("data", "code") and 0x9000 <= s["addr"] < 0xE000:
            seen.setdefault(s["name"], set()).add(s["addr"])
    checked = 0
    for n, a in labels.items():
        if not ((LO <= a < HI) or n in EXTRA) or n not in seen:
            continue
        if len(seen[n]) == 1:           # names defined twice in the sources (VGCNTR) are skipped
            if a not in seen[n]:
                fail("%s: listing $%04X, symbols.json $%04X" % (n, a, min(seen[n])))
            checked += 1

    for hi, lo in HIGH_ALIASES:
        if labels.get(hi) != labels.get(lo, -2) + 1:
            fail("%s is not %s+1" % (hi, lo))

    rd16 = lambda a: img[a] | (img[a + 1] << 8)
    for tab, targets in DISPATCH:
        base = labels[tab]
        for i, t in enumerate(targets):
            if rd16(base + 2 * i) + 1 != labels[t]:
                fail("%s[%d] = $%04X, expected %s-1 ($%04X)" % (tab, i, rd16(base + 2 * i), t, labels[t] - 1))

    mod = sorted((a, n) for n, a in labels.items() if LO <= a < HI and not re.match(r"^L[0-9A-F]{4}$", n))
    h = os.path.join(CSRC, "aldis2_data.h")
    with open(h, "w", encoding="utf-8", newline="\n") as f:
        f.write("/* aldis2_data.h - GENERATED by c_src/tools/gen_aldis2.py from the ROM image\n"
                " * (cross-checked with the assembled listing and symbols.json) - do not edit.\n"
                " * ALDIS2 ($B1B6-$C79F) label addresses; table bytes are read with cpu_rd(). */\n")
        f.write("#ifndef ALDIS2_DATA_H\n#define ALDIS2_DATA_H\n\n")
        f.write("/* ---- ALDIS2 labels (code and data) ---- */\n")
        for a, n in mod:
            f.write("#define ALDIS2_%-8s 0x%04X\n" % (n, a))
        f.write("\n/* ---- ROM tables and routines of other modules ALDIS2 reads or dispatches to ---- */\n")
        for n in EXTRA:
            f.write("#define ROM_%-8s 0x%04X\n" % (n, labels[n]))
        f.write("\n/* RTS-dispatch table sizes (entries; checked against their targets) */\n")
        for tab, targets in DISPATCH:
            f.write("#define ALDIS2_%s_N %d  /* %s */\n" % (tab, len(targets), " ".join(targets)))
        f.write("\n#endif\n")
    print("wrote aldis2_data.h (%d ALDIS2 labels, %d extra ROM labels)" % (len(mod), len(EXTRA)))


if __name__ == "__main__":
    main()
