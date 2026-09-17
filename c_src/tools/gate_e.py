"""Gate E (EAROM round trip) - M9 batch B4.  Run from c_src after build_all.bat:

    python tools\\gate_e.py [--work DIR]

The ER2055 image layout is read from ALEARO's own tables (TEAX / TEACNT /
TEASRL, $DDDD): group 0 = image $00-$08 <- INITAL+15..+23 (top-3 initials),
$09 checksum; group 1 = $0A-$14 <- HSCORL+15..+25 (top-3 scores + 2 option
bytes), $15 checksum; group 2 = $16-$21 <- BOOKKS..BOOKKE-1, $22 checksum;
checksum = sum of the group's bytes mod 256; $23-$3F unused.

E1 power cycle (oracle, tests\\scenarios\\earom.txt): refrun to pass 2301 (the
   boot after `2300 reset`): EABAD 0 and the initials / scores / bookkeeping
   cells equal the image (the image, not RAM before the reset: SECOUL $0406
   keeps counting), the image has a real high score and bookkeeping, and it is
   the image the full 2600-pass run leaves (tests\\earom\\earom.nv).  C side:
   lockstep earom 2600 PASS with both RESET regions, its trace ==
   tests\\gate\\earom_2600.trc and its image == earom.nv, gate.exe PASS.
E2 image round trip between the oracle and the native build, both ways:
   O = tests\\earom\\earom.nv (written by the ROM on refrun), N = written by the
   native build (tempest_selftest --earom-write: the translated EAUPD over
   er2055.c).  For each image, refrun --earom-in and tempest_selftest
   --earom-boot power on from it: both read EABAD 0 and cells == image, their
   boot passes are byte-identical (RAM except the stack $01E9-$01FF, vector
   RAM, colour RAM), and after 600 passes both chips still hold the image.
   Plus lockstep / gate on earom_readback (O through the C modules).
E3 erase paths (oracle images from truncated refrun runs; C side = gate.exe on
   the two self-test traces, whose EADAL/EACTL I/O is compared call by call):
   EAZERO on a blank chip (selftest_boot, power-on self test): image groups
   all 0, $23-$3F still blank; reboot: INIINI re-induces 010101 / ROM initials,
   bookkeeping 0.  EAZHIS (selftest_midrun 1420): groups 0/1 zeroed, group 2
   kept; EAZBOO (1470): group 2 zeroed, groups 0/1 kept - each isolated by a
   derived copy of the script without the other press (written into the work
   directory; in the real scenario the two erases overlap); both: all zero;
   after TEST off INIINI re-induces.
A boot-pass compare exempts SPARE3 ($0133, see below) besides the stack.
Prints GATE E: PASS / FAIL; exit code 0 / 1.
"""
import argparse
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gatelib as gl  # noqa: E402

A_EABAD, A_INITAL, A_HSCORL, A_BOOKKS, A_QSTATE, A_EARCND = 0x01C9, 0x0606, 0x0706, 0x0406, 0x0000, 0x007C
ROM_SCOINI = 0xAC08
STACK_EXEMPT = (0x01E9, 0x01FF)      # gate.exe's rule: the stack above the lowest SP since RESET
# SPARE3 ($0133): the IRQ counts the passes it sees the AVG halted.  The native seam services
# IRQs in the frame wait instead of mid-pass (NOTES_m8.md known gap), so on the boot pass one
# more IRQ sees the boot list halted: native 09, ROM 08 - identical on a blank boot, unrelated
# to the EAROM.  Exempt and reported.
SPARE3 = 0x0133


def cells_vs_image(ram, img):
    """list of (group, offset, ram, image) differences for the three groups"""
    diffs = []
    for k, lo, hi, src in gl.earom_groups():
        for i in range(hi - lo):
            if ram[src + i] != img[lo + i]:
                diffs.append((k, lo + i, ram[src + i], img[lo + i]))
    return diffs


def frame(dirname, n, ext):
    return gl.read_bin(os.path.join(dirname, "frame_%04d.%s" % (n, ext)))


