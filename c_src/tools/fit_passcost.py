"""fit_passcost.py - fit app_loop.c's pass-cost table (M8) from tests\\passcost.exe.

    tests\\passcost.exe --frames 5000 --no-dumps > pc_attract.txt
    tests\\passcost.exe --frames 3000 --no-dumps --script tests\\scenarios\\coin_start.txt > pc_coin_start.txt
    (likewise high_score, superzapper, fuseball_pulsar)
    python tools\\fit_passcost.py pc_*.txt

Per (QSTATE, QDSTATE) at the pass start with >= 30 passes, least squares
work = a + b_r*reads + b_w*writes + b_ops*avg_ops (+ a global fit), then a
replay of every recorded pass through app_loop.c's timeline (pass_charge +
hw_wait_frame) with the measured and with the fitted work, printing the
IRQs-per-pass histograms against the oracle's, and the C table rows.
"""
import collections, math, sys

P = 6144
H_NAT = 61.0          # app_loop.c TP_IRQ_SEAM_CYC


def load(paths):
    rows = []
    for fn in paths:
        for line in open(fn):
            p = line.split()
            if len(p) < 13 or p[0] != 'PASSCOST' or p[1] == 'frame':
                continue
            rows.append(dict(src=fn, irqs=int(p[2]), work=int(p[3]), handler=int(p[4]),
                             io_r=int(p[6]), io_w=int(p[7]), qs=int(p[8]), qd=int(p[9]), ops=int(p[10])))
    return rows


def lsq(xs, ys):
    n = len(xs[0]) + 1
    A = [[0.0] * n for _ in range(n)]
    B = [0.0] * n
    for x, y in zip(xs, ys):
        v = [1.0] + list(x)
        for i in range(n):
            B[i] += v[i] * y
            for j in range(n):
                A[i][j] += v[i] * v[j]
    for i in range(n):
        piv = max(range(i, n), key=lambda k: abs(A[k][i]))
        A[i], A[piv] = A[piv], A[i]
        B[i], B[piv] = B[piv], B[i]
        if abs(A[i][i]) < 1e-9:
            continue
        for k in range(n):
            if k != i:
                f = A[k][i] / A[i][i]
                for j in range(n):
                    A[k][j] -= f * A[i][j]
                B[k] -= f * B[i]
    return [B[i] / A[i][i] if abs(A[i][i]) > 1e-9 else 0.0 for i in range(n)]


def fit(rs):
    c = lsq([(r['io_r'], r['io_w'], r['ops']) for r in rs], [r['work'] for r in rs])
    h = sum(r['handler'] for r in rs) / sum(r['irqs'] for r in rs)
    res = [r['work'] - (c[0] + c[1] * r['io_r'] + c[2] * r['io_w'] + c[3] * r['ops']) for r in rs]
    sd = math.sqrt(sum(x * x for x in res) / len(res))
    return c, h, sd


# The residual spread (M8 part 2).  IRQs per pass is a convex function of the
# work (max(9, ...)), so a fit that is right ON AVERAGE under-charges: the
# passes whose real work crosses a gate are averaged away (the attract PLAY
# demo came out 9.02 IRQs per pass against the oracle's 9.09, NEWV2 10.06
# against 10.71).  app_loop.c adds the state's residual sd times a fixed
# 16-step sequence of standard-normal quantiles (Z16, bit-reversed order) to
# the fitted work, which restores the spread deterministically.
Z16 = None


def z16():
    global Z16
    if Z16 is None:
        from statistics import NormalDist
        q = [NormalDist().inv_cdf((i + 0.5) / 16) for i in range(16)]
        order = [int('{:04b}'.format(i)[::-1], 2) for i in range(16)]
        Z16 = [q[o] for o in order]
    return Z16


def replay(rs, tab, glob_, measured, spread=True, per_state=None):
    mach, nxt, hist, k = H_NAT, P, collections.Counter(), 0
    for r in rs:
        if measured:
            w, h = r['work'], r['handler'] / r['irqs']
        else:
            c, h, sd = tab.get((r['qs'], r['qd']), glob_)
            w = c[0] + c[1] * r['io_r'] + c[2] * r['io_w'] + c[3] * r['ops']
            if spread:
                w += sd * z16()[k & 15]
                k += 1
            w = min(max(w, 0.0), 120000.0)
        cio = 4 * r['io_r'] + 8 * r['io_w']
        mach += cio + max(w * P / (P - max(h - H_NAT, 0.0)) - cio, 0.0)
        fr = 0
        while fr < 9 or nxt <= mach:
            mach = max(mach, nxt) + H_NAT
            nxt += P
            fr += 1
        hist[fr] += 1
        if per_state is not None:
            per_state[(r['qs'], r['qd'])].append(fr)
    return hist


