"""Emit disasm/tempest_defines.asm - hardware map, RAM variables and equates (Ophis .alias form).

Layout copied from Space Duel's spaceduel_defines.asm: a hand-written memory map with
descriptive names, then sections generated from build/symbols.json (Atari's names from
ALCOMN.MAC and the modules, as assembled by macasm.py):
  Hardware registers   every I/O equate (ALCOMN 'HARDWARE DEFINITIONS'), with bit notes
  Zero page variables  RAM $0000-$00FF
  Stack page           RAM $0100-$01FF (Tempest keeps game variables below the stack)
  Game RAM             RAM $0200-$07FF
  Vector RAM           ALVROM sub-buffer equates the program uses
  Vector ROM           vector ROM labels the program uses
  Constants            every ALCOMN constant, plus module equates the listing uses
Identifiers are the same ones tempest_program_rom.asm uses (emit.Names), so this file
defines every symbol the listing needs (build/program_refs.json lists them).
Descriptions are Atari's own comments on the definitions; the hardware bit notes come
from ALCOMN.MAC and the AAE driver (tempest.cpp), see _survey/tempest_inputs.md.
Aliases with no source comment take a description from define_notes_*.txt
(NAME<TAB>text per line, '#' comments); unknown or already-commented names are warned about.

Run:  python emit_defines.py   (after emit.py)
"""
import os, sys, json, collections, glob
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from emit import Names, clean, tidy, LO, HI

BUILD = os.path.join(HERE, "build")
OUT = os.path.join(HERE, "tempest_defines.asm")

MEMORY_MAP = [
    ("ZeroPageRam",  0x0000, "Through $00FF. Control, work, player and display variables."),
    ("PageOneRam",   0x0100, "Through $01FF. Game variables; the 6502 stack grows down from $01FF (the IRQ requires SP >= $D0)."),
    ("GameRam",      0x0200, "Through $07FF. Object tables, explosions, enemy lines, high scores."),
    ("ColorRam",     0x0800, "Through $080F. 16 colour entries, 4 bits active low: b0 red-low, b1 red, b2 blue, b3 green."),
    ("VectorRam",    0x2000, "Through $2FFF. AVG display lists (sub-buffers, see Vector RAM below)."),
    ("VectorRom",    0x3000, "Through $3FFF. Characters and pictures (tempest_vector_rom.asm)."),
    ("AuxBoardIo",   0x6000, "EAROM, Mathbox, two POKEYs, LED/flip latch (HARDWA)."),
    ("ProgramRom",   0x9000, "Through $DFFF (tempest_program_rom.asm); vectors at $DFFA."),
    ("RomMirror",    0xE000, "Through $FFFF. Mirror of the top program ROM; the 6502 reads its vectors here."),
]

