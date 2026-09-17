"""Emit disasm/tempest_vector_rom.asm - byte-exact listing of $3000-$3FFF.

Adapted from the Space Duel emit_vrom.py. Every statement comes from
ALVROM.MAC/ANVGAN.MAC walked by vromsrc.py and locked onto the ROM; each line
is re-encoded (avg.encode) and compared before it is written, falling back to
raw .word/.byte if it would not round-trip. verify_vrom.py re-checks the file.
"""
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import avg, vrender
from vrender import clean

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "tempest_vector_rom.asm")


def color_note(cols):
    return ", ".join("%d %s" % (c, vrender.COLOR_NAMES.get(c, "?")) for c in cols)


def emit(model=None, path=OUT):
    M = model or vrender.Model()
    mem = M.mem
    ok, bad, unk = M.check
    L = [
        ";Tempest (Atari, 1981) - vector ROM $3000-$3FFF (136002-123/124, or 138),",
        ";byte-exact listing. Identical in every Tempest ROM set.",
        ";",
        ";Source: ALVROM.MAC (Dave Theurer) with ANVGAN.MAC (Ed Logg's character",
        ";set) included at $3000; VGMC.MAC macros. The source was walked statement",
        ";by statement and locked onto the ROM (disasm/vromsrc.py): %d of %d" % (ok, ok + bad + unk),
        ";statements re-encode exactly as VGMC.MAC would assemble them, %d differ," % bad,
        ";%d cannot be evaluated (QCHKS0/QCHKS1 checksum equates live in ALHAR2, and" % unk,
        ";BOKLIT's 'VCTR -24.*15.,-6C.,0' - '6C.' is not a legal decimal; MAC65 took",
        ";C as digit value 12 -> -72, which is what the ROM holds).",
        ";",
        ";AVG (see avg.py): VCTR dx, dy, z = long vector; SVEC = short vector",
        ";(VGMC picks it automatically for even deltas <= 30); z 0 = blank, 1 = use",
        ";STAT intensity, else brightness z*2. CSTAT c = .WORD $68C0+c: on Tempest a",
        ";STAT with bit 11 set loads the COLOUR latch (colour RAM $0800+c); without",
        ";bit 11 it loads the intensity ((w>>4)&$F). Tempest has no sparkle/flip STAT",
        ";bits (those are Major Havoc). SCAL b, $ll = binary / linear scale.",
        ";JSRL/JMPL operand = (target-$2000)/2; targets below $3000 are vector RAM",
        ";sub-buffers set up by ALDIS2 (named from ALVROM's equates below).",
        ";Labels are Atari's names with '.' -> '_' (CHAR.A -> CHAR_A; CHAR. = CHAR_",
        ";is the blank). Program-ROM tables that belong to ALVROM's .CSECT ($CDDE-",
        ";$CF23: SCALOC, SCORES template, BUFASL.., JMPMAL, PICLO) are listed in",
        ";build/vector_tables.json for the program-ROM track.",
        ";Program-ROM references come from build/srclines.json + symbols.json (the",
        ";byte-exact assembly of every module): each 6502 statement whose operand",
        ";names a vector ROM label or a PT* picture code, shown by routine name.",
        ";Vector-ROM and ALVROM .CSECT table references come from the JSRL/JMPL words.",
        ";Re-verify with: python disasm/verify_vrom.py",
        "",
        ".org $3000",
        "",
        ";----------------------------[ vector RAM equates (ALVROM.MAC) ]----------------------------",
    ]
    for v in sorted(M.vram):
        L.append(".alias %-16s $%04X" % (clean(M.vram[v]), v))
    L.append("")
    L.append(";----------------------------[ AVG display lists ]----------------------------")

    last_src = None
    exact = raw = 0
    covered = 0x3000
    ref_lines = {}
    for s in M.stmts:
        assert s.addr == covered, "gap at $%04X" % covered
        covered = s.addr + s.size
        names = M.label_at.get(s.addr, []) if s.labels else []
        if s.labels:
            # aliases defined by '=' (CHAR.0 = CHAR.O) share the address
            names = list(dict.fromkeys(list(s.labels) + M.label_at.get(s.addr, [])))
        if names:
            L.append("")
            first = names[0]
            L.append("%s:%s;JSRL operand $%03X  (source name %s%s)"
                     % (clean(first), " " * max(1, 16 - len(clean(first))),
                        (s.addr - 0x2000) // 2, first,
                        ", %s:%d" % (s.file, s.line)))
            for extra in names[1:]:
                L.append("%s:%s;same address (source name %s)"
                         % (clean(extra), " " * max(1, 16 - len(clean(extra))), extra))
            for nm in names:
                ds = vrender.describe(nm)
                if ds:
                    L.append(";  %s" % ds)
            if first not in vrender.DATA_LABELS and first != "VGMSGA":
                segs, cols, info = vrender.render(mem, s.addr, one_op=first in vrender.WORD_LABELS)
                if cols:
                    L.append(";  colour: %s" % color_note(cols))
                if not any(br > 0 for *_x, br in segs):
                    L.append(";  no lit vectors - beam positioning / advance / vector-RAM list")
                if info["vram_calls"]:
                    L.append(";  calls vector RAM (built at run time by ALDIS2)")
            rs = M.refs.get(s.addr, [])
            for a, how in rs[:4]:
                L.append(";  refs: %s at $%04X" % (how, a))
            if len(rs) > 4:
                L.append(";  refs: ... and %d more (see cross reference)" % (len(rs) - 4))
        # ---- the statement bytes
        if s.kind == "avg":
            a = s.addr
            mn, oper, ln = avg.decode(mem, a)
            words = avg.encode(mn, oper)
            good = words is not None and len(words) * 2 == ln and all(
                avg.word(mem, a + 2 * k) == w for k, w in enumerate(words))
            rawhex = " ".join("%04X" % avg.word(mem, a + 2 * k) for k in range(ln // 2))
            note = ""
            first_of_line = (s.file, s.line) != last_src
            last_src = (s.file, s.line)
            if s.comment and first_of_line:
                note = "  " + s.comment
            if mn in ("JSRL", "JMPL"):
                oper = M.sym(int(oper[1:], 16))
            elif mn == "CSTAT":
                oper = "%s" % oper
                note = "  %s%s" % (vrender.COLOR_NAMES.get(int(oper), "?"), note)
            elif mn == "STAT":
                d = avg.decode_raw(mem, a)
                note = ("  colour %d" % d["color"] if d["color"] is not None
                        else "  intensity %d" % d["intensity"]) + note
            src = re.sub(r"^([A-Z0-9.$_]+::?\s*)+", "", s.src or "")
            if s.file == "ALVROM.MAC" and src and first_of_line and \
                    not src.upper().startswith(mn) and not src.upper().startswith("VCTR"):
                if len(src) > 40:
                    src = src[:37] + "..."
                note = note + "  [" + src + "]"
            if good:
                exact += 1
                L.append(("V%04X:  %-6s %-22s ;%s%s" % (a, mn, oper, rawhex, note)).rstrip())
            else:
                raw += 1
                ws = ", ".join("$%04X" % avg.word(mem, a + 2 * k) for k in range(ln // 2))
                L.append("V%04X:  .word %-17s ;%s %s%s" % (a, ws, mn, oper, note))
        else:
            first_names = M.label_at.get(s.addr, [""])
            if s.op == ".WORD":
                for k in range(len(s.args)):
                    a = s.addr + 2 * k
                    v = avg.word(mem, a)
                    nm = M.name(v)
                    L.append("V%04X:  .word %-17s ;%s" % (a, clean(nm) if nm else "$%04X" % v,
                                                          "option %d literal" % k))
            else:
                bs = ", ".join("$%02X" % mem[x] for x in range(s.addr, s.addr + s.size))
                txt = ".BYTE " + ",".join(s.args)
                extra = ""
                if s.size == 2 and mem[s.addr + 1] == 0xC0:
                    extra = " - decodes as RTSL $%04X" % avg.word(mem, s.addr)
                L.append("V%04X:  .byte %-17s ;%s%s" % (s.addr, bs, txt, extra))
    assert covered == 0x4000, "listing ends at $%04X" % covered

    # ---- cross reference
    L += ["", ";----------------------------[ cross reference ]----------------------------",
          ";name              addr   referenced from"]
    for a in sorted(M.label_at):
        if not (0x3000 <= a <= 0x3FFF):
            continue
        nm = M.label_at[a][0]
        rs = M.refs.get(a, [])
        items = [("$%04X %s" % (r, M.routine_refs[(a, r)])) if (a, r) in M.routine_refs
                 else "$%04X" % r for r, _h in rs] or ["(no static reference)"]
        chunks, cur = [], ""
        for it in items:                      # wrap at item boundaries
            if cur and len(cur) + len(it) + 2 > 90:
                chunks.append(cur + ",")
                cur = it
            else:
                cur = (cur + ", " + it) if cur else it
        chunks.append(cur)
        L.append(";%-17s $%04X  %s" % (clean(nm), a, chunks[0]))
        for c in chunks[1:]:
            L.append(";%-17s        %s" % ("", c))
    open(path, "w", encoding="ascii", errors="replace").write("\n".join(L) + "\n")
    return exact, raw, len(M.stmts)


if __name__ == "__main__":
    e, r, n = emit()
    print("wrote", OUT)
    print("  statements           : %d" % n)
    print("  AVG round-trip exact : %d" % e)
    print("  AVG emitted as .word : %d" % r)
