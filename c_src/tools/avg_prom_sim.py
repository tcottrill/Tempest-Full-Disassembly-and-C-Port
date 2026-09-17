#!/usr/bin/env python3
"""avg_prom_sim.py - check avg.c's AVG draw-time model against MAME and the real PROM.

avg.c charges every AVG instruction a fixed number of state-PROM ticks plus
the vector timer (avg.h TIMING), a table inherited from the Space Duel and
Gravitar ports.  This file derives the same number independently: it is a
transcription of MAME's actual AVG state machine, run against the REAL 256x4
state PROM of the Tempest ROM set (136002-125.d7) over the oracle's vector
RAM dumps, and compared with what tests\\avgtime.exe (avg.c) prints for the
same dumps.  The Gravitar port's tool of the same name, with Tempest's memory
map (4K vector RAM + 4K vector ROM) and frame end (the jump to address 0).

    python tools/avg_prom_sim.py DIR [DIR ...]      every DIR/frame_NNNN.vram
    python tools/avg_prom_sim.py --prom FILE DIR

Transcribed from mame0286s/src/devices/video/avgdvg.cpp:
    run_state_machine()        line 1202 - 8 master cycles per PROM step
    avg_device::state_addr(), handler_0..3 (latch0..3)
    handler_4 (strobe0)        line 490  - normalizer -> timer
    handler_5 (strobe1)        line 541  - binary scale -> timer
    avg_common_strobe2         line 558  - the jump to 0 = Tempest's frame end
                                           ("Tempest and Quantum keep the AVG in
                                           an endless loop", lines 568-584)
    avg_common_strobe3         line 618  - $100 - (timer & $FF) / $8000 - timer
    avg_tempest handler_6 / 7  lines 679-722: colour and intensity only, the
                                           timing is avg_common_strobe3's
    MASTER_CLOCK 12,096,000    line 29
Clocks, mame0286s/src/mame/atari/tempest.cpp lines 295-296, 642-644:
    M6502 = MASTER_CLOCK / 8, IRQ = MASTER_CLOCK / 4096 / 12 = 246.09 Hz.

A looping list is timed over one whole traversal in the steady state: from
one jump to address 0 to the next.  A list that HALTs is timed from VGGO to
the halt, less the idle tick after VGGO (avg.h AVG_VGGO_LEADIN).
"""
import argparse
import glob
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CSRC = os.path.dirname(HERE)
ROOT = os.path.dirname(CSRC)
MASTER_CLOCK = 12_096_000
PROM_DEFAULT = os.path.join(ROOT, "disasm", "_survey", "roms_extracted", "tempest", "136002-125.d7")


