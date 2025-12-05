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
import numpy as np

warnings.filterwarnings("ignore")
thredhold_small = 33500 + 1
save_folder = 'plots'
adapt_plane = True

bytes_sent_map = {}

sent = {}

def correct_color(name, palette):
    if (name == "SwingL"):
        return palette[0]
    elif (name == "SwingB"):
        return palette[0]
    elif (name == "Swing"):
        return palette[0]
    elif (name == "RecDoub"):
        return palette[1]
    elif (name == "RecDoubL"):
        return palette[1]
    elif (name == "RecDoubB"):
        return palette[1]
    elif (name == "Rings"):
        return palette[2]
    elif (name == "Bucket"):
        return palette[3]
    elif (name == "RecDoubLM"):
        return palette[4]
    elif (name == "RecDoubBM"):
        return palette[4]
    elif (name == "RecDoubM"):
        return palette[4]
    else:
        print("error, name algo not found!")
        exit(1)

def correct_marker(name):
    if (name == "SwingB"):
        return "o"
    elif (name == "SwingL"):
        return "o"
    elif (name == "Swing"):
        return "o"
    elif (name == "RecDoub"):
        return "P"
    elif (name == "RecDoubL"):
        return "P"
    elif (name == "RecDoubB"):
        return "P"
    elif (name == "Rings"):
        return "s"
    elif (name == "Bucket"):
        return "D"
    elif (name == "RecDoubLM" or name == "RecDoubBM" or name == "RecDoubM"):
        return "X"
    else:
        print("error, name algo not found!")
        exit(1)

def adapt_names(names):
    if (names == "SwingB"):
        return "Swing (BW)"
    elif (names == "RecDoubB"):
        return "Recursive doubling"
    elif (names == "RecDoubL"):
        return "Recursive doubling\nlatency opt."
    elif (names == "Rings"):
        return "Hamiltonian rings"
    elif (names == "Bucket"):
        return "Bucket"
    elif (names == "SwingL"):
        return "Swing (L)"
    else:
        return names
    