HW_NOTES = {
    "COLPOR":  "Colour RAM $0800-$080F (write): 16 entries, 4 bits active low (b0 red-low, b1 red, b2 blue, b3 green).",
    "IN1":     "Read: b0 right coin, b1 centre coin, b2 left coin, b3 slam, b4 self test, b5 diag step, b6 VG HALT (1 = done), b7 3 kHz clock.",
    "INOP0":   "Read: option switches N13 (coinage).",
    "INOP1":   "Read: option switches L12 (minimum credits, language, bonus life, lives).",
    "OUT0":    "Write: b0 right coin counter, b1 centre, b2 left, b3 invert X (flip), b4 invert Y.",
    "VGSTAR":  "Write: start the AVG at vector RAM $2000.",
    "WTCHDG":  "Write: watchdog clear (the IRQ does it only while the software watchdog is happy).",
    "INTACK":  "Same address as WTCHDG: the write also acknowledges the IRQ.",
    "VGSTOP":  "Write: reset (halt) the AVG.",
    "HARDWA":  "Auxiliary board I/O base.",
    "EADAL":   "Write $6000-$603F: EAROM address = offset, data = value latched for the next write.",
    "EACTL":   "Write: EAROM control (EACK clock, EAC2, EAC1 inverted, EACE chip select).",
    "MSTAT":   "Read: Mathbox status, D7 = 1 while busy.",
    "EAIN":    "Read: EAROM data.",
    "MYLOW":   "Read: Mathbox result low byte.",
    "MYHIGH":  "Read: Mathbox result high byte.",
    "MBSTAR":  "Write $6080-$609F: Mathbox register load / function start (MAL..MDYPL below).",
    "POKEY":   "POKEY 1 ($60C0-$60CF): sound channels 1-4, spinner and cocktail input.",
    "ALLPOT":  "POKEY 1 read: b0-b3 spinner (4-bit position), b4 cocktail. (Write: AUDCTL.)",
    "RANDOM":  "POKEY 1 read: random number (also used by the POKEY protection checks).",
    "POTGO":   "POKEY 1 write: start pot scan.",
    "SKCTL":   "POKEY 1 serial/keyboard control.",
    "POKEY2":  "POKEY 2 ($60D0-$60DF): sound channels 5-8 and buttons.",
    "ALLPO2":  "POKEY 2 read: b0-b1 difficulty, b2 rating, b3 superzapper, b4 fire, b5 start 1, b6 start 2. (Write: AUD2CTL.)",
    "RANDO2":  "POKEY 2 read: random number.",
    "POTGO2":  "POKEY 2 write: start pot scan.",
    "SKCTL2":  "POKEY 2 serial/keyboard control.",
    "OUTANK":  "Write: b0 LED 2 (MLED2), b1 LED 1 (MLED1), b2 flip (MFLIP); LEDs active low.",
}

def load_notes():
    """Descriptions written for this disassembly, for aliases Atari left uncommented.
    Every define_notes_*.txt next to this script: NAME<TAB>description, '#' lines are comments."""
    notes = {}
    for path in sorted(glob.glob(os.path.join(HERE, "define_notes_*.txt"))):
        with open(path, encoding="utf-8") as fp:
            for n, line in enumerate(fp, 1):
                line = line.rstrip("\r\n")
                if not line.strip() or line.lstrip().startswith("#"):
                    continue
                name, sep, text = line.partition("\t")
                name, text = name.strip(), text.strip()
                if not sep or not name or not text:
                    print("!! %s:%d: expected NAME<TAB>description" % (os.path.basename(path), n))
                    continue
                if name in notes:
                    print("warning: %s:%d: %s described again (keeping the later one)" %
                          (os.path.basename(path), n, name))
                notes[name] = (text, "%s:%d" % (os.path.basename(path), n))
    return notes

def banner(t):
    return ";" + "-" * 34 + "[ " + t + " ]" + "-" * 34

def fmt(name, value, comment):
    v = ("$%02X" % value) if value < 0x100 else ("$%04X" % value)
    s = ".alias %-16s %s" % (name, v)
    if comment:
        s = "%-32s ;%s" % (s, comment)
    return s

