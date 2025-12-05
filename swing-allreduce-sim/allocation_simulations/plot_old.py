import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import os

def get_fat_tree_injection_ports(net):
    if "Hx2Large" in net:
        # Numbers reported by generate_jobs are per board (so do not take into account board size, 
        # but this is not a problem because we could consider a hierarchical alltoall with 
        # first alltoall within the board and then between boards)
        # For this reason, we consider that each board has 4 ports (N,E,S,W)
        return 64*128*4 
    elif "Hx4Large" in net:
        # Numbers reported by generate_jobs are per board (so do not take into account board size, 
        # but this is not a problem because we could consider a hierarchical alltoall with 
        # first alltoall within the board and then between boards)
        # For this reason, we consider that each board has 2 ports (N,S)
        return 32*64*2 

scenario_to_name = {}
scenario_to_name["dummy"] = "1D"
scenario_to_name["notranspose_nochangeratio_unsorted"] = "Greedy"
scenario_to_name["nochangeratio_unsorted"] = "Greedy + Transpose"
scenario_to_name["unsorted"] = "Greedy + Transpose + Change Ratio"
scenario_to_name["sorted"] = "Greedy + Transpose + Change Ratio + Sort Jobs"
scenario_to_name["sorted_perm"] = "Greedy + Transpose + Change Ratio + Permute + Sort Jobs"
scenario_to_name["min_crossing_a2a"] = "Greedy + Transpose + Change Ratio + Min Crossing (A2A)"
scenario_to_name["min_crossing_ar"] = "Greedy + Transpose + Change Ratio + Min Crossing (AR)"
scenario_to_name["sorted_min_crossing_a2a"] = "Greedy + Transpose + Change Ratio + Sort Jobs + Min Crossing (A2A)"
scenario_to_name["sorted_min_crossing_ar"] = "Greedy + Transpose + Change Ratio + Sort Jobs + Min Crossing (AR)"
scenario_to_name["single_job"] = "Single Job"

glob_df = pd.DataFrame()
glob_df_full = pd.DataFrame()

#for scenario in ["unsorted_perm", "sorted_perm", "unsorted", "sorted"]:
for scenario in ["sorted", "unsorted"]:
    for net in ["Hx2Small", "Hx4Small",  "Hx2Large", "Hx4Large"]:
        for faults in ["0", "10", "20", "30", "40", "50", "60", "80", "100", "0%", "10%", "20%", "30%", "40%", "50%"]:
            if faults == "0" or faults == "0%":
                filename = "logs/log_" + net + "_" + scenario + ".csv"
            else:
                filename = "logs/log_" + net + "_" + scenario + "_faults-" + str(faults) + ".csv"
            
            if not os.path.isfile(filename):
                continue
            df = pd.read_csv(filename, delimiter="\t")
            df["Faults"] = faults
            if scenario == "sorted":
                df["Network"] = net + " (Sorted Jobs)"
            else:
                df["Network"] = net + " (Unsorted Jobs)"
            df["99th Percentile"] = df["99pUtilization"].astype(float)*100
            df["Average"] = df["AverageUtilization"].astype(float)*100
            glob_df = pd.concat([glob_df, df])

            df = pd.read_csv(filename.replace(".", "_full."), delimiter="\t")
            df["Faults"] = faults
            if scenario == "sorted":
                df["Network"] = net + " (Sorted Jobs)"
            else:
                df["Network"] = net + " (Unsorted Jobs)"
            df["Utilization (%)"] = df["Utilization"].astype(float)*100
            glob_df_full = pd.concat([glob_df_full, df])

    glob_df.reset_index(inplace=True)    
    glob_df = glob_df.drop(glob_df[glob_df.AverageUtilization == -1].index)
    glob_df_full.reset_index(inplace=True)

    sns.set_style("whitegrid")
    ###################
    # Absolute faults #
    ###################
    ax = sns.lineplot(data=glob_df[~glob_df['Faults'].str.contains("%")], x="Faults", y="AverageUtilization", markers=True, dashes=True, hue="Network", style="Network")
    #ax.set_title(scenario)
    ax.figure.savefig('out/faults/average_' + scenario + '.pdf', format='pdf', dpi=100)
    plt.clf()

    ax = sns.lineplot(data=glob_df[~glob_df['Faults'].str.contains("%")], x="Faults", y="99pUtilization", markers=True, dashes=True, hue="Network", style="Network")
    #ax.set_title(scenario)
    ax.figure.savefig('out/faults/99p_' + scenario + '.pdf', format='pdf', dpi=100)
    plt.clf()

    ax = sns.lineplot(data=glob_df_full[~glob_df_full['Faults'].str.contains("%")], x="Faults", y="Utilization (%)", markers=True, dashes=True, hue="Network", style="Network")
    #ax.set_title(scenario)
    ax.figure.savefig('out/faults/full_' + scenario + '.pdf', format='pdf', dpi=100)
    plt.clf()

    #####################
    # Percentage faults #
    #####################
    ax = sns.lineplot(data=glob_df[glob_df['Faults'].str.contains("%")], x="Faults", y="AverageUtilization", markers=True, dashes=True, hue="Network", style="Network")
    #ax.set_title(scenario)
    ax.figure.savefig('out/faults/average_' + scenario + '_perc.pdf', format='pdf', dpi=100)
    plt.clf()

    ax = sns.lineplot(data=glob_df[glob_df['Faults'].str.contains("%")], x="Faults", y="99pUtilization", markers=True, dashes=True, hue="Network", style="Network")
    #ax.set_title(scenario)
    ax.figure.savefig('out/faults/99p_' + scenario + '_perc.pdf', format='pdf', dpi=100)
    plt.clf()

    ax = sns.lineplot(data=glob_df_full[glob_df_full['Faults'].str.contains("%")], x="Faults", y="Utilization (%)", markers=True, dashes=True, hue="Network", style="Network")
    #ax.set_title(scenario)
    ax.figure.savefig('out/faults/full_' + scenario + '_perc.pdf', format='pdf', dpi=100)
    plt.clf()