def per_state_replay(rows, tab, glob_):
    st = collections.defaultdict(list)
    for src in sorted(set(r['src'] for r in rows)):
        replay([r for r in rows if r['src'] == src], tab, glob_, False, spread=True, per_state=st)
    return {k: sum(v) / len(v) for k, v in st.items()}


def calibrate(rows, groups, tab, glob_):
    """Per-state bias on `a` (cycles) so the replayed IRQs per pass of every
    fitted state equals the oracle's over the same passes (bisection, a few
    sweeps because states share the timeline's phase)."""
    target = {k: sum(r['irqs'] for r in rs) / len(rs) for k, rs in groups.items()}
    for sweep in range(3):
        for k in sorted(tab, key=lambda k: -len(groups[k])):
            if target[k] <= 9.0 + 1e-9:
                continue                      # never past the gate: nothing to charge
            lo, hi = -20000.0, 20000.0
            c0, h, sd = tab[k]
            for _ in range(22):
                mid = (lo + hi) / 2
                tab[k] = ([c0[0] + mid] + list(c0[1:]), h, sd)
                got = per_state_replay(rows, tab, glob_)[k]
                if got < target[k]:
                    lo = mid
                else:
                    hi = mid
            tab[k] = ([c0[0] + (lo + hi) / 2] + list(c0[1:]), h, sd)
    return tab


def main():
    rows = load(sys.argv[1:])
    groups = collections.defaultdict(list)
    for r in rows:
        groups[(r['qs'], r['qd'])].append(r)
    tab = {k: fit(rs) for k, rs in groups.items() if len(rs) >= 30}
    glob_ = fit(rows)
    raw = dict(tab)
    tab = calibrate(rows, groups, tab, glob_)
    for k in sorted(tab):
        if tab[k][0][0] != raw[k][0][0]:
            print('calibrated $%02X $%02X: a %+.1f cycles' % (k[0], k[1], tab[k][0][0] - raw[k][0][0]))
    mean = lambda hh: sum(k * v for k, v in hh.items()) / sum(hh.values())
    st_o, st_f0, st_f1 = (collections.defaultdict(list) for _ in range(3))
    for src in sys.argv[1:]:
        rs = [r for r in rows if r['src'] == src]
        if not rs:
            continue
        for r in rs:
            st_o[(r['qs'], r['qd'])].append(r['irqs'])
        o = collections.Counter(r['irqs'] for r in rs)
        m = replay(rs, tab, glob_, True)
        f0 = replay(rs, tab, glob_, False, spread=False, per_state=st_f0)
        f = replay(rs, tab, glob_, False, spread=True, per_state=st_f1)
        print('%s: %d passes  oracle %.3f  measured-work %.3f  fitted %.3f  fitted+spread %.3f' %
              (src, len(rs), mean(o), mean(m), mean(f0), mean(f)))
        print('   oracle %s\n   fitted+spread %s' % (sorted(o.items()), sorted(f.items())))
    avg = lambda v: sum(v) / len(v)
    print('\nper state: QS QD passes  oracle  fitted (diff%%)  fitted+spread (diff%%)')
    for k in sorted(st_o, key=lambda k: -len(st_o[k])):
        if len(st_o[k]) < 20:
            continue
        a, b, c_ = avg(st_o[k]), avg(st_f0[k]), avg(st_f1[k])
        print('   $%02X $%02X %6d  %6.3f  %6.3f (%+5.2f)  %6.3f (%+5.2f)' %
              (k[0], k[1], len(st_o[k]), a, b, (b / a - 1) * 100, c_, (c_ / a - 1) * 100))
    print('\n/* QSTATE QDSTATE      a      b_r      b_w   b_ops  handler  resid_sd */')
    for k in sorted(tab):
        c, h, sd = tab[k]
        print('    { 0x%02X, 0x%02X, %8.1f, %7.2f, %7.2f, %6.2f, %6.1f, %7.1f },  /* %d passes */' %
              (k[0], k[1], c[0], c[1], c[2], c[3], h, sd, len(groups[k])))
    c, h, sd = glob_
    print('global { 0, 0, %.1f, %.2f, %.2f, %.2f, %.1f, %.1f }  /* %d passes */' % (c[0], c[1], c[2], c[3], h, sd, len(rows)))
    print('Z16 = { %s }' % ', '.join('%.4f' % z for z in z16()))


if __name__ == '__main__':
    main()
