#!/usr/bin/env python3
"""PNG pictures of the best schemes (run with py_env/bin/python).

viz/gallery_<torus>.png: one row per R, one torus panel per round.
viz/cost_vs_R.png: L against R for every torus, with the regular baselines.
"""
import argparse
import math
import os
from collections import Counter

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

from make_viz_data import ALPHA, HERE, collect  # noqa: E402

PART_COLORS = [plt.cm.tab10(i) for i in (0, 2, 1, 4, 9, 6, 8, 3)]
IDLE = "#e4e4e4"


def rounds_info(e):
    nx, ny, R = e["nx"], e["ny"], e["R"]
    P = nx * ny
    keys, depth = [""] * P, []
    for r in range(R + 1):
        ids = {}
        depth.append([ids.setdefault(k, len(ids)) for k in keys])
        if r < R:
            keys = [k + e["codes"][r][v] for v, k in enumerate(keys)]
    out = []
    for r in range(R):
        set_of, part_of = depth[r], depth[r + 1]
        parts = {}
        for v in range(P):
            parts.setdefault(set_of[v], Counter())[part_of[v]] += 1
        active = [len(parts[set_of[v]]) > 1 for v in range(P)]
        pat = Counter()
        for c in parts.values():
            sizes = sorted(c.values(), reverse=True)
            if sum(sizes) > 1:
                pat[(sum(sizes), "{" + ",".join(map(str, sizes)) + "}" if len(sizes) > 1 else "холостой")] += 1
        pats = [f"{k} × ({m} → {s})" for (m, s), k in sorted(pat.items(), key=lambda x: (-x[0][0], -x[1]))]
        out.append(dict(set_of=set_of, part_of=part_of, rank=[ALPHA.index(c) for c in e["codes"][r]],
                        active=active, patterns=pats))
    return out


def short(p, n=30):
    return p if len(p) <= n else p[:n - 2] + "…}"


def draw_panel(ax, e, info, r, width_in, patterns=True):
    nx, ny = e["nx"], e["ny"]
    cell_pt = 72 * width_in / nx
    thick = min(2.4, max(0.8, 0.2 * cell_pt))
    for v in range(nx * ny):
        x, y = v % nx, v // nx
        color = PART_COLORS[info["rank"][v] % len(PART_COLORS)] if info["active"][v] else IDLE
        ax.add_patch(plt.Rectangle((x, y), 1, 1, facecolor=color, alpha=0.75 if info["active"][v] else 1,
                                   edgecolor="white", linewidth=min(0.4, 0.04 * cell_pt)))
    set_of, part_of = info["set_of"], info["part_of"]
    for y in range(ny):
        for x in range(nx):
            v = y * nx + x
            for u, horiz in ((y * nx + (x + 1) % nx, False), (((y + 1) % ny) * nx + x, True)):
                kind = 2 if set_of[v] != set_of[u] else (1 if part_of[v] != part_of[u] else 0)
                if not kind:
                    continue
                style = dict(color="black", linewidth=thick, solid_capstyle="projecting") if kind == 2 else \
                    dict(color="#555555", linewidth=0.4 * thick)
                if not horiz:
                    xs = [x + 1] if x < nx - 1 else [nx, 0]
                    for xx in xs:
                        ax.plot([xx, xx], [y, y + 1], **style)
                else:
                    ys = [y + 1] if y < ny - 1 else [ny, 0]
                    for yy in ys:
                        ax.plot([x, x + 1], [yy, yy], **style)
    ax.add_patch(plt.Rectangle((0, 0), nx, ny, fill=False, edgecolor="#999999", linewidth=0.8))
    ax.set_xlim(-0.15, nx + 0.15)
    ax.set_ylim(ny + 0.15, -0.15)
    ax.set_aspect("equal")
    ax.set_xticks([])
    ax.set_yticks([])
    for s in ax.spines.values():
        s.set_visible(False)
    ax.set_title(f"раунд {r + 1}: {e['rc'][r]}", fontsize=9, pad=3)
    if patterns:
        txt = "\n".join(map(short, info["patterns"][:3])) + ("\n…" if len(info["patterns"]) > 3 else "")
        ax.text(nx / 2, ny + 0.4, txt, ha="center", va="top", fontsize=6.5, color="#444444")


def best_regular(e):
    vals = [v for v in (e.get("digit"), e.get("lat"), e.get("halving")) if v is not None]
    return min(vals) if vals else None


def row_label(e):
    reg = best_regular(e)
    src = "точно" if e["src"] == "exact" else f"отжиг {e.get('hit') or ''}".strip()
    gain = f", −{100 * (reg - e['L']) / reg:.1f}%" if reg and reg > e["L"] else ""
    return f"R = {e['R']}\nL = {e['L']}\n({src})\nрегул. {reg}{gain}"


