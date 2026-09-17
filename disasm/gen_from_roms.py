#!/usr/bin/env python3
"""Rebuild the Tempest rev-3 CPU image from a ROM set and regenerate + verify every
derived listing.

ROMs are matched by part number (136002-xxx) and CRC32, not by file name, so a
MAME `tempest3.zip` (2K chips) or `tempest.zip` (4K chips, same image), a directory
holding those zips, or a directory of loose files all work.

    2K set (tempest3)            4K set (tempest)
    136002-123 np3  $3000        136002-138 np3  $3000-$3FFF  vector ROM
    136002-124 r3   $3800
    136002-113 d1   $9000        136002-133 d1   $9000-$9FFF
    136002-114 e1   $9800
    136002-115 f1   $A000        136002-134 f1   $A000-$AFFF
    136002-316 h1   $A800
    136002-217 j1   $B000        136002-235 j1   $B000-$BFFF
    136002-118 k1   $B800
    136002-119 lm1  $C000        136002-136 lm1  $C000-$CFFF
    136002-120 mn1  $C800
    136002-121 p1   $D000        136002-237 p1   $D000-$DFFF
    136002-222 r1   $D800
Everything else in the 64K image is $FF (the $E000-$FFFF mirror is not written;
the listings document it).

Usage:
    python gen_from_roms.py [ROMDIR-or-ZIP] [--check] [--cc65 DIR] [--macasm] [--image-only]

Default ROM location: ../roms.  Steps:
  1. build _survey/roms_extracted/tempest3_cpu64k.bin (the image every tool reads)
  2. macasm.py (re-assemble Atari's source; only if build/symbols.json or
     build/srclines.json is missing or older than macasm.py / the source, or --macasm),
     commented_src.py if build/commented_src.json is missing
  3. Track B, if present: emit_vrom.py, verify_vrom.py, emit_shapes.py
     (emit_shapes.py also writes build/vector_tables.json + cam_scripts.json)
  4. emit.py, emit_defines.py, verify.py
  5. --check: emit_ca65.py (ca65 + ld65 round trip of the program listing)
A failing step is reported with '!!' and the chain continues; exit status 1 if any failed.
"""
import argparse, os, subprocess, sys, zipfile, zlib, glob

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
IMAGE = os.path.join(HERE, "_survey", "roms_extracted", "tempest3_cpu64k.bin")
BUILD = os.path.join(HERE, "build")
SRC_DIR = os.path.join(ROOT, "tempest-main", "tempest-main")

# part number -> (load address, size, crc32)
SET_2K = {
    "123": (0x3000, 0x800, 0x29f7e937), "124": (0x3800, 0x800, 0xc16ec351),
    "113": (0x9000, 0x800, 0x65d61fe7), "114": (0x9800, 0x800, 0x11077375),
    "115": (0xA000, 0x800, 0xf3e2827a), "316": (0xA800, 0x800, 0xaeb0f7e9),
    "217": (0xB000, 0x800, 0xef2eb645), "118": (0xB800, 0x800, 0xbeb352ab),
    "119": (0xC000, 0x800, 0xa4de050f), "120": (0xC800, 0x800, 0x35619648),
    "121": (0xD000, 0x800, 0x73d38e47), "222": (0xD800, 0x800, 0x707bd5c3),
}
SET_4K = {
    "138": (0x3000, 0x1000, 0x9995256d), "133": (0x9000, 0x1000, 0x1d0cc503),
    "134": (0xA000, 0x1000, 0xc88e3524), "235": (0xB000, 0x1000, 0xa4b2ce3f),
    "136": (0xC000, 0x1000, 0x65a9a9f9), "237": (0xD000, 0x1000, 0xde4e9e34),
}

def candidates(path):
    """Yield (label, bytes) for every file reachable from path (dir, zip or file)."""
    paths = []
    if os.path.isdir(path):
        for p in sorted(glob.glob(os.path.join(path, "*"))):
            if os.path.isfile(p):
                paths.append(p)
    else:
        paths.append(path)
    zips = []
    for p in paths:
        if p.lower().endswith(".zip"):
            zips.append(p)
            continue
        if os.path.getsize(p) in (0x800, 0x1000):
            yield os.path.basename(p), open(p, "rb").read()
    # prefer the rev-3 zips when a whole directory of sets is given
    zips.sort(key=lambda z: (os.path.basename(z).lower() not in ("tempest3.zip", "tempest.zip"), z))
    for z in zips:
        with zipfile.ZipFile(z) as zf:
            for info in zf.infolist():
                if info.file_size in (0x800, 0x1000):
                    yield "%s:%s" % (os.path.basename(z), info.filename), zf.read(info)