##############
# Mixed plot #
##############

############
# Lineplot #
############
fig, axes = plt.subplots(1, 2, sharey=True, figsize=(10,3))

hue_order = ["Hx2Small (Unsorted Jobs)", "Hx2Small (Sorted Jobs)", "Hx4Small (Unsorted Jobs)", "Hx4Small (Sorted Jobs)"]
#ax = sns.lineplot(data=glob_df_full[glob_df_full["Network"].str.contains("Small") & ~glob_df_full['Faults'].str.contains("%") & (glob_df_full['Faults'].str.startswith("0") | glob_df_full['Faults'].str.startswith("10") | glob_df_full['Faults'].str.startswith("20") | glob_df_full['Faults'].str.startswith("30") | glob_df_full['Faults'].str.startswith("40")) & ~glob_df_full['Faults'].str.startswith("100")], x="Faults", y="Utilization (%)", markers=True, dashes=True, hue="Network", style="Network", ax=axes[0], hue_order=hue_order)
ax = sns.lineplot(data=glob_df_full[glob_df_full["Network"].str.contains("Small") & ~glob_df_full['Faults'].str.contains("%") & (glob_df_full['Faults'].str.startswith("0") | glob_df_full['Faults'].str.startswith("20") | glob_df_full['Faults'].str.startswith("40")) & ~glob_df_full['Faults'].str.startswith("100")], x="Faults", y="Utilization (%)", markers=True, dashes=True, hue="Network", style="Network", ax=axes[0], hue_order=hue_order)
#ax.set_title("Small HxMeshes")
ax.set_xlim(["0", "40"])
ax.get_legend().set_title(None)

hue_order = ["Hx2Large (Unsorted Jobs)", "Hx2Large (Sorted Jobs)", "Hx4Large (Unsorted Jobs)", "Hx4Large (Sorted Jobs)"]
#ax = sns.lineplot(data=glob_df_full[glob_df_full["Network"].str.contains("Large") & ~glob_df_full['Faults'].str.contains("%") & (glob_df_full['Faults'].str.startswith("0") | glob_df_full['Faults'].str.startswith("20") | glob_df_full['Faults'].str.startswith("40") | glob_df_full['Faults'].str.startswith("60") | glob_df_full['Faults'].str.startswith("80") | glob_df_full['Faults'].str.startswith("100"))], x="Faults", y="Utilization (%)", markers=True, dashes=True, hue="Network", style="Network", ax=axes[1], hue_order=hue_order)
ax = sns.lineplot(data=glob_df_full[glob_df_full["Network"].str.contains("Large") & ~glob_df_full['Faults'].str.contains("%") & (glob_df_full['Faults'].str.startswith("0") | glob_df_full['Faults'].str.startswith("50") | glob_df_full['Faults'].str.startswith("100"))], x="Faults", y="Utilization (%)", markers=True, dashes=True, hue="Network", style="Network", ax=axes[1], hue_order=hue_order)
ax.set_ylabel("")
#ax.set_title("Large HxMeshes")
ax.set_xlim(["0", "100"])
ax.get_legend().set_title(None)