def compare_boot(d_oracle, d_native):
    """byte compare of two boot-pass dumps; returns (n ram diffs, first few, vram diffs, col diffs)"""
    ro, rn = frame(d_oracle, 1, "ram"), frame(d_native, 1, "ram")
    rd = [a for a in range(0x800) if ro[a] != rn[a] and not (STACK_EXEMPT[0] <= a <= STACK_EXEMPT[1]) and a != SPARE3]
    vd = sum(1 for a, b in zip(frame(d_oracle, 1, "vram"), frame(d_native, 1, "vram")) if a != b)
    cd = sum(1 for a, b in zip(frame(d_oracle, 1, "col"), frame(d_native, 1, "col")) if a != b)
    first = " ".join("$%04X ROM=%02X C=%02X" % (a, ro[a], rn[a]) for a in rd[:4])
    first += " (SPARE3 exempt: ROM %02X C %02X)" % (ro[SPARE3], rn[SPARE3])
    return len(rd), first, vd, cd


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--work", help="work directory (default obj\\gate_e)")
    args = ap.parse_args()
    W = gl.work_dir(args, "gate_e")
    ck = gl.Checker("GATE E")
    print("GATE E: EAROM round trip (M9) - work directory %s" % W)
    O = os.path.join(gl.C_SRC, "tests", "earom", "earom.nv")
    N = os.path.join(W, "native_written.nv")

    def d(name):
        p = os.path.join(W, name)
        os.makedirs(p, exist_ok=True)
        return p

    # the native writer first (seconds): its image feeds the parallel batch
    with open(os.path.join(W, "native_write.log"), "w") as log:
        rc_nw = subprocess.call([os.path.join(gl.C_SRC, "tests", "tempest_selftest.exe"), "--earom-write", N],
                                cwd=gl.C_SRC, stdout=log, stderr=subprocess.STDOUT)
    print("  native writer exit %d" % rc_nw)

    E1D, OOR, ONA, NOR, NNA = d("e1_refrun"), d("o_refrun"), d("o_native"), d("n_refrun"), d("n_native")
    Z0, Z1, HA, HB, HZ, HC = (d("e3_eazero_300"), d("e3_reboot_1361"), d("e3_mid_1419"), d("e3_eazboo_only_1700"),
                              d("e3_eazhis_only_1700"), d("e3_mid_1700"))
    boot, mid, ear = "tests\\scenarios\\selftest_boot.txt", "tests\\scenarios\\selftest_midrun.txt", "tests\\scenarios\\earom.txt"
    # selftest_midrun presses EAZHIS (1420) and EAZBOO (1470) while the first erase is still running
    # (DSPSYS re-requests while the buttons are held), so each path is isolated by a derived script
    # without the other press (the cursor spin lines stay, so the option positions are unchanged)
    src = open(os.path.join(gl.C_SRC, mid)).read().splitlines()

    def derived(name, drop):
        path = os.path.join(W, name)
        with open(path, "w") as f:
            f.write("# derived from tests\\scenarios\\selftest_midrun.txt by tools\\gate_e.py: lines %s removed\n" % (drop,))
            for line in src:
                p = line.split("#", 1)[0].split()
                if len(p) >= 2 and (int(p[0]), p[1]) in drop:
                    continue
                f.write(line + "\n")
        return path
    only_hisc = derived("midrun_eazhis_only.txt", {(1470, "fire"), (1470, "start1"), (1476, "fire"), (1476, "start1")})
    only_book = derived("midrun_eazboo_only.txt", {(1420, "fire"), (1420, "start2"), (1426, "fire"), (1426, "start2")})
    rr = "tests\\refrun.exe"
    jobs = [
        ("e1_refrun", [rr, "--frames", "2301", "--script", ear, "--outdir", E1D, "--capture-every", "2301",
                       "--earom-out", os.path.join(E1D, "earom.nv")]),
        ("e1_lockstep", ["tests\\lockstep.exe", "--frames", "2600", "--no-dumps", "--script", ear,
                         "--earom-out", os.path.join(W, "lockstep_earom_2600.nv"),
                         "--trace-out", os.path.join(W, "earom_2600.trc")]),
        ("e1_gate", ["tests\\gate.exe", "tests\\gate\\earom_2600.trc"]),
        ("o_refrun", [rr, "--frames", "600", "--script", "tests\\scenarios\\earom_readback.txt", "--earom-in", O,
                      "--outdir", OOR, "--capture-every", "1000000", "--earom-out", os.path.join(OOR, "earom_exit.nv")]),
        ("o_native", ["tests\\tempest_selftest.exe", "--earom-boot", O, "--dump", ONA, "--passes", "600"]),
        ("o_lockstep", ["tests\\lockstep.exe", "--frames", "600", "--no-dumps", "--script",
                        "tests\\scenarios\\earom_readback.txt", "--earom-in", O,
                        "--earom-out", os.path.join(W, "lockstep_readback_600.nv"),
                        "--trace-out", os.path.join(W, "earom_readback_600.trc")]),
        ("o_gate", ["tests\\gate.exe", "tests\\gate\\earom_readback_600.trc"]),
        ("n_refrun", [rr, "--frames", "600", "--earom-in", N, "--outdir", NOR, "--capture-every", "1000000",
                      "--earom-out", os.path.join(NOR, "earom_exit.nv")]),
        ("n_native", ["tests\\tempest_selftest.exe", "--earom-boot", N, "--dump", NNA, "--passes", "600"]),
        ("e3_eazero", [rr, "--frames", "300", "--script", boot, "--outdir", Z0, "--capture-every", "300",
                       "--earom-out", os.path.join(Z0, "earom.nv")]),
        ("e3_reboot", [rr, "--frames", "1361", "--script", boot, "--outdir", Z1, "--capture-every", "1361",
                       "--earom-out", os.path.join(Z1, "earom.nv")]),
        ("e3_mid_1419", [rr, "--frames", "1419", "--script", mid, "--outdir", HA, "--capture-every", "1419",
                         "--earom-out", os.path.join(HA, "earom.nv")]),
        ("e3_eazboo_only", [rr, "--frames", "1700", "--script", only_book, "--outdir", HB, "--capture-every", "1700",
                            "--earom-out", os.path.join(HB, "earom.nv")]),
        ("e3_eazhis_only", [rr, "--frames", "1700", "--script", only_hisc, "--outdir", HZ, "--capture-every", "1700",
                            "--earom-out", os.path.join(HZ, "earom.nv")]),
        ("e3_mid_1700", [rr, "--frames", "1700", "--script", mid, "--outdir", HC, "--capture-every", "1700",
                         "--earom-out", os.path.join(HC, "earom.nv")]),
        ("e3_gate_boot", ["tests\\gate.exe", "tests\\gate\\selftest_boot_1800.trc"]),
        ("e3_gate_mid", ["tests\\gate.exe", "tests\\gate\\selftest_midrun_2500.trc"]),
    ]
    res = gl.run_parallel(jobs, W)
    groups = gl.earom_groups()
    print("\n  image layout from ALEARO's tables: " + "; ".join(
        "group %d %s = $%02X-$%02X <- $%04X, checksum $%02X" % (k, gl.EAROM_GROUP_NAMES[k], lo, hi - 1, src, hi)
        for k, lo, hi, src in groups))

    def refrun_ok(name):
        s = gl.refrun_summary(res[name][1])
        return s["pass"] and res[name][0] == 0, s

    # ---------------------------------------------------------------- E1
    ck.section("E1 power-cycled boot reads the image back (earom.txt, reset at 2300)")
    ok, s = gl.refrun_snapshot_ok(res["e1_refrun"][1])
    ck.check(ok and s["resets"] == (1, 0, 0, 1), "refrun earom cut at pass 2301, 1 script reset",
             "stop %s, RESET arrivals %s" % ((s["stop"] or ("?",))[0], s["resets"]))
    img = gl.read_bin(os.path.join(E1D, "earom.nv"))
    ram = frame(E1D, 2301, "ram")
    ck.check(all(gl.group_checksum_ok(img, g) for g in groups), "image: 3 groups with good checksums",
             "checksums " + " ".join("$%02X" % img[g[2]] for g in groups))
    scores = img[groups[1][1]:groups[1][1] + 9]
    book = img[groups[2][1]:groups[2][2]]
    ck.check(any(b != 0x01 for b in scores) and any(book), "image holds a real entry and bookkeeping",
             "top-3 scores %s (top %02X%02X%02X), bookkeeping %s" % (gl.hexs(scores), scores[8], scores[7], scores[6],
                                                                     gl.hexs(book)))
    diffs = cells_vs_image(ram, img)
    ck.check(ram[A_EABAD] == 0 and not diffs, "frame 2301: EABAD 0, cells == image",
             "EABAD %02X; %d cells differ %s" % (ram[A_EABAD], len(diffs),
                                                 " ".join("g%d/$%02X RAM=%02X IMG=%02X" % x for x in diffs[:4])))
    ck.check(gl.same_file(os.path.join(E1D, "earom.nv"), O), "image at 2301 == tests\\earom\\earom.nv (end of run)",
             "no erase/write after the reset; the checked-in image is current")
    ls = gl.lockstep_summary(res["e1_lockstep"][1])
    r = ls["reset"] or (-1,) * 9
    ck.check(ls["pass"] and (ls["calls"] or (0, 1))[1] == 0 and r[0] == 2 and r[5] == 2 and r[6] == 2,
             "C: lockstep earom 2600 PASS, 2 RESET regions",
             "%s calls, failed %s; RESET arrivals %d, regions %d compared %d passed" %
             ((ls["calls"] or ("?", "?"))[0], (ls["calls"] or ("?", "?"))[1], r[0], r[5], r[6]))
    ck.check(gl.same_file(os.path.join(W, "earom_2600.trc"), os.path.join(gl.C_SRC, "tests", "gate", "earom_2600.trc")) and
             gl.same_file(os.path.join(W, "lockstep_earom_2600.nv"), O),
             "C: trace == tests\\gate, image == earom.nv", "")
    g = gl.gate_summary(res["e1_gate"][1])
    ck.check(g["pass"] and g["resets"] == (1, 0, 0, 1), "C: gate.exe earom_2600.trc PASS",
             "passes %s, RESET arrivals %s" % (g["run"], g["resets"]))

    # ---------------------------------------------------------------- E2
    ck.section("E2 image round trip oracle <-> native")
    nimg = gl.read_bin(N) if os.path.exists(N) else b""
    inj_ok = len(nimg) == 64 and \
        list(nimg[groups[0][1]:groups[0][1] + 9]) == [0x0A + i for i in range(9)] and \
        list(nimg[groups[1][1]:groups[1][1] + 9]) == [0x12, 0x34, 0x05, 0x78, 0x56, 0x07, 0x90, 0x21, 0x09] and \
        list(nimg[groups[2][1] + 3:groups[2][2]]) == [0x21 + i for i in range(9)]
    ck.check(rc_nw == 0 and inj_ok and all(gl.group_checksum_ok(nimg, x) for x in groups),
             "native writer: EAUPD wrote the table, checksums good",
             ("image " + gl.hexs(nimg[:0x23])) if nimg else "no image")
    for tag, image_path, dor, dna, desc in (("O", O, OOR, ONA, "oracle-written tests\\earom\\earom.nv"),
                                            ("N", N, NOR, NNA, "native-written image")):
        im = gl.read_bin(image_path)
        ok, s = refrun_ok(tag.lower() + "_refrun")
        nat_rc = res[tag.lower() + "_native"][0]
        ck.check(ok and nat_rc == 0, "%s: refrun --earom-in PASS, native boot ran" % tag, desc)
        ro, rn = frame(dor, 1, "ram"), frame(dna, 1, "ram")
        do_, dn = cells_vs_image(ro, im), cells_vs_image(rn, im)
        ck.check(ro[A_EABAD] == 0 and not do_, "%s: oracle reads it: EABAD 0, cells == image" % tag,
                 "EABAD %02X, %d cells differ" % (ro[A_EABAD], len(do_)))
        ck.check(rn[A_EABAD] == 0 and not dn, "%s: native reads it: EABAD 0, cells == image" % tag,
                 "EABAD %02X, %d cells differ" % (rn[A_EABAD], len(dn)))
        nr, first, vd, cd = compare_boot(dor, dna)
        ck.check(nr == 0 and vd == 0 and cd == 0, "%s: boot pass oracle == native" % tag,
                 "RAM %d differ (stack $%04X-$%04X exempt) %s, vector RAM %d, colour %d" %
                 (nr, STACK_EXEMPT[0], STACK_EXEMPT[1], first, vd, cd))
        eo = gl.read_bin(os.path.join(dor, "earom_exit.nv"))
        en = gl.read_bin(os.path.join(dna, "earom_exit.nv"))
        ck.check(eo == im and en == im, "%s: chip unchanged after 600 passes (both)" % tag,
                 "oracle %s, native %s" % ("same" if eo == im else "CHANGED", "same" if en == im else "CHANGED"))
    ls = gl.lockstep_summary(res["o_lockstep"][1])
    g = gl.gate_summary(res["o_gate"][1])
    ck.check(ls["pass"] and (ls["calls"] or (0, 1))[1] == 0 and g["pass"] and
             gl.same_file(os.path.join(W, "earom_readback_600.trc"),
                          os.path.join(gl.C_SRC, "tests", "gate", "earom_readback_600.trc")) and
             gl.same_file(os.path.join(W, "lockstep_readback_600.nv"), O),
             "O: lockstep + gate earom_readback PASS", "calls %s; trace current; image unchanged" % (ls["calls"] or ("?",))[0])

    # ---------------------------------------------------------------- E3
    ck.section("E3 erase paths leave the expected image")
    blank = bytes([0xFF] * 64)

    def zeroed(base, ks):
        b = bytearray(base)
        for k, lo, hi, src in groups:
            if k in ks:
                b[lo:hi + 1] = bytes(hi + 1 - lo)
        return bytes(b)

    ok, s = gl.refrun_snapshot_ok(res["e3_eazero"][1])
    z = gl.read_bin(os.path.join(Z0, "earom.nv"))
    r1, r300 = frame(Z0, 1, "ram"), frame(Z0, 300, "ram")
    ec = s["earom_cells"] or (-1, -1, -1)
    ck.check(ok and r1[A_EARCND] != 0 and z == zeroed(blank, (0, 1, 2)) and ec[1] == 0 and ec[2] == 0 and
             r300[A_EARCND] == 0 and r300[A_QSTATE] == 2,
             "EAZERO (blank chip, power-on self test)",
             "diag pass 1 EARCND %02X (blank fails); pass 300: EAFLG %02X EAREQU %02X, EARCND %02X, QSTATE %d; image $00-$22 "
             "%s, $23-$3F %s" % (r1[A_EARCND], ec[1], ec[2], r300[A_EARCND], r300[A_QSTATE],
                                 "all 00" if not any(z[:0x23]) else gl.hexs(z[:0x23]),
                                 "blank" if z[0x23:] == blank[0x23:] else "CHANGED"))
    ok, s = gl.refrun_snapshot_ok(res["e3_reboot"][1])
    zr = gl.read_bin(os.path.join(Z1, "earom.nv"))
    rr1361 = frame(Z1, 1361, "ram")
    scoini = bytes(gl.rom_byte(ROM_SCOINI + i) for i in range(24))
    ck.check(ok and zr == z and rr1361[A_EABAD] & 3 == 0 and rr1361[A_HSCORL:A_HSCORL + 24] == bytes([1] * 24) and
             rr1361[A_INITAL:A_INITAL + 24] == scoini and not any(rr1361[A_BOOKKS:A_BOOKKS + 12]),
             "EAZERO: reboot re-induces the defaults",
             "pass 1361 (watchdog reboot): EABAD %02X, scores all 01: %s, initials == ROM SCOINI: %s, bookkeeping %s; "
             "image unchanged: %s" % (rr1361[A_EABAD], rr1361[A_HSCORL:A_HSCORL + 24] == bytes([1] * 24),
                                      rr1361[A_INITAL:A_INITAL + 24] == scoini, gl.hexs(rr1361[A_BOOKKS:A_BOOKKS + 12]),
                                      zr == z))
    runs = {}
    for name in ("e3_mid_1419", "e3_eazhis_only", "e3_eazboo_only", "e3_mid_1700"):
        runs[name] = gl.refrun_snapshot_ok(res[name][1])
    ia = gl.read_bin(os.path.join(HA, "earom.nv"))
    iz = gl.read_bin(os.path.join(HZ, "earom.nv"))
    ib = gl.read_bin(os.path.join(HB, "earom.nv"))
    ic = gl.read_bin(os.path.join(HC, "earom.nv"))
    idle = all((runs[n][1]["earom_cells"] or (0, 1, 1))[1:] == (0, 0) for n in runs)
    ck.check(all(runs[n][0] for n in runs) and idle,
             "midrun cut at 1419 (before) and 1700 (x3): EAROM idle",
             "EABAD/EAFLG/EAREQU " + " ".join("%s %s" % (n, runs[n][1]["earom_cells"]) for n in runs))
    ck.check(ia != zeroed(ia, (0, 1)) and iz == zeroed(ia, (0, 1)),
             "EAZHIS alone (FIRE+START2 at 1420): groups 0/1 zeroed",
             "before $00-$15 %s; after %s; group 2 and $23-$3F kept: %s" %
             (gl.hexs(ia[:0x16]), gl.hexs(iz[:0x16]), iz[0x16:] == ia[0x16:]))
    ck.check(any(ia[0x16:0x22]) and ib == zeroed(ia, (2,)),
             "EAZBOO alone (FIRE+START1 at 1470): group 2 zeroed",
             "bookkeeping before %s, after %s; groups 0/1 and $23-$3F kept: %s" %
             (gl.hexs(ia[0x16:0x23]), gl.hexs(ib[0x16:0x23]), ib[:0x16] == ia[:0x16] and ib[0x23:] == ia[0x23:]))
    ck.check(ic == zeroed(ia, (0, 1, 2)), "both (selftest_midrun as checked in): all groups zeroed",
             "image at 1700 $00-$22 %s, $23-$3F kept: %s" %
             ("all 00" if not any(ic[:0x23]) else gl.hexs(ic[:0x23]), ic[0x23:] == ia[0x23:]))
    rc1700 = frame(HC, 1700, "ram")
    ck.check(rc1700[A_EABAD] & 3 == 0 and rc1700[A_HSCORL:A_HSCORL + 24] == bytes([1] * 24) and
             not any(rc1700[A_BOOKKS + 3:A_BOOKKS + 12]),
             "after TEST off: INIINI re-induces, bookkeeping 0",
             "pass 1700: EABAD %02X, scores all 01: %s, BOOKKS %s (SECOUL $0406-$0408 counts on)" %
             (rc1700[A_EABAD], rc1700[A_HSCORL:A_HSCORL + 24] == bytes([1] * 24),
              gl.hexs(rc1700[A_BOOKKS:A_BOOKKS + 12])))
    for name, trc in (("e3_gate_boot", "selftest_boot_1800"), ("e3_gate_mid", "selftest_midrun_2500")):
        g = gl.gate_summary(res[name][1])
        run = g["run"] or (-1, -2)
        ck.check(g["pass"] and run[0] == run[1], "C: gate.exe %s PASS (erase I/O checked)" % trc,
                 "%d of %d passes identical" % run)

    return ck.verdict()


if __name__ == "__main__":
    sys.exit(main())
