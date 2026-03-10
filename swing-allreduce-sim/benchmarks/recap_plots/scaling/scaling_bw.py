# %%
from cmath import sqrt
import os
from pyexpat.errors import XML_ERROR_XML_DECL
from mpl_toolkits.axes_grid1.inset_locator import mark_inset
import re
import matplotlib.pyplot as plt
import math
from matplotlib import rcParams
from matplotlib.ticker import FuncFormatter
import seaborn as sns
from matplotlib.patches import Rectangle, ConnectionPatch
from matplotlib import _api, docstring
from matplotlib.offsetbox import AnchoredOffsetbox
from matplotlib.patches import Patch, Rectangle
from matplotlib.path import Path
from matplotlib.transforms import Bbox, BboxTransformTo
from matplotlib.transforms import IdentityTransform, TransformedBbox
from mpl_toolkits.axes_grid1.inset_locator import inset_axes
from argparse import ArgumentParser
from pathlib import Path    
import pandas as pd
from datetime import datetime
import warnings

warnings.filterwarnings("ignore")
output_folder = 'output/'
thredhold_small = 32768 + 1
save_folder = 'plots_bw'
save_folder_pdf = save_folder + "/" + "pdf"
save_folder_img = save_folder + "/" + "img"
adapt_plane = True

bytes_sent_map = {}

sent = {}

def correct_color(name, palette):
    if (name == "SwingB"):
        return palette[0]
    elif (name == "RecDoub"):
        return palette[1]
    elif (name == "Tree"):
        return palette[2]
    elif (name == "Rings"):
        return palette[3]
    elif (name == "Torus"):
        return palette[4]
    elif (name == "SwingL"):
        return palette[5]
    elif (name == "Swing"):
        return palette[0]
    else:
        print("error, name algo not found!")
        exit(1)

def adapt_names(names):
    if (names == "SwingB"):
        return "Swing (BW)"
    elif (names == "RecDoub"):
        return "Recursive doubling"
    elif (names == "Tree"):
        return "Recursive doubling\nlatency opt."
    elif (names == "Rings"):
        return "Hamiltonian rings"
    elif (names == "Torus"):
        return "Bucket"
    elif (names == "SwingL"):
        return "Swing (L)"
    else:
        return names
    
def get_short_name(name):
    if ("Torus" in name):
        return "B"
    elif ("RecDoub" in name):
        return "RecDoub"
    elif ("Rings" in name):
        return "H"
    elif ("Tree" in name):
        return "RL"

def bytes_to_mb(list_b):
    list_mb = []
    for b in list_b:
        if (b / 1000000000000 < 1):
            value = "{}\nGiB".format(int(b / 2**30))
        if (b / 1000000000 < 1):
            value = "{}\nMiB".format(int(b / 2**20))
        if (b / 1000000 < 1):
            value = "{}\nKiB".format(int(b / 2**10))
        if (b / 1000 < 1):
            value = "{}\nB".format(int(b / 1))

        list_mb.append(value)
    return list_mb

def time_correction(list_b):
    list_mb = []
    for b in list_b:
        if (b / 1000000000 < 1):
            value = "{}ms".format(round(b/1000000))
        if (b / 1000000 < 1):
            value = "{}μs".format(round(b/1000))
        if (b / 1000 < 1):
            value = "{}ns".format(round(b))
        list_mb.append(value)
    return list_mb

def get_values_zoom(min, max):
    if (max == 128):
        return 58, 8800
        #return 66, 23800
    elif (max == 2147483648):
        #return 58, 8800
        return 66, 23800