plt.tight_layout()
fig.savefig('out/faults/full_mix.pdf', format='pdf', dpi=100)
plt.clf()

##############
# Violinplot #
##############
fig, axes = plt.subplots(1, 2, sharey=True, figsize=(10,3))

hue_order = ["Hx2Small (Unsorted Jobs)", "Hx2Small (Sorted Jobs)", "Hx4Small (Unsorted Jobs)", "Hx4Small (Sorted Jobs)"]
#ax = sns.violinplot(data=glob_df_full[glob_df_full["Network"].str.contains("Small") & ~glob_df_full['Faults'].str.contains("%") & (glob_df_full['Faults'].str.startswith("0") | glob_df_full['Faults'].str.startswith("10") | glob_df_full['Faults'].str.startswith("20") | glob_df_full['Faults'].str.startswith("30") | glob_df_full['Faults'].str.startswith("40")) & ~glob_df_full['Faults'].str.startswith("100")], x="Faults", y="Utilization (%)", hue="Network", ax=axes[0], hue_order=hue_order)
ax = sns.violinplot(data=glob_df_full[glob_df_full["Network"].str.contains("Small") & ~glob_df_full['Faults'].str.contains("%") & (glob_df_full['Faults'].str.startswith("0") | glob_df_full['Faults'].str.startswith("20") | glob_df_full['Faults'].str.startswith("40")) & ~glob_df_full['Faults'].str.startswith("100")], x="Faults", y="Utilization (%)", hue="Network", ax=axes[0], hue_order=hue_order, cut=0, linewidth=0)
#patch_violinplot()
#ax.set_title("Small HxMeshes")
handles, labels = ax.get_legend_handles_labels()
ax.get_legend().set_title(None)
x = 0
pos = [-0.3, -0.1, 0.1, 0.3]
for faults in ["0", "20", "40"]:
    hue = 0
    for net in hue_order:
        line = glob_df[glob_df['Faults'].str.startswith(faults) & glob_df['Faults'].str.endswith(faults) & glob_df["Network"].str.startswith(net) & glob_df["Network"].str.endswith(net)]
        
        y = int(line["99th Percentile"])        
        axes[0].scatter(x=x+pos[hue], y=y, color="white", edgecolors=sns.color_palette()[hue], marker="^")
        
        y = int(line["Average"])
        axes[0].scatter(x=x+pos[hue], y=y, color="white", edgecolors=sns.color_palette()[hue], marker="s")
        
        line = glob_df_full[glob_df_full['Faults'].str.startswith(faults) & glob_df_full['Faults'].str.endswith(faults) & glob_df_full["Network"].str.startswith(net) & glob_df_full["Network"].str.endswith(net)]
        y = int(np.median(line["Utilization (%)"]))
        axes[0].scatter(x=x+pos[hue], y=y, color="white", edgecolors=sns.color_palette()[hue], marker="o")
        
        hue += 1
    x += 1
ax.set_ylim(ymax=100)