def main():
    symbols = json.load(open(os.path.join(BUILD, "symbols.json")))
    refs = json.load(open(os.path.join(BUILD, "program_refs.json")))["refs"]
    names = Names(symbols, None, None)
    out = [
        ";Tempest (Atari, 1981) - hardware map, RAM variables and equates for",
        ";tempest_program_rom.asm (rev 3).",
        ";Names are Atari's identifiers from ALCOMN.MAC and the program modules",
        ";(MAC65 symbols are significant to 6 characters, so a longer spelling such as",
        ";QDSTATE is the same symbol as QDSTAT; '.' -> '_' and '$' -> 'S_').  A name the",
        ";source defines with two different values keeps the name for the main one;",
        ";ROM/RAM labels among the others get an _ADDR suffix.",
        ";Descriptions are Atari's comments on the definitions (upper case) and, for the",
        ";hardware, bit notes from ALCOMN.MAC and the AAE driver tempest.cpp.",
        ";Generated by disasm/emit_defines.py; every symbol the program listing uses is here.",
        "",
        banner("Memory map"),
    ]
    for n, a, c in MEMORY_MAP:
        out.append(fmt(n, a, c))

    done = set(n for n, _, _ in MEMORY_MAP)
    rows = collections.defaultdict(list)     # section -> [(addr, ident, comment, sortkey)]
    for s in symbols:
        if s["kind"] == "local":
            continue
        ident = names.ident.get((s["name"], s["addr"]))
        if ident is None or ident in done:
            continue
        a, k = s["addr"], s["kind"]
        com = tidy(s.get("comment") or "")
        if k == "io":
            sec = "Hardware registers"
            note = HW_NOTES.get(s["name"])
            if com and note and not note.upper().startswith(com.upper()):
                com = com + ". " + note
            else:
                com = note or com
        elif k == "ram" and a < 0x100:
            sec = "Zero page variables"
        elif k == "ram" and a < 0x200:
            sec = "Stack page"
        elif k == "ram":
            sec = "Game RAM"
        elif ident in refs and 0x2000 <= a < 0x3000:
            sec = "Vector RAM"
        elif ident in refs and 0x3000 <= a < 0x4000 and k in ("code", "data"):
            sec = "Vector ROM"
        elif k in ("code", "data") and LO <= a < HI:
            continue                                  # labels live in the listing
        elif ident in refs or (s["file"] == "ALCOMN.MAC" and k == "equate" and not s.get("redefined")
                               and not s.get("from_macro")):
            sec = "Constants"
        else:
            continue
        if ident in refs or k in ("ram", "io") or sec == "Constants":
            done.add(ident)
            rows[sec].append((a, ident, com, (s["file"], s["line"]), s))
    missing = [r for r in refs if r not in done]
    for r in missing:
        info = refs[r]
        rows["Constants"].append((info["addr"], r, tidy(info.get("comment") or ""), (info["file"], info["line"]),
                                  {"file": info["file"], "sbttl": ""}))
        done.add(r)

    notes = load_notes()
    seen = set()
    def described(ident, com):
        if ident not in notes:
            return com
        seen.add(ident)
        if com:
            print("warning: %s: %s already has a comment; note not used" % (notes[ident][1], ident))
            return com
        return notes[ident][0]

    for sec in ("Hardware registers", "Zero page variables", "Stack page", "Game RAM", "Vector RAM", "Vector ROM"):
        out.append("")
        out.append(banner(sec))
        for a, ident, com, _, s in sorted(rows[sec], key=lambda x: (x[0], x[3])):
            out.append(fmt(ident, a, described(ident, com)))
    out.append("")
    out.append(banner("Constants"))
    last = None
    for a, ident, com, key, s in sorted(rows["Constants"], key=lambda x: x[3]):
        head = "%s  %s" % (s["file"], s.get("sbttl", ""))
        if head != last:
            out.append(";" + head.strip())
            last = head
        out.append(fmt(ident, a, described(ident, com)))
    for n, _, c in MEMORY_MAP:
        if n in notes:
            described(n, c)
    for name in sorted(set(notes) - seen):
        print("warning: %s: %s is not an alias in tempest_defines.asm" % (notes[name][1], name))
    uncommented = sum(1 for l in out if l.startswith(".alias") and ";" not in l)
    with open(OUT, "w", newline="\n") as fp:
        fp.write("\n".join(out) + "\n")
    cnt = {k: len(v) for k, v in rows.items()}
    print("wrote %s: %s; %d identifiers referenced by the listing, all defined" %
          (os.path.relpath(OUT, HERE), ", ".join("%s %d" % kv for kv in sorted(cnt.items())), len(refs)))
    print("  %d descriptions from define_notes_*.txt; %d aliases still without a comment" %
          (len(seen - set(n for n, _, _ in MEMORY_MAP)), uncommented))

if __name__ == "__main__":
    main()
