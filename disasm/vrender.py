"""Shared model for the Tempest vector ROM: names, descriptions, cross
references, palette and a small AVG renderer (segments with colour).

Used by emit_vrom.py (listing), verify_vrom.py (symbols) and emit_shapes.py
(preview). Names come from vromsrc.py (ALVROM.MAC + ANVGAN.MAC walked and
locked onto the ROM).
"""
import os, re
import avg, vromsrc

HERE = os.path.dirname(os.path.abspath(__file__))
SRCLINES = os.path.join(HERE, "build", "srclines.json")    # track A output
SYMBOLS = os.path.join(HERE, "build", "symbols.json")      # track A output

# ---------------------------------------------------------------- colours
# ALVROM.MAC's own CSTAT colour equates (index into colour RAM $0800-$080F).
# NOTE ALVROM says BLUE=7 while ALCOMN says BLUE=6 (well) and BLULET=7
# (letters): both 6 and 7 are blue in the wave-1 palette.
COLOR_NAMES = {0: "WHITE", 1: "YELLOW", 2: "PURPLE", 3: "RED", 4: "TURQOI",
               5: "GREEN", 6: "BLUE (well)", 7: "BLUE (BLULET)", 8: "PSHCTR",
               9: "PDIWHI", 10: "PDIYEL", 11: "PDIRED", 12: "NYMCOL",
               13: "colour 13", 14: "colour 14", 15: "FLASH"}
# Wave-1 palette as INICOL loads it from COLTAB ($C1FD): low nibbles -> 0-7,
# high nibbles -> 8-15. Colour RAM is active low: b0 red-low, b1 red, b2 blue,
# b3 green (tempest_strobe3). PDIWHI/PDIYEL/PDIRED are rotated at run time
# (ROTCOL); their nominal names are used here.
def nibble_rgb(n):
    r = (0 if n & 2 else 0xF3) + (0 if n & 1 else 0x0C)
    g = 0 if n & 8 else 0xF3
    b = 0 if n & 4 else 0xF3
    return "#%02X%02X%02X" % (r, g, b)


def palette(mem, wave_index=0):
    base = 0xC1FD + 8 * wave_index
    pal = [nibble_rgb(mem[base + i] & 0xF) for i in range(8)]
    pal += [nibble_rgb(mem[base + i] >> 4) for i in range(8)]
    pal[9], pal[10], pal[11] = "#FFFFFF", "#FFFF00", "#FF3030"   # PDIWHI/YEL/RED nominal
    return pal


# ---------------------------------------------------------------- names
def clean(name):
    """Legal assembler identifier: CHAR.A -> CHAR_A, $$CRDT -> S_S_CRDT."""
    return name.replace("$", "S_").replace(".", "_")


# JSRL words the 6502 copies (not display lists executed in place)
WORD_LABELS = {"LSYMBL": "JSRL word copied by the 6502 into the score template (life symbol)",
               "LSYMB0": "JSRL word copied by the 6502 (blank life slot)",
               "JSRDOT": "JSRL word copied by the 6502 (ALDIS2 reads $3DB2/$3DB3) - a dot"}
DATA_LABELS = {"CHKSM0": "checksum byte QCHKS0 hidden in an RTSL word ($C0xx)",
               "CHKSM1": "checksum byte QCHKS1 hidden in an RTSL word ($C0xx)",
               "SYSOPT": "6502 pointer table (.WORD) of self-test option literals"}

CHAR_TEXT = {0: " "}
for _i in range(10):
    CHAR_TEXT[1 + _i] = str(_i)
for _i in range(26):
    CHAR_TEXT[11 + _i] = chr(65 + _i)
CHAR_TEXT.update({37: " ", 38: "-", 39: "½", 40: "©"})

