"""Parse 'Tempest Commented Source.txt' (1999-2004 third-party commented dump of rev 3)
into build/commented_src.json:  {"XXXX": comment}.

  * inline "; ..." comment of an address line -> that address
  * indented comment-only lines continue the previous address's comment
  * column-0 ";" comment blocks are attached to the NEXT address, placed before
    its inline comment and separated by "\n" (these are routine headers such as
    "SUBROUTINE: Game initialization.")
  * the header block before the first address line is skipped
  * typo fix: line 4504 "BC1E" is really $B21E (survey)
Each listed byte is verified against the rev-3 image; the count is printed.
"""
import os, re, json

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "Tempest Commented Source.txt")
ROM = os.path.join(HERE, "_survey", "roms_extracted", "tempest3_cpu64k.bin")
OUT = os.path.join(HERE, "build", "commented_src.json")
LINE_RE = re.compile(r"^([0-9A-F]{4}) ([0-9A-F]{2})([: ][0-9A-F]{2})?([: ][0-9A-F]{2})?:?\s+(\S+)(.*)$")

def main():
    rom = open(ROM, "rb").read()
    text = open(SRC, "rb").read().decode("latin-1").replace("\r\n", "\n").split("\n")
    inline, block = {}, {}
    pending, last, started = [], None, False
    ok = bad = 0
    for n, raw in enumerate(text, 1):
        m = LINE_RE.match(raw)
        if m:
            addr = m.group(1)
            if n == 4504 and addr == "BC1E":
                addr = "B21E"
            a = int(addr, 16)
            bs = [m.group(2)] + [g[1:] for g in (m.group(3), m.group(4)) if g]
            for i, b in enumerate(bs):
                if rom[a + i] == int(b, 16):
                    ok += 1
                else:
                    bad += 1
            started = True
            rest = m.group(6)
            c = rest.split(";", 1)[1].strip() if ";" in rest else ""
            if pending:
                block[addr] = block.get(addr, []) + pending
                pending = []
            if c:
                inline[addr] = (inline[addr] + " " + c) if addr in inline else c
            last = addr
            continue
        s = raw.strip()
        if not s.startswith(";") or not started:
            continue
        c = s[1:].strip()
        if raw[:1] == ";":
            if c:
                pending.append(c)
        elif last and c:
            inline[last] = (inline[last] + " " + c) if last in inline else c
    out = {}
    for addr in sorted(set(inline) | set(block)):
        parts = block.get(addr, []) + ([inline[addr]] if addr in inline else [])
        out[addr] = "\n".join(parts)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as fp:
        json.dump(out, fp, indent=0, sort_keys=True)
    print("commented_src: %d addresses with comments (%d with block headers); bytes verified %d ok, %d differ"
          % (len(out), len(block), ok, bad))

if __name__ == "__main__":
    main()
