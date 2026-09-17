"""Atari Analog Vector Generator (AVG) decoder/encoder for Tempest.

Adapted from the Space Duel disassembly's avg.py. Tempest-specific semantics
were checked against:

  * tempest-main/VGMC.MAC (Ed Logg's macros: VCTR, SVEC short form, STAT, SCAL,
    CNTR, HALT, JSRL, RTSL, JMPL) and ALVROM.MAC (CSTAT = .WORD 68C0+colour)
  * the AAE/MAME AVG core used by tempest.cpp (mame_late_avgdvg.cpp,
    avg_tempest: tempest_strobe2 / tempest_strobe3)

Opcode = word >> 13 (the first word, little-endian in the ROM):

  0 VCTR  2 words: w1 = DY & $1FFF, w2 = Z<<13 | DX & $1FFF  (13-bit two's
          complement deltas, NOT sign-magnitude as on the DVG)
  1 HALT  $2000
  2 SVEC  short vector $4000 | Z<<5 | (DX/2)&$1F | ((DY/2)&$1F)<<8
  3 STAT  $6xxx when bit 12 clear.  TEMPEST: if bit 11 ($0800) is set the low
          nibble loads the COLOUR latch (index into colour RAM $0800-$080F);
          otherwise (w>>4)&$F loads the INTENSITY latch.  ALVROM's CSTAT c
          writes $68C0+c; ALDIS2 calls VGSTAT with MZCOLO=8 (colour) or
          MZBRIT=0 (intensity).  Tempest has no sparkle / X-flip bits: those
          ($0800 sparkle, $0400 flip) belong to Major Havoc's AVG
          (mhavoc_strobe2); on Tempest the $0400/$0200/$0100 bits that VGMC's
          STAT macro can set (HI/LOW, IN/OUT window) are ignored.
      SCAL  $7000 | b<<8 | linear  (bit 12 set): binary scale b (0..7),
          linear scale l (factor (255-l)/256 ... $00 = full, $80 = 1/2)
  4 CNTR  $8040 - centre the beam
  5 JSRL  $A000 | target/2  (VG address space: CPU $2000 + 2*(w & $1FFF))
  6 RTSL  $C000  (ALVROM hides checksum bytes in RTSL low bytes: CHKSM0/1)
  7 JMPL  $E000 | target/2

Vector intensity (tempest_strobe3): z = top 3 bits of the vector's X word
(SVEC: bits 7-5 of the low byte). z == 1 means "use the STAT intensity",
otherwise the brightness is z*2 (0 = beam off). Colour is always the last
colour STAT.
"""
import os

VG_BASE = 0x2000
HERE = os.path.dirname(os.path.abspath(__file__))
ROM64K = os.path.join(HERE, "_survey", "roms_extracted", "tempest3_cpu64k.bin")
VROM_LO, VROM_HI = 0x3000, 0x3FFF


def load_image(path=ROM64K):
    """64K CPU image as bytes (index = CPU address)."""
    data = open(path, "rb").read()
    assert len(data) == 0x10000, path
    return data


def s13(v):
    """13-bit two's complement delta (VGMC: .WORD DY&^H1FFF)."""
    v &= 0x1FFF
    return v - 0x2000 if v & 0x1000 else v


def s5(v):
    v &= 0x1F
    return v - 0x20 if v & 0x10 else v


def word(mem, a):
    return mem[a] | (mem[a + 1] << 8)


def vg_target(w):
    return VG_BASE + ((w & 0x1FFF) * 2)


def decode_raw(mem, a):
    """-> dict(op, len, fields...) without text formatting."""
    w = word(mem, a)
    top = w >> 13
    if top == 0:
        w2 = word(mem, a + 2)
        return dict(op="VCTR", len=4, dx=s13(w2), dy=s13(w), z=(w2 >> 13) & 7,
                    words=[w, w2])
    if top == 1:
        return dict(op="HALT", len=2, words=[w])
    if top == 2:
        return dict(op="SVEC", len=2, dx=s5(w) * 2, dy=s5(w >> 8) * 2,
                    z=(w >> 5) & 7, words=[w])
    if top == 3:
        if w & 0x1000:
            return dict(op="SCAL", len=2, b=(w >> 8) & 0xF, l=w & 0xFF, words=[w])
        if (w & 0x0F00) == 0x0800 and (w & 0xF0) == 0xC0:
            return dict(op="CSTAT", len=2, color=w & 0xF, words=[w])
        return dict(op="STAT", len=2, words=[w],
                    color=(w & 0xF) if w & 0x0800 else None,
                    intensity=None if w & 0x0800 else (w >> 4) & 0xF)
    if top == 4:
        return dict(op="CNTR", len=2, words=[w])
    if top == 5:
        return dict(op="JSRL", len=2, target=vg_target(w), words=[w])
    if top == 6:
        return dict(op="RTSL", len=2, words=[w])
    return dict(op="JMPL", len=2, target=vg_target(w), words=[w])


