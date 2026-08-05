#!/usr/bin/env python3
"""ECDG deadlock check for merlin torus_trees.

VC assignment matches torus_trees.cc turn+dateline bump:
  - axis turn → vc++
  - else leaving coord[axis]==0 after a network hop (not NIC) → vc++
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import DefaultDict, Dict, Iterator, List, Optional, Set, Tuple

Channel = Tuple[int, int, int]  # (src_router, dst_router, vc)


# --- torus_trees.cc routing -------------------------------------------------

class TorusTrees:
    def __init__(self, shape: List[int], width: Optional[List[int]] = None):
        self.d = len(shape)
        self.dim = list(shape)
        self.width = list(width or [1] * self.d)
        self.port_start, p = [], 0
        for w in self.width:
            self.port_start.append([p, p + w])
            p += 2 * w
        self.local_port = p

    def loc(self, rid: int) -> List[int]:
        out, r = [0] * self.d, rid
        for i in range(self.d - 1, 0, -1):
            div = 1
            for j in range(i):
                div *= self.dim[j]
            v, r = r // div, r % div
            out[self.d - i - 1] = v
        out[self.d - 1] = r
        return out

    def rid(self, loc: List[int]) -> int:
        res, f = 0, 1
        for i in range(self.d - 1, -1, -1):
            res += loc[i] * f
            f *= self.dim[i]
        return res

    def shift_path(self, at: int, path: List[int]) -> List[int]:
        cur, base = self.loc(at), self.loc(path[0])
        return [
            self.rid([(self.loc(n)[i] + cur[i] - base[i] + self.dim[i]) % self.dim[i] for i in range(self.d)])
            for n in path
        ]

    def _port_peer(self, router: int, port: int, out: bool) -> int:
        loc = self.loc(router)
        for d, (p0, p1) in enumerate(self.port_start):
            for dr, p in enumerate((p0, p1)):
                if p != port:
                    continue
                sc = self.d - d - 1
                step = 1 if (dr == 0) == out else -1
                loc[sc] = (loc[sc] + step) % self.dim[d]
                return self.rid(loc)
        raise ValueError(f"bad port {port}")

    def incoming_port(self, out_port: int) -> int:
        for p0, p1 in self.port_start:
            if out_port == p0:
                return p1
            if out_port == p1:
                return p0
        raise ValueError(f"bad port {out_port}")

    def route_packet(
        self,
        router: int,
        port_in: int,
        vc: int,
        route_path: List[int],
        hop_i: int,
        dest: int,
        prev_shift: int = -1,
        num_vcs: int = 5,
    ) -> Tuple[int, int, int, int]:
        """Match torus_trees.cc route_packet turn+dateline bump.

        Returns (port_out, vc, hop_i', prev_shift').
        Dateline bump only when port_in is a network port (not NIC injection).
        """
        if router == dest:
            return self.local_port, vc, hop_i, prev_shift
        cur = self.loc(router)
        if route_path and hop_i + 1 < len(route_path):
            nxt = self.loc(route_path[hop_i + 1])
        else:
            nxt = self.loc(dest)
        sc = next(i for i in range(self.d) if cur[i] != nxt[i])
        vn_base = (vc // num_vcs) * num_vcs
        vn_max = vn_base + num_vcs - 1
        if prev_shift >= 0 and prev_shift != sc:
            vc += 1
        elif cur[sc] == 0 and port_in < self.local_port:
            vc += 1
        if vc > vn_max:
            vc = vn_max
        rsc = self.d - sc - 1
        pos = nxt[sc] % self.dim[rsc] == (cur[sc] + 1) % self.dim[rsc]
        return (
            self.port_start[rsc][0 if pos else 1],
            vc,
            hop_i + (1 if route_path else 0),
            sc,
        )

    def walk(
        self,
        src: int,
        dst: int,
        path: Optional[List[int]] = None,
        num_vcs: int = 5,
    ) -> List[Channel]:
        """Return channel list for src->dst (source path or greedy if path is None)."""
        route = self.shift_path(src, path) if path else []
        hops, vc, port_in, r, hi, prev_shift = [], 0, self.local_port, src, 0, -1
        limit = sum(self.dim) + 2
        for _ in range(limit):
            if r == dst:
                break
            port_out, vc, hi, prev_shift = self.route_packet(
                r, port_in, vc, route, hi, dst, prev_shift, num_vcs
            )
            if route:
                nxt = route[hi] if hi < len(route) else dst
            else:
                nxt = self._port_peer(r, port_out, True)
            hops.append((r, nxt, vc))
            if nxt == dst:
                break
            r, port_in = nxt, self.incoming_port(port_out)
        else:
            raise RuntimeError(f"no route {src}->{dst}")
        return hops

    def walk_topology(self, src: int, dst: int, path: Optional[List[int]] = None) -> List[Tuple[int, int]]:
        return [(u, v) for u, v, _ in self.walk(src, dst, path)]


# --- Ember / Trees_4 flow enumeration ---------------------------------------

def _coord(rank: int, n: int, d: int = 2) -> List[int]:
    c, nodes, r = [0] * d, n**d, rank
    for i in range(d):
        nodes //= n
        c[i], r = r // nodes, r % nodes
    return c


def _rank(c: List[int], n: int, d: int = 2) -> int:
    res, f = 0, 1
    for i in range(d - 1, -1, -1):
        res += c[i] * f
        f *= n
    return res


def _shift_rank(rank: int, c: List[int], o: List[int], n: int, d: int = 2, back: bool = False) -> int:
    p = _coord(rank, n, d)
    s = [(p[i] + (1 if back else -1) * (c[i] - o[i]) + n) % n for i in range(d)]
    return _rank(s, n, d)


def load_json(path: str) -> Tuple[Dict[Tuple[int, int], int], List[List[int]]]:
    """Load BDMS routing table.

    Old format (64x64 and most tables): [[src, dst], [src, ..., dst]]
    New/flat format (some smaller tables): [src, ..., dst]
    """
    data = json.loads(Path(path).read_text() or "[]")
    table, paths = {}, []
    for i, e in enumerate(data):
        # Prefer old format when recognizable: [[src, dst], path]
        if (
            isinstance(e, list)
            and len(e) == 2
            and isinstance(e[0], list)
            and len(e[0]) == 2
            and isinstance(e[1], list)
            and e[1]
            and all(isinstance(x, int) for x in e[0])
            and all(isinstance(x, int) for x in e[1])
        ):
            src_dst, path = e[0], e[1]
            table[(int(src_dst[0]), int(src_dst[1]))] = i
            paths.append(path)
        else:
            # New/flat: path only; endpoints are path[0], path[-1]
            table[(int(e[0]), int(e[-1]))] = i
            paths.append(e)
    return table, paths


def parse_edges(header: str, var: str) -> List[Tuple[int, int]]:
    text = Path(header).read_text()
    m = re.search(rf"{re.escape(var)}\s*=\s*\{{(.*?)\}};", text, re.S)
    if not m:
        raise ValueError(f"{var} not in {header}")
    return [(int(a), int(b)) for a, b in re.findall(r"\{(\d+),\s*(\d+),", m.group(1))]


@dataclass
class Flow:
    src: int
    dst: int
    path: List[int] = field(default_factory=list)
    ag_reverse: bool = False
    label: str = ""


def enumerate_trees_k(
    json_path: str, header: str, k: int, n: int = 16
) -> Tuple[List[Flow], dict]:
    """All RS+AG flows for Trees_k (tree_set = k-1)."""
    table, paths = load_json(json_path)
    main = _rank([(n - 1) // 2] * 2, n)
    text = Path(header).read_text()
    m = re.search(rf"bdms_center_16x16_{k}\s*=\s*(\d+)", text)
    if not m:
        raise ValueError(f"bdms_center_16x16_{k} not in {header}")
    tree_c = int(m.group(1))
    trees = [
        f"bdms_edges_16x16_{k}",
        f"bdms_edges_16x16_{k}_rotate1",
        f"bdms_edges_16x16_{k}_rotate2",
        f"bdms_edges_16x16_{k}_rotate3",
    ]
    seen: Set[Tuple[int, int, Tuple[int, ...], bool]] = set()
    flows: List[Flow] = []
    stats: DefaultDict[str, int] = defaultdict(int)

    def add(src: int, dst: int, path: List[int], label: str, ag_rev: bool = False):
        key = (src, dst, tuple(path), ag_rev)
        if key in seen:
            stats["dup"] += 1
            return
        seen.add(key)
        flows.append(Flow(src, dst, path, ag_rev, label))
        stats["direct" if not path else "multi"] += 1

    for i, p in enumerate(paths):
        add(p[0], p[-1], p, f"json#{i}")

    def lookup(f: int, t: int, center: int) -> Tuple[int, List[int]]:
        cc, oc = _coord(center, n), _coord(main, n)
        fs, ts = _shift_rank(f, cc, oc, n), _shift_rank(t, cc, oc, n)
        if (fs, ts) not in table:
            return -1, []
        return table[(fs, ts)], [
            _shift_rank(node, cc, oc, n, back=True) for node in paths[table[(fs, ts)]]
        ]

    for tid, var in enumerate(trees):
        edges = parse_edges(header, var)
        for center in range(n * n):
            cc, oc = _coord(center, n), _coord(tree_c, n)
            for ef, et in edges:
                g0 = _shift_rank(ef, cc, oc, n)
                g1 = _shift_rank(et, cc, oc, n)
                rid, path = lookup(g0, g1, center)
                add(g0, g1, [] if rid < 0 else path, "rs_direct" if rid < 0 else "rs_table")
                rid, path = lookup(g1, g0, center)
                add(
                    g1, g0, [] if rid < 0 else path,
                    "ag_direct" if rid < 0 else "ag_table", ag_rev=rid >= 0,
                )

    stats["total"] = len(flows)
    return flows, dict(stats)


def enumerate_trees4(json_path: str, header: str, n: int = 16) -> Tuple[List[Flow], dict]:
    return enumerate_trees_k(json_path, header, k=4, n=n)


# --- ECDG -------------------------------------------------------------------

class ECDG:
    def __init__(self):
        self.adj: DefaultDict[Channel, Set[Channel]] = defaultdict(set)
        self.meta: DefaultDict[Tuple[Channel, Channel], List[int]] = defaultdict(list)

    def add_route(self, rid: int, hops: List[Channel]) -> None:
        for i in range(len(hops) - 1):
            self.adj[hops[i]].add(hops[i + 1])
            self.meta[(hops[i], hops[i + 1])].append(rid)

    @property
    def nodes(self) -> Set[Channel]:
        s: Set[Channel] = set()
        for u, vs in self.adj.items():
            s.add(u)
            s.update(vs)
        return s

    def has_cycle(self) -> bool:
        indeg = {n: 0 for n in self.nodes}
        for vs in self.adj.values():
            for v in vs:
                indeg[v] += 1
        q = [n for n, d in indeg.items() if d == 0]
        seen = 0
        while q:
            u = q.pop()
            seen += 1
            for v in self.adj[u]:
                indeg[v] -= 1
                if indeg[v] == 0:
                    q.append(v)
        return seen != len(indeg)

    def find_cycle(self) -> Optional[List[Channel]]:
        vis, stk, par = set(), set(), {}
        def dfs(u: Channel) -> Optional[List[Channel]]:
            vis.add(u)
            stk.add(u)
            for v in self.adj[u]:
                if v not in vis:
                    par[v] = u
                    if c := dfs(v):
                        return c
                elif v in stk:
                    cyc, cur = [v], u
                    while cur != v:
                        cyc.append(cur)
                        cur = par[cur]
                    cyc.reverse()
                    cyc.append(v)
                    return cyc
            stk.remove(u)
            return None
        for n in sorted(self.nodes):
            if n not in vis and (c := dfs(n)):
                return c
        return None


def simulate_flows(
    torus: TorusTrees,
    flows: List[Flow],
    vc_mode: str = "runtime",
    pi_order: str = "lex",
) -> Tuple[ECDG, List[List[Channel]], dict]:
    ecdg, all_hops = ECDG(), []
    max_viol, hist = 0, defaultdict(int)
    for i, f in enumerate(flows):
        path = list(reversed(f.path)) if f.ag_reverse else (f.path or None)
        topo = torus.walk_topology(f.src, f.dst, path)
        if vc_mode == "bump":
            vcs = bump_vcs(torus, topo, pi_order)
            hops = [(u, v, vc) for (u, v), vc in zip(topo, vcs)]
        else:
            hops = torus.walk(f.src, f.dst, path)
        max_viol = max(max_viol, max((vc for _, _, vc in hops), default=0))
        for _, _, vc in hops:
            hist[vc] += 1
        all_hops.append(hops)
        ecdg.add_route(i, hops)
    meta = {"max_viol": max_viol, "num_vc": max_viol + 1, "hist": dict(hist)}
    return ecdg, all_hops, meta


def ch_fmt(c: Channel) -> str:
    return f"({c[0]}->{c[1]}, vc={c[2]})"


# --- Bump VC (Dally–Seitz / updn) -------------------------------------------

def pi_rank(torus: TorusTrees, router: int, order: str) -> Tuple[int, ...]:
    """Total order π on routers: lex coords (dim0, dim1, …) or flat id."""
    return tuple(torus.loc(router)) if order == "lex" else (router,)


def hop_violates(torus: TorusTrees, u: int, v: int, prev_dim: Optional[int], order: str) -> bool:
    """
    True if hop u→v needs a new VC "level".

    order="turn": matches torus_trees.cc —
      (a) axis turn (prev_dim set and != current axis), or
      (b) dateline: leaving coord==0 on the active axis, but ONLY when the
          packet arrived from the network (prev_dim is not None).
          Injection from NIC never bumps for dateline alone
          (C++: `port < local_port_start`).
    Backward-direction hops are NOT a violation by themselves.

    order="lex"/"rank": older, overly strict variants kept for comparison —
    they also flag every backward hop, which massively over-counts VCs
    (empirically 25 VC on Trees_4, vs 4 for order="turn").
    """
    cur, nxt = torus.loc(u), torus.loc(v)
    d = next(i for i in range(torus.d) if cur[i] != nxt[i])
    if order == "turn":
        if prev_dim is not None and d != prev_dim:
            return True
        # Dateline only after a network hop (not NIC injection).
        if prev_dim is not None and cur[d] == 0:
            return True
        return False
    rsc = torus.d - d - 1
    forward = (nxt[d] % torus.dim[rsc]) == (cur[d] + 1) % torus.dim[rsc]
    if not forward:
        return True
    if prev_dim is not None and d != prev_dim:
        return True
    if order == "lex" and tuple(nxt) <= tuple(cur):
        return True
    if order == "rank" and pi_rank(torus, v, order) <= pi_rank(torus, u, order):
        return True
    return False


def bump_vcs(torus: TorusTrees, hops: List[Tuple[int, int]], order: str = "turn") -> List[int]:
    """Per-path viol counter; hop i uses VC = viol after applying violations at i."""
    viol, prev_dim, out = 0, None, []
    for u, v in hops:
        if hop_violates(torus, u, v, prev_dim, order):
            viol += 1
        out.append(viol)
        prev_dim = next(i for i in range(torus.d) if torus.loc(u)[i] != torus.loc(v)[i])
    return out


# --- CLI --------------------------------------------------------------------

def report_ecdg(name: str, ecdg: ECDG, hops: List[List[Channel]], meta: dict) -> bool:
    flat = [vc for h in hops for _, _, vc in h]
    hist = dict(sorted(meta["hist"].items()))
    print(f"\n[{name}]  channels={len(ecdg.nodes)}  deps={sum(map(len, ecdg.adj.values()))}  "
          f"hops={sum(map(len, hops))}  max_vc={max(flat) if flat else 0}  "
          f"num_vc={meta['num_vc']}  hist={hist}")
    if not ecdg.has_cycle():
        print(f"[{name}] RESULT: no ECDG cycle")
        return False
    print(f"[{name}] RESULT: ECDG has cycle(s)")
    cycle = ecdg.find_cycle()
    if cycle:
        print(f"[{name}] cycle ({len(cycle) - 1} edges): "
              + " -> ".join(ch_fmt(c) for c in cycle[:8])
              + (" ..." if len(cycle) > 9 else ""))
    return True


def main() -> int:
    ap = argparse.ArgumentParser(description="ECDG check for torus_trees")
    ap.add_argument("--json", default="scratch/src/sst-elements/src/sst/elements/ember/mpi/motifs/emberroutingtables/ember_bdmst_routing_table_16_4.json")
    ap.add_argument("--trees-header", default="scratch/src/sst-elements/src/sst/elements/ember/mpi/motifs/embertrees/emberbdmstrees_16_16d4.h")
    ap.add_argument("--shape", default="16x16")
    ap.add_argument("--mode", choices=("json", "trees4"), default="trees4")
    ap.add_argument(
        "--vc-mode", choices=("runtime", "bump", "both"), default="both",
        help="runtime=torus_trees.cc route_packet (turn+dateline bump); "
             "bump=same rule via hop list; both=compare (should match for turn)",
    )
    ap.add_argument(
        "--pi-order", choices=("turn", "lex", "rank"), default="turn",
        help="turn=axis-turn+dateline-after-network (matches C++, num_vc=4); "
             "lex/rank=stricter legacy variants that over-count (num_vc=25)",
    )
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    torus = TorusTrees([int(x) for x in args.shape.split("x")])
    _, paths = load_json(args.json)

    if args.mode == "json":
        flows = [Flow(p[0], p[-1], p, label=f"json#{i}") for i, p in enumerate(paths)]
        stats = {"total": len(flows), "direct": 0, "dup": 0}
    else:
        flows, stats = enumerate_trees4(args.json, args.trees_header)

    print(f"mode={args.mode}  json={len(paths)}  flows={stats['total']}  "
          f"direct={stats.get('direct', 0)}  multi={stats.get('multi', 0)}  dup={stats.get('dup', 0)}")

    modes = ["runtime", "bump"] if args.vc_mode == "both" else [args.vc_mode]
    results: Dict[str, bool] = {}
    hops_by_mode: Dict[str, List[List[Channel]]] = {}
    for mode in modes:
        ecdg, hops, meta = simulate_flows(torus, flows, mode, args.pi_order)
        hops_by_mode[mode] = hops
        label = mode + (f", π={args.pi_order}" if mode == "bump" else "")

        if args.verbose:
            for i, (f, h) in enumerate(zip(flows, hops)):
                for j, c in enumerate(h):
                    print(f"  {mode} {i:5d} {f.src:3d}->{f.dst:3d} [{f.label}] hop{j} {ch_fmt(c)}")

        results[mode] = report_ecdg(label, ecdg, hops, meta)

    if args.vc_mode == "both":
        if args.pi_order == "turn":
            mism = sum(
                1
                for a, b in zip(hops_by_mode["runtime"], hops_by_mode["bump"])
                if a != b
            )
            print(f"\nruntime vs bump(π=turn) VC-seq mismatches: {mism}/{len(flows)}")
        print("\n--- summary ---")
        print(f"runtime VC (torus_trees.cc turn+dateline bump): "
              f"{'DEADLOCK (ECDG cycle)' if results['runtime'] else 'OK'}")
        print(f"bump VC (π={args.pi_order}): "
              f"{'DEADLOCK (ECDG cycle)' if results['bump'] else 'OK'}")
        # Both modes should agree for π=turn; exit 0 iff no cycle in either.
        return 0 if not any(results.values()) else 1

    # Single-mode: exit 1 iff that mode has a cycle.
    return 1 if next(iter(results.values())) else 0


if __name__ == "__main__":
    sys.exit(main())