def main(args):
    # Iterate through results, sort them by size and parse them

    allreduce_sizes_all = [2**3, 2**5, 2**7, 2**9, 2**11, 2**13, 2**15, 2**17, 2**19, 2**21, 2**23, 2**25, 2**27]
    allreduce_sizes_all = [x * 4 for x in allreduce_sizes_all]
    bw100 = "../../allreduce/output/torus_8x8_100Gb/diff"
    bw200 = "../../allreduce/output/torus_8x8_200Gb/diff"
    bw400 = "../../allreduce/output/torus_8x8/diff"
    bw800 = "../../allreduce/output/torus_8x8_800Gb/diff"
    bw1600 = "../../allreduce/output/torus_8x8_1600Gb/diff"
    bw3200 = "../../allreduce/output/torus_8x8_3200Gb/diff"

    rcParams['figure.figsize'] = 8,5
    fig, ax = plt.subplots()

    palette = list_my_colors = ["#44739d", "#d48640", "#539045", "#b14743", "#8e73ae", "#7e5d55", "#cb88ba", "#7f7f7f"]

    performance_diff = []
    with open(bw100) as f:
        performance_diff = f.read().splitlines()
    performance_diff = [int(float(x)) for x in performance_diff][0:len(allreduce_sizes_all)]
    data_plot64 = pd.DataFrame({"X":allreduce_sizes_all, "Y":performance_diff})
    sns.lineplot(x = "X", y = "Y", data=data_plot64, label="100Gb/s", marker='o', legend=False, color=palette[0], linewidth=2.5, markersize=9)

    performance_diff = []
    with open(bw200) as f:
        performance_diff = f.read().splitlines()
    performance_diff = [int(float(x)) for x in performance_diff][0:len(allreduce_sizes_all)]
    data_plot256 = pd.DataFrame({"X":allreduce_sizes_all, "Y":performance_diff})
    sns.lineplot(x = "X", y = "Y", data=data_plot256, label="200Gb/s", marker='P', legend=False, color=palette[1], linewidth=2.5, markersize=9)
    
    performance_diff = []
    with open(bw400) as f:
        performance_diff = f.read().splitlines()
    performance_diff = [int(float(x)) for x in performance_diff][0:len(allreduce_sizes_all)]
    data_plot1024 = pd.DataFrame({"X":allreduce_sizes_all, "Y":performance_diff})
    sns.lineplot(x = "X", y = "Y", data=data_plot1024, label="400Gb/s", marker='X', legend=False, color=palette[2], linewidth=2.5, markersize=9)

    with open(bw800) as f:
        performance_diff = f.read().splitlines()
    performance_diff = [int(float(x)) for x in performance_diff][0:len(allreduce_sizes_all)]
    data_plot4096 = pd.DataFrame({"X":allreduce_sizes_all, "Y":performance_diff})
    sns.lineplot(x = "X", y = "Y", data=data_plot4096, label="800Gb/s", marker='D', legend=False, color=palette[3], linewidth=2.5, markersize=9)

    with open(bw1600) as f:
        performance_diff = f.read().splitlines()
    performance_diff = [int(float(x)) for x in performance_diff][0:len(allreduce_sizes_all)]
    data_plot4096 = pd.DataFrame({"X":allreduce_sizes_all, "Y":performance_diff})
    sns.lineplot(x = "X", y = "Y", data=data_plot4096, label="1600Gb/s", marker='^', legend=False, color=palette[4], linewidth=2.5, markersize=9)

    with open(bw3200) as f:
        performance_diff = f.read().splitlines()
    performance_diff = [int(float(x)) for x in performance_diff][0:len(allreduce_sizes_all)]
    data_plot4096 = pd.DataFrame({"X":allreduce_sizes_all, "Y":performance_diff})
    sns.lineplot(x = "X", y = "Y", data=data_plot4096, label="3200Gb/s", marker='s', legend=False, color=palette[5], linewidth=2.5, markersize=9)

    ymin, ymax = ax.get_ylim()
    ymin = abs(ymin) + round(abs(ymin) / 10)
    ymax = ymax + round(ymax / 10)
    if (ymin > ymax):
        ax.set_ylim([-10, ymin])
    else:
        ax.set_ylim([-10, ymax])
    ax.axhspan(ymin=-50  - 20, ymax=0, facecolor="red", alpha=0.15)
    ax.axhspan(ymin=0, ymax=ymax + 20, facecolor='green', alpha=0.15)

    ax.tick_params(axis='both', labelsize=13)
    ax.set_xscale('log', base=2)
    ax.set_xticks(allreduce_sizes_all)
    locs1 = ax.get_xticks()
    ax.set_xticklabels(bytes_to_mb(locs1))
    #plt.legend( fontsize=12, title_fontsize=0, loc='upper right')
    ax.axhline(y=0, ls='--', c='black', alpha=0.55)
    ax.yaxis.set_major_formatter(FuncFormatter('{:.0f}%'.format))
    #ax.set_title('Swing goodput gain (on Torus) for different link bandwidth'.format(args.topo), fontdict={'fontsize': 17})
    ax.set_xlabel("Allreduce size", fontsize=13.5)
    ax.set_ylabel("Swing Goodput Gain vs. Best Known Algo. (%)", fontsize=13.5)
    ax.grid(linewidth = 0.30)
    
    plt.show()
    plt.tight_layout()
    Path(save_folder_pdf).mkdir(parents=True, exist_ok=True)
    Path(save_folder_img).mkdir(parents=True, exist_ok=True)
    file_name = datetime.now().strftime('%Y-%m-%d|%H:%M:%S')
    plt.savefig(Path(save_folder_img) / (str("RingAllReduce") + file_name))
    plt.savefig(Path(save_folder_pdf) / (str("RingAllReduce") + file_name + ".pdf"))

        
if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument("--input_folder", type=str, help="Input folder name", default="input")
    parser.add_argument("--topo", type=str, help="Name Topo Used", default="Torus")
    parser.add_argument("--zoomed_in", type=str, help="Time or BW", default="BW")
    parser.add_argument("--show_bounds", type=str, help="Theoretical Bounds", default="True")
    parser.add_argument("--merge_lines", type=str, help="Merge latency and bw optimal", default="True")
    parser.add_argument("--show_switch", type=str, help="Display switching point", default="False")
    parser.add_argument("--gather_ratio", type=int, help="Running all reduce (1) or gather (2)", default=1)
    parser.add_argument("--y_zoomed_in", type=int, help="Max Y value for zoomed in plot", default=40000)
    args = parser.parse_args()
    main(args)


# %%
