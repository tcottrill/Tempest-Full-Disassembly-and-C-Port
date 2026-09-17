"""Light MAC65 walker for ALVROM.MAC (+ ANVGAN.MAC) - vector ROM names only.

This is NOT a general assembler (track A owns macsrc.py/macasm.py). It knows
just enough of the dialect to walk ALVROM statement by statement:

  * .RADIX, .=, .ASECT/.CSECT, .INCLUDE ANVGAN (VGMC macros are built in)
  * NAME: / NAME:: labels, NAME=expr / NAME==expr equates
  * .IF/.IFF/.IFT/.IFTF/.ENDC, .IIF, .MACRO/.ENDM (with textual argument
    substitution and .NARG), .REPT/.ENDR, .IRPC (closed by .ENDR or .ENDM)
  * .BYTE/.WORD, VGMC ops VCTR CNTR HALT RTSL JSRL JMPL SCAL STAT
  * RT-11 expressions: left to right, <> grouping, + - * / & !, ^H ^D ^C,
    trailing '.' = decimal

Each AVG instruction is LOCKED onto the ROM: its length comes from decoding
the ROM at the running address, and the kind (VCTR/SVEC, STAT, SCAL, CNTR,
JSRL, JMPL, RTSL, HALT) must agree. After the walk, every statement whose
operands could be evaluated is re-encoded the way VGMC.MAC would encode it and
compared word for word with the ROM (check()).

walk() returns a Walk with:
  labels   {name: addr}            (ALVROM/ANVGAN labels, both sections)
  stmts    [Stmt]                  one per emitted AVG op / .BYTE / .WORD item group
  equates  {name: value}           numeric equates (PT* picture codes, colours ...)
  errors   [text]                  kind desyncs
"""
import os, re
import avg

HERE = os.path.dirname(os.path.abspath(__file__))
SRCDIR = os.path.join(os.path.dirname(HERE), "tempest-main", "tempest-main")
CSECT_BASE = 0xCDDE          # ALVROM .CSECT link base (ALEXEC.MAP section table)

SYM = r"[A-Z0-9.$_]+"
VG_OPS = ("VCTR", "CNTR", "HALT", "RTSL", "JSRL", "JMPL", "SCAL", "STAT")


def read_mac(name):
    raw = open(os.path.join(SRCDIR, name), "rb").read()
    txt = raw.replace(b"\0", b"").decode("latin-1")
    return txt.replace("\r\n", "\n").replace("\r", "\n").split("\n")


class Stmt:
    __slots__ = ("addr", "size", "kind", "op", "args", "env", "radix", "file",
                 "line", "src", "comment", "top", "words", "value_ok", "labels")

    def __repr__(self):
        return "<%04X %s %s %s>" % (self.addr, self.kind, self.op, self.args)


class Unknown(Exception):
    pass


def split_args(s):
    """Comma split honouring <...> groups."""
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch == "<":
            depth += 1
        elif ch == ">":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip() or out:
        out.append(cur.strip())
    return out


def strip_comment(s):
    i = s.find(";")
    return (s[:i], s[i + 1:].strip()) if i >= 0 else (s, "")


def evaluate(expr, env, radix):
    """RT-11 expression, left to right, 16-bit result."""
    s = expr.replace(" ", "").replace("\t", "")
    pos = [0]

    def term():
        if pos[0] >= len(s):
            raise Unknown(expr)
        c = s[pos[0]]
        if c == "<":
            pos[0] += 1
            v = binary()
            if pos[0] < len(s) and s[pos[0]] == ">":
                pos[0] += 1
            return v
        if c == "-":
            pos[0] += 1
            return -term()
        if c == "+":
            pos[0] += 1
            return term()
        if c == "^":
            k = s[pos[0] + 1]
            pos[0] += 2
            if k == "C":
                return ~term() & 0xFFFF
            m = re.match(r"[0-9A-F]+", s[pos[0]:])
            base = {"H": 16, "D": 10, "O": 8, "B": 2}[k]
            pos[0] += len(m.group(0))
            return int(m.group(0), base)
        m = re.match(SYM, s[pos[0]:])
        if not m:
            raise Unknown(expr)
        tok = m.group(0)
        pos[0] += len(tok)
        if tok[0].isdigit():
            if tok.endswith("."):
                return int(tok[:-1], 10)
            return int(tok, radix)
        if tok in env:
            return env[tok]
        raise Unknown(tok)

    def binary():
        v = term()
        while pos[0] < len(s) and s[pos[0]] in "+-*/&!":
            op = s[pos[0]]
            pos[0] += 1
            r = term()
            if op == "+": v = v + r
            elif op == "-": v = v - r
            elif op == "*": v = v * r
            elif op == "/":
                if r == 0:
                    raise Unknown(expr)
                q = abs(v) // abs(r)
                v = q if (v >= 0) == (r >= 0) else -q
            elif op == "&": v = (v & 0xFFFF) & (r & 0xFFFF)
            elif op == "!": v = (v & 0xFFFF) | (r & 0xFFFF)
        return v

    v = binary()
    if pos[0] != len(s):
        raise Unknown(expr)
    return v


