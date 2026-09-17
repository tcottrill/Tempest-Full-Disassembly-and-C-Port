#!/usr/bin/env python3
"""Generate allang_data.c/.h: the ALLANG message tables ($D031-$D702) and the
ROM label addresses ALSCO2 reads, from the ROM image - never hand-typed.

Inputs
  ../disasm/_survey/roms_extracted/tempest3_cpu64k.bin   64K CPU image (the dump)
  ../disasm/build/program.bin                            ca65 build of tempest_program_rom.asm
  ../disasm/build/symbols.json                           labels (name, addr, kind, module)
  ../disasm/tempest_defines.asm                          message numbers (MGAMOV = $00 ...), comments only

Checks (the script stops on any failure)
  * the ALLANG range of the dump equals the assembled listing byte for byte
  * every pointer in ENGMSG/FREMSG/GERMSG/SPAMSG lands on a literal label of
    that language (E/F/G/S + message name), and LNGTAB points at the 4 tables
  * every literal ends with a byte that has bit 7 set, inside the module

Outputs (c_src/)
  allang_data.h  ALLANG_<LABEL> addresses (all ALLANG labels), ROM_<LABEL>
                 addresses of the other ROM tables ALSCO2 indexes, table sizes,
                 extern declarations
  allang_data.c  the tables as C arrays:
                   allang_msgptr[4][30]  per-language literal pointers (ENGMSG..)
                   allang_msglbs[60]     MSGLBS: colour<<4|scale, Y per message
                   allang_lngtab[4]      LNGTAB
                   allang_tblifi[8]      TBLIFI (bonus life interval per switch)
                   allang_gamlvs[4]      GAMLVS (lives per game per switch)
                   allang_lit_<LABEL>[]  every literal: X byte, then VGMSGA codes
                                         (index*2, bit 7 = last); aliases are
                                         #defines to the first label

How the C port uses them (alsco2.c / allang.c): fixed tables indexed by a
masked register (TBLIFI, GAMLVS, LNGTAB, MSGLBS) are read from the arrays;
literals are reached through the RAM pointers LITRAL / INDYLO exactly as the
ROM does, i.e. with cpu_rd() over progrom (same bytes, checked above).

Rerun:  python c_src/tools/gen_allang.py
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
SYMS = os.path.join(ROOT, "disasm", "build", "symbols.json")
DEFINES = os.path.join(ROOT, "disasm", "tempest_defines.asm")

LO, HI = 0xD031, 0xD703            # ALLANG module (CHKSMA at $D703 is ALHAR2)
NMSG = 30
LANGS = [("ENGMSG", "E"), ("FREMSG", "F"), ("GERMSG", "G"), ("SPAMSG", "S")]

# ROM tables in other modules that ALSCO2 indexes (addresses only)
EXTRA = ["TCOMOD", "SCOSOL", "IHALF", "SCOINI", "HITRNK", "XPOTAB", "MSGTAB", "BOXTAB",
         "ENDMSG", "SCALOC", "LIVLOC", "SCOLOC", "HISLOC", "HIILOC", "SCORES", "SCECOU",
         "LEVEL", "ZATC4S"]

# ASCVG character codes (VGMSGA index * 2), for the comments only
def ch(code):
    c = code & 0x7F
    if c == 0x00:
        return " "
    if 0x02 <= c <= 0x14:
        return chr(ord("0") + (c - 0x02) // 2)
    if 0x16 <= c <= 0x48:
        return chr(ord("A") + (c - 0x16) // 2)
    return {0x4A: "?", 0x4C: "-", 0x4E: "/", 0x50: "(c)"}.get(c, "<%02X>" % c)


def fail(msg):
    sys.exit("gen_allang: " + msg)


def main():
    img = open(IMAGE, "rb").read()
    prog = open(PROGBIN, "rb").read()
    if len(img) != 0x10000 or len(prog) != 0x5000:
        fail("image sizes wrong")
    if img[LO:HI] != prog[LO - 0x9000:HI - 0x9000]:
        fail("ALLANG bytes of the dump differ from the assembled listing")
    syms = json.load(open(SYMS))
    by_name = {}
    allang = []
    for s in syms:
        if s["kind"] in ("data", "code") and s["name"] not in by_name:
            by_name[s["name"]] = s["addr"]
        if s.get("module") == "ALLANG" and s["kind"] in ("data", "code"):
            allang.append((s["addr"], s["name"]))
    allang.sort()
    at = {}
    for a, n in allang:
        at.setdefault(a, []).append(n)

    rd8 = lambda a: img[a]
    rd16 = lambda a: img[a] | (img[a + 1] << 8)

    # pointer tables
    ptr = []
    for tab, pfx in LANGS:
        base = by_name[tab]
        row = []
        for i in range(NMSG):
            p = rd16(base + 2 * i)
            if not any(n.startswith(pfx) for n in at.get(p, [])):
                fail("%s[%d] = $%04X is not a %s literal" % (tab, i, p, pfx))
            row.append(p)
        ptr.append(row)
    lng = [rd16(by_name["LNGTAB"] + 2 * i) for i in range(4)]
    if lng != [by_name[t] for t, _ in LANGS]:
        fail("LNGTAB does not point at ENGMSG/FREMSG/GERMSG/SPAMSG")
    msglbs = img[by_name["MSGLBS"]:by_name["MSGLBS"] + 2 * NMSG]
    tblifi = img[by_name["TBLIFI"]:by_name["TBLIFI"] + 8]
    gamlvs = img[by_name["GAMLVS"]:by_name["GAMLVS"] + 4]
    if by_name["GAMLVS"] + 4 != HI:
        fail("GAMLVS is not the last table of the module")

    # message names: the English pointer entries carry them (EGAMOV -> GAMOV);
    # where two messages share a literal, take the one whose name matches the
    # defines file's message number (MPLAYR = $02, MPLYR2 = $04)
    msgnum = {}
    for line in open(DEFINES):
        m = re.match(r"^\.alias\s+M(\w+)\s+\$([0-9A-Fa-f]+)", line)
        if m:
            msgnum.setdefault(int(m.group(2), 16), []).append(m.group(1))
    names = []
    for i, p in enumerate(ptr[0]):
        cand = [n[1:] for n in at[p] if n.startswith("E")]
        hit = [n for n in msgnum.get(2 * i, []) if n in cand]
        if len(hit) != 1:
            fail("message $%02X: no unique English label among %s" % (2 * i, cand))
        names.append(hit[0])
    # preferred array name: a language label, not a checksum marker (ZATLIS/ZATLIE)
    for a in at:
        at[a].sort(key=lambda n: n.startswith("ZAT"))

    # literals: every address a pointer table references
    lits = sorted(set(p for row in ptr for p in row))
    litlen = {}
    for p in lits:
        q = p + 1
        while q < HI and not (img[q] & 0x80):
            q += 1
        if q >= HI:
            fail("literal $%04X has no end byte" % p)
        litlen[p] = q + 1 - p

    h = os.path.join(CSRC, "allang_data.h")
    c = os.path.join(CSRC, "allang_data.c")
    with open(h, "w", encoding="utf-8", newline="\n") as f:
        f.write("/* allang_data.h - GENERATED by c_src/tools/gen_allang.py from the ROM image\n"
                " * (cross-checked with the assembled listing) - do not edit.\n"
                " * ALLANG ($D031-$D702): message tables in four languages. */\n")
        f.write("#ifndef ALLANG_DATA_H\n#define ALLANG_DATA_H\n#include <stdint.h>\n\n")
        f.write("/* ---- ALLANG labels ---- */\n")
        for a, n in allang:
            f.write("#define ALLANG_%-8s 0x%04X\n" % (n, a))
        f.write("\n/* ---- ROM tables of other modules indexed by ALSCO2 ---- */\n")
        for n in EXTRA:
            if n not in by_name:
                fail("no label " + n)
            f.write("#define ROM_%-8s 0x%04X\n" % (n, by_name[n]))
        f.write("\n#define ALLANG_NMSG %d  /* messages per language (message number = index * 2) */\n\n" % NMSG)
        f.write("extern const uint16_t allang_msgptr[4][ALLANG_NMSG];  /* ENGMSG, FREMSG, GERMSG, SPAMSG */\n")
        f.write("extern const uint8_t  allang_msglbs[2 * ALLANG_NMSG]; /* MSGLBS */\n")
        f.write("extern const uint16_t allang_lngtab[4];                /* LNGTAB */\n")
        f.write("extern const uint8_t  allang_tblifi[8];                /* TBLIFI */\n")
        f.write("extern const uint8_t  allang_gamlvs[4];                /* GAMLVS */\n\n")
        f.write("/* literals: signed X byte, then VGMSGA codes (index*2), bit 7 = last */\n")
        for p in lits:
            first = at[p][0]
            f.write("extern const uint8_t allang_lit_%s[%d];\n" % (first, litlen[p]))
            for alias in at[p][1:]:
                f.write("#define allang_lit_%s allang_lit_%s\n" % (alias, first))
        f.write("\n#endif\n")
    with open(c, "w", encoding="utf-8", newline="\n") as f:
        f.write("/* allang_data.c - GENERATED by c_src/tools/gen_allang.py from the ROM image\n"
                " * (cross-checked with the assembled listing) - do not edit. */\n")
        f.write('#include "allang_data.h"\n\n')
        f.write("const uint16_t allang_msgptr[4][ALLANG_NMSG] = {\n")
        for (tab, _), row in zip(LANGS, ptr):
            f.write("    { /* %s $%04X */\n" % (tab, by_name[tab]))
            for i, p in enumerate(row):
                f.write("        0x%04X, /* M%-6s $%02X -> %s */\n" % (p, names[i], 2 * i, at[p][0]))
            f.write("    },\n")
        f.write("};\n\nconst uint8_t allang_msglbs[2 * ALLANG_NMSG] = {  /* $%04X */\n" % by_name["MSGLBS"])
        for i in range(NMSG):
            f.write("    0x%02X, 0x%02X, /* M%-6s colour %d, scale %d, Y %d */\n"
                    % (msglbs[2 * i], msglbs[2 * i + 1], names[i], msglbs[2 * i] >> 4, msglbs[2 * i] & 15,
                       msglbs[2 * i + 1] - 256 if msglbs[2 * i + 1] & 0x80 else msglbs[2 * i + 1]))
        f.write("};\n\nconst uint16_t allang_lngtab[4] = { %s };  /* $%04X */\n"
                % (", ".join("0x%04X" % x for x in lng), by_name["LNGTAB"]))
        f.write("const uint8_t  allang_tblifi[8] = { %s };  /* $%04X */\n"
                % (", ".join("0x%02X" % x for x in tblifi), by_name["TBLIFI"]))
        f.write("const uint8_t  allang_gamlvs[4] = { %s };  /* $%04X */\n\n"
                % (", ".join("0x%02X" % x for x in gamlvs), by_name["GAMLVS"]))
        for p in lits:
            bs = img[p:p + litlen[p]]
            x = bs[0] - 256 if bs[0] & 0x80 else bs[0]
            text = "".join(ch(b) for b in bs[1:])
            f.write("/* $%04X %s: X %d \"%s\" */\n" % (p, " ".join(at[p]), x, text))
            f.write("const uint8_t allang_lit_%s[%d] = { %s };\n"
                    % (at[p][0], litlen[p], ", ".join("0x%02X" % b for b in bs)))
    print("wrote allang_data.c/.h (%d labels, %d literals, %d extra ROM labels)"
          % (len(allang), len(lits), len(EXTRA)))


if __name__ == "__main__":
    main()
