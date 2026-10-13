#!/usr/bin/env python3
"""Reference hierarchies for partition_sa --load (plain text, one line per vertex v = y*nx + x)."""
import argparse


def bits_scheme(n, order):
    """order: list of (axis, bit); round r splits by that bit of that coordinate."""
    rows = []
    for y in range(n):
        for x in range(n):
            c = (x, y)
            rows.append([(c[a] >> b) & 1 for a, b in order])
    return rows


def digits_scheme(n, order):
    """order: list of (axis, radix, weight); label = (coord // weight) % radix."""
    rows = []
    for y in range(n):
        for x in range(n):
            c = (x, y)
            rows.append([(c[a] // w) % q for a, q, w in order])
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, default=8)
    ap.add_argument("--kind", required=True,
                    choices=["halving_hi", "halving_lo", "rows_cols", "r3_4x4x4", "r3_blocks"])
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    n = a.n
    k = n.bit_length() - 1
    if a.kind == "halving_hi":
        order = [(ax, b) for b in reversed(range(k)) for ax in (0, 1)]
        rows = bits_scheme(n, order)
    elif a.kind == "halving_lo":
        order = [(ax, b) for b in range(k) for ax in (0, 1)]
        rows = bits_scheme(n, order)
    elif a.kind == "rows_cols":
        rows = digits_scheme(n, [(0, n, 1), (1, n, 1)])
    elif a.kind == "r3_4x4x4":
        # round 1: x // 2 (4 column bands), round 2: y // 2, round 3: 2x2 all-to-all
        rows = []
        for y in range(n):
            for x in range(n):
                rows.append([x // 2, y // 2, (x % 2) * 2 + (y % 2)])
    elif a.kind == "r3_blocks":
        # round 1: 2x2 sublattice cosets, round 2: x // 4 * 2 + y // 4 inside, round 3: rest
        rows = []
        for y in range(n):
            for x in range(n):
                rows.append([(x % 2) * 2 + (y % 2), (x // 4) * 2 + (y // 4), ((x // 2) % 2) * 2 + (y // 2) % 2])
    with open(a.out, "w") as f:
        for r in rows:
            f.write(" ".join(map(str, r)) + "\n")


if __name__ == "__main__":
    main()