FRAME_DESC = [
    (r"^EXPL(\d)$", "Explosion, frame {0} of 4 (16 spokes; 1 = smallest) - PICLO PTEXP1+"),
    (r"^STAR(\d)$", "Star field, frame {0} of 4 (warp dots) - PTSTR1+"),
    (r"^SPIRA(\d)$", "Spiker/trailer spiral, frame {0} of 4 - PTSPI1+"),
    (r"^SPARK(\d)$", "Sparkle (shot hit), frame {0} of 2 - PTSPAR+"),
    (r"^ESHOT(\d)$", "Enemy shot, frame {0} of 4 - PTESHO+"),
    (r"^SPLAT(\d)$", "Player-death splat entry {0}: sets scale then JSRL SPLAT and falls through the rest (PTSPLA)"),
    (r"^SPLFU(\d)$", "Fuseball-kills-player explosion, frame {0} of 7 (star field STAR1B at shrinking scale) - PTSPLF+"),
    (r"^FUSE(\d)$", "Fuseball, frame {0} (0-3) - PTFUSE+"),
    (r"^CHAR\.([A-Z0-9])$", "Character '{0}'"),
]
DESC = {
    "CHAR.": "Character blank (space): advance only",
    "VGMSGA": "Character JSRL table: index 0 blank, 1-10 digits, 11-36 A-Z (ASCVG code = index*2)",
    "DASH": "Character '-' (VGMSGA index 38; ASCVG '\\')",
    "COPYR": "Copyright circle-C (VGMSGA index 40; ASCVG '^')",
    "HALF": "'1/2' glyph (VGMSGA index 39)",
    "LIFEY": "Player life symbol in yellow (CSTAT YELLOW + LIFE1)",
    "LIFE1": "Player life symbol (small claw)",
    "LIFE0": "Blank life slot (advance only)",
    "SEVEN": "Self test: six intensity levels (lines at z=2..7)",
    "SEVEN2": "Self test: entry inside SEVEN",
    "INTEST": "Self test: colour / intensity bars picture",
    "HATCH": "Self test: cross-hatch pattern, then the alphabet (VGMSGA)",
    "CHEKER": "Self test: grid (15 horizontal + 11 vertical lines)",
    "HYSTER": "Self test: hysteresis box",
    "EASING": "Self test: 'ERASING' (EAROM) message",
    "COCMSG": "Self test: cocktail game marker 'C'",
    "CNWHSC": "Common init: CSTAT WHITE, CNTR, SCAL 1,0",
    "ROMRPI": "Self test: bad-ROM report frame (midline, then position for text)",
    "BONDRY": "Screen boundary rectangle (white)",
    "VORBOX": "Screen boundary rectangle, entry after colour/scale",
    "EXPL4": "Explosion, frame 4 of 4 (largest; no ICVEC) - PTEXP1+6",
    "DIARA2": "Player shot / charge: diamond of dots (PTCURS)",
    "STAR1B": "Star field burst: intensity 15, STAR1 + STAR2",
    "TANKP": "Pulsar tanker (tanker body + pulsar zigzag) - PTTANP",
    "TANKF": "Fuseball tanker (tanker body + coloured cross) - PTTANF",
    "TANKR": "Tanker (plain flipper tanker) - PTTANK",
    "GENTNK": "Tanker body (purple), shared tail of TANKR/TANKP/TANKF",
    "JADOT": "Single dot (VCTR 0,0,CB)",
    "SPLAT": "Player-death splat (multi-coloured, PDIWHI/PDIYEL/PDIRED)",
    "SHRAP": "Player-death shrapnel pieces (uses vector-RAM SCALE sub-list)",
    "FUSEX1": "Fuseball kill score '750'",
    "FUSEX2": "Fuseball kill score '500'",
    "FUSEX3": "Fuseball kill score '250'",
    "FIFTY": "Digits '50' (tail of FUSEX1/FUSEX3)",
    "ZERO": "Digit '0' (tail JMPL CHAR.0)",
    "SWNORM": "Master display list for play: JSRL every vector-RAM sub-buffer, JMPL VECRAM",
    "SWMSGS": "Master display list for messages only (JSRL SWINFO, JMPL VECRAM)",
    "SWHALT": "HALT the vector generator",
    "BOKLIT": "Bookkeeping screen literals (SECONDS ON / PLAYED / 1 & 2 PLAYER GAMES / AVERAGE / X1 BONUS ADDER)",
    "SECONDS": "Literal 'SECONDS '",
    "PLYGAM": "Literal ' PLAYER GAMES'",
    "FIRED": "Literal 'PRESS FIRE AND ' (red)",
    "TOZERO": "Literal ' TO ZERO '",
    "ENTEST": "Literal 'PRESS FIRE AND ZAP FOR SELF TEST'",
    "ZTIMES": "Literal 'PRESS FIRE AND START 1 TO ZERO TIMES'",
    "ZHISCO": "Literal 'PRESS FIRE AND START 2 TO ZERO SCORES'",
    "ZEASY": "Literal 'EASY'",
    "ZMED": "Literal 'MEDIUM'",
    "ZHARD": "Literal 'HARD'",
    "VORLIT": "TEMPEST logo (global name; same address as TEMLIT)",
    "TEMLIT": "TEMPEST logo",
    "T": "Logo letter T", "E": "Logo letter E", "M": "Logo letter M",
    "P": "Logo letter P", "S": "Logo letter S",
    "KILLER": "Beam to the corners with blank vectors, then RTSL (CHKSM1 word)",
}
DESC.update(WORD_LABELS)
DESC.update(DATA_LABELS)