def vecrom_bytes():
    src = open(os.path.join(CSRC, "vecrom.c")).read()
    body = src[src.index("{") + 1:src.rindex("}")]
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    b = bytes(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", body))
    assert len(b) == 0x1000, len(b)
    return b


class Avg:
    """MAME's avg_device, transcribed; the vector output is dropped, every
    register that feeds the cycle counts is kept."""

    def __init__(self, prom, mem):
        self.prom, self.mem = prom, mem
        self.state_latch = 0
        self.bin_scale = self.scale = 0
        self.pc = self.sp = 0
        self.halt = 0
        self.dvx = self.dvy = self.dvy12 = self.op = 0
        self.timer = self.int_latch = 0
        self.data = 0
        self.stack = [0] * 4
        self.jump0 = 0

    def OP0(self): return self.op & 1
    def OP1(self): return (self.op >> 1) & 1
    def OP2(self): return (self.op >> 2) & 1

    def state_addr(self):
        return ((((self.state_latch >> 4) ^ 1) << 7) | (self.op << 4) | (self.state_latch & 0xF))

    def h0(self):
        self.dvy = (self.dvy & 0x1F00) | self.data
        self.pc += 1
        return 0

    def h1(self):
        self.dvy12 = (self.data >> 4) & 1
        self.op = self.data >> 5
        self.int_latch = 0
        self.dvy = (self.dvy12 << 12) | ((self.data & 0xF) << 8)
        self.dvx = 0
        self.pc += 1
        return 0

    def h2(self):
        self.dvx = (self.dvx & 0x1F00) | self.data
        self.pc += 1
        return 0

    def h3(self):
        self.int_latch = self.data >> 4
        self.dvx = ((self.int_latch & 1) << 12) | ((self.data & 0xF) << 8) | (self.dvx & 0xFF)
        self.pc += 1
        return 0

    def h4(self):
        if self.OP0():
            self.stack[self.sp & 3] = self.pc
        else:
            i = 0
            while ((((self.dvy ^ (self.dvy << 1)) & 0x1000) == 0)
                   and (((self.dvx ^ (self.dvx << 1)) & 0x1000) == 0) and i < 16):
                i += 1
                self.dvy = (self.dvy & 0x1000) | ((self.dvy << 1) & 0x1FFF)
                self.dvx = (self.dvx & 0x1000) | ((self.dvx << 1) & 0x1FFF)
                self.timer = (self.timer >> 1) | 0x4000 | (self.OP1() << 7)
            if self.OP1():
                self.timer &= 0xFF
        return 0

    def h5(self):
        if not self.OP2():
            for _ in range(self.bin_scale):
                self.timer = (self.timer >> 1) | 0x4000 | (self.OP1() << 7)
            if self.OP1():
                self.timer &= 0xFF
        if self.OP2():
            self.sp = (self.sp - 1) & 0xF if self.OP1() else (self.sp + 1) & 0xF
        return 0

    def h6(self):
        if self.OP2():
            if self.OP0():
                self.pc = self.dvy << 1
                if self.dvy == 0:
                    self.jump0 = 1
            else:
                self.pc = self.stack[self.sp & 3]
        elif self.dvy12:
            self.scale = self.dvy & 0xFF
            self.bin_scale = (self.dvy >> 8) & 7
        return 0

    def h7(self):
        cycles = 0
        self.halt = self.OP0()
        if not self.OP0() and not self.OP2():
            cycles = (0x100 - (self.timer & 0xFF)) if self.OP1() else (0x8000 - self.timer)
            self.timer = 0
        if self.OP2():
            cycles = 0x8000 - self.timer
            self.timer = 0
        return cycles

    def run(self, limit=4_000_000):
        """-> ('LOOP', cycles of one steady-state traversal) or ('HALT', cycles less the lead-in)."""
        H = (self.h0, self.h1, self.h2, self.h3, self.h4, self.h5, self.h6, self.h7)
        cycles = steps = 0
        marks = []
        while steps < limit:
            self.state_latch = (self.state_latch & 0x10) | (self.prom[self.state_addr()] & 0xF)
            if (self.state_latch >> 3) & 1:
                self.data = self.mem[(self.pc ^ 1) & 0x1FFF]
                cycles += H[self.state_latch & 7]()
            self.state_latch = (self.halt << 4) | (self.state_latch & 0xF)
            cycles += 8
            steps += 1
            if self.jump0:
                self.jump0 = 0
                marks.append(cycles)
                if len(marks) == 2:
                    return "LOOP", marks[1] - marks[0]
            if self.halt:
                return "HALT", cycles - 8
        return "BUDGET", cycles


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("dirs", nargs="+", help="folders of frame_NNNN.vram (tests\\refrun.exe --outdir)")
    ap.add_argument("--prom", default=PROM_DEFAULT, help="the AVG state PROM 136002-125 (256 bytes)")
    args = ap.parse_args()

    prom = open(args.prom, "rb").read()
    assert len(prom) == 0x100, "%s: %d bytes" % (args.prom, len(prom))
    vrom = vecrom_bytes()
    exe = os.path.join(CSRC, "tests", "avgtime.exe")
    total = bad = 0
    for d in args.dirs:
        table = {}
        for line in subprocess.run([exe, d], capture_output=True, text=True).stdout.splitlines():
            m = re.match(r"frame_(\d+)\s+(\S+)\s+\d+\s+\d+\s+(\d+)", line)
            if m:
                table[int(m.group(1))] = (m.group(2), int(m.group(3)))
        nd = nbad = 0
        for p in sorted(glob.glob(os.path.join(d, "frame_*.vram"))):
            n = int(re.search(r"frame_(\d+)\.vram$", p).group(1))
            stop, cyc = Avg(prom, open(p, "rb").read() + vrom).run()
            tstop, tcyc = table.get(n, ("?", -1))
            nd += 1
            if (stop, cyc) != (tstop, tcyc):
                nbad += 1
                if nbad <= 10:
                    print("frame_%04d  PROM %s %d   avg.c %s %d   *** MISMATCH" % (n, stop, cyc, tstop, tcyc))
        print("%s: %d lists, %d mismatches" % (d, nd, nbad))
        total += nd
        bad += nbad
    print("%d lists, %d mismatches: avg.c's draw time is %s" % (
        total, bad, "CONFIRMED against MAME's state machine on the real PROM." if not bad and total
        else "NOT confirmed."))
    return 0 if not bad and total else 1


if __name__ == "__main__":
    sys.exit(main())
