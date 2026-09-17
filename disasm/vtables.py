"""Vector-related data tables in the Tempest program ROM (rev 3), decoded
enough to render them. Writes build/vector_tables.json for the program-ROM
track and feeds emit_shapes.py.

  * VGMSGA character JSRL table (vector ROM $31E4) + ASCVG encoding
  * ALVROM .CSECT tables at $CDDE-$CF23 (walked by vromsrc.py and verified):
    SCALOC/LIVLOC/SCOLOC/HISLOC/HIILOC, SCORES template, SCECOU, BUFASL,
    BUFBSL, BUFSWL, JMPALO, JMPBLO, JMPMAL, PICLO/PICHI picture table (PT*)
  * ALLANG messages: ENGMSG/FREMSG/GERMSG/SPAMSG pointer tables, MSGLBS
    colour/scale/Y table, LNGTAB, the literal strings in all 4 languages
  * ALDIS2 'between two points' pictures PCOUNT/PINDEX/VBASE (flipper INVA1,
    player claw NCRS1-8, pulsar PULS4-0) - not AVG, a byte format ONELN2
    turns into vectors relative to the two lane end points
  * COLTAB colour table (INICOL), SYSOPT
"""
import json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import avg, vromsrc, vrender

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "build", "vector_tables.json")
H = lambda a: "$%04X" % a

ALCOMN_COLORS = {0: "WHITE", 1: "YELLOW", 2: "PURPLE", 3: "RED", 4: "TURQOI",
                 5: "GREEN", 6: "BLUE", 7: "BLULET"}
LANGS = ["English", "French", "German", "Spanish"]


def find(mem, pat, lo=0x9000, hi=0xE000):
    pat = bytes(pat)
    return [i for i in range(lo, hi) if mem[i:i + len(pat)] == pat]