def part_of(label):
    name = os.path.basename(label.split(":")[-1])
    import re
    m = re.search(r"136002[.\-_](\d{3})(?!\d)", name)
    return m.group(1) if m else None

def build_image(path):
    found = {}
    for label, data in candidates(path):
        p = part_of(label)
        if p is None:
            continue
        crc = zlib.crc32(data) & 0xFFFFFFFF
        for s in (SET_2K, SET_4K):
            if p in s and s[p][1] == len(data) and s[p][2] == crc and p not in found:
                found[p] = (label, data)
    for name, s in (("tempest3 (2K chips)", SET_2K), ("tempest (4K chips)", SET_4K)):
        if all(p in found for p in s):
            img = bytearray(b"\xff" * 0x10000)
            for p, (a, n, _) in s.items():
                img[a:a + n] = found[p][1]
            print("ROM set %s:" % name)
            for p, (a, n, _) in sorted(s.items(), key=lambda kv: kv[1][0]):
                print("  136002-%s  $%04X-$%04X  %s" % (p, a, a + n - 1, found[p][0]))
            old = open(IMAGE, "rb").read() if os.path.exists(IMAGE) else None
            os.makedirs(os.path.dirname(IMAGE), exist_ok=True)
            with open(IMAGE, "wb") as fp:
                fp.write(img)
            print("wrote %s%s" % (os.path.relpath(IMAGE, ROOT),
                                  "" if old is None else (" (unchanged)" if old == bytes(img) else " (CHANGED)")))
            return True
    have = sorted(found)
    print("!! no complete rev-3 set found under %s (matched parts: %s)" % (path, ", ".join(have) or "none"))
    return False

def run(script, *args):
    p = os.path.join(HERE, script)
    print("\n== %s %s" % (script, " ".join(args)))
    sys.stdout.flush()
    r = subprocess.run([sys.executable, p] + list(args), cwd=HERE)
    if r.returncode != 0:
        print("!! %s failed (exit %d)" % (script, r.returncode))
    return r.returncode == 0

def macasm_needed():
    outs = [os.path.join(BUILD, f) for f in ("symbols.json", "srclines.json")]
    if not all(os.path.exists(o) for o in outs):
        return True
    t = min(os.path.getmtime(o) for o in outs)
    ins = [os.path.join(HERE, f) for f in ("macasm.py", "macsrc.py", "rtexpr.py", "m6502.py")]
    ins += glob.glob(os.path.join(SRC_DIR, "*.MAC"))
    return any(os.path.exists(i) and os.path.getmtime(i) > t for i in ins)

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("roms", nargs="?", default=os.path.join(ROOT, "roms"),
                    help="ROM directory, set zip or loose file directory (default ../roms)")
    ap.add_argument("--check", action="store_true", help="ca65/ld65 round trip of the program listing")
    ap.add_argument("--cc65", default=None, help="directory holding ca65/ld65 (default ../cc65-win32/bin)")
    ap.add_argument("--macasm", action="store_true", help="always re-run macasm.py")
    ap.add_argument("--image-only", action="store_true", help="only build the 64K image")
    a = ap.parse_args()
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(line_buffering=True)
    if not os.path.exists(a.roms):
        sys.exit("ROM path %s does not exist" % a.roms)
    ok = build_image(os.path.abspath(a.roms))
    if not ok:
        sys.exit(1)
    if a.image_only:
        return
    if not os.path.isdir(SRC_DIR):
        print("!! Atari source not found at %s: macasm/emit need it" % SRC_DIR)
        sys.exit(1)
    if a.macasm or macasm_needed():
        ok &= run("macasm.py")
    else:
        print("\n(macasm.py outputs are up to date; --macasm forces a re-run)")
    if not os.path.exists(os.path.join(BUILD, "commented_src.json")) and os.path.exists(os.path.join(HERE, "commented_src.py")):
        ok &= run("commented_src.py")
    for script in ("emit_vrom.py", "verify_vrom.py", "emit_shapes.py"):      # Track B (vector ROM)
        if os.path.exists(os.path.join(HERE, script)):
            ok &= run(script)
        else:
            print("\n(%s not present - skipped)" % script)
    ok &= run("emit.py")
    ok &= run("emit_defines.py")
    ok &= run("verify.py")
    if a.check:
        ok &= run("emit_ca65.py", *(["--cc65", a.cc65] if a.cc65 else []))
    print("\n%s" % ("all steps passed" if ok else "!! some steps failed; see the !! lines above"))
    sys.exit(0 if ok else 1)

if __name__ == "__main__":
    main()
