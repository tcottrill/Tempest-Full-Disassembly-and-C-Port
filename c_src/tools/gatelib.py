"""Shared helpers for tools/gate_s.py and tools/gate_e.py (M9 batch B4).

Runs the test executables from c_src in parallel (logs in a work directory),
parses the summary lines of refrun / lockstep / gate / tempest_selftest, and
decodes ER2055 images with the ROM's own ALEARO tables (TEAX / TEACNT /
TEASRL at $DDDD, read from disasm/build/program.bin).
"""
import json
import os
import re
import subprocess
import sys
import time

C_SRC = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
DISASM_BUILD = os.path.normpath(os.path.join(C_SRC, "..", "disasm", "build"))


# --------------------------------------------------------------------------- checks
class Checker:
    def __init__(self, gate):
        self.gate = gate
        self.fails = 0
        self.count = 0

    def check(self, ok, name, detail=""):
        self.count += 1
        if not ok:
            self.fails += 1
        print("  [%s] %-44s %s" % ("PASS" if ok else "FAIL", name, detail))
        return ok

    def section(self, title):
        print("\n%s %s" % (self.gate, title))

    def verdict(self):
        print("\n%s: %s (%d of %d checks failed)" % (self.gate, "FAIL" if self.fails else "PASS",
                                                    self.fails, self.count))
        return 1 if self.fails else 0


# --------------------------------------------------------------------------- processes
def run_parallel(jobs, workdir):
    """jobs: list of (name, [argv...]); argv[0] relative to c_src.  Runs all at
    once with cwd = c_src, stdout+stderr into workdir/name.log.  Returns
    {name: (exit code, log text, seconds)}."""
    procs = {}
    for name, argv in jobs:
        log = open(os.path.join(workdir, name + ".log"), "w")
        exe = os.path.join(C_SRC, argv[0])
        procs[name] = (subprocess.Popen([exe] + argv[1:], cwd=C_SRC, stdout=log, stderr=subprocess.STDOUT),
                       log, time.time())
        print("  started %-22s %s" % (name, " ".join(argv)))
    out = {}
    for name, (p, log, t0) in procs.items():
        rc = p.wait()
        log.close()
        with open(os.path.join(workdir, name + ".log"), "r", errors="replace") as f:
            text = f.read()
        out[name] = (rc, text, time.time() - t0)
    for name in out:
        print("  finished %-21s exit %d, %.0f s" % (name, out[name][0], out[name][2]))
    return out


def work_dir(args, default_name):
    d = args.work if args.work else os.path.join(C_SRC, "obj", default_name)
    os.makedirs(d, exist_ok=True)
    return d


def same_file(a, b):
    try:
        with open(a, "rb") as fa, open(b, "rb") as fb:
            return fa.read() == fb.read()
    except OSError:
        return False


def read_bin(path):
    with open(path, "rb") as f:
        return f.read()


# --------------------------------------------------------------------------- parsers
def grab(text, pattern, cast=int):
    """First match of pattern; returns a tuple of cast groups or None."""
    m = re.search(pattern, text, re.M)
    if not m:
        return None
    return tuple(cast(x) if cast else x for x in m.groups())


def lockstep_rows(text):
    """{routine: (addr, checked, passed, failed, tainted)} from the LOCKSTEP table."""
    rows = {}
    for m in re.finditer(r"^LOCKSTEP  (\S+)\s+\$([0-9A-F]{4})\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)", text, re.M):
        rows[m.group(1)] = (int(m.group(2), 16), int(m.group(3)), int(m.group(4)), int(m.group(5)), int(m.group(6)))
    return rows


def lockstep_summary(text):
    s = {}
    s["pass"] = re.search(r"^LOCKSTEP: PASS", text, re.M) is not None
    s["refrun_pass"] = re.search(r"^REFRUN: PASS", text, re.M) is not None
    s["calls"] = grab(text, r"^LOCKSTEP: (\d+) calls compared, (\d+) failed, (\d+) tainted by an IRQ, I/O log overflows (\d+), "
                            r"IRQ list overflows (\d+), depth overflows (\d+)")
    s["moved"] = grab(text, r"IRQ moved to entry/exit: (\d+) calls compared that way.*?; (\d+) tainted calls matched neither")
    s["reset"] = grab(text, r"^LOCKSTEP: RESET arrivals (\d+) \(power-on (\d+), JMP RESET (\d+), watchdog (\d+), script (\d+)\); "
                            r"RESET regions (\d+) compared, (\d+) passed, (\d+) failed, (\d+) skipped")
    s["ended"] = grab(text, r"runs ended by a RESET arrival: (\d+) routine checks, (\d+) MAINLN / (\d+) DIAG passes; "
                            r"(\d+) empty passes dropped")
    s["probe"] = grab(text, r"pass probe: boot pass (\w+); MAINLN passes (\d+) compared, (\d+) passed, (\d+) failed, (\d+) skipped; "
                            r"DIAG passes (\d+) compared, (\d+) passed, (\d+) failed, (\d+) skipped", cast=None)
    if s["probe"]:
        s["probe"] = (s["probe"][0],) + tuple(int(x) for x in s["probe"][1:])
    s["unplaceable"] = [int(x) for x in re.findall(r"^LOCKSTEP: unplaceable IRQs.*: (\d+)\s*$", text, re.M)]
    s["loops"] = grab(text, r"loop passes: MAINLN \(\$C7AD\) (\d+), DIAG \(\$DA8D\) (\d+)")
    return s