hue_order = ["Hx2Large (Unsorted Jobs)", "Hx2Large (Sorted Jobs)", "Hx4Large (Unsorted Jobs)", "Hx4Large (Sorted Jobs)"]
#ax = sns.violinplot(data=glob_df_full[glob_df_full["Network"].str.contains("Large") & ~glob_df_full['Faults'].str.contains("%") & (glob_df_full['Faults'].str.startswith("0") | glob_df_full['Faults'].str.startswith("20") | glob_df_full['Faults'].str.startswith("40") | glob_df_full['Faults'].str.startswith("60") | glob_df_full['Faults'].str.startswith("80") | glob_df_full['Faults'].str.startswith("100"))], x="Faults", y="Utilization (%)", hue="Network", ax=axes[1], hue_order=hue_order)
ax = sns.violinplot(data=glob_df_full[glob_df_full["Network"].str.contains("Large") & ~glob_df_full['Faults'].str.contains("%") & (glob_df_full['Faults'].str.startswith("0") | glob_df_full['Faults'].str.startswith("50") | glob_df_full['Faults'].str.startswith("100"))], x="Faults", y="Utilization (%)", hue="Network", ax=axes[1], hue_order=hue_order, cut=0, linewidth=0)
#patch_violinplot()
#ax.set_title("Large HxMeshes")
ax.set_ylabel("")
handles, labels = ax.get_legend_handles_labels()
ax.get_legend().set_title(None)
x = 0
pos = [-0.3, -0.1, 0.1, 0.3]
for faults in ["0", "50", "100"]:
    hue = 0
    for net in hue_order:
        line = glob_df[glob_df['Faults'].str.startswith(faults) & glob_df['Faults'].str.endswith(faults) & glob_df["Network"].str.startswith(net) & glob_df["Network"].str.endswith(net)]
        y = int(line["99th Percentile"])
        axes[1].scatter(x=x+pos[hue], y=y, color="white", edgecolors=sns.color_palette()[hue], marker="^")
        y = int(line["Average"])
        axes[1].scatter(x=x+pos[hue], y=y, color="white", edgecolors=sns.color_palette()[hue], marker="s")
        line = glob_df_full[glob_df_full['Faults'].str.startswith(faults) & glob_df_full['Faults'].str.endswith(faults) & glob_df_full["Network"].str.startswith(net) & glob_df_full["Network"].str.endswith(net)]
        y = int(np.median(line["Utilization (%)"]))
        axes[1].scatter(x=x+pos[hue], y=y, color="white", edgecolors=sns.color_palette()[hue], marker="o")
        hue += 1
    x += 1
ax.set_ylim(ymax=100)
plt.tight_layout()
fig.savefig('out/faults/full_mix_violin.pdf', format='pdf', dpi=100)
plt.clf()


###########################
# Comparison between opts #
###########################
glob_links_df = pd.DataFrame()
for faults in ["0", "50", "100"]:
    glob_df = pd.DataFrame()
    glob_df_full = pd.DataFrame()

    for net in ["Hx2Small", "Hx4Small",  "Hx2Large", "Hx4Large"]:
        for scenario in ["notranspose_nochangeratio_unsorted", "nochangeratio_unsorted", "unsorted", "sorted", "dummy", "min_crossing_a2a", "min_crossing_ar", "sorted_min_crossing_a2a", "sorted_min_crossing_ar"]:
                if faults == "0" or faults == "0%":
                    filename = "logs/log_" + net + "_" + scenario + ".csv"
                else:
                    filename = "logs/log_" + net + "_" + scenario + "_faults-" + str(faults) + ".csv"
                
                if not os.path.isfile(filename):
                    continue
                df = pd.read_csv(filename, delimiter="\t")
                df["Faults"] = faults
                df["Network"] = net
                df["Scenario"] = scenario_to_name[scenario]
                df["99th Percentile"] = df["99pUtilization"].astype(float)*100
                df["Average"] = df["AverageUtilization"].astype(float)*100
                if scenario != "dummy" and scenario != "single_job":
                    glob_df = pd.concat([glob_df, df])
                if "Large" in net:
                    df["Percentage Lvl1 Traffic (A2A)"] = (df["UsedLvl1Links(A2A)"] / (df["UsedLvl1Links(A2A)"] + df["UsedLvl0Links(A2A)"]))*100.0
                    df["Percentage Lvl1 Traffic (AR)"] = (df["UsedLvl1Links(AR)"] / (df["UsedLvl1Links(AR)"] + df["UsedLvl0Links(AR)"]))*100.0
                    # "Percentage Lvl1 Traffic" can be at most 50% (if all the traffic crosses the upper tree level). 
                    # If we want to report it in relative terms (as a fraction of the upper level bandwidth), we must scale it
                    # considering 50% as the maximum
                    #df["Lvl1 Links Utilization % (A2A)"] = (df["Percentage Lvl1 Traffic (A2A)"] / 50.0) * 100.0
                    #df["Lvl1 Links Utilization % (AR)"] = (df["Percentage Lvl1 Traffic (AR)"] / 50.0) * 100.0
                    num_fattree_host_ports = get_fat_tree_injection_ports(net)
                    df["Lvl1 Links Utilization % (A2A)"] = ((df["UsedLvl1Links(A2A)"]) / num_fattree_host_ports) * 100.0
                    df["Lvl1 Links Utilization % (AR)"] = ((df["UsedLvl1Links(AR)"]) / num_fattree_host_ports) * 100.0
                    # TODO: Should compute this as fraction of available links?
                    glob_links_df = pd.concat([glob_links_df, df])

                df = pd.read_csv(filename.replace(".", "_full."), delimiter="\t")
                df["Faults"] = faults
                df["Network"] = net
                df["Scenario"] = scenario_to_name[scenario]
                df["Utilization (%)"] = df["Utilization"].astype(float)*100
                if scenario != "dummy":
                    glob_df_full = pd.concat([glob_df_full, df])


    #print(glob_df)
    glob_df.reset_index(inplace=True)    
    glob_df = glob_df.drop(glob_df[glob_df.AverageUtilization == -1].index)
    glob_df_full.reset_index(inplace=True)

    ###############
    # Utilization #
    ###############
    if faults != "100":
        ax = sns.violinplot(data=glob_df_full, x="Network", y="Utilization (%)", hue="Scenario", cut=0, linewidth=0, saturation=1)    
        ax.get_legend().set_title(None)
        x = 0
        pos = [-0.3, -0.1, 0.1, 0.3]
        for net in ["Hx2Small", "Hx4Small",  "Hx2Large", "Hx4Large"]:
            hue = 0
            for scenario in ["notranspose_nochangeratio_unsorted", "nochangeratio_unsorted", "unsorted", "sorted"]:
                line = glob_df[glob_df['Faults'].str.startswith(faults) & glob_df['Faults'].str.endswith(faults) & glob_df["Network"].str.startswith(net) & glob_df["Scenario"].str.startswith(scenario_to_name[scenario]) & glob_df["Scenario"].str.endswith(scenario_to_name[scenario])]
                print(line)
                y = int(line["99th Percentile"])
                plt.scatter(x=x+pos[hue], y=y, color="white", edgecolors=sns.color_palette()[hue], marker="^")
                y = int(line["Average"])
                plt.scatter(x=x+pos[hue], y=y, color="white", edgecolors=sns.color_palette()[hue], marker="s")
                line = glob_df_full[glob_df_full['Faults'].str.startswith(faults) & glob_df_full['Faults'].str.endswith(faults) & glob_df_full["Network"].str.startswith(net) & glob_df_full["Scenario"].str.startswith(scenario_to_name[scenario]) & glob_df_full["Scenario"].str.endswith(scenario_to_name[scenario])]
                y = int(np.median(line["Utilization (%)"]))
                plt.scatter(x=x+pos[hue], y=y, color="white", edgecolors=sns.color_palette()[hue], marker="o")
                hue += 1
            x += 1
        ax.set_ylim(ymax=100)
        ax.figure.savefig('out/faults/opt_comparison_' + str(faults) + 'faults.pdf', format='pdf', dpi=100)
        plt.clf()