def s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


class Walker:
    def __init__(self, mem):
        self.mem = mem
        self.env = {}
        self.labels = {}
        self.label_file = {}
        self.equates = {}
        self.aliases = {}
        self.macros = {}
        self.stmts = []
        self.errors = []
        self.radix = 16
        self.pc = 0
        self.pending = []          # labels waiting for the next statement
        self.section = "ASECT"
        self.asect_pc = 0
        self.top = None            # (file, line, text, comment) of the source line

    # ---------------------------------------------------------------- helpers
    def ev(self, expr):
        return evaluate(expr, self.env, self.radix)

    def define_label(self, name):
        self.labels[name] = self.pc
        self.env[name] = self.pc
        self.pending.append(name)

    def emit(self, kind, op, args, size):
        st = Stmt()
        st.addr, st.size, st.kind, st.op, st.args = self.pc, size, kind, op, args
        st.env, st.radix = dict(self.env), self.radix
        st.file, st.line, st.src, st.comment = self.top
        st.words, st.value_ok, st.labels = None, None, self.pending
        self.pending = []
        self.stmts.append(st)
        self.pc += size
        return st

    def avg_op(self, op, args):
        d = avg.decode_raw(self.mem, self.pc)
        want = {"VCTR": ("VCTR", "SVEC"), "STAT": ("STAT", "CSTAT"),
                "CSTAT": ("CSTAT", "STAT")}.get(op, (op,))
        if op == "RTSL" and d["op"] == "RTSL":
            pass
        elif d["op"] not in want:
            self.errors.append("%s:%d $%04X source %s but ROM decodes %s"
                               % (self.top[0], self.top[1], self.pc, op, d["op"]))
        self.emit("avg", op, args, d["len"])

    # ---------------------------------------------------------------- walking
    def run_file(self, name):
        lines = read_mac(name)
        self.run_lines([(name, i + 1, t) for i, t in enumerate(lines)], top=True)

    def run_lines(self, lines, top=False, margs=None):
        """Execute a list of (file, lineno, text)."""
        i, n = 0, len(lines)
        cond = []                         # stack of [active, value, parent]
        while i < n:
            fn, ln, raw = lines[i]
            i += 1
            code, comment = strip_comment(raw)
            body = code.strip()
            if not body:
                continue
            head = body.split(None, 1)[0].upper()
            rest = body[len(head):].strip() if head else ""
            active = all(c[0] for c in cond)
            # ---- conditionals are tracked even when inactive
            if head in (".IF",):
                val = self.cond(rest) if active else False
                cond.append([val, val, active])
                continue
            if head == ".IFF":
                cond[-1][0] = cond[-1][2] and not cond[-1][1]
                continue
            if head == ".IFT":
                cond[-1][0] = cond[-1][2] and cond[-1][1]
                continue
            if head == ".IFTF":
                cond[-1][0] = cond[-1][2]
                continue
            if head == ".ENDC":
                cond.pop()
                continue
            if not active:
                # skip whole macro definitions / repeat blocks inside dead code
                if head == ".MACRO":
                    i = self.skip_block(lines, i, (".MACRO",), (".ENDM",))
                continue
            if top:
                self.top = (fn, ln, body, comment)
            mlab = re.match(r"^((?:%s::?\s*)+)(\.(?:IRPC|REPT|MACRO)\b.*)$" % SYM, body)
            if mlab:
                for nm in re.findall(r"(%s)::?" % SYM, mlab.group(1)):
                    self.define_label(nm)
                body = mlab.group(2)
                head = body.split(None, 1)[0].upper()
                rest = body[len(head):].strip()
            # ---- block constructs
            if head == ".MACRO":
                parts = [p for p in re.split(r"[\s,]+", rest.strip()) if p]
                mname = parts[0]
                params = parts[1:]
                j = self.skip_block(lines, i, (".MACRO",), (".ENDM",))
                self.macros[mname.upper()] = (params, lines[i:j - 1])
                i = j
                continue
            if head == ".REPT":
                j = self.skip_block(lines, i, (".REPT", ".IRPC", ".IRP"), (".ENDR",))
                cnt = self.ev(rest)
                blk = lines[i:j - 1]
                for _ in range(cnt):
                    self.run_lines(blk)
                i = j
                continue
            if head == ".IRPC":
                a = split_args(rest)
                var, text = a[0], a[1]
                if text.startswith("<") and text.endswith(">"):
                    text = text[1:-1]
                j = self.skip_block(lines, i, (".REPT", ".IRPC", ".IRP"), (".ENDR", ".ENDM"))
                blk = lines[i:j - 1]
                for ch in text:
                    sub = [(f, l, self.subst(t, {var: ch})) for f, l, t in blk]
                    self.run_lines(sub)
                i = j
                continue
            self.statement(body, comment, margs)
        return

    def skip_block(self, lines, i, openers, closers):
        depth = 1
        while i < len(lines):
            code = strip_comment(lines[i][2])[0].strip()
            h = code.split(None, 1)[0].upper() if code else ""
            i += 1
            if h in openers:
                depth += 1
            elif h in closers:
                depth -= 1
                if depth == 0:
                    return i
        return i

    def cond(self, rest):
        a = split_args(rest)
        c = a[0].strip().upper()
        e = a[1] if len(a) > 1 else ""
        if c in ("B", "NB"):
            blank = e.strip() == ""
            return blank if c == "B" else not blank
        if c in ("DF", "NDF"):
            d = e.strip() in self.env
            return d if c == "DF" else not d
        v = s16(self.ev(e))
        return {"EQ": v == 0, "NE": v != 0, "LT": v < 0, "GT": v > 0,
                "LE": v <= 0, "GE": v >= 0}[c]

    def subst(self, text, mapping):
        def rep(m):
            t = m.group(0)
            return mapping.get(t, t)
        out = re.sub(SYM, rep, text)
        return out.replace("'", "")

    def statement(self, body, comment, margs):
        # labels (possibly several, possibly followed by a statement)
        while True:
            m = re.match(r"^(%s)(::?)\s*(.*)$" % SYM, body)
            if not m or m.group(1)[0].isdigit():
                break
            self.define_label(m.group(1))
            body = m.group(3).strip()
            if not body:
                return
        # equates / assignments
        m = re.match(r"^(%s)?\s*==?\s*(.*)$" % SYM, body)
        if m and not body.upper().startswith((".IIF", ".BYTE", ".WORD")):
            name, expr = m.group(1), m.group(2)
            if name == ".":
                self.pc = self.ev(expr)          # '.=addr' origin
                return
            if name is None:
                return
            try:
                v = self.ev(expr)
                self.env[name] = v
                self.equates[name] = v
            except Unknown:
                mm = re.match(r"^(%s)$" % SYM, expr.strip())
                if mm:
                    self.aliases[name] = mm.group(1)
            return
        head = body.split(None, 1)[0]
        H = head.upper()
        rest = body[len(head):].strip()
        if H == ".RADIX":
            self.radix = int(rest, 10)
            return
        if H == ".INCLUDE":
            f = rest.strip().upper()
            if f == "ANVGAN":
                saved = self.top
                self.run_file("ANVGAN.MAC")
                self.top = saved
            return
        if H == ".CSECT":
            self.asect_pc = self.pc
            self.pc = CSECT_BASE
            self.section = "CSECT"
            return
        if H == ".ASECT":
            return
        if H == ".NARG":
            self.env[rest.strip()] = len(margs or [])
            return
        if H == ".IIF":
            a = split_args(rest)
            c = a[0]
            remainder = ",".join(a[1:])
            if c.upper() in ("B", "NB", "DF", "NDF"):
                e, stmt = a[1], ",".join(a[2:])
            else:
                mm = re.match(r"^(\S+?)(?:\s+|,)(.*)$", remainder)
                e, stmt = mm.group(1), mm.group(2)
            if self.cond(c + "," + e):
                self.statement(stmt.strip(), comment, margs)
            return
        if H in (".BYTE", ".WORD"):
            items = split_args(rest)
            size = 1 if H == ".BYTE" else 2
            self.emit("data", H, items, size * len(items))
            return
        if H in (".SBTTL", ".TITLE", ".PAGE", ".GLOBL", ".NLIST", ".LIST",
                 ".ENABL", ".END", ".DSABL"):
            return
        if H in VG_OPS or H == "CSTAT":
            self.avg_op(H, split_args(rest) if rest else [])
            return
        if H in self.macros:
            params, blk = self.macros[H]
            args = split_args(rest) if rest else []
            mapping = {}
            for k, p in enumerate(params):
                v = args[k] if k < len(args) else ""
                if v.startswith("<") and v.endswith(">"):
                    v = v[1:-1]
                mapping[p] = v
            sub = [(f, l, self.subst(t, mapping)) for f, l, t in blk]
            self.run_lines(sub, margs=args)
            return
        self.errors.append("%s:%d unhandled statement %r" % (self.top[0], self.top[1], body))

    # ---------------------------------------------------------------- results
    def finish(self):
        for a, b in self.aliases.items():
            if b in self.labels:
                self.labels.setdefault(a, self.labels[b])
        return self