def char_of(code):
    return vrender.CHAR_TEXT.get((code & 0x7F) // 2, "?")


def s8(b):
    return b - 256 if b & 0x80 else b


def messages(mem):
    lines = vromsrc.read_mac("ALLANG.MAC")
    mess = []
    for ln in lines:
        m = re.match(r"^MESS\s+(\w+),(\w+),([0-9A-F]+),(-?[0-9A-F]+\.?)\s*;?(.*)$", ln.strip())
        if m:
            mess.append((m.group(1), m.group(2), m.group(5).strip()))
    n = len(mess)
    lng = find(mem, [0x31, 0xD0], 0xD031, 0xD703)
    # LNGTAB = four words ENG, ENG+2n, ENG+4n, ENG+6n
    eng = 0xD031
    lngtab = find(mem, [eng & 0xFF, eng >> 8, (eng + 2 * n) & 0xFF, (eng + 2 * n) >> 8], 0xD031, 0xD703)
    tables = [eng + 2 * n * k for k in range(4)]
    msglbs = eng + 8 * n
    out = []
    for i, (nm, col, desc) in enumerate(mess):
        cs, y = mem[msglbs + 2 * i], mem[msglbs + 2 * i + 1]
        ent = dict(number=2 * i, name="M" + nm, comment=desc,
                   color=cs >> 4, color_name=ALCOMN_COLORS.get(cs >> 4, str(cs >> 4)),
                   source_color=col, scale=cs & 0xF, y=s8(y), texts={})
        for k, lang in enumerate(LANGS):
            p = avg.word(mem, tables[k] + 2 * i)
            codes, a = [], p + 1
            while True:
                codes.append(mem[a])
                if mem[a] & 0x80 or len(codes) > 40:
                    break
                a += 1
            ent["texts"][lang] = dict(addr=H(p), x=s8(mem[p]),
                                      text="".join(char_of(c) for c in codes),
                                      codes=[(c & 0x7F) // 2 for c in codes])
        out.append(ent)
    return dict(count=n, ENGMSG=H(tables[0]), FREMSG=H(tables[1]), GERMSG=H(tables[2]),
                SPAMSG=H(tables[3]), MSGLBS=H(msglbs),
                LNGTAB=H(lngtab[0]) if lngtab else None,
                format="per language: .WORD pointer per message (index = message number/2); "
                       "literal = signed X byte, then ASCVG codes (VGMSGA index*2, bit7 = last). "
                       "MSGLBS: byte (colour<<4 | scale), signed Y byte",
                messages=out)


def between_points(mem):
    names = ["INVA1"] + ["NCRS%d" % k for k in range(1, 9)] + ["PULS%d" % k for k in (4, 3, 2, 1, 0)]
    desc = {"INVA1": "Flipper (INVADER 1)", "PULS4": "Pulsar, frame PULS4 (tallest zigzag)",
            "PULS0": "Pulsar, frame PULS0 (flat line)"}
    counts = [8] * 8 + [9, 6, 7, 7, 4, 2]
    pc = find(mem, counts)
    assert len(pc) == 1
    pcount = pc[0]
    pindex, vbase = pcount + 14, pcount + 28
    pics = []
    for k, nm in enumerate(names):
        cnt, idx = mem[pcount + k], mem[pindex + k]
        vecs = []
        for j in range(cnt):
            b0, b1 = mem[vbase + idx + 2 * j], mem[vbase + idx + 2 * j + 1]
            ux = (b0 & 7) * (-1 if b0 & 0x40 else 1)
            uz = ((b0 >> 3) & 7) * (-1 if b0 & 0x80 else 1)
            vecs.append(dict(unit=ux, perp=uz, bright=b1, bytes="%02X %02X" % (b0, b1)))
        d = desc.get(nm) or ("Player claw (cursor), frame %s" % nm[4:] if nm.startswith("NCRS")
                             else "Pulsar, frame %s" % nm)
        pics.append(dict(name=nm, index=k, count=cnt, offset=idx, addr=H(vbase + idx), desc=d, vectors=vecs))
    return dict(PCOUNT=H(pcount), PINDEX=H(pindex), VBASE=H(vbase),
                format="2 bytes per vector. byte0: D7 sign of perpendicular multiplier, D6 sign of "
                       "unit multiplier, D5-D3 |perp|, D2-D0 |unit|. Vector = unit*U + perp*P where U is the "
                       "(scaled, ~1/16) vector from lane point 1 to point 2 and P = U rotated +90 degrees. "
                       "byte1: 0 beam off, 1 depth-cue intensity, $10 dot, else intensity.",
                pictures=pics)


def csect_tables(M):
    w, mem = M.w, M.mem
    pt = {v: k for k, v in w.equates.items() if re.match(r"^PT[A-Z0-9]+$", k)}
    out = {"section": "ALVROM .CSECT $CDDE-$CF23 (vromsrc walk, %d statements verified)"
                      % sum(1 for s in M.cstmts if s.value_ok)}
    labs = sorted((a, n) for n, a in w.labels.items() if a >= vromsrc.CSECT_BASE)
    out["labels"] = {n: H(a) for a, n in labs}
    # picture table
    piclo = w.labels["PICLO"]
    pics = []
    for s in M.cstmts:
        if s.addr >= piclo and s.kind == "avg" and s.op == "JSRL":
            t = avg.decode_raw(mem, s.addr)["target"]
            code = s.addr - piclo
            pics.append(dict(code=code, code_name=pt.get(code), addr=H(s.addr), target=H(t),
                             target_name=M.name(t), comment=s.comment))
    out["PICLO"] = dict(addr=H(piclo), PICHI=H(piclo + 1), entries=pics,
                        format="JSRL word per picture; code = offset (even); ALDIS2 copies PICLO,Y / PICHI,Y into vector RAM")
    # score template and pointer tables: list every statement
    rows = []
    for s in M.cstmts:
        if s.addr >= piclo:
            break
        if s.kind == "avg":
            mn, oper, ln = avg.decode(mem, s.addr)
            if mn in ("JSRL", "JMPL"):
                oper = M.sym(int(oper[1:], 16))
            text = "%s %s" % (mn, oper)
        else:
            vals = [avg.word(mem, s.addr + 2 * k) for k in range(len(s.args))] if s.op == ".WORD" \
                else list(mem[s.addr:s.addr + s.size])
            text = "%s %s" % (s.op.lower(), ", ".join(("$%04X" if s.op == ".WORD" else "$%02X") % v for v in vals))
        rows.append(dict(addr=H(s.addr), labels=s.labels, text=text, source=s.args, comment=s.comment,
                         verified=s.value_ok))
    out["statements"] = rows
    return out


def build(M=None):
    M = M or vrender.Model()
    mem, w = M.mem, M.w
    vg = w.labels["VGMSGA"]
    chars = []
    for k in range(41):
        t = avg.decode_raw(mem, vg + 2 * k)["target"]
        chars.append(dict(index=k, code=H(2 * k)[1:].replace("00", "") or "0", ascvg=2 * k,
                          char=vrender.CHAR_TEXT[k], addr=H(vg + 2 * k), target=H(t), target_name=M.name(t)))
    for c in chars:
        c["code"] = "$%02X" % c["ascvg"]
    coltab = find(mem, [0, 4, 8, 0xC, 0xC3, 7, 0xB, 0xB])[0]
    d = dict(
        rom="tempest3 rev 3 64K image", generated_by="disasm/vtables.py",
        characters=dict(VGMSGA=H(vg), entries=chars,
                        format="JSRL word per character; ASCVG: ' '=0, '0'-'9'=1..10, 'A'-'Z'=11..36, "
                               "37 blank, 38 '-' ('\\'), 39 1/2, 40 (c) ('^'); code byte = index*2, bit7 = last"),
        csect=csect_tables(M),
        messages=messages(mem),
        between_points=between_points(mem),
        COLTAB=dict(addr=H(coltab), format="8 bytes per wave colour set (INICOL picks by (CURWAV&$70)>>1|7): "
                    "low nibble -> colour RAM 0-7, high nibble -> 8-15; active-low b0 red-low, b1 red, b2 blue, b3 green",
                    sets=[["$%02X" % mem[coltab + 8 * s + k] for k in range(8)] for s in range(6)],
                    palette_wave1=vrender.palette(mem, 0)),
        SYSOPT=dict(addr=H(w.labels["SYSOPT"]),
                    entries=[M.name(avg.word(mem, w.labels["SYSOPT"] + 2 * k)) for k in range(8)]),
        vector_rom_labels={n: H(a) for n, a in sorted(w.labels.items(), key=lambda x: x[1]) if a < 0x4000},
        vector_ram_equates={M.vram[v]: H(v) for v in sorted(M.vram)},
        picture_codes={k: v for k, v in sorted(w.equates.items()) if re.match(r"^PT[A-Z0-9]+$", k)},
    )
    return d


if __name__ == "__main__":
    d = build()
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    json.dump(d, open(OUT, "w", encoding="utf-8"), indent=1, ensure_ascii=False)
    m = d["messages"]
    print("characters %d, PICLO %d entries, messages %d x 4 languages, between-point pictures %d"
          % (len(d["characters"]["entries"]), len(d["csect"]["PICLO"]["entries"]), m["count"],
             len(d["between_points"]["pictures"])))
    print("LNGTAB", m["LNGTAB"], "MSGLBS", m["MSGLBS"], "COLTAB", d["COLTAB"]["addr"])
    for e in m["messages"]:
        print("  %-7s c%-2d s%d y%-4d %s | %s | %s | %s" % (e["name"], e["color"], e["scale"], e["y"],
              *[e["texts"][l]["text"] for l in LANGS]))
    print("wrote", OUT)
