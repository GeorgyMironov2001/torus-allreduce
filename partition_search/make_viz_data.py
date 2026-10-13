#!/usr/bin/env python3
"""Collect best schemes as one JSON object per line (array elements for the visualization canvas).
Optional arguments restrict the output to the given tori, e.g. `make_viz_data.py 12x12 16x16`.

Per round r, every vertex gets the index of its part inside its depth-r set, numbered by first
appearance in row-major order, encoded as one character.
"""
import glob
import json
import os
import re
import sys

ALPHA = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
HERE = os.path.dirname(os.path.abspath(__file__))
# recursive halving, high bits first (partition_sa --load base/halving_hi*.txt --iters 0)
HALVING = {("8x8", 6): 512}
# logs of runs made before --init mixed existed (partition_sa --R 2 --enum-digits)
DIGIT_OVERRIDE = {("8x8", 2): 2048}


def compact_codes(codes, R):
    out = []
    for r in range(R):
        rank, s = {}, []
        for c in codes:
            grp = rank.setdefault(tuple(c[:r]), {})
            if c[r] not in grp:
                grp[c[r]] = len(grp)
            if grp[c[r]] >= len(ALPHA):
                raise ValueError("too many parts for compact encoding")
            s.append(ALPHA[grp[c[r]]])
        out.append("".join(s))
    return out


def grep(path, pattern):
    if not os.path.exists(path):
        return None
    with open(path) as f:
        m = re.search(pattern, f.read())
    return m


def lattice(tag, R):
    m = grep(os.path.join(HERE, f"results_viz/lattice_{tag}.txt"), rf"R={R}: OPT_lattice = (\d+)")
    return int(m.group(1)) if m else None


def lower_bound(log, R):
    m = grep(log, r"lower bound for this class, R=1\.\.\d+:([ \d]+)")
    return int(m.group(1).split()[R - 1]) if m else None


def digit_from_log(log):
    """(value, blocks_only) of the digit enumeration printed in a partition_sa log."""
    m = grep(log, r"(block )?digit schemes evaluated: \d+, best L = (\d+)")
    return (int(m.group(2)), bool(m.group(1))) if m else (None, False)


def digit_small(tag, R):
    m = grep(os.path.join(HERE, "results_viz/digits_small.txt"), rf"{tag} R={R} best L = (\d+)")
    return int(m.group(1)) if m else None


def chains_hit(log, L):
    if not os.path.exists(log):
        return None
    vals = [int(x) for x in re.findall(r"^chain .*L = (\d+)", open(log).read(), re.M)]
    return f"{sum(v == L for v in vals)}/{len(vals)}" if vals else None


def collect(only=()):
    entries = []
    for tag in ["3x3", "4x3", "5x3", "4x4"]:
        for R in (2, 3, 4):
            entries.append((tag, R, f"results_viz/{tag}_R{R}.json", f"results_viz/{tag}_R{R}.log", "exact"))
    for R in range(2, 7):
        entries.append(("6x6", R, f"results_viz/6x6_R{R}.json", f"results_viz/6x6_R{R}.log", "sa"))
    for R in range(2, 6):
        entries.append(("8x4", R, f"results_viz/8x4_R{R}.json", f"results_viz/8x4_R{R}.log", "sa"))
    for R in range(2, 9):
        if R == 2:
            j, l = "results/best_8x8_R2.json", "results/log_8x8_R2.txt"
        elif R == 5:
            j, l = "results2/best_8x8_R5_long.json", "results2/log_8x8_R5_long.txt"
        else:
            j, l = f"results2/best_8x8_R{R}.json", f"results2/log_8x8_R{R}.txt"
        entries.append(("8x8", R, j, l, "sa"))
    for tag in ["12x12", "16x16"]:
        for R in range(2, 9):
            entries.append((tag, R, f"results_viz/{tag}_R{R}.json", f"results_viz/{tag}_R{R}.log", "sa"))

    only = set(only)
    out = []
    for tag, R, jpath, lpath, src in entries:
        if only and tag not in only:
            continue
        # a second, block-seeded pass may exist next to the first one
        runs = [(jpath, lpath), (jpath.replace(".json", "_b.json"), lpath.replace(".log", "_b.log"))]
        runs = [(os.path.join(HERE, j), os.path.join(HERE, l)) for j, l in runs]
        runs = [(json.load(open(j)), l) for j, l in runs if os.path.exists(j)]
        if not runs:
            print(f"missing {jpath}", file=sys.stderr)
            continue
        d, lpath = min(runs, key=lambda x: x[0]["cost"])
        if src == "exact":
            digit, blocks = digit_small(tag, R), False
        else:
            found = [digit_from_log(l) for _, l in runs]
            full = [v for v, b in found if v is not None and not b]
            part = [v for v, b in found if v is not None and b]
            digit, blocks = (min(full), False) if full else ((min(part), True) if part else (None, False))
            if digit is None and (tag, R) in DIGIT_OVERRIDE:
                digit = DIGIT_OVERRIDE[(tag, R)]
        e = {
            "t": tag, "nx": d["nx"], "ny": d["ny"], "R": R, "L": d["cost"], "rc": d["round_costs"],
            "src": src, "codes": compact_codes(d["codes"], R),
            "digit": digit, "lat": lattice(tag, R), "lb": lower_bound(lpath, R),
        }
        if blocks:
            e["digitBlocks"] = True
        if src == "sa":
            e["hit"] = chains_hit(lpath, d["cost"])
        if (tag, R) in HALVING:
            e["halving"] = HALVING[(tag, R)]
        out.append(e)
    return out


def main():
    for e in collect(sys.argv[1:]):
        print("  " + json.dumps(e, separators=(",", ":")) + ",")


if __name__ == "__main__":
    main()
