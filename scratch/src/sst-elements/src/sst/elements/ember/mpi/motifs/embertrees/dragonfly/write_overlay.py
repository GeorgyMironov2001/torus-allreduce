#!/usr/bin/env python3
"""Write OverlayAllreduce schedule for balanced Dragonfly (h=p, a=2p, g=ah+1)."""
import argparse
import itertools
import json
from pathlib import Path


def rank(grp, rtr, loc, *, p, a):
    return grp * (a * p) + rtr * p + loc


def tree_edges(root, *, p, a, g):
    j, r, i = root
    edges = []
    for grp, rtr, loc in itertools.product(range(g), range(a), range(p)):
        if loc != i:
            edges.append((rank(grp, rtr, loc, p=p, a=a), rank(grp, rtr, i, p=p, a=a), 0))
    for grp, rtr in itertools.product(range(g), range(a)):
        if rtr != r:
            edges.append((rank(grp, rtr, i, p=p, a=a), rank(grp, r, i, p=p, a=a), 1))
    for grp in range(g):
        if grp != j:
            edges.append((rank(grp, r, i, p=p, a=a), rank(j, r, i, p=p, a=a), 2))
    return edges


def build_schedule(p=2):
    a, h, g = 2 * p, p, 2 * p * p + 1
    k = 3
    n = p * a * g
    trees = []
    n_trees = p * a * g
    share = 1.0 / n_trees
    for tid, root in enumerate(itertools.product(range(g), range(a), range(p))):
        trees.append(
            {
                "id": tid,
                "root": rank(*root, p=p, a=a),
                "share": share,
                "edges": [
                    {
                        "from": int(u),
                        "to": int(v),
                        "rsStage": int(st),
                        "agStage": int(k - 1 - st),
                        "route_class": -1,
                    }
                    for u, v, st in tree_edges(root, p=p, a=a, g=g)
                ],
            }
        )
    return {
        "version": 1,
        "n": n,
        "mode": "reduce_bcast",
        "chunking": "equal_by_tree",
        "p": p,
        "a": a,
        "h": h,
        "g": g,
        "k": k,
        "stage_policy": "dragonfly3",
        "trees": trees,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-p", type=int, default=2)
    ap.add_argument("-o", type=Path, default=None)
    args = ap.parse_args()
    sched = build_schedule(args.p)
    out = args.o or Path(__file__).with_name(
        f"overlay_df_p{args.p}_a{2*args.p}_g{2*args.p*args.p+1}_k3.json"
    )
    out.write_text(json.dumps(sched, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {out}: n={sched['n']} trees={len(sched['trees'])}")


if __name__ == "__main__":
    main()