def vgmc_words(st, labels):
    """Encode an op the way VGMC.MAC does (16-bit truncation), or None."""
    env = dict(st.env)
    env.update(labels)

    def e(x):
        return evaluate(x, env, st.radix) & 0xFFFF
    a = st.args
    op = st.op
    if op == "VCTR":
        dx, dy, zz = e(a[0]), e(a[1]), e(a[2])
        x1, y1 = abs(s16(dx)), abs(s16(dy))
        if x1 + y1 != 0 and ((x1 | y1) & 0xFFE1) == 0:
            return [(0x4000 + zz * 0x20 + ((s16(dx) // 2 if s16(dx) >= 0 else -((-s16(dx)) // 2)) & 0x1F)
                     + ((dy * 0x80) & 0x1F00)) & 0xFFFF]
        return [dy & 0x1FFF, (zz * 0x2000 + (dx & 0x1FFF)) & 0xFFFF]
    if op == "CSTAT":
        return [(0x68C0 + e(a[0])) & 0xFFFF]
    if op == "CNTR":
        return [0x8040]
    if op == "HALT":
        return [0x2000]
    if op == "RTSL":
        return [0xC000]
    if op in ("JSRL", "JMPL"):
        t = e(a[0])
        return [((t & 0x1FFF) // 2 + (0xA000 if op == "JSRL" else 0xE000)) & 0xFFFF]
    if op == "SCAL":
        ls = e(a[1]) if len(a) > 1 and a[1] != "" else 0
        return [(0x7000 + ls + e(a[0]) * 0x100) & 0xFFFF]
    if op == "STAT":
        z = e(a[0])
        if len(a) > 1 and (a[1] + (a[2] if len(a) > 2 else "")).strip():
            one, hi, inn = 0, e(a[1]), e(a[2])
        else:
            one, hi, inn = 1, 0, 0
        return [(0x6000 + one * 0x400 + hi * 0x200 + inn * 0x100 + z * 0x10) & 0xFFFF]
    return None


def check(w):
    """Re-encode every statement per VGMC and compare with the ROM bytes.
    Sets st.words / st.value_ok. Returns (ok, bad, unknown)."""
    mem = w.mem
    ok = bad = unk = 0
    for st in w.stmts:
        # forward-referenced equates (e.g. MX used by ROMRPI before 'MX =500.')
        # take their final value, as the assembler's pass 1 would supply it
        env0 = dict(w.equates)
        env0.update(st.env)
        st.env = env0
        try:
            if st.kind == "avg":
                words = vgmc_words(st, w.labels)
                rom = [avg.word(mem, st.addr + 2 * k) for k in range(st.size // 2)]
                # CSTAT expands to .WORD ^H68C0+c; that is handled as data
                good = words == rom
            else:
                env = dict(st.env)
                env.update(w.labels)
                vals = [evaluate(x, env, st.radix) for x in st.args]
                if st.op == ".BYTE":
                    good = bytes(v & 0xFF for v in vals) == mem[st.addr:st.addr + st.size]
                else:
                    good = all(avg.word(mem, st.addr + 2 * k) == v & 0xFFFF
                               for k, v in enumerate(vals))
                words = vals
            st.words, st.value_ok = words, good
            ok += good
            bad += not good
        except (Unknown, KeyError, IndexError, ValueError):
            st.value_ok = None
            unk += 1
    return ok, bad, unk


def walk(mem=None):
    mem = mem or avg.load_image()
    w = Walker(mem)
    w.run_file("ALVROM.MAC")
    w.finish()
    return w


if __name__ == "__main__":
    w = walk()
    ok, bad, unk = check(w)
    print("statements: %d   labels: %d   desync errors: %d" % (len(w.stmts), len(w.labels), len(w.errors)))
    for e in w.errors[:20]:
        print("  !!", e)
    print("value check: %d exact, %d MISMATCH, %d not evaluable" % (ok, bad, unk))
    for st in w.stmts:
        if st.value_ok is False:
            print("  mismatch %04X %s %s  (%s:%d %s)" % (st.addr, st.op, st.args, st.file, st.line, st.src))
    for st in w.stmts:
        if st.value_ok is None:
            print("  unknown  %04X %s %s  (%s:%d)" % (st.addr, st.op, st.args, st.file, st.line))
    asect_end = max(s.addr + s.size for s in w.stmts if s.addr < 0x4000)
    print("ASECT end $%04X   CSECT end $%04X" % (asect_end, max(s.addr + s.size for s in w.stmts)))