def describe(name):
    if name in DESC:
        return DESC[name]
    for pat, txt in FRAME_DESC:
        m = re.match(pat, name)
        if m:
            return txt.format(*m.groups())
    return ""


# ---------------------------------------------------------------- model
class Model:
    def __init__(self, mem=None):
        self.mem = mem or avg.load_image()
        self.w = vromsrc.walk(self.mem)
        self.check = vromsrc.check(self.w)
        w = self.w
        self.stmts = [s for s in w.stmts if avg.VROM_LO <= s.addr <= avg.VROM_HI]
        self.cstmts = [s for s in w.stmts if s.addr >= vromsrc.CSECT_BASE]
        # labels by address, definition order (aliases such as CHAR.0 last)
        self.label_at = {}
        for nm, a in w.labels.items():
            self.label_at.setdefault(a, []).append(nm)
        # vector RAM equates usable as JSRL/JMPL operands
        self.vram = {}
        for nm, v in w.equates.items():
            if 0x2000 <= v <= 0x2FFF and re.match(r"^(SW|BA|BB)[A-Z]+$|^(VECRAM|SCOBUF|SCALE)$", nm):
                self.vram.setdefault(v, nm)
        self.refs = self._xrefs()

    def name(self, a):
        if a in self.label_at:
            return self.label_at[a][0]
        if a in self.vram:
            return self.vram[a]
        return None

    def sym(self, a):
        n = self.name(a)
        return clean(n) if n else "$%04X" % a

    def _xrefs(self):
        refs = {}
        self.routine_refs = {}
        for s in self.w.stmts:
            if s.kind == "avg" and s.op in ("JSRL", "JMPL"):
                t = avg.decode_raw(self.mem, s.addr)["target"]
                where = "vector ROM" if s.addr < 0x4000 else "program ROM table"
                refs.setdefault(t, []).append((s.addr, "%s %s" % (where, s.op)))
            elif s.kind == "data" and s.op == ".WORD":
                for k in range(len(s.args)):
                    t = avg.word(self.mem, s.addr + 2 * k)
                    if 0x3000 <= t <= 0x3FFF:
                        refs.setdefault(t, []).append((s.addr + 2 * k, "vector ROM .WORD"))
        # program ROM: every source statement (track A's byte-exact assembly,
        # build/srclines.json) whose operand names a vector ROM label, or a
        # PT* picture code (resolved through PICLO to the picture it selects)
        self.routine_refs = {}
        if not (os.path.exists(SRCLINES) and os.path.exists(SYMBOLS)):
            return refs
        import json, bisect
        syms = json.load(open(SYMBOLS))
        routines = sorted((s["addr"], s["name"]) for s in syms
                          if s["kind"] in ("code", "data") and 0x9000 <= s["addr"] <= 0xDFFF)
        rkeys = [a for a, _n in routines]
        vlabels = {n: a for n, a in self.w.labels.items() if 0x3000 <= a <= 0x3FFF}
        piclo = self.w.labels["PICLO"]
        ptcodes = {n: v for n, v in self.w.equates.items() if re.match(r"^PT[A-Z0-9]+$", n)}
        for ln in json.load(open(SRCLINES)):
            a = ln["addr"]
            if not (0x9000 <= a <= 0xDFFF) or ln["module"] == "ALVROM":
                continue                       # ALVROM's own CSECT tables are covered above
            text = ln.get("mnemonic_text") or ""
            hits = []
            for tok in re.findall(r"[A-Z0-9.$_]+", text.split(";")[0].upper()):
                if tok in vlabels:
                    hits.append((vlabels[tok], tok))
                elif tok in ptcodes:
                    t = avg.decode_raw(self.mem, piclo + ptcodes[tok])["target"]
                    hits.append((t, tok))
            if not hits:
                continue
            k = bisect.bisect_right(rkeys, a) - 1
            rname, radr = (routines[k][1], routines[k][0]) if k >= 0 else ("?", a)
            where = rname if radr == a else "%s+$%X" % (rname, a - radr)
            for t, tok in dict.fromkeys(hits):
                if (t, a) in self.routine_refs:
                    continue
                refs.setdefault(t, []).append(
                    (a, "program ROM %s (%s): %s" % (where, ln["module"], " ".join(text.split()))))
                self.routine_refs[(t, a)] = rname
        return refs


