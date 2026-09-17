#!/usr/bin/env python3
"""Generate alwelg_data.h: the addresses of every ALWELG label ($9000-$A8AF), the
RTS-dispatch tables and the ROM tables of other modules ALWELG reads - never
hand-typed.  The C port reads the table bytes (wave parameter records, CAM
scripts, LEVEL, BONPTM/BONPTH, CHANCE, ...) with cpu_rd() over the generated
progrom[] image at these addresses, as the ROM does.

Inputs
  ../disasm/_survey/roms_extracted/tempest3_cpu64k.bin   64K CPU image (the dump)
  ../disasm/build/program.bin                            ca65 build of tempest_program_rom.asm
  ../disasm/tempest_program_rom.asm                      labels ("NAME:" then "Lxxxx:"), .alias lines
  ../disasm/build/symbols.json                           cross-check of the addresses it lists

Checks (the script stops on any failure)
  * the ALWELG range of the dump equals the assembled listing byte for byte
  * every label address agrees with symbols.json where symbols.json has it
  * the RTS-dispatch tables hold label-1 of the routines named in their source
    comments: SPARAD (parameter types), NPARAD (record skips), NYMTAD (new
    enemy types), TABJSR (20 CAM opcodes); entry 0 of SPARAD/NPARAD is $0000
  * no RTS-dispatch target follows a JSR (lockstep opens their checks on an RTS
    arrival, which must then be the dispatch)
  * WTABLE: 27 records of (record list in ALWELG, RAM cell < $0800)
  * the CAM scripts (CAM .. CAM+$98) hold only even opcodes < $28, and every
    branch / jump operand (VSETPC, VELOOP, VBR0PC) points inside the scripts
  * BONPTH is BONPTM + 1

Output (c_src/)
  alwelg_data.h  ALWELG_<LABEL> (all labels in the module), ROM_<LABEL> (other
                 modules), ALWELG_<TABLE>_N (dispatch table sizes)

Rerun:  python c_src/tools/gen_alwelg.py
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

LO, HI = 0x9000, 0xA8B0

# ROM tables outside ALWELG that it reads
EXTRA = ["D70MSK"]
DISPATCH = [
    ("SPARAD", [None, "SAMALL", "ITMIZE", "DOTZAN", "DOTA", "DOTB", "DOTR"]),
    ("NPARAD", [None, "ONEBYT", "NITMIZ", "NITMIZ", "TWOBYT", "ONEBYT", "TWOBYT"]),
    ("NYMTAD", ["NEWFLI", "NEWPUL", "NEWTAN", "NEWSPI", "NEWFUS"]),
    ("TABJSR", ["JEXIT", "JSLOOP", "JSKIP0", "JSETPC", "JELOOP", "JNOOP", "JSMOVE", "JSTRAI",
                "JSLOPB", "JJUMPS", "JJUMPM", "JCHROT", "JKITST", "JBR0PC", "JELTST", "JFUSEUP",
                "JFUSKI", "JPULMO", "JCHPLA", "JCHKPU"]),
]
CAM_BRANCH_OPS = (0x06, 0x08, 0x1A)     # VSETPC, VELOOP, VBR0PC: operand = target - CAM - 1
CAM_OPERAND_OPS = (0x02, 0x10)          # VSLOOP, VSLOPB: operand = value / zero-page cell
CAM_LEN = 0xA18F - 0xA0F7


def fail(msg):
    sys.exit("gen_alwelg: " + msg)


def main():
    img = open(IMAGE, "rb").read()
    prog = open(PROGBIN, "rb").read()
    if len(img) != 0x10000 or len(prog) != 0x5000:
        fail("image sizes wrong")
    if img[LO:HI] != prog[LO - 0x9000:HI - 0x9000]:
        fail("ALWELG bytes of the dump differ from the assembled listing")

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
    for n, a in labels.items():
        if not ((LO <= a < HI) or n in EXTRA) or n not in seen:
            continue
        if len(seen[n]) == 1 and a not in seen[n]:
            fail("%s: listing $%04X, symbols.json $%04X" % (n, a, min(seen[n])))

    if labels["BONPTH"] != labels["BONPTM"] + 1:
        fail("BONPTH is not BONPTM+1")

    rd16 = lambda a: img[a] | (img[a + 1] << 8)
    for tab, targets in DISPATCH:
        base = labels[tab]
        for i, t in enumerate(targets):
            w = rd16(base + 2 * i)
            if t is None:
                if w != 0:
                    fail("%s[%d] = $%04X, expected $0000" % (tab, i, w))
                continue
            if w + 1 != labels[t]:
                fail("%s[%d] = $%04X, expected %s-1 ($%04X)" % (tab, i, w, t, labels[t] - 1))
            if img[labels[t] - 3] == 0x20:
                fail("%s ($%04X) follows a JSR: an RTS arrival there is not only the dispatch" % (t, labels[t]))

    wt, we = labels["WTABLE"], labels["WTABEND"]
    if (we - wt) % 4:
        fail("WTABLE length is not a multiple of 4")
    for i in range(wt, we, 4):
        rec, cell = rd16(i), rd16(i + 2)
        if not (LO <= rec < HI) or cell >= 0x0800:
            fail("WTABLE entry at $%04X: records $%04X, cell $%04X" % (i, rec, cell))

    cam = labels["CAM"]
    if labels["MOVCHA"] - cam != CAM_LEN:
        fail("CAM scripts length $%X" % (labels["MOVCHA"] - cam))
    p = 0
    while p < CAM_LEN:
        op = img[cam + p]
        if op & 1 or op >= 0x28:
            fail("CAM+$%02X: opcode $%02X" % (p, op))
        if op in CAM_BRANCH_OPS:
            if not ((img[cam + p + 1] + 1) & 0xFF) < CAM_LEN:
                fail("CAM+$%02X: branch operand $%02X outside the scripts" % (p, img[cam + p + 1]))
            p += 2
        elif op in CAM_OPERAND_OPS:
            p += 2
        else:
            p += 1

    mod = sorted((a, n) for n, a in labels.items() if LO <= a < HI and not re.match(r"^L[0-9A-F]{4}$", n))
    h = os.path.join(CSRC, "alwelg_data.h")
    with open(h, "w", encoding="utf-8", newline="\n") as f:
        f.write("/* alwelg_data.h - GENERATED by c_src/tools/gen_alwelg.py from the ROM image\n"
                " * (cross-checked with the assembled listing and symbols.json) - do not edit.\n"
                " * ALWELG ($9000-$A8AF) label addresses; table bytes are read with cpu_rd(). */\n")
        f.write("#ifndef ALWELG_DATA_H\n#define ALWELG_DATA_H\n\n")
        f.write("/* ---- ALWELG labels (code and data) ---- */\n")
        for a, n in mod:
            f.write("#define ALWELG_%-8s 0x%04X\n" % (n, a))
        f.write("\n/* ---- ROM tables of other modules ALWELG reads ---- */\n")
        for n in EXTRA:
            f.write("#define ROM_%-8s 0x%04X\n" % (n, labels[n]))
        f.write("\n/* RTS-dispatch table sizes (entries; checked against their targets) */\n")
        for tab, targets in DISPATCH:
            f.write("#define ALWELG_%s_N %d  /* %s */\n" % (tab, len(targets), " ".join(t or "0" for t in targets)))
        f.write("\n#endif\n")
    print("wrote alwelg_data.h (%d ALWELG labels, %d extra ROM labels)" % (len(mod), len(EXTRA)))


if __name__ == "__main__":
    main()