#########
# Links #
#########
single_job_a2a_links_l1 = {}
single_job_ar_links_l1 = {}
single_job_a2a_links_l1["Hx2Large"] = (109051904.0 / get_fat_tree_injection_ports("Hx2Large"))*100.0
single_job_a2a_links_l1["Hx4Large"] = (3145728.0 / get_fat_tree_injection_ports("Hx4Large"))*100.0
single_job_ar_links_l1["Hx2Large"] = (2048.0 / get_fat_tree_injection_ports("Hx2Large"))*100.0
single_job_ar_links_l1["Hx4Large"] = (256.0 / get_fat_tree_injection_ports("Hx4Large"))*100.0

for net in ["Hx2Large", "Hx4Large"]:
    ax = sns.barplot(data=glob_links_df[glob_links_df['Network'].str.startswith(net)], x="Faults", y="Lvl1 Links Utilization % (A2A)", hue="Scenario", linewidth=0, saturation=1)
    #ax.axhline(single_job_a2a_links_l1[net], ls='--', linewidth=3, color='red')
    plt.tight_layout()
    ax.figure.savefig('out/faults/opt_comparison_links_a2a_' + net + '.pdf', format='pdf', dpi=100)    
    plt.clf()
    
    ax = sns.barplot(data=glob_links_df[glob_links_df['Network'].str.startswith(net)], x="Faults", y="Lvl1 Links Utilization % (AR)", hue="Scenario", linewidth=0, saturation=1)
    ax.axhline(single_job_ar_links_l1[net], ls='--', linewidth=3, color='red')
    plt.tight_layout()
    ax.figure.savefig('out/faults/opt_comparison_links_ar_' + net + '.pdf', format='pdf', dpi=100)
    plt.clf()