def decode(mem, a):
    """-> (mnemonic, operand_text, length) in Space Duel listing style."""
    d = decode_raw(mem, a)
    op = d["op"]
    if op in ("VCTR", "SVEC"):
        return op, "%d, %d, %d" % (d["dx"], d["dy"], d["z"]), d["len"]
    if op == "SCAL":
        return op, "%d, $%02X" % (d["b"], d["l"]), 2
    if op == "CSTAT":
        return op, "%d" % d["color"], 2
    if op == "STAT":
        return op, "$%04X" % d["words"][0], 2
    if op in ("JSRL", "JMPL"):
        return op, "$%04X" % d["target"], 2
    if op == "CNTR" and d["words"][0] != 0x8040:
        return op, "$%04X" % d["words"][0], 2
    if op == "RTSL" and d["words"][0] != 0xC000:
        return op, "$%04X" % d["words"][0], 2
    return op, "", d["len"]


def _ints(s):
    out = []
    for x in s.split(","):
        x = x.strip()
        out.append(int(x[1:], 16) if x.startswith("$") else int(x))
    return out


def encode(mn, oper):
    """Inverse of decode(): (mnemonic, operand text) -> list of 16-bit words,
    or None when the text cannot be encoded. JSRL/JMPL operands must be $hex
    (resolve labels before calling)."""
    oper = (oper or "").strip()
    if mn == "HALT":
        return [0x2000]
    if mn == "RTSL":
        return [_ints(oper)[0]] if oper else [0xC000]
    if mn == "CNTR":
        return [_ints(oper)[0]] if oper else [0x8040]
    if mn == "VCTR":
        dx, dy, z = _ints(oper)
        return [dy & 0x1FFF, ((z & 7) << 13) | (dx & 0x1FFF)]
    if mn == "SVEC":
        dx, dy, z = _ints(oper)
        if dx & 1 or dy & 1:
            return None
        return [0x4000 | ((z & 7) << 5) | ((dx >> 1) & 0x1F) | (((dy >> 1) & 0x1F) << 8)]
    if mn == "SCAL":
        b, l = _ints(oper)
        return [0x7000 | ((b & 0xF) << 8) | (l & 0xFF)]
    if mn == "CSTAT":
        return [0x68C0 | (_ints(oper)[0] & 0xF)]
    if mn == "STAT":
        return [_ints(oper)[0]]
    if mn in ("JSRL", "JMPL"):
        t = _ints(oper)[0]
        if t & 1 or not (VG_BASE <= t < VG_BASE + 0x4000):
            return None
        return [(0xA000 if mn == "JSRL" else 0xE000) | ((t - VG_BASE) >> 1)]
    return None


def scal_factor(b, lin):
    """aae_avg: timer shifted b times, linear scale complemented:
    factor = ((~lin)&0xFF)/256 / 2^b  (SCAL 0,$00 ~ full size)."""
    return ((~lin) & 0xFF) / 256.0 / (1 << (b & 7))


def targets(mem, lo=VROM_LO, hi=VROM_HI):
    out, a = set(), lo
    while a < hi:
        d = decode_raw(mem, a)
        if d["op"] in ("JSRL", "JMPL") and lo <= d["target"] <= hi:
            out.add(d["target"])
        a += d["len"]
    return out


if __name__ == "__main__":
    mem = load_image()
    counts, a = {}, VROM_LO
    while a < VROM_HI:
        d = decode_raw(mem, a)
        counts[d["op"]] = counts.get(d["op"], 0) + 1
        a += d["len"]
    print("AVG opcode census over $3000-$3FFF (linear sweep):")
    for k, v in sorted(counts.items(), key=lambda x: -x[1]):
        print("   %-6s %5d" % (k, v))
    print("JSRL/JMPL targets inside vector ROM: %d" % len(targets(mem)))