def get_short_name(name):
    if ("Bucket" in name):
        return "B"
    elif ("Rings" in name):
        return "H"
    elif (name == "RecDoub"):
        return "D"
    elif (name == "RecDoubM"):
        return "M"

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
    bw_map = {}
    x_sizes = {}
    time = {}
    time_map = {}

    max_len = 0
    last_x_value_rect = 0
    last_y_value_rect = 0
    x_switch_swing = 0
    y_switch_swing = 0
    x_switch_recdoub = 0
    y_switch_recdoub = 0
    x_switch_recdoub_m = 0
    y_switch_recdoub_m = 0

    shape = args.shape
    if args.topo.lower() == "hx2":
        shape = str(int(int(args.shape.split("x")[0])/2)) + "x" + str(int(int(args.shape.split("x")[1])/2))
    elif args.topo.lower() == "hx4":
        shape = str(int(int(args.shape.split("x")[0])/4)) + "x" + str(int(int(args.shape.split("x")[1])/4))


    bw = ""
    if args.netBW != "400Gb/s":
        bw = "_" + args.netBW.split("/")[0]
    output_folder = "output/" + args.topo.lower() + "_" + shape + bw + "/"

    for root, subdirectories, files in os.walk(output_folder):
        for subdirectory in subdirectories:
            filelist = os.listdir(os.path.join(root, subdirectory))
            filelist = sorted(filelist,key=lambda x: int(os.path.splitext(x)[0]))

            bytes_sent_map[subdirectory] = list()
            time_map[subdirectory] = list()
            x_sizes[subdirectory] = list()
            bw_map[subdirectory] = list()
            time_map[subdirectory] = list()

            sent[subdirectory] = list()
            time[subdirectory] = list()
            if (len(filelist) > max_len):
                    max_len = len(filelist)
            for file in filelist:
                min_bw = 100000000000
                max_time = 0

                with open(os.path.join(root, subdirectory, file)) as fp:
                    Lines = fp.readlines()
                    count = 0
                    for line in Lines:

                        match = re.search("count=(\d+)", line)
                        if match:
                            count = (int(match.group(1)) * 4)
                            x_sizes[subdirectory].append(int(match.group(1)) * 4)

                        match = re.search("STATS (\d+) (\d+) EMBER (\d+) (\d+) (\d+) (\d+)", line)
                        if match:
                            tmp_bytes = count
                            tmp_time = ((int(match.group(5))))
                            if (subdirectory == "SwingB"):
                                tmp_time *= args.gather_ratio
                            if tmp_time == 0:
                                tmp_time = min_bw
                            elif (tmp_bytes / tmp_time < min_bw):
                                min_bw = tmp_bytes / tmp_time

                            if (tmp_time > max_time):
                                max_time = tmp_time

                    if (min_bw == 100000000000):
                        min_bw = 0
                    bw_map[subdirectory].append(min_bw)
                    time_map[subdirectory].append(max_time)


    temp_bw = {}
    temp_x_values = {}
    temp_time = {}
    temp_time2 = {}
    best_swing_tmp = []
    best_recdoub_tmp = []
    best_recdoub_m_tmp = []
    best_rec_tmp = []
    print(bw_map)
    print(time_map)
    is_first_swing = True
    is_first_recdoub = True
    is_first_recdoub_m = True
    temp_time2["Swing"] = []
    temp_time2["RecDoub"] = []
    temp_time2["RecDoubM"] = []
    if (args.merge_lines == "True"):
        for key, value in bw_map.items():
            if (key == "SwingB"):
                for idx, ele in enumerate(value):
                    if (idx < len(bw_map["SwingL"])):
                        best_swing_tmp.append(max(ele, bw_map["SwingL"][idx]))
                        temp_time2["Swing"].append(min(time_map["SwingB"][idx], time_map["SwingL"][idx]))
                        if (ele > bw_map["SwingL"][idx] and is_first_swing):
                            x_switch_swing = x_sizes[key][idx]
                            y_switch_swing = ele
                            is_first_swing = False
                    else:
                        best_swing_tmp.append(ele)
                        temp_time2["Swing"].append(time_map["SwingB"][idx])
                    
                temp_bw["Swing"] = best_swing_tmp
                temp_x_values["Swing"] = x_sizes["SwingB"]
                temp_time["Swing"] = time["SwingB"]                            
            elif (key == "SwingL"):
                continue
            elif (key == "RecDoubB"):
                for idx, ele in enumerate(value):
                    if (idx < len(bw_map["RecDoubL"])):
                        best_recdoub_tmp.append(max(ele, bw_map["RecDoubL"][idx]))
                        temp_time2["RecDoub"].append(min(time_map["RecDoubB"][idx], time_map["RecDoubL"][idx]))
                        if (ele > bw_map["RecDoubL"][idx] and is_first_recdoub):
                            x_switch_recdoub = x_sizes[key][idx]
                            y_switch_recdoub = ele
                            is_first_recdoub = False
                    else:
                        best_recdoub_tmp.append(ele)
                        temp_time2["RecDoub"].append(time_map["RecDoubB"][idx])
                    
                temp_bw["RecDoub"] = best_recdoub_tmp
                temp_x_values["RecDoub"] = x_sizes["RecDoubB"]
                temp_time["RecDoub"] = time["RecDoubB"]                            
            elif (key == "RecDoubL"):
                continue
            elif (key == "RecDoubBM"):
                for idx, ele in enumerate(value):
                    if (idx < len(bw_map["RecDoubLM"])):
                        best_recdoub_m_tmp.append(max(ele, bw_map["RecDoubLM"][idx]))
                        temp_time2["RecDoubM"].append(min(time_map["RecDoubBM"][idx], time_map["RecDoubLM"][idx]))
                        if (ele > bw_map["RecDoubLM"][idx] and is_first_recdoub_m):
                            x_switch_recdoub_m = x_sizes[key][idx]
                            y_switch_recdoub_m = ele
                            is_first_recdoub_m = False
                    else:
                        best_recdoub_m_tmp.append(ele)
                        temp_time2["RecDoubM"].append(time_map["RecDoubBM"][idx])
                    
                temp_bw["RecDoubM"] = best_recdoub_m_tmp
                temp_x_values["RecDoubM"] = x_sizes["RecDoubBM"]
                temp_time["RecDoubM"] = time["RecDoubBM"]                            
            elif (key == "RecDoubLM"):
                continue
            else:
                temp_bw[key] = value
                temp_x_values[key] = x_sizes[key]
                temp_time[key] = time[key]
                temp_time2[key] = time_map[key]
        bw_map = temp_bw
        x_sizes = temp_x_values
        time = temp_time
        time_map = temp_time2

    best_competition = [None] * max_len
    best_competition_name = [None] * max_len
    best_swing = [None] * max_len
    best_rec_doub = [None] * max_len
    rcParams['figure.figsize'] = 10,6
    fig, ax = plt.subplots()
    palette = list_my_colors = ["#44739d", "#d48640", "#539045", "#b14743", "#8e73ae", "#7e5d55", "#cb88ba", "#7f7f7f"]


    sortednames=sorted(bw_map.keys(), key=lambda x:x.lower())
    most_x = []
    x_data_most = [None]
    print(sortednames)
    zoomed_in_x_start = 0.11
    zoomed_in_x_width =.3
    zoomed_in_y_start = 0.13
    zoomed_in_y_width =.33
    zoomed_in = ax.inset_axes((zoomed_in_x_start,zoomed_in_y_start,zoomed_in_x_width,zoomed_in_y_width))
    for key in sortednames:
        
        x_data = x_sizes[key]
        if len(x_data) > len(x_data_most):
            x_data_most = x_data
        y_data = bw_map[key]
        y_data_new = []
        x_data_new = []
        y_data_small = []
        y_data_small_time = []
        x_data_small = []
        #print(key)
        #print(y_data)
        #time_bw = time[key]
        if (len(x_data) > len(most_x)):
            most_x = x_data
        #print("topo {} {}".format(key,x_data))
        if (key == "Bucket"):
            y_data = [element / 1 for element in y_data] # To Bit
        if ("fattree" in key or key == "dragonfly" and adapt_plane):
            for idx, ele in enumerate(x_data):
                if (idx > 0):
                    res = ele / ((x_data[idx - 1]) / y_data[idx - 1])
                    x_data_new.append(x_data[idx])
                    y_data_new.append(res)            
            x_data = x_data_new
            y_data = y_data_new

        y_data = [element * 8 for element in y_data] # To Bit
        for idx, ele in enumerate(x_data):
            if (ele < thredhold_small):
                if (last_x_value_rect < ele):
                    last_x_value_rect = ele
                if (last_y_value_rect < y_data[idx]):
                    last_y_value_rect = y_data[idx]   
                    
                if (args.zoomed_in == "BW"):
                    y_data_small.append(y_data[idx])
                else:
                    print(time_map)
                    y_data_small.append(time_map[key][idx])
                x_data_small.append(ele)
        
        data_plot = pd.DataFrame({"X":x_data, "Y":y_data})
        data_plot_small = pd.DataFrame({"X":x_data_small, "Y":y_data_small})
        #print(key)
        #print(data_plot)
        #print(max_len)
        if ("rings" in key.lower() or "bucket" in key.lower()  or ("recdoub" in key.lower() and not "m" in key.lower())):
            for idx, value in enumerate(y_data):
                if (best_competition[idx] is None or best_competition[idx] < value):
                    best_competition[idx] = value
                    best_competition_name[idx] = key
        elif ("swing" in key.lower()):
            for idx, value in enumerate(y_data):
                if (best_swing[idx] is None or best_swing[idx] < value):
                    best_swing[idx] = value
                    if ("RecDoubB" in key.lower()):
                        best_rec_doub[idx] = value
        print(best_swing)

        sns.lineplot(x = "X", y = "Y", data=data_plot, label=adapt_names(key), marker=correct_marker(key), ax=ax, clip_on=False, color=correct_color(key, palette), linewidth=2.5, markersize=9)
        print("------ SMALL ------ ")
        print(key)
        print(data_plot_small)
        if (key != "Rings"):
            sns.lineplot(x = "X", y = "Y", data=data_plot_small, label=key, ax=zoomed_in, color=correct_color(key, palette), linewidth=2.5, marker=correct_marker(key), markersize=9)
        else:
            max_swing_small = max(data_plot_small["Y"])
    x11, x22 = ax.get_xlim()
    new_y_arrival = (ax.get_ylim()[1] * zoomed_in_y_start)
    new_x_arrival = (x22 * zoomed_in_x_start)
    
    if (args.show_switch):
        ax.scatter(x_switch_swing, y_switch_swing*8, marker='o', s=300, color=correct_color("Swing", palette))
        '''sns.lineplot(x_switch_swing, y_switch_swing,  lw=1, ax=ax,  color='red', alpha=1)'''
        ax.scatter(x_switch_recdoub, y_switch_recdoub*8, marker='o', s=300, color=correct_color("RecDoub", palette))
        ax.scatter(x_switch_recdoub_m, y_switch_recdoub_m*8, marker='o', s=300, color=correct_color("RecDoubM", palette))

    xa, xb = get_values_zoom(0, 2147483648)
    
    #[[x,x],[y,y]]
    sns.lineplot([(32-x11) / (x22-x11), zoomed_in_x_start], [0,zoomed_in_y_start],  lw=1, ax=ax,  color='grey', alpha=0.7, transform=ax.transAxes)
    #sns.lineplot([(thredhold_small+500000000-x11) / (x22-x11), zoomed_in_x_start+zoomed_in_x_width], [0, zoomed_in_y_start],  lw=1, ax=ax,  color='grey', alpha=0.7, transform=ax.transAxes)
    
    if max(data_plot["X"]) < pow(2, 31):
        sns.lineplot([thredhold_small, xb], [0, new_y_arrival + 10],  lw=1, ax=ax,  color='grey', alpha=0.7)           
    else:
        sns.lineplot([thredhold_small, xb+20000], [0, new_y_arrival + 10],  lw=1, ax=ax,  color='grey', alpha=0.7)

    #sns.lineplot([32, xa], [0, new_y_arrival + 10],  lw=1, ax=ax,  color='grey', alpha=0.7)
    #sns.lineplot([thredhold_small, xb], [0, new_y_arrival + 10],  lw=1, ax=ax,  color='grey', alpha=0.7)
    '''
    sns.lineplot([(32-x11) / (x22-x11), zoomed_in_x_start], [0,zoomed_in_y_start],  lw=1, ax=ax,  color='grey', alpha=0.7, transform=ax.transAxes)
    sns.lineplot([(thredhold_small+500000000-x11) / (x22-x11), zoomed_in_x_start+zoomed_in_x_width], [0, zoomed_in_y_start],  lw=1, ax=ax,  color='grey', alpha=0.7, transform=ax.transAxes)
    '''

    # If we have enabled also theoretical limits, calculate them and show lines
    rings_upper_bound = []
    bucket_upper_bound = []
    swing_upper_bound = []
    rec_doub_upper_bound = []
    total_latency = 150 * 2 + 100
    inverse_bw = 1/400
    num_nodes = 64
    dim = 2
    for ele in x_sizes["Swing"]:
        # Rings
        calcualte = ((ele*8) / (num_nodes*(2*total_latency)+(((ele*8)/4)*(inverse_bw))))/2
        rings_upper_bound.append(calcualte)
        # Bucket
        first_part = dim * ((num_nodes ** (1/dim)) - 1) * total_latency
        second_part = 0
        for i in range(dim):
            second_part += ((ele*8/2/dim) / ((num_nodes ** (1/dim))**i)) * inverse_bw

        calcualte = dim * (num_nodes ** (1/dim)) * total_latency + ((ele*8) / (2*dim)) * inverse_bw
        bucket_upper_bound.append((ele*8) / ((calcualte)*2))
        # Swing
        e = (2**dim-1)/(2**dim)
        for_limit = round(math.log2(num_nodes-1) / dim)
        sum = 0
        for i in range(for_limit):
            sum += (2**(i+1) - (-1)**(i+1)) / (3*(2**(i*dim)))
        e = e * sum
        calcualte = math.log2(num_nodes) * total_latency + ((ele*8) * inverse_bw * e)  / (2*dim)
        swing_upper_bound.append(((ele*8) / (calcualte*2)))
        # Rec Doub
        calcualte = math.log2(num_nodes) * total_latency + ((ele*8) * inverse_bw * ((2**dim - 1) / (2**dim - 2))) / (2*dim)
        rec_doub_upper_bound.append((ele*8) / (calcualte*1.75))

    if (args.show_bounds == "True"):
        print(swing_upper_bound)
        print(rec_doub_upper_bound)
        print(bucket_upper_bound)
        print(rings_upper_bound)
        print("END UPPER BOUNDS")
        data_plot = pd.DataFrame({"X":x_sizes["Swing"], "Y":rings_upper_bound})
        sns.lineplot(x = "X", y = "Y", data=data_plot, marker='o', ax=ax, color="orange", linestyle='dashed', legend=False)
        data_plot = pd.DataFrame({"X":x_sizes["Swing"], "Y":bucket_upper_bound})
        sns.lineplot(x = "X", y = "Y", data=data_plot, marker='o', ax=ax, color="red", linestyle='dashed', legend=False)
        data_plot = pd.DataFrame({"X":x_sizes["Swing"], "Y":swing_upper_bound})
        sns.lineplot(x = "X", y = "Y", data=data_plot, marker='o', ax=ax, color="green",  linestyle='dashed', legend=False)
        data_plot = pd.DataFrame({"X":x_sizes["Swing"], "Y":rec_doub_upper_bound})
        sns.lineplot(x = "X", y = "Y", data=data_plot, marker='o', ax=ax, color="blue", linestyle='dashed', legend=False)

    performance_diff = [None] * max_len
    performance_diff_rec = [None] * max_len
    '''print(len(best_swing))
    print(len(best_competition))
    print((best_swing))
    print((best_competition))'''
    for idx, value in enumerate(best_swing):
        if (best_swing[idx] > best_competition[idx]):
            performance_diff[idx] = ((best_swing[idx] - best_competition[idx]) / best_competition[idx]) * 100
        else:
            performance_diff[idx] = -((best_competition[idx] - best_swing[idx]) / best_swing[idx]) * 100
        #performance_diff_rec[idx] = ((best_rec_doub[idx] - best_competition[idx]) / best_rec_doub[idx]) * 100
    
    '''axins = ax.inset_axes((0.68,0.12,.28,.35))'''
    axins = ax.inset_axes((zoomed_in_x_start,zoomed_in_y_start + 0.45,zoomed_in_x_width,zoomed_in_y_width))
    x_data = x_data_most
    data_plot = pd.DataFrame({"X":x_data, "Y":performance_diff})
    sns.lineplot(x = "X", y = "Y", data=data_plot, label=key, marker='o', ax=axins, legend="full", color=palette[0],  linewidth=2.5, markersize=9)
    #data_plot2 = pd.DataFrame({"X":x_data, "Y":performance_diff_rec})
    #sns.lineplot(x = "X", y = "Y", data=data_plot2, label=key, marker='o', ax=axins, legend=True, color=pa[1])

    zoomed_in.get_legend().remove()
    axins.get_legend().remove()
    ax.get_legend().remove()
    ax.set_xscale('log', base=2)
    ax.xaxis.set_tick_params(labelsize=12)
    #ax.title.set_text('Allreduce performance - {} 64 nodes'.format(args.topo))

    if "x" in args.shape:
        nnodes = np.prod([int(x) for x in args.shape.split("x")])
        #if args.topo == "Hx2":
        #    nnodes *= 4
        #elif args.topo == "Hx4":
        #    nnodes *= 16
    else:
        nnodes = 1
        for lvl in args.shape.split(":"):
            nnodes *= int(lvl.split(",")[0])

    # Manage thousands
    nnodes = f'{nnodes:,}'

    if "hx2" in args.topo.lower():
        shape_and_topo = str(int(shape.split("x")[0])*2) + "x" + str(int(shape.split("x")[1])*2) + " " + "Hx2Mesh"
    elif "hx4" in args.topo.lower():
        shape_and_topo = str(int(shape.split("x")[0])*4) + "x" + str(int(shape.split("x")[1])*4) + " " + "Hx4Mesh"
    else:
        shape_and_topo = shape + " " + args.topo
    if "fattree" in args.topo:
        first_lvl = args.shape.split(":")[0]
        down = int(first_lvl.split(",")[0])
        up = int(first_lvl.split(",")[1])
        if up < down:
            shape_and_topo = str(int(down/up)) + ":1 Blocking Fat Tree"
        else:
            shape_and_topo = "Non Blocking Fat Tree"
        
    ax.set_title('Allreduce - {} ({} nodes)'.format(shape_and_topo, nnodes), fontdict={'fontsize': 18.5})
    ax.set_xlabel("Allreduce size", fontsize=17.7)
    ax.set_ylabel("Goodput (Gb/s)", fontsize=17.7)
    '''ax.indicate_inset_zoom(zoomed_in, edgecolor="black")'''
    #mark_inset(ax, zoomed_in,  loc1=3, loc2=4, fc="none", ec='grey', alpha=0.7)
    #rectpatch, connects=ax.indicate_inset_zoom(zoomed_in, edgecolor="black")
    rect = Rectangle(
        [32, 0], width=last_x_value_rect, height=last_y_value_rect, 
        transform=ax.transData, fc="none", ec='grey', zorder=2, alpha=0.7
    )
    ax.add_patch(rect)
    
    
    # Zoomed In Plot
    zoomed_in.set_xlabel("")
    zoomed_in.set_ylabel("Runtime", fontsize=14.3)
    zoomed_in.yaxis.set_label_coords(-0.235, 0.5)
    zoomed_in.set_xscale('log', base=2)
    zoomed_in.yaxis.set_tick_params(labelsize=9)
    zoomed_in.xaxis.set_tick_params(labelsize=9)
    zoomed_in.set_xticks(x_data_small)
    locs1 = zoomed_in.get_xticks()
    zoomed_in.set_xticklabels(bytes_to_mb(locs1))
    zoomed_in.set_navigate(False) 
    
    
    #zoomed_in.set_yticklabels([])
    #zoomed_in.set_xticks([])
    # Comparison Plot
    axins.set_xscale('log', base=2)
    ymin, ymax = axins.get_ylim()
    
    if (min(performance_diff) > 0):
        ymin = 0
    isNegative = False
    if (ymin < 0):
        isNegative = True
    ymin = abs(ymin) + round(abs(ymin) / 10)
    ymax = ymax + round(ymax / 10)
    

    if (isNegative):
        ymin = -ymin
    axins.set_ylim([ymin, ymax + abs(ymax) / 5])
    axins.axhspan(ymin=ymin  - 200, ymax=0, facecolor="red", alpha=0.15)
    axins.axhspan(ymin=0, ymax=ymax + 200, facecolor='green', alpha=0.15)
    '''axins.axhspan(ymin=-ymax  - 200, ymax=0, facecolor="red", alpha=0.13)
    axins.axhspan(ymin=ymin, ymax=ymax + 200, facecolor='green', alpha=0.13)'''
    if (ymin != 0):
        axins.axhline(y=0, ls='--', c='black', alpha=0.55)
    axins.set_xlabel("")
    axins.set_ylabel("")

    axins.yaxis.set_major_formatter(FuncFormatter('{:.0f}%'.format))

    x_data_rel = []
    for idx in range(0,len(x_data),2):
        x_data_rel.append(x_data[idx])
    axins.set_xticks(x_data_rel)
    locs1 = axins.get_xticks()
    axins.set_xticklabels(bytes_to_mb(locs1))

    lim2 = zoomed_in.get_ylim()[1]
    lim2 = args.y_zoomed_in
    zoomed_in.set_ylim([0, lim2])
    #zoomed_in.arrow(32, lim2 - (lim2*20/100) , 0, (lim2*15/100), head_width=8, head_length=(lim2*2/100), color=correct_color("Rings", palette),width=1.5,zorder=10)
    #zoomed_in.text(0.08, 0.88, "{}".format(time_correction([max_swing_small])[0]), transform=zoomed_in.transAxes, color=correct_color("Rings", palette), fontsize=11,zorder=10)
    #zoomed_in.plot([0.045], [0.92], marker='s', markersize=5.3, color=correct_color("Rings", palette), transform=zoomed_in.transAxes)

     
    #axins.set_xlabel("AllReduce Size", fontsize=10)
    #axins.set_ylabel("% Increase / Decrease vs best competition", fontsize=10)
    axins.set_title("Relative performance Swing (S) vs SotA", fontsize=14)
    axins.set_title("Swing Goodput Gain vs. Best Known Algo.", fontsize=14.3)
    '''locs = ax.get_xticks()
    ax.set_xticklabels(bytes_to_mb(locs))'''

    print(ymax + abs(ymax) / 5 + abs(ymin))
    size_y_axin = ymax + abs(ymax) / 5 + abs(ymin)
    label_shift = int(size_y_axin * 5 / 100)

    print("Performance Diff")
    print(performance_diff)
    for idx, value in enumerate(x_data):
        if (performance_diff[idx] > 0):
            axins.text(x_data[idx], round(performance_diff[idx]) + label_shift, get_short_name(best_competition_name[idx]), size=13.3, color='green')
        elif (performance_diff[idx] == 0 or performance_diff[idx] == -0.0):
            axins.text(x_data[idx], round(performance_diff[idx]) + label_shift, get_short_name(best_competition_name[idx]), size=13.3, color='black')
        else:
            axins.text(x_data[idx], round(performance_diff[idx]) + label_shift, get_short_name(best_competition_name[idx]), size=13.3, color='red')

    with open("{}diff".format(output_folder), 'w') as output:
        for row in performance_diff:
            output.write(str(row) + '\n')

    ax.set_xticks(x_data)
    locs1 = ax.get_xticks()
    ax.set_xticklabels(bytes_to_mb(locs1))
    if "torus" in args.topo.lower():
        ax.set_ylim([0, 400*(args.shape.count("x") + 1) + 10])
    else:
        ax.set_ylim([0, 810])    
    ax.grid(alpha=0.67, linewidth = 0.30)
    zoomed_in.grid(alpha=0.6, linewidth = 0.25)
    axins.grid(alpha=0.6, linewidth = 0.25)

    ax.tick_params(axis='both', labelsize=15)
    zoomed_in.tick_params(axis='both', labelsize=12.5)
    axins.tick_params(axis='both', labelsize=12.5)

    
    print("Switch point (Swing) is {} {}".format(x_switch_swing, y_switch_swing))
    print("Switch point (RecDoub) is {} {}".format(x_switch_recdoub, y_switch_recdoub))
    print("Switch point (RecDoubM) is {} {}".format(x_switch_recdoub_m, y_switch_recdoub_m))

    locs2 = zoomed_in.get_yticks()
    zoomed_in.set_yticklabels(time_correction(locs2))
    #plt.legend( fontsize=14, title_fontsize=0, loc='lower right')
    
    plt.show()
    plt.tight_layout()

    bw = ""
    if args.netBW != "400Gb/s":
        bw = "_" + args.netBW.split("/")[0]
    save_folder_pdf = save_folder + "/" + args.topo.lower() + "_" + shape + bw + "/" + "pdf"
    save_folder_img = save_folder + "/" + args.topo.lower() + "_" + shape + bw + "/" + "img"

    Path(save_folder_pdf).mkdir(parents=True, exist_ok=True)
    Path(save_folder_img).mkdir(parents=True, exist_ok=True)
    file_name = datetime.now().strftime('%Y-%m-%d|%H:%M:%S')
    plt.savefig(Path(save_folder_img) / (file_name))
    plt.savefig(Path(save_folder_pdf) / (file_name + ".pdf"))

        
if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument("--topo", type=str, help="Name Topo Used", default="Torus")
    parser.add_argument("--shape", type=str, help="Topo shape", default="8x8")
    parser.add_argument("--zoomed_in", type=str, help="Time or BW", default="Time")
    parser.add_argument("--show_bounds", type=str, help="Theoretical Bounds", default="False")
    parser.add_argument("--merge_lines", type=str, help="Merge latency and bw optimal", default="True")
    parser.add_argument("--show_switch", type=str, help="Display switching point", default="True")
    parser.add_argument("--gather_ratio", type=int, help="Running all reduce (1) or gather (2)", default=1)
    parser.add_argument("--netBW", type=str, help="Link bandwidth", default="400Gb/s")
    parser.add_argument("--y_zoomed_in", type=int, help="Max Y value for zoomed in plot", default=40000)
    args = parser.parse_args()
    main(args)


# %%