# ---------------------------------------------------------------- renderer
def render(mem, addr, max_ops=4000, one_op=False, state=None):
    """Execute AVG from addr. Returns (segments, colours_used, info).
    segment = (x0, y0, x1, y1, colour_or_None, brightness 0..15)."""
    st = state or {"x": 0.0, "y": 0.0, "scale": avg.scal_factor(1, 0),
                   "color": None, "intensity": 12}
    segs, colors, stack, pc, n = [], [], [], addr, 0
    info = {"vram_calls": 0}
    while n < max_ops and avg.VROM_LO <= pc <= avg.VROM_HI - 1:
        n += 1
        d = avg.decode_raw(mem, pc)
        op = d["op"]
        if op in ("VCTR", "SVEC"):
            nx = st["x"] + d["dx"] * st["scale"]
            ny = st["y"] + d["dy"] * st["scale"]
            z = d["z"]
            br = st["intensity"] if z == 1 else z * 2
            segs.append((st["x"], st["y"], nx, ny, st["color"], br))
            st["x"], st["y"] = nx, ny
        elif op == "CSTAT" or (op == "STAT" and d["color"] is not None):
            st["color"] = d["color"]
            if d["color"] not in colors:
                colors.append(d["color"])
        elif op == "STAT":
            st["intensity"] = d["intensity"]
        elif op == "SCAL":
            st["scale"] = avg.scal_factor(d["b"], d["l"])
        elif op == "CNTR":
            st["x"] = st["y"] = 0.0
        elif op == "JSRL":
            t = d["target"]
            if avg.VROM_LO <= t <= avg.VROM_HI and len(stack) < 8 and not one_op:
                stack.append(pc + 2)
                pc = t
                continue
            if one_op and avg.VROM_LO <= t <= avg.VROM_HI:
                s2, c2, _ = render(mem, t, max_ops, False, st)
                segs += s2
                colors += [c for c in c2 if c not in colors]
                break
            info["vram_calls"] += 1
        elif op == "JMPL":
            t = d["target"]
            if avg.VROM_LO <= t <= avg.VROM_HI:
                pc = t
                continue
            info["vram_calls"] += 1
            if not stack:
                break
            pc = stack.pop()
            continue
        elif op == "RTSL":
            if not stack:
                break
            pc = stack.pop()
            continue
        elif op == "HALT":
            break
        pc += d["len"]
        if one_op:
            break
    return segs, colors, info


def lit(segs):
    return any(br > 0 and (a, b) != (c, d) or (br > 0) for a, b, c, d, _col, br in segs)
