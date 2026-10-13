#!/usr/bin/env python3
"""Exact optimum over lattice (coset) hierarchies on the torus Z_nx x Z_ny.

Depth-r sets are cosets of a subgroup H_r, G = H_0 >= H_1 >= ... >= H_R = {0}.
By translation invariance a round splitting cosets of H into cosets of K costs
    P * sum_{C in H/K, C != K} min_{c in C} |c|_torus.
"""
import argparse


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--nx", type=int, default=8)
    ap.add_argument("--ny", type=int, default=8)
    ap.add_argument("--R", type=int, default=8)
    ap.add_argument("--codes-prefix", default="", help="write codes of the optimum for every R to PREFIX_R<r>.txt")
    a = ap.parse_args()
    nx, ny = a.nx, a.ny
    P = nx * ny
    elems = [(x, y) for y in range(ny) for x in range(nx)]
    idx = {e: i for i, e in enumerate(elems)}

    def add(g, h):
        return ((g[0] + h[0]) % nx, (g[1] + h[1]) % ny)

    def norm(g):
        return min(g[0], nx - g[0]) + min(g[1], ny - g[1])

    def span(g1, g2):
        seen, frontier = {(0, 0)}, [(0, 0)]
        while frontier:
            nxt = []
            for e in frontier:
                for g in (g1, g2):
                    f = add(e, g)
                    if f not in seen:
                        seen.add(f)
                        nxt.append(f)
            frontier = nxt
        m = 0
        for e in seen:
            m |= 1 << idx[e]
        return m

    subs = {}
    for i, g1 in enumerate(elems):
        for g2 in elems[i:]:
            m = span(g1, g2)
            subs.setdefault(m, (g1, g2))
    masks = sorted(subs, key=lambda m: bin(m).count("1"))
    members = {m: [elems[k] for k in range(P) if m >> k & 1] for m in masks}
    print(f"torus {nx}x{ny}: {len(masks)} subgroups")

    def round_cost(H, K):
        seen, s = 0, 0
        for h in members[H]:
            if seen >> idx[h] & 1:
                continue
            coset = [add(h, k) for k in members[K]]
            for c in coset:
                seen |= 1 << idx[c]
            if h in members[K]:
                continue
            s += min(norm(c) for c in coset)
        return P * s

    full, zero = masks[-1], masks[0]
    cost = {}
    for H in masks:
        for K in masks:
            if K != H and K & ~H == 0:
                cost[(H, K)] = round_cost(H, K)

    INF = float("inf")
    f = [{H: (0 if H == zero else INF) for H in masks}]
    choice = [{}]
    for r in range(1, a.R + 1):
        fr, ch = {}, {}
        for H in masks:
            best, arg = f[r - 1][H], H
            for K in masks:
                c = cost.get((H, K))
                if c is not None and c + f[r - 1][K] < best:
                    best, arg = c + f[r - 1][K], K
            fr[H], ch[H] = best, arg
        f.append(fr)
        choice.append(ch)

    def chain(r):
        H, out = full, [full]
        for rr in range(r, 0, -1):
            H = choice[rr][H]
            out.append(H)
        return out

    for r in range(1, a.R + 1):
        c = chain(r)
        steps = []
        for H, K in zip(c, c[1:]):
            if H == K:
                steps.append("idle")
            else:
                g1, g2 = subs[K]
                steps.append(f"|H|={bin(H).count('1')}->{bin(K).count('1')} (K=<{g1},{g2}>, cost {cost[(H, K)]})")
        print(f"R={r}: OPT_lattice = {f[r][full]}")
        for s in steps:
            print("    " + s)
        if a.codes_prefix:
            coset_id = []
            for K in c:
                ids, lab = {}, [0] * P
                for e in elems:
                    rep = min(idx[add(e, k)] for k in members[K])
                    lab[idx[e]] = ids.setdefault(rep, len(ids))
                coset_id.append(lab)
            with open(f"{a.codes_prefix}_R{r}.txt", "w") as fo:
                for v in range(P):
                    fo.write(" ".join(str(coset_id[l + 1][v]) for l in range(r)) + "\n")


if __name__ == "__main__":
    main()