def refrun_summary(text):
    s = {}
    s["pass"] = re.search(r"^REFRUN: PASS", text, re.M) is not None
    s["stop"] = grab(text, r"^REFRUN: stop = (.*)$", cast=None)
    s["loops"] = grab(text, r"loop passes: MAINLN \(\$C7AD\) (\d+), DIAG \(\$DA8D\) (\d+)")
    s["resets"] = grab(text, r"RESET arrivals: power-on (\d+), JMP RESET (\d+), watchdog (\d+), script (\d+)")
    s["bites"] = grab(text, r"hardware watchdog: timeout (\d+) cycles, bites (\d+) \(not at \$DAF7: (\d+)")
    m = re.search(r"self test \(RAM at the last diag pass, frame (\d+)\): CHKSMS((?: [0-9A-F]{2}){12})\s+MBCOND ([0-9A-F]{2}) "
                  r"RAMCND ([0-9A-F]{2}) PK1CND ([0-9A-F]{2}) PK2CND ([0-9A-F]{2}) EARCND ([0-9A-F]{2})", text)
    s["cells"] = None
    if m:
        s["cells"] = {"frame": int(m.group(1)), "CHKSMS": [int(x, 16) for x in m.group(2).split()],
                      "MBCOND": int(m.group(3), 16), "RAMCND": int(m.group(4), 16), "PK1CND": int(m.group(5), 16),
                      "PK2CND": int(m.group(6), 16), "EARCND": int(m.group(7), 16)}
    s["earom_cells"] = grab(text, r"EAROM cells at exit: EABAD ([0-9A-F]{2}) EAFLG ([0-9A-F]{2}) EAREQU ([0-9A-F]{2})",
                            cast=lambda x: int(x, 16))
    s["runaway"] = grab(text, r"Mathbox starts \d+ \(\d+ microsteps, runaway (\d+)")
    s["passes"] = grab(text, r"^  passes (\d+) \(target (\d+)\)")
    s["brk"] = grab(text, r"BRK executed (\d+)")
    return s


def refrun_snapshot_ok(text):
    """A run cut at a chosen pass (to read the EAROM image or RAM there): the
    display-list part of refrun's verdict does not apply (a boot pass has an
    empty list), the hardware part does: target reached, no BRK, no watchdog
    bite outside $DAF7."""
    s = refrun_summary(text)
    p = s["passes"] or (0, 1)
    b = s["bites"] or (0, 0, 0)
    return (s["stop"] or ("",))[0] == "frames reached" and p[0] == p[1] and s["brk"] == (0,) and b[2] == 0, s


def gate_summary(text):
    s = {}
    s["pass"] = re.search(r"^GATE: PASS", text, re.M) is not None
    s["run"] = grab(text, r"^GATE RUN: PASS - (\d+) passes byte-identical \(of (\d+) in the trace\)")
    s["loops"] = grab(text, r"^GATE: passes MAINLN (\d+), DIAG (\d+)")
    s["resets"] = grab(text, r"RESET arrivals compared: power-on (\d+), JMP RESET (\d+), watchdog (\d+), script (\d+)")
    return s


def sched_kinds(path):
    """frame_sched.txt -> {frame: 'M' | 'D'}"""
    kinds = {}
    with open(path) as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            p = line.split()
            kinds[int(p[0])] = "D" if len(p) > 3 and p[3] == "D" else "M"
    return kinds


def script_lines(path):
    """[(pass, port, value or None)] of a tests\\scenarios script."""
    out = []
    with open(os.path.join(C_SRC, path)) as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            p = line.split()
            out.append((int(p[0], 0), p[1], int(p[2], 0) if len(p) > 2 else None))
    return out


def coverage_pcs(path):
    with open(path) as f:
        return {int(x, 16) for x in f.read().split()}


# --------------------------------------------------------------------------- ROM facts
_ROM = None


def rom_byte(addr):
    """program ROM $9000-$DFFF (disasm/build/program.bin)"""
    global _ROM
    if _ROM is None:
        _ROM = read_bin(os.path.join(DISASM_BUILD, "program.bin"))
    return _ROM[addr - 0x9000]


def altes2_headers():
    with open(os.path.join(DISASM_BUILD, "program_refs.json")) as f:
        r = json.load(f)["routines"]
    return sorted(((k, int(v["addr"][1:], 16)) for k, v in r.items() if v["module"] == "ALTES2"), key=lambda kv: kv[1])


def altes2_code_labels():
    with open(os.path.join(DISASM_BUILD, "symbols.json")) as f:
        s = json.load(f)
    return sorted(((x["name"], x["addr"]) for x in s if x.get("module") == "ALTES2" and x["kind"] == "code"),
                  key=lambda kv: kv[1])


EAROM_GROUP_NAMES = ["initials (top 3)", "high scores (top 3) + 2 option bytes", "bookkeeping"]


def earom_groups():
    """[(k, lo, hi(checksum offset), ram source address)] from ALEARO's TEAX/TEACNT/TEASRL."""
    g = []
    for k in range(3):
        lo = rom_byte(0xDDDD + 2 * k)
        hi = rom_byte(0xDDDE + 2 * k)
        src = rom_byte(0xDDE3 + 2 * k) | (rom_byte(0xDDE4 + 2 * k) << 8)
        g.append((k, lo, hi, src))
    return g


def group_checksum_ok(img, grp):
    k, lo, hi, src = grp
    return (sum(img[lo:hi]) & 0xFF) == img[hi]


def hexs(bs):
    return " ".join("%02X" % b for b in bs)
