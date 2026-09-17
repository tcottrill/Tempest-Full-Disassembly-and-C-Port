"""Gate S (self test) - M9 batch B4.  Run from c_src after build_all.bat:

    python tools\\gate_s.py [--work DIR]

Runs in parallel (logs in DIR, default obj\\gate_s):
  lockstep  selftest_boot 1800 / selftest_midrun 2500 (+ --trace-out)
  refrun    the same two scenarios with dumps (frame_sched.txt, coverage_pcs.txt)
  gate.exe  tests\\gate\\selftest_boot_1800.trc / selftest_midrun_2500.trc
  tests\\tempest_selftest.exe
and checks:
  S1 lockstep: PASS, 0 failed / tainted / IRQ moved / moved mismatch / skipped,
     every RESET arrival compared as a RESET region and passed, every loop-head
     pass (MAINLN and DIAG) compared, the regenerated trace == tests\\gate\\*.trc
  S2 coverage: every ALTES2 routine header checked (or proven by its region /
     PC), every ALTES2 code label executed, the bad-hardware paths (HIBAD,
     BRAMREP, HIRBAD, HIRBD2, JMPHIB, BADBOX and the statements behind the
     condition cells) never executed and justified by the self test's result
     cells; the ALVGUT / ALEARO routines only the self test calls directly
  S3 gate.exe PASS on both traces, RESET arrivals equal to lockstep's
  S4 refrun PASS, CHKSMS 12 x 0 and MBCOND/RAMCND/PK1CND/PK2CND/EARCND 0, one
     watchdog bite per TEST switch-off in the diag loop and none elsewhere
  S5 tempest_selftest: PASS, its self test / options checks all PASS
Prints GATE S: PASS / FAIL; exit code 0 / 1.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gatelib as gl  # noqa: E402

SCEN = [("selftest_boot", 1800), ("selftest_midrun", 2500)]

# bad-hardware paths: (label, first PC, last PC, why it cannot run on genuine hardware)
UNREACHABLE = [
    ("HIBAD", 0xD8CA, 0xD8CC, "zero-page march ($D9A9) found a bit that did not hold: RAMCND 0"),
    ("BRAMREP", 0xD8CD, 0xD92E, "bad-RAM report (tones, LED, then ROMTST); entered only from HIBAD / HIRBD2"),
    ("HIRBAD", 0xD92F, 0xD930, "RAM march $0100-$07FF / $2000-$2FFF ($D9D6) mismatch: RAMCND 0"),
    ("HIRBD2", 0xD931, 0xD93E, "block number for BRAMREP; only after HIRBAD"),
    ("JMPHIB", 0xD9B9, 0xD9BB, "JMP HIBAD from the zero-page march"),
    ("(VG ROM tone)", 0xDA3A, 0xDA43, "CHKSMS[0] != 0 (vector ROM $3000-$37FF): all 12 checksums of rev 3 are 0"),
    ("(PK1CND store)", 0xDA51, 0xDA52, "RANDOM equal on 7 reads: the POKEY poly counter shifts every cycle"),
    ("(PK2CND store)", 0xDA60, 0xDA61, "RANDO2 equal on 7 reads: same, second POKEY"),
    ("BADBOX", 0xDC15, 0xDC18, "Mathbox divide wrong (A != 1 or Y != 0): mathbox.c runs the real microcode, MBCOND 0"),
    ("(bad-ROM report)", 0xDC8B, 0xDCA3, "CHKSMS[x] != 0: VGHEX rom number + POSDIG checksum; never on rev 3"),
]
UNREACHABLE_LABELS = {"HIBAD", "BRAMRE", "HIRBAD", "HIRBD2", "JMPHIB", "BADBOX"}

# ALVGUT / ALEARO routines whose direct callers are the self test (M9 targets)
DIRECT = ["VGHALT", "VGVTR", "VGHEX", "VGCNTR", "VGSCAL", "VGSTAT", "VGADD2", "EAZERO", "EAZBOO", "EAZHIS"]

# routine header -> lockstep row name, when they differ
ROW_ALIAS = {"SFTJSE": "SSTATE"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--work", help="work directory (default obj\\gate_s)")
    args = ap.parse_args()
    W = gl.work_dir(args, "gate_s")
    ck = gl.Checker("GATE S")
    print("GATE S: self test (M9) - work directory %s" % W)

    jobs = []
    for sc, n in SCEN:
        script = "tests\\scenarios\\%s.txt" % sc
        jobs.append(("lockstep_" + sc, ["tests\\lockstep.exe", "--frames", str(n), "--no-dumps", "--script", script,
                                        "--trace-out", os.path.join(W, "%s_%d.trc" % (sc, n))]))
        dd = os.path.join(W, "refrun_" + sc)
        os.makedirs(dd, exist_ok=True)
        jobs.append(("refrun_" + sc, ["tests\\refrun.exe", "--frames", str(n), "--script", script, "--outdir", dd,
                                      "--capture-every", "1000000"]))
        jobs.append(("gate_" + sc, ["tests\\gate.exe", "tests\\gate\\%s_%d.trc" % (sc, n)]))
    jobs.append(("tempest_selftest", ["tests\\tempest_selftest.exe"]))
    res = gl.run_parallel(jobs, W)

    rows = {}
    ls = {}
    rr = {}
    cov = set()
    for sc, n in SCEN:
        ls[sc] = gl.lockstep_summary(res["lockstep_" + sc][1])
        rows[sc] = gl.lockstep_rows(res["lockstep_" + sc][1])
        rr[sc] = gl.refrun_summary(res["refrun_" + sc][1])
        p = os.path.join(W, "refrun_" + sc, "coverage_pcs.txt")
        if os.path.exists(p):
            cov |= gl.coverage_pcs(p)

    # ---------------------------------------------------------------- S1
    ck.section("S1 lockstep (routine checks, RESET regions, MAINLN + DIAG pass probe)")
    for sc, n in SCEN:
        s = ls[sc]
        ok = s["pass"] and s["refrun_pass"] and res["lockstep_" + sc][0] == 0
        ck.check(ok, "%s: LOCKSTEP / REFRUN PASS" % sc, "exit %d" % res["lockstep_" + sc][0])
        c = s["calls"] or (0, -1, -1, -1, -1, -1)
        ck.check(s["calls"] is not None and c[1] == 0 and c[2] == 0 and c[3] == 0 and c[4] == 0 and c[5] == 0,
                 "%s: 0 failed, 0 tainted, no overflow" % sc,
                 "%d calls compared, %d failed, %d tainted, overflows %d/%d/%d" % c)
        mv = s["moved"] or (-1, -1)
        ck.check(s["moved"] is not None and mv == (0, 0), "%s: 0 IRQ moved, 0 moved mismatch" % sc, "%d / %d" % mv)
        r = s["reset"] or (-1,) * 9
        ck.check(s["reset"] is not None and r[5] == r[0] and r[6] == r[5] and r[7] == 0 and r[8] == 0,
                 "%s: every RESET arrival a passed region" % sc,
                 "arrivals %d (power-on %d, JMP RESET %d, watchdog %d, script %d); regions %d compared, %d passed, "
                 "%d failed, %d skipped" % r)
        pr = s["probe"] or ("?",) + (-1,) * 8
        e = s["ended"] or (-1,) * 4
        lp = s["loops"] or (-1, -1)
        # every loop-head arrival opens a pass; all are closed and compared except the one open
        # at the end of the run and script resets' empty passes (dropped: no instruction ran)
        total_ok = pr[1] + pr[5] + e[3] + 1 == lp[0] + lp[1]
        ck.check(s["probe"] is not None and pr[0] == "PASS" and pr[2] == pr[1] and pr[3] == 0 and pr[4] == 0 and
                 pr[6] == pr[5] and pr[7] == 0 and pr[8] == 0 and pr[5] > 0 and total_ok,
                 "%s: every MAINLN / DIAG pass compared" % sc,
                 "boot pass %s; MAINLN %d/%d passed, DIAG %d/%d passed, 0 skipped; heads M %d + D %d = compared %d + "
                 "dropped %d + last 1" % (pr[0], pr[2], pr[1], pr[6], pr[5], lp[0], lp[1], pr[1] + pr[5], e[3]))
        ck.check(len(s["unplaceable"]) == 3 and not any(s["unplaceable"]), "%s: 0 unplaceable IRQs" % sc,
                 str(s["unplaceable"]))
        a = os.path.join(W, "%s_%d.trc" % (sc, n))
        b = os.path.join(gl.C_SRC, "tests", "gate", "%s_%d.trc" % (sc, n))
        ck.check(gl.same_file(a, b), "%s: trace == tests\\gate (current)" % sc,
                 "%d bytes" % (os.path.getsize(a) if os.path.exists(a) else -1))

    # ---------------------------------------------------------------- S2
    ck.section("S2 ALTES2 coverage (checked calls selftest_boot / selftest_midrun, all 0 failed)")
    print("    %-11s %-6s %9s %9s  %s" % ("routine", "addr", "boot", "midrun", "verified by"))
    resets = sum((ls[sc]["reset"] or (0,) * 9)[5] for sc, _ in SCEN)
    for name, addr in gl.altes2_headers():
        row = ROW_ALIAS.get(name, name)
        cb = rows["selftest_boot"].get(row)
        cm = rows["selftest_midrun"].get(row)
        unr = [u for u in UNREACHABLE if u[0] == name]
        if cb or cm:
            nb = cb[1] if cb else 0
            nm = cm[1] if cm else 0
            fl = (cb[3] if cb else 0) + (cm[3] if cm else 0)
            ok = nb + nm > 0 and fl == 0
            how = "lockstep routine check" + (" (row %s)" % row if row != name else "")
        elif name == "RESET":
            nb = (ls["selftest_boot"]["reset"] or (0,) * 9)[5]
            nm = (ls["selftest_midrun"]["reset"] or (0,) * 9)[5]
            ok = nb + nm > 0 and nb + nm == resets
            how = "RESET regions (power-on path, RAM/ROM/POKEY/EAROM tests to $C7AD or $DA8D)"
        elif name == "NOOPR_DB21":
            nb = rows["selftest_boot"].get("SSTATE", (0, 0))[1]
            nm = rows["selftest_midrun"].get("SSTATE", (0, 0))[1]
            ok = addr in cov and nb + nm > 0
            how = "SSTATE's PHA/PHA/RTS dispatch: runs inside every SSTATE check (PC hit %s)" % ("yes" if addr in cov else "NO")
        elif unr:
            nb = nm = 0
            ok = not any(unr[0][1] <= pc <= unr[0][2] for pc in cov)
            how = "unreachable on genuine hardware (never executed): " + unr[0][3]
        else:
            nb = nm = 0
            ok = False
            how = "NOT VERIFIED"
        ck.check(ok, "%-11s $%04X %6s %6s" % (name, addr, nb, nm), how)

    ck.section("S2 ALTES2 code labels executed (refrun coverage, both scenarios)")
    missing = [("%s $%04X" % (n, a)) for n, a in gl.altes2_code_labels() if n not in UNREACHABLE_LABELS and a not in cov]
    ck.check(not missing, "every reachable code label executed",
             "%d labels, missing: %s" % (len(gl.altes2_code_labels()), ", ".join(missing) or "none"))

    ck.section("S2 bad-hardware paths: never executed, conditions all good")
    for label, lo, hi, why in UNREACHABLE:
        hit = sorted(pc for pc in cov if lo <= pc <= hi)
        ck.check(not hit, "%s $%04X-$%04X not executed" % (label, lo, hi),
                 why if not hit else "EXECUTED at " + " ".join("$%04X" % x for x in hit[:6]))
    for sc, _ in SCEN:
        cells = rr[sc]["cells"]
        ok = cells is not None and not any(cells["CHKSMS"]) and cells["MBCOND"] == 0 and cells["RAMCND"] == 0 and \
            cells["PK1CND"] == 0 and cells["PK2CND"] == 0
        ck.check(ok, "%s: condition cells at the last diag pass" % sc,
                 ("frame %d: CHKSMS %s, MBCOND %02X RAMCND %02X PK1CND %02X PK2CND %02X" %
                  (cells["frame"], gl.hexs(cells["CHKSMS"]), cells["MBCOND"], cells["RAMCND"], cells["PK1CND"],
                   cells["PK2CND"])) if cells else "no self test line")

    ck.section("S2 ALVGUT / ALEARO routines called directly by the self test")
    for name in DIRECT:
        cb = rows["selftest_boot"].get(name, (0, 0, 0, 0, 0))
        cm = rows["selftest_midrun"].get(name, (0, 0, 0, 0, 0))
        ck.check(cb[1] + cm[1] > 0 and cb[3] + cm[3] == 0, "%-8s checked %d / %d" % (name, cb[1], cm[1]),
                 "failed %d" % (cb[3] + cm[3]))

    # ---------------------------------------------------------------- S3
    ck.section("S3 gate.exe trace replay (C modules alone)")
    for sc, n in SCEN:
        g = gl.gate_summary(res["gate_" + sc][1])
        r = ls[sc]["reset"] or (0,) * 9
        run = g["run"] or (-1, -2)
        ck.check(g["pass"] and res["gate_" + sc][0] == 0 and run[0] == run[1] == n and g["resets"] == r[1:5],
                 "%s: GATE PASS, every pass identical" % sc,
                 "%d of %d passes; MAINLN/DIAG %s; RESET arrivals %s (lockstep %s)" %
                 (run[0], run[1], g["loops"], g["resets"], r[1:5]))

    # ---------------------------------------------------------------- S4
    ck.section("S4 refrun (oracle) verdict, self-test result cells, watchdog bites")
    for sc, n in SCEN:
        s = rr[sc]
        ck.check(s["pass"] and res["refrun_" + sc][0] == 0, "%s: REFRUN PASS" % sc, "stop = %s" % (s["stop"] or ("?",))[0])
        cells = s["cells"]
        ck.check(cells is not None and not any(cells["CHKSMS"]) and cells["EARCND"] == 0 and
                 cells["MBCOND"] | cells["RAMCND"] | cells["PK1CND"] | cells["PK2CND"] == 0,
                 "%s: CHKSMS 12 x 0, all condition cells 0" % sc,
                 ("CHKSMS %s EARCND %02X" % (gl.hexs(cells["CHKSMS"]), cells["EARCND"])) if cells else "missing")
        kinds = gl.sched_kinds(os.path.join(W, "refrun_" + sc, "frame_sched.txt"))
        offs = [p for p, port, v in gl.script_lines("tests\\scenarios\\%s.txt" % sc) if port == "test" and v == 0]
        in_diag = [p for p in offs if kinds.get(p) == "D"]
        # a bite reboots with TEST open: the next loop head is MAINLN.  D -> M happens only that way here.
        d_to_m = [f for f in sorted(kinds) if kinds[f] == "D" and kinds.get(f + 1) == "M"]
        b = s["bites"] or (0, -1, -1)
        rs = s["resets"] or (-1,) * 4
        ck.check(b[1] == len(in_diag) and b[2] == 0 and rs[2] == b[1] and d_to_m == in_diag,
                 "%s: one bite per TEST off in the diag loop" % sc,
                 "TEST off at passes %s (in the diag loop: %s); bites %d (not at $DAF7: %d), watchdog RESETs %d; "
                 "diag -> MAINLN after passes %s" % (offs, in_diag, b[1], b[2], rs[2], d_to_m))

    # ---------------------------------------------------------------- S5
    ck.section("S5 tests\\tempest_selftest.exe (native seam)")
    text = res["tempest_selftest"][1]
    lines = [l.strip() for l in text.splitlines() if l.strip().startswith("[")]
    st = [l for l in lines if l[7:].startswith("self test:") or l[7:].startswith("options:")]
    ck.check("SELFTEST: PASS" in text and res["tempest_selftest"][0] == 0, "SELFTEST: PASS",
             ([l for l in text.splitlines() if l.startswith("SELFTEST: ")] or ["no verdict"])[-1])
    ck.check(len(st) >= 15 and all(l.startswith("[PASS]") for l in st), "self test / options checks",
             "%d checks, %d PASS" % (len(st), sum(l.startswith("[PASS]") for l in st)))
    for l in st:
        print("        " + l[:110])

    return ck.verdict()


if __name__ == "__main__":
    sys.exit(main())
