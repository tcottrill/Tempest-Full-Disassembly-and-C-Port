"""Decode Tempest's CAM enemy-motion scripts (ALWELG.MAC 'PLAY - INVADER CAM
TABLES') into readable pseudo-ops with Atari's opcode names.

The byte code: each instruction is one opcode byte (an even index into the
TABJSR handler table: VEXIT=0, VSLOOP=2 ...), optionally followed by one
operand byte:
  CAMA2I ops (VSLOOP, VSLOPB)          operand = immediate value / zero-page address
  CAMA2F ops (VSETPC, VELOOP, VBR0PC)  operand = target-CAM-1 (the dispatcher's
                                       INC CAMPC after every op lands on target)
The interpreter is MOVINV: per enemy it runs ops from INVCAM(X) until VEXIT
clears EXICAM, i.e. VEXIT ends that enemy's turn for this frame.

Addresses are located in the rev-3 image (not assumed): TABJSR from the JSRCAM
dispatcher bytes, CAM from the MOVINV 'LDA CAM,Y', and the source statements
are locked onto the ROM byte by byte. Writes build/cam_scripts.json.
"""
import json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import avg, vromsrc

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "build", "cam_scripts.json")

DOC = {
    "VEXIT":  "End this enemy's processing for the frame (JEXIT clears EXICAM)",
    "VSLOOP": "Set loop counter INVLOO(X) = operand",
    "VSKIP0": "If CAMSTA = 0, skip the next 2-byte instruction",
    "VSETPC": "Go to label",
    "VELOOP": "Decrement INVLOO(X); if not zero go to label, else continue",
    "VNOOP":  "No operation",
    "VSMOVE": "Move one step along the lane (up, or down when INVAC2 says so); at the top convert to chaser (TOPPER)",
    "VSTRAI": "Trailer (spiker) processing: lay/extend the spike on this line; CAMSTA=0 converts to carrier",
    "VSLOPB": "Set loop counter INVLOO(X) = contents of zero-page operand",
    "VJUMPS": "Start a jump (flip) to the adjacent lane in the current rotation direction",
    "VJUMPM": "Continue the jump one angle step; CAMSTA = 0 when the flip is finished",
    "VCHROT": "Reverse the jump (rotation) direction (INVAC1 ^= INVROT)",
    "VKITST": "Top chaser: kill the player if on the same lane legs (INIPSQ)",
    "VBR0PC": "If CAMSTA = 0 go to label",
    "VELTST": "CAMSTA = 0 if the enemy is on an enemy (spike) line, else 1",
    "VSFUSE": "Fuseball up/down motion along the lane (JFUSEUP)",
    "VFUSKI": "Fuseball kills the player if at the same height and lane",
    "VSPUMO": "Pulsar move (faster outside the power zone; reverses at PULPOT)",
    "VCHPLA": "Set jump direction the shortest way toward the player",
    "VCHKPU": "CAMSTA = $80 if the pulsars are pulsing now or within 4 frames, else 0",
}
SCRIPT_DOC = {
    "TRALUP": "Trailer (spiker) moving up, laying a spike; converts to a carrier",
    "NOJUMP": "Move up, never flip",
    "MOVJMP": "Move up 8 frames, then flip once, repeat",
    "SPIRAL": "Smooth upward spiral: flip continuously while moving up",
    "SPIRCH": "Spiral, changing flip direction after 2 then 3 flips",
    "TOPPER": "Chaser on the rim: wait 4 frames testing for a kill, then flip toward the player (WTTFRA steps/frame)",
    "COWJM2": "Flip and move on open lanes, just move up while on an enemy (spike) line",
    "FUSEUP": "Fuseball moving up/down a lane, killing the player on contact",
    "FUSELR": "Fuseball moving left/right between lanes (3-frame delay per step)",
    "PULSCH": "Pulsar chasing the player: move PUCHDE frames, wait out a pulse, flip toward player",
    "AVOIDR": "Avoider flipper: flip away from the player while moving, then move up 4 frames",
}


