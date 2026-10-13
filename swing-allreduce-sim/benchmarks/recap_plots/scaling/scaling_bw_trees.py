#!/usr/bin/env python3
"""Trees goodput gain vs best known competitor, across torus link bandwidths.

Analog of scaling_bw.py (Swing Fig. 8), but baseline is Trees (Trees_8 or
best Trees_* by min final_time) and competitors are Swing / RecDoub / Rings /
Bucket from the same output folder.

Reads SST logs under ../../allreduce/output/torus_8x8[_*Gb]/, writes plots
to plots_bw_trees/{pdf,img}/.
"""
from __future__ import annotations

import os
import re
import warnings
from datetime import datetime
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from matplotlib import rcParams
from matplotlib.ticker import FixedLocator, FuncFormatter

warnings.filterwarnings("ignore")

OUTPUT_ROOT = Path(__file__).resolve().parents[2] / "allreduce" / "output"
SAVE_ROOT = Path(__file__).resolve().parent / "plots_bw_trees"

# (label, output subdir relative to OUTPUT_ROOT)
BW_SERIES = [
    ("100Gb/s", "torus_8x8_100Gb"),
    ("200Gb/s", "torus_8x8_200Gb"),
    ("400Gb/s", "torus_8x8"),
    ("800Gb/s", "torus_8x8_800Gb"),
    ("1600Gb/s", "torus_8x8_1600Gb"),
    ("3200Gb/s", "torus_8x8_3200Gb"),
]

MARKERS = ["o", "P", "X", "D", "^", "s"]
PALETTE = ["#44739d", "#d48640", "#539045", "#b14743", "#8e73ae", "#7e5d55"]

# Competitors used for "best known algo" (SwingB/L and RecDoubB/L are merged).
COMPETITOR_GROUPS = {
    "Swing": ("SwingB", "SwingL"),
    "RecDoub": ("RecDoubB", "RecDoubL"),
    "Rings": ("Rings",),
    "Bucket": ("Bucket",),
}


def parse_final_time_ns(path: Path) -> int | None:
    """Max rank_time (ns) across TIME lines — completion time of the allreduce."""
    time_lines = []
    try:
        text = path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return None
    if "option --numVCs not recognized" in text:
        return None
    for line in text.splitlines():
        if line.startswith("TIME"):
            time_lines.append(line)
    if not time_lines:
        return None

    max_time = None
    for line in time_lines:
        parts = line.split()
        try:
            if len(parts) > 5 and parts[2] == "start_time":
                t = int(parts[5])  # rank_time
            else:
                t = int(parts[3])  # stop
        except (ValueError, IndexError):
            continue
        if max_time is None or t > max_time:
            max_time = t
    return max_time


def load_algo_goodput(topo_dir: Path, algo: str) -> dict[int, float]:
    """size_bytes -> goodput (Gb/s). Folder name is float count; bytes = count * 4."""
    algo_dir = topo_dir / algo
    if not algo_dir.is_dir():
        return {}
    out: dict[int, float] = {}
    for name in os.listdir(algo_dir):
        if not name.isdigit():
            continue
        count = int(name)
        t_ns = parse_final_time_ns(algo_dir / name)
        if t_ns is None or t_ns <= 0:
            continue
        size_bytes = count * 4
        # bytes/ns * 8 = bits/ns = Gb/s
        out[size_bytes] = (size_bytes / t_ns) * 8.0
    return out


def merge_max_goodput(series_list: list[dict[int, float]]) -> dict[int, float]:
    """Pointwise max goodput across variants (e.g. SwingB vs SwingL)."""
    merged: dict[int, float] = {}
    for series in series_list:
        for size, gp in series.items():
            if size not in merged or gp > merged[size]:
                merged[size] = gp
    return merged


def merge_trees(topo_dir: Path) -> dict[int, float]:
    """Prefer Trees_8; else best (max goodput = min time) among Trees_*."""
    preferred = load_algo_goodput(topo_dir, "Trees_8")
    if preferred:
        return preferred
    variants = []
    for name in os.listdir(topo_dir):
        if re.fullmatch(r"Trees_\d+", name):
            variants.append(load_algo_goodput(topo_dir, name))
    return merge_max_goodput(variants)


def best_competitor(topo_dir: Path) -> dict[int, float]:
    groups = []
    for _label, algos in COMPETITOR_GROUPS.items():
        parts = [load_algo_goodput(topo_dir, a) for a in algos]
        parts = [p for p in parts if p]
        if parts:
            groups.append(merge_max_goodput(parts))
    return merge_max_goodput(groups)


def gain_pct(trees: dict[int, float], best: dict[int, float]) -> dict[int, float]:
    """Same signed gain as ThreeInOnePlot / scaling_bw.py."""
    sizes = sorted(set(trees) & set(best))
    out = {}
    for s in sizes:
        t, b = trees[s], best[s]
        if t > b:
            out[s] = ((t - b) / b) * 100.0
        else:
            out[s] = -((b - t) / t) * 100.0 if t > 0 else 0.0
    return out


def bytes_to_label(sizes: list[int]) -> list[str]:
    labels = []
    for b in sizes:
        if b >= 2**30:
            labels.append(f"{b // 2**30}\nGiB")
        elif b >= 2**20:
            labels.append(f"{b // 2**20}\nMiB")
        elif b >= 2**10:
            labels.append(f"{b // 2**10}\nKiB")
        else:
            labels.append(f"{b}\nB")
    return labels


