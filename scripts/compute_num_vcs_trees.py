#!/usr/bin/env python3
"""Compute NUM_VCS_TREES_{N}x{N}: num_vc = max_vc+1 on Trees_k RS+AG flows.

Uses turn+dateline bump (matches torus_trees.cc / ecdg_check_torus_trees.py).
"""
from __future__ import annotations

import argparse
import re
import sys
import time
from collections import Counter
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ecdg_check_torus_trees import (  # noqa: E402
    TorusTrees,
    bump_vcs,
    load_json,
    parse_edges,
    _coord,
    _rank,
    _shift_rank,
)

ROOT = Path(__file__).resolve().parents[1] / (
    "scratch/src/sst-elements/src/sst/elements/ember/mpi/motifs"
)
TABLES = ROOT / "emberroutingtables"
TREES = ROOT / "embertrees"


def max_vc_over_shifts(torus: TorusTrees, hops_rel, n: int) -> int:
    """Max turn+dateline violation count over all translations of hops_rel."""
    if not hops_rel:
        return 0
    turns = 0
    dl0, dl1 = Counter(), Counter()
    prev = None
    for u, v in hops_rel:
        cu, cv = torus.loc(u), torus.loc(v)
        d = next(i for i in range(2) if cu[i] != cv[i])
        if prev is None:
            prev = d
            continue
        if d != prev:
            turns += 1
        else:
            need = (-cu[d]) % n
            (dl0 if d == 0 else dl1)[need] += 1
        prev = d
    return turns + (max(dl0.values()) if dl0 else 0) + (max(dl1.values()) if dl1 else 0)


def enumerate_num_vc(n: int, k: int) -> tuple[int, int]:
    table, paths = load_json(str(TABLES / f"ember_bdmst_routing_table_{n}_{k}.json"))
    header = TREES / f"emberbdmstrees_{n}_{n}d{k}.h"
    text = header.read_text()
    tree_c = int(re.search(rf"bdms_center_{n}x{n}_{k}\s*=\s*(\d+)", text).group(1))
    tree_vars = [f"bdms_edges_{n}x{n}_{k}"] + [
        f"bdms_edges_{n}x{n}_{k}_rotate{i}" for i in (1, 2, 3)
    ]
    torus = TorusTrees([n, n])
    main = _rank([(n - 1) // 2] * 2, n)
    oc_tree = _coord(tree_c, n)
    oc_main = _coord(main, n)
    max_vc = 0
    nn = n * n

    def bump_max(hops):
        nonlocal max_vc
        if hops:
            max_vc = max(max_vc, max(bump_vcs(torus, hops, "turn")))

    def shifts_max(hops):
        nonlocal max_vc
        max_vc = max(max_vc, max_vc_over_shifts(torus, hops, n))

    # JSON paths (RS), all translations
    for p in paths:
        shifts_max(torus.walk_topology(p[0], p[-1], p))

    directed = set()
    for var in tree_vars:
        directed.update(parse_edges(str(header), var))

    for ef, et in directed:
        # Greedy envelope over all placements (table replaces only few centers)
        shifts_max(torus.walk_topology(ef, et, None))
        shifts_max(torus.walk_topology(et, ef, None))

        for center in range(nn):
            cc = _coord(center, n)
            g0 = _shift_rank(ef, cc, oc_tree, n)
            g1 = _shift_rank(et, cc, oc_tree, n)
            fs = _shift_rank(g0, cc, oc_main, n)
            ts = _shift_rank(g1, cc, oc_main, n)
            if (fs, ts) in table:
                p = [
                    _shift_rank(node, cc, oc_main, n, back=True)
                    for node in paths[table[(fs, ts)]]
                ]
                bump_max(torus.walk_topology(g0, g1, p))
            if (ts, fs) in table:
                q = [
                    _shift_rank(node, cc, oc_main, n, back=True)
                    for node in paths[table[(ts, fs)]]
                ]
                bump_max(torus.walk_topology(g1, g0, list(reversed(q))))

    return k, max_vc + 1


def _worker(args):
    n, k = args
    t0 = time.time()
    k, num_vc = enumerate_num_vc(n, k)
    return k, num_vc, time.time() - t0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, required=True, choices=(16, 32, 64))
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--k-min", type=int, default=1)
    ap.add_argument("--k-max", type=int, default=None)
    args = ap.parse_args()
    k_max = args.k_max or args.n
    ks = list(range(args.k_min, k_max + 1))
    results = {}
    t0 = time.time()
    with ProcessPoolExecutor(max_workers=args.jobs) as ex:
        futs = {ex.submit(_worker, (args.n, k)): k for k in ks}
        for fut in as_completed(futs):
            k, num_vc, dt = fut.result()
            results[k] = num_vc
            print(f"n={args.n} k={k}: num_vc={num_vc} ({dt:.1f}s)", flush=True)
    print(f"done in {time.time() - t0:.1f}s")
    print(f"NUM_VCS_TREES_{args.n}x{args.n} = {{")
    for k in sorted(results):
        print(f"    {k}: {results[k]},")
    print("}")


if __name__ == "__main__":
    main()