def parse_source():
    lines = vromsrc.read_mac("ALWELG.MAC")
    # opcode table
    ops = {}
    for ln in lines:
        m = re.match(r"^\s*(CAMAC|CAMA2I|CAMA2F)\s+(\w+),(\w+),([0-9A-F]+)", ln)
        if m:
            ops[int(m.group(4), 16)] = dict(kind=m.group(1), handler=m.group(2), name=m.group(3))
    # script statements
    start = next(i for i, ln in enumerate(lines) if re.match(r"^CAM:", ln))
    stmts, pend, comments = [], [], []
    names = {o["name"]: code for code, o in ops.items()}
    for i in range(start + 1, len(lines)):
        code, _, com = lines[i].partition(";")
        body = code.strip()
        if not body:
            if com.strip():
                comments.append(com.strip())
            continue
        m = re.match(r"^(\w+):\s*(.*)$", body)
        if m:
            pend.append(m.group(1))
            body = m.group(2).strip()
            if not body:
                if com.strip():
                    comments.append(com.strip())
                continue
        parts = body.split(None, 1)
        if parts[0] not in names:
            break
        stmts.append(dict(labels=pend, op=parts[0], arg=parts[1].strip() if len(parts) > 1 else None,
                          comment=com.strip(), block_comment=comments, line=i + 1))
        pend, comments = [], []
    externals = []
    for ln in lines:
        for nm in re.findall(r"(\w+)-CAM\b", ln.split(";")[0]):
            if nm not in ("...Z",) and nm not in externals:
                externals.append(nm)
    return ops, stmts, externals


def find(mem, pat, lo=0x9000, hi=0xE000):
    pat = bytes(pat)
    return [i for i in range(lo, hi) if mem[i:i + len(pat)] == pat]