def gallery(tag, entries, outdir):
    rows = [e for e in entries if e["t"] == tag]
    if not rows:
        return None
    ncols = max(e["R"] for e in rows)
    e0 = rows[0]
    pw = 1.55 if max(e0["nx"], e0["ny"]) <= 8 else 2.3
    ph = pw * e0["ny"] / e0["nx"] + 0.75
    fig, axes = plt.subplots(len(rows), ncols + 1, figsize=((ncols + 0.8) * pw, len(rows) * ph),
                             gridspec_kw=dict(width_ratios=[0.8] + [1] * ncols), squeeze=False)
    for i, e in enumerate(rows):
        info = rounds_info(e)
        axes[i][0].axis("off")
        axes[i][0].text(0.0, 0.5, row_label(e), ha="left", va="center", fontsize=8.5,
                        transform=axes[i][0].transAxes)
        for r in range(ncols):
            ax = axes[i][r + 1]
            if r < e["R"]:
                draw_panel(ax, e, info[r], r, 0.85 * pw)
            else:
                ax.axis("off")
    fig.suptitle(f"Тор {tag.replace('x', '×')}: лучшие найденные разбиения по раундам", fontsize=12, y=0.995)
    fig.tight_layout(rect=(0, 0, 1, 0.985))
    path = os.path.join(outdir, f"gallery_{tag}.png")
    fig.savefig(path, dpi=110, bbox_inches="tight")
    plt.close(fig)
    return path


def single(e, outdir):
    info = rounds_info(e)
    ncols = e["R"] if e["R"] <= 5 else math.ceil(e["R"] / 2)
    nrows = math.ceil(e["R"] / ncols)
    pw = 2.6 if max(e["nx"], e["ny"]) <= 8 else 3.4
    fig, axes = plt.subplots(nrows, ncols, figsize=(ncols * pw, nrows * (pw * e["ny"] / e["nx"] + 0.9)),
                             squeeze=False)
    for k in range(nrows * ncols):
        ax = axes[k // ncols][k % ncols]
        if k < e["R"]:
            draw_panel(ax, e, info[k], k, 0.9 * pw)
        else:
            ax.axis("off")
    reg = best_regular(e)
    src = "точный оптимум" if e["src"] == "exact" else f"отжиг, {e.get('hit')} цепочек"
    fig.suptitle(f"Тор {e['t'].replace('x', '×')}, R = {e['R']}: L = {e['L']} ({src}); "
                 f"лучшая регулярная {reg}", fontsize=11)
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    path = os.path.join(outdir, f"scheme_{e['t']}_R{e['R']}.png")
    fig.savefig(path, dpi=120, bbox_inches="tight")
    plt.close(fig)
    return path


def cost_chart(entries, outdir):
    tori = list(dict.fromkeys(e["t"] for e in entries))
    ncols = 4
    nrows = math.ceil(len(tori) / ncols)
    fig, axes = plt.subplots(nrows, ncols, figsize=(ncols * 3.4, nrows * 2.9), squeeze=False)
    for k, t in enumerate(tori):
        ax = axes[k // ncols][k % ncols]
        rows = [e for e in entries if e["t"] == t]
        Rs = [e["R"] for e in rows]
        series = [("найдено", "L", "o-", "black"), ("цифровые", "digit", "s--", "tab:blue"),
                  ("решётчатые", "lat", "^--", "tab:orange"), ("нижняя граница", "lb", ":", "tab:gray")]
        for name, key, fmt, color in series:
            pts = [(e["R"], e[key]) for e in rows if e.get(key) is not None]
            if pts:
                ax.plot(*zip(*pts), fmt, color=color, label=name, markersize=4, linewidth=1.2)
        ax.set_xticks(Rs)
        ax.set_title(f"тор {t.replace('x', '×')}", fontsize=10)
        ax.set_xlabel("число раундов R", fontsize=8)
        ax.set_ylabel("L, хопы", fontsize=8)
        ax.tick_params(labelsize=7)
        ax.grid(alpha=0.3)
    for k in range(len(tori), nrows * ncols):
        axes[k // ncols][k % ncols].axis("off")
    axes[0][0].legend(fontsize=7)
    fig.suptitle("Сумма длин сообщений L по числу раундов: найденные схемы и регулярные эталоны", fontsize=12)
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    path = os.path.join(outdir, "cost_vs_R.png")
    fig.savefig(path, dpi=110, bbox_inches="tight")
    plt.close(fig)
    return path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tori", nargs="*", default=[])
    ap.add_argument("--single", nargs="*", default=[], help="e.g. 8x8:5 4x4:3")
    a = ap.parse_args()
    outdir = os.path.join(HERE, "viz")
    os.makedirs(outdir, exist_ok=True)
    entries = collect(a.tori)
    for t in dict.fromkeys(e["t"] for e in entries):
        print(gallery(t, entries, outdir))
    for s in a.single:
        t, R = s.split(":")
        for e in entries:
            if e["t"] == t and e["R"] == int(R):
                print(single(e, outdir))
    print(cost_chart(entries, outdir))


if __name__ == "__main__":
    main()