def subsample_x4(sizes: list[int]) -> list[int]:
    """Keep every other power-of-two size so consecutive steps are ×4 (1 KiB → 4 KiB)."""
    # Prefer sizes with even log2 (…, 2^8, 2^10=1KiB, 2^12=4KiB, …).
    kept = [s for s in sizes if s > 0 and (s & (s - 1)) == 0 and (s.bit_length() - 1) % 2 == 0]
    return kept if kept else sizes[::2]


def make_green_stretch_yscale(y_lo: float, y_hi: float, green_frac: float = 0.55):
    """Piecewise-linear Y map: keep full [y_lo, y_hi], stretch [0, y_hi] (green).

    green_frac — share of plot height for the positive band (rest is red/negative).
    """
    if y_lo >= 0 or y_hi <= 0:
        raise ValueError("need y_lo < 0 < y_hi for green stretch")
    neg_frac = 1.0 - green_frac

    def forward(y):
        y = np.asarray(y, dtype=float)
        out = np.empty_like(y)
        neg = y <= 0
        out[neg] = neg_frac * (y[neg] - y_lo) / (0.0 - y_lo)
        out[~neg] = neg_frac + green_frac * (y[~neg] / y_hi)
        return out

    def inverse(u):
        u = np.asarray(u, dtype=float)
        out = np.empty_like(u)
        neg = u <= neg_frac
        out[neg] = y_lo + (0.0 - y_lo) * (u[neg] / neg_frac)
        out[~neg] = y_hi * ((u[~neg] - neg_frac) / green_frac)
        return out

    return forward, inverse


def main() -> None:
    rcParams["figure.figsize"] = 8, 5
    fig, ax = plt.subplots()

    plotted_any = False
    all_sizes: list[int] = []

    for (label, subdir), marker, color in zip(BW_SERIES, MARKERS, PALETTE):
        topo_dir = OUTPUT_ROOT / subdir
        if not topo_dir.is_dir():
            print(f"skip missing {topo_dir}")
            continue
        trees = merge_trees(topo_dir)
        best = best_competitor(topo_dir)
        gains = gain_pct(trees, best)
        if not gains:
            print(f"skip empty gains for {subdir} (trees={len(trees)} best={len(best)})")
            continue
        sizes = subsample_x4(sorted(gains))
        if not sizes:
            print(f"skip empty x4 subsample for {subdir}")
            continue
        ys = [gains[s] for s in sizes]
        print(f"{label} ({subdir}): {len(sizes)} points, "
              f"gain [{min(ys):.1f}%, {max(ys):.1f}%]")
        if not all_sizes or len(sizes) > len(all_sizes):
            all_sizes = sizes
        ax.plot(
            sizes,
            ys,
            label=label,
            marker=marker,
            color=color,
            linewidth=2.5,
            markersize=9,
        )
        # Inline BW label near peak (paper style)
        peak_i = max(range(len(ys)), key=lambda i: ys[i])
        ax.annotate(
            label,
            xy=(sizes[peak_i], ys[peak_i]),
            xytext=(8, 4),
            textcoords="offset points",
            fontsize=10,
            color=color,
            va="bottom",
        )
        plotted_any = True

    if not plotted_any:
        raise SystemExit(f"No data under {OUTPUT_ROOT}")

    # Fixed bands: red [-120%, 0%], green [0%, 20%]; stretch green on screen.
    y_lo, y_hi = -120.0, 15.0
    fwd, inv = make_green_stretch_yscale(y_lo, y_hi, green_frac=0.55)
    ax.set_yscale("function", functions=(fwd, inv))
    ax.set_ylim(y_lo, y_hi)
    ax.axhspan(ymin=y_lo, ymax=0, facecolor="red", alpha=0.15)
    ax.axhspan(ymin=0, ymax=y_hi, facecolor="green", alpha=0.15)
    neg_ticks = list(range(-120, 0, 20))
    pos_ticks = list(range(0, 16, 5))
    ax.yaxis.set_major_locator(FixedLocator(neg_ticks + pos_ticks))

    ax.tick_params(axis="both", labelsize=13)
    ax.set_xscale("log", base=2)
    ax.set_xticks(all_sizes)
    ax.set_xticklabels(bytes_to_label(all_sizes))
    ax.axhline(y=0, ls="--", c="black", alpha=0.55)
    ax.yaxis.set_major_formatter(FuncFormatter("{:.0f}%".format))
    ax.set_xlabel("Allreduce size", fontsize=13.5)
    ax.set_ylabel("Trees Goodput Gain vs. Best Known Algo. (%)", fontsize=13.5)
    ax.grid(linewidth=0.30)
    ax.set_title("8x8 Torus — Trees vs best of Swing / RecDoub / Rings / Bucket")

    plt.tight_layout()
    pdf_dir = SAVE_ROOT / "pdf"
    img_dir = SAVE_ROOT / "img"
    pdf_dir.mkdir(parents=True, exist_ok=True)
    img_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now().strftime("%Y-%m-%d|%H:%M:%S")
    stem = f"TreesBwGain_{stamp}"
    fig.savefig(img_dir / stem)
    fig.savefig(pdf_dir / f"{stem}.pdf")
    print(f"saved {img_dir / stem}")
    print(f"saved {pdf_dir / (stem + '.pdf')}")
    plt.show()


if __name__ == "__main__":
    main()