def decode(mem=None):
    mem = mem or avg.load_image()
    ops, stmts, externals = parse_source()
    # locate TABJSR via JSRCAM: TAY / LDA TABJSR+1,Y / PHA / LDA TABJSR,Y / PHA / RTS
    tabjsr = None
    for i in range(0x9000, 0xA8B0):
        if mem[i] == 0xA8 and mem[i + 1] == 0xB9 and mem[i + 4] == 0x48 and \
                mem[i + 5] == 0xB9 and mem[i + 8] == 0x48 and mem[i + 9] == 0x60:
            t = mem[i + 6] | mem[i + 7] << 8
            # the CAM table has 20 handlers; JNOOP (index 5) follows JEXIT closely
            w = [avg.word(mem, t + 2 * k) + 1 for k in range(20)]
            if 0 < w[5] - w[0] < 8:
                tabjsr, jsrcam = t, i
    assert tabjsr, "TABJSR not found"
    # the CAM base: first statement bytes of the source, confirmed by 'LDA CAM,Y'
    first = [int([c for c, o in ops.items() if o["name"] == s["op"]][0]) for s in stmts[:3]]
    cands = [a for a in find(mem, first) if find(mem, [0xB9, a & 0xFF, a >> 8])]
    assert len(cands) == 1, cands
    cam = cands[0]
    code_of = {o["name"]: c for c, o in ops.items()}
    # lock statements
    pc, labels, errors = cam, {}, []
    for s in stmts:
        c = code_of[s["op"]]
        size = 1 if ops[c]["kind"] == "CAMAC" else 2
        if mem[pc] != c:
            errors.append("line %d: %s expected $%02X, ROM $%02X at $%04X" % (s["line"], s["op"], c, mem[pc], pc))
        s["addr"], s["size"], s["code"] = pc, size, c
        for lb in s["labels"]:
            labels[lb] = pc
        pc += size
    end = pc
    for s in stmts:
        if s["size"] == 2:
            b = mem[s["addr"] + 1]
            kind = ops[s["code"]]["kind"]
            if kind == "CAMA2F":
                tgt = cam + ((b + 1) & 0xFF)
                s["target"] = tgt
                s["target_label"] = s["arg"]
                if labels.get(s["arg"]) != tgt:
                    errors.append("line %d: %s %s -> $%04X but label is $%04X"
                                  % (s["line"], s["op"], s["arg"], tgt, labels.get(s["arg"], -1)))
                s["operand"] = s["arg"]
            else:
                s["value"] = b
                arg = s["arg"].rstrip(".")
                if s["op"] == "VSLOPB":
                    s["operand"] = "%s ($%02X)" % (s["arg"], b)
                else:
                    s["operand"] = "%d" % b
                    val = int(arg, 10 if s["arg"].endswith(".") else 16)
                    if val != b:
                        errors.append("line %d: operand %s != ROM %d" % (s["line"], s["arg"], b))
    # handler addresses
    handlers = []
    for k in range(len(ops)):
        code = 2 * k
        o = ops[code]
        handlers.append(dict(code=code, name=o["name"], handler=o["handler"], macro=o["kind"],
                             operand={"CAMAC": None, "CAMA2I": "value", "CAMA2F": "label"}[o["kind"]],
                             handler_addr="$%04X" % (avg.word(mem, tabjsr + code) + 1),
                             doc=DOC.get(o["name"], "")))
    # scripts
    scripts = []
    entry_names = [n for n in externals if n in labels]
    starts = sorted((labels[n], n) for n in entry_names)
    for idx, (a, n) in enumerate(starts):
        nxt = starts[idx + 1][0] if idx + 1 < len(starts) else end
        body = []
        for s in stmts:
            if a <= s["addr"] < nxt:
                body.append(dict(
                    addr="$%04X" % s["addr"], offset=s["addr"] - cam,
                    bytes=" ".join("%02X" % mem[s["addr"] + k] for k in range(s["size"])),
                    labels=s["labels"], op=s["op"], operand=s.get("operand"),
                    target=("$%04X" % s["target"]) if "target" in s else None,
                    target_offset=(s["target"] - cam) if "target" in s else None,
                    value=s.get("value"), comment=s["comment"], line=s["line"]))
        scripts.append(dict(name=n, addr="$%04X" % a, offset=a - cam, size=nxt - a,
                            doc=SCRIPT_DOC.get(n, ""), ops=body))
    # users of the scripts
    users = {}
    camwav = find(mem, [6, 1, 99, labels["NOJUMP"] - cam, labels["MOVJMP"] - cam], 0x9000, 0xA8B0)
    if camwav:
        a = camwav[0]
        inv = {v - cam: k for k, v in labels.items() if k in entry_names}
        users["CAMWAV"] = dict(addr="$%04X" % a, format="TZANDF,1,99 then 16 bytes: flipper CAM per (wave-1)&15",
                               entries=[inv.get(mem[a + 3 + k], "$%02X" % mem[a + 3 + k]) for k in range(16)])
    tn = find(mem, [labels["NOJUMP"] - cam, labels["PULSCH"] - cam, labels["NOJUMP"] - cam,
                    labels["TRALUP"] - cam, labels["FUSEUP"] - cam], 0x9000, 0xA8B0)
    if tn:
        users["TNEWCAM"] = dict(addr="$%04X" % tn[0],
                                format="initial CAM per enemy type (flipper, pulsar, tanker, spiker, fuseball)",
                                entries=["NOJUMP", "PULSCH", "NOJUMP", "TRALUP", "FUSEUP"])
    users["immediates"] = ["CHASER: LDA #TOPPER-CAM-1 / STA CAMPC", "fuse: LDA #FUSELR-CAM / STA CAMPC"]
    return dict(
        source="tempest-main/ALWELG.MAC 'PLAY - INVADER CAM TABLES' (rev 3 image)",
        cam_addr="$%04X" % cam, cam_end="$%04X" % end, tabjsr_addr="$%04X" % tabjsr,
        jsrcam_addr="$%04X" % jsrcam,
        interpreter="MOVINV: per active enemy, CAMPC=INVCAM(X); loop {op=CAM[CAMPC]; JSRCAM; CAMPC++} until EXICAM=0; INVCAM(X)=CAMPC",
        branch_encoding="operand byte = target-CAM-1",
        opcodes=handlers, scripts=scripts, labels={k: "$%04X" % v for k, v in labels.items()},
        users=users, errors=errors)


def text(d):
    out = []
    for sc in d["scripts"]:
        out.append("%s  (%s, CAM+$%02X)  %s" % (sc["name"], sc["addr"], sc["offset"], sc["doc"]))
        for o in sc["ops"]:
            lab = ",".join(o["labels"])
            out.append("  %-7s %-7s %-7s %-14s %s" % (o["addr"], lab + (":" if lab else ""), o["op"],
                                                    o["operand"] or "", o["comment"]))
    return "\n".join(out)


if __name__ == "__main__":
    d = decode()
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    json.dump(d, open(OUT, "w"), indent=1)
    print(text(d))
    print("CAM %s-%s  TABJSR %s  scripts %d  ops %d  errors %d"
          % (d["cam_addr"], d["cam_end"], d["tabjsr_addr"], len(d["scripts"]),
             sum(len(s["ops"]) for s in d["scripts"]), len(d["errors"])))
    for e in d["errors"]:
        print("  !!", e)
    print("wrote", OUT)
