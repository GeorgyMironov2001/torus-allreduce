#!/usr/bin/env python3
import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import random
import os
import matplotlib.patches as mpatches
import tarfile
import matplotlib.ticker as ticker


cut_violins = 0
show_legend = True
networks = ["Hx2Small", "Hx4Small",  "Hx2Large", "Hx4Large"]

scenario_to_name = {}
scenario_to_name["dummy"] = "1D"
scenario_to_name["notranspose_nochangeratio_unsorted"] = "Greedy"
scenario_to_name["nochangeratio_unsorted"] = "Greedy + Transp."
scenario_to_name["unsorted"] = "Greedy + Transp. + Ch. Ratio"
scenario_to_name["sorted"] = "Greedy + Transp. + Ch. Ratio + Sort Jobs"
scenario_to_name["sorted_perm"] = "Greedy + Transp. + Ch. Ratio + Permute + Sort Jobs"
scenario_to_name["unsorted_min_crossing"] = "Greedy + Transp. + Ch. Ratio + Locality"
scenario_to_name["sorted_min_crossing"] = "Greedy + Transp. + Ch. Ratio + Sort Jobs + Locality"

def get_fat_tree_injection_ports(net):
    if "Hx2Large" in net:
        # Numbers reported by generate_jobs are per board (so do not take into account board size, 
        # but this is not a problem because we could consider a hierarchical alltoall with 
        # first alltoall within the board and then between boards)
        # For this reason, we consider that each board has 4 ports (N,E,S,W)
        return 64*64*4 
    elif "Hx4Large" in net:
        # Numbers reported by generate_jobs are per board (so do not take into account board size, 
        # but this is not a problem because we could consider a hierarchical alltoall with 
        # first alltoall within the board and then between boards)
        # For this reason, we consider that each board has 2 ports (N,S)
        return 32*32*2
    else:
        return 0

def decompose(num, aspect_ratio):
    bestfound = [1,num]
    bestratio = 1/num
    for a in range(1,num): # iterate all possible factors
        b = int(num/a)
        if(b == num/a): # i divides num
            ratio = min(a,b)/max(a,b) 
            #print(a, "x", b,":", ratio, " ", aspect_ratio, " ", bestratio)
            if(abs(ratio-aspect_ratio) < abs(bestratio-aspect_ratio)):
                bestratio=ratio
                bestfound=[a,b]
    return bestfound

def gen_jobs(netshape, boardshape, perc_alloc, max_dims, broken, single_job, alibaba):
    size=netshape[0]*netshape[1]-broken
    jobs = []
    size = int(size*perc_alloc/100)

    if alibaba:
        df = pd.read_csv("alibaba_cdf_correct.csv", index_col=None)
        df['prob'] = df['perc_jobs_lte'] - df['perc_jobs_lte'].shift(1)
        df.loc[0, 'prob'] = df.loc[0, 'perc_jobs_lte'] # Prob is the probability that a job will have that size
        #alibaba_trace_gpus =  6742
        #num_gpus = netshape[0]*netshape[1]*boardshape[0]*boardshape[1]
        #df['scaled_gpus'] = df['job_size'] * (num_gpus/float(alibaba_trace_gpus))
        #df['scaled_boards'] = np.ceil(df['scaled_gpus'] / (boardshape[0]*boardshape[1])) # Number of boards per job
        df['scaled_boards'] = df['job_size']
        df = df.groupby(['scaled_boards'], as_index=False).sum()[["scaled_boards", "prob"]]
        df = df[(df['prob'] > 0)]
        
    while size > 0:
        #jobsize=random.randrange(1,size+1)
        #avg_job_size = size/4
        avg_job_size = size/16 # TODO (?)
        if alibaba:
            rnd = random.random() * 100
            cumul = 0
            sel = 0
            for index, row in df.iterrows():
                cumul += row['prob']
                if cumul > rnd:
                    sel = row['scaled_boards']
                    break
            jobsize = int(sel)
        else:
            jobsize = int(np.random.exponential(1)*avg_job_size+1)
        jobsize = min(jobsize, size) # To avoid having the last job bigger than the number of remaining nodes        
        if single_job:
            jobsize = size
        [a,b] = decompose(jobsize, 1)
        # only use job shapes that fit dimensions of hxnet!
        if(single_job or (max(max_dims) >= max([a,b]) and min(max_dims) >= min(a,b))):            
            size -= jobsize
            #print(jobsize,":", a,"x", b)
            jobs.append(jobsize)
    return jobs

single_job_a2a_links_l1 = {}
single_job_ar_links_l1 = {}
single_job_a2a_links_l1["Hx2Large"] = 80.19
single_job_a2a_links_l1["Hx4Large"] = 76.19 
single_job_ar_links_l1["Hx2Large"] = 6.25
single_job_ar_links_l1["Hx4Large"] = 6.25

def overlay_markers(glob_df, scenarios, name, ax):
    x = 0
    for net in networks:
        if "Links Utilization" in name:
            if "Small" in net:
                x += 1
                continue
        pos = [-0.333, -0.2, -0.065, 0.065, 0.2, 0.333]
        hue = 0
        for scenario in scenarios:
            if "min_crossing" in scenario and "Small" in net:
                hue += 1
                continue
            lines = glob_df[glob_df['Scenario'].str.startswith(scenario_to_name[scenario]) & glob_df['Scenario'].str.endswith(scenario_to_name[scenario]) & glob_df["Network"].str.startswith(net) & glob_df["Network"].str.endswith(net)]
            
            #print(net + " " + scenario)
            #print(lines)

            p99 = lines[name].quantile(0.01)
            median = lines[name].median()
            mean = lines[name].mean()                
            
            ax.scatter(x=x+pos[hue], y=p99, color="white", edgecolors=sns.color_palette()[hue], marker="^")
            ax.scatter(x=x+pos[hue], y=mean, color="white", edgecolors=sns.color_palette()[hue], marker="s")
            ax.scatter(x=x+pos[hue], y=median, color="white", edgecolors=sns.color_palette()[hue], marker="o")

            hue += 1
        x += 1        

def overlay_markers_faults(glob_df, large, ax):
    x = 0
    nands = []
    faults = []
    if large:
        faults = ["0", "50", "100"]
        nands = ["Hx2Large (Unsorted Jobs)", "Hx2Large (Sorted Jobs)", "Hx4Large (Unsorted Jobs)", "Hx4Large (Sorted Jobs)"]
    else:
        faults = ["0", "20", "40"]
        nands = ["Hx2Small (Unsorted Jobs)", "Hx2Small (Sorted Jobs)", "Hx4Small (Unsorted Jobs)", "Hx4Small (Sorted Jobs)"]
    for f in faults:
        pos = [-0.3, -0.1, 0.1, 0.3]
        hue = 0
        for n in nands:
            lines = glob_df[glob_df['NetworkAndScenario'].str.startswith(n) & glob_df['NetworkAndScenario'].str.endswith(n) & glob_df['Faults'].str.startswith(f) & glob_df['Faults'].str.endswith(f)]
            #print(net + " " + scenario)
            #print(lines)

            p99 = lines["System Utilization (%)"].quantile(0.01)
            median = lines["System Utilization (%)"].median()
            mean = lines["System Utilization (%)"].mean()                
            
            ax.scatter(x=x+pos[hue], y=p99, color="white", edgecolors=sns.color_palette()[hue], marker="^")
            ax.scatter(x=x+pos[hue], y=mean, color="white", edgecolors=sns.color_palette()[hue], marker="s")
            ax.scatter(x=x+pos[hue], y=median, color="white", edgecolors=sns.color_palette()[hue], marker="o")

            hue += 1
        x += 1        

def main():    
    print("Decompressing logs ...")
    file = tarfile.open('logs.tar.gz')
    file.extractall('.')
    file.close()

    if show_legend:
        show_legend_suffix = ""
    else:
        show_legend_suffix = "no_legend"
    ################################################################################
    # Comparison of different allocation algorithms, for a network with 0 faults   #
    # Two subplots, one with system utilization and one with tree lvl1 utilization #
    ################################################################################
    scenarios = ["notranspose_nochangeratio_unsorted", "nochangeratio_unsorted", "unsorted", "unsorted_min_crossing", "sorted", "sorted_min_crossing"]
    #scenarios = ["unsorted", "sorted", "unsorted_min_crossing", "sorted_min_crossing"]    
    #for dataset in ["_", "_alibaba_"]:
    for dataset in ["_alibaba_"]:
        glob_df = pd.DataFrame()
        for scenario in scenarios:        
            for net in networks:
                if "min_crossing" in scenario and "Small" in net:
                    continue            
                if "min_crossing" in scenario:
                    # For system utilization we only consider the minimization on A2A pattern (results on AR are the same)            
                    filename = "logs/log_" + net + dataset + scenario + "_a2a_full.csv"
                else:
                    filename = "logs/log_" + net + dataset + scenario + "_full.csv"
                if not os.path.isfile(filename):
                    print("[Not found] " + filename)
                    continue
                df = pd.read_csv(filename, delimiter="\t")
                df["Scenario"] = scenario_to_name[scenario]
                df["Network"] = net
                df["System Utilization (%)"] = df["Utilization"].astype(float) * 100.0   

                if "Large" in net:
                    df["Alltoall Upper\nLayer Traffic (%)"] = df["Lvl1TraffRatio(A2A)"].astype(float) * 100.0
                    if "min_crossing" in scenario:
                        filename = "logs/log_" + net + dataset + scenario + "_ar_full.csv"
                    else:
                        filename = "logs/log_" + net + dataset + scenario + "_full.csv"
                    df_tmp = pd.read_csv(filename, delimiter="\t")
                    df["Allreduce Upper\nLayer Traffic (%)"] = df_tmp["Lvl1TraffRatio(AR)"].astype(float) * 100.0
                #if "crossing" in scenario or True:
                #    pd.set_option('display.max_rows', None)
                glob_df = pd.concat([glob_df, df])

        glob_df.reset_index(inplace=True)    
        glob_df = glob_df.drop(glob_df[glob_df.Utilization == -1].index)

        ######################
        # System Utilization #
        ######################
        fig, axes = plt.subplots(1, 1, figsize=(10,2.4))
        ax = sns.violinplot(data=glob_df, x="Network", y="System Utilization (%)", hue="Scenario", cut=cut_violins, linewidth=0, order=networks, saturation=1, ax=axes)    
        ax.get_legend().set_title(None)
        overlay_markers(glob_df, scenarios, "System Utilization (%)", ax)
        ax.get_legend().remove()
        ax.set_ylim(ymin=65, ymax=102)
        if show_legend:
            fig.legend(loc='upper center', bbox_to_anchor=(0.5, 1), ncol=3)
        plt.tight_layout()        
        box = ax.get_position()
        if show_legend:
            ax.set_position([box.x0, box.y0 - box.height * 0.115, box.width, box.height * 0.885])
        ax.figure.savefig('plots/opt_comparison_sys_ut' + dataset + show_legend_suffix + '.pdf', format='pdf', dpi=100)
        plt.clf()



        fig, axes = plt.subplots(1, 2, figsize=(20,3))
        ##############################
        # Indirect conns ratio (A2A) #
        ##############################
        ax = sns.violinplot(data=glob_df, x="Network", y="RatioIndirect(A2A)", hue="Scenario", cut=cut_violins, linewidth=0, saturation=1, order=networks, ax=axes[0])    
        ax.get_legend().set_title(None)
        ##############################
        # Indirect conns ratio (AR) #
        ##############################
        ax = sns.violinplot(data=glob_df, x="Network", y="RatioIndirect(AR)", hue="Scenario", cut=cut_violins, linewidth=0, saturation=1, order=networks, ax=axes[1])    
        ax.get_legend().remove()
        plt.tight_layout()
        fig.savefig('plots/opt_comparison_indirect' + dataset + show_legend_suffix + '.pdf', format='pdf', dpi=100)
        plt.clf()



        fig, axes = plt.subplots(1, 2, figsize=(10,2.4))
        #########################
        # A2A Links Utilization #
        #########################
        ax = sns.violinplot(data=glob_df[glob_df['Network'].str.contains("Large")], x="Network", y="Alltoall Upper\nLayer Traffic (%)", hue="Scenario", cut=cut_violins, linewidth=0, saturation=1, order=networks, ax=axes[0])    
        #ax = sns.boxplot(data=glob_df[glob_df['Network'].str.contains("Large")], x="Network", y="Alltoall Upper\nLayer Traffic (%)", hue="Scenario", linewidth=0, saturation=1, order=networks, ax=axes[0], notch=True)    
        #ax.get_legend().set_title(None)
        ax.get_legend().remove()
        ax_one = ax
        if show_legend:
            fig.legend(loc='upper center', bbox_to_anchor=(0.5, 1), ncol=3)
        #ax.axhline(single_job_a2a_links_l1["Hx2Large"], ls='--', linewidth=3, color='red')
        #ax.axhline(single_job_a2a_links_l1["Hx4Large"], ls='--', linewidth=3, color='blue')
        overlay_markers(glob_df, scenarios, "Alltoall Upper\nLayer Traffic (%)", ax)
        ########################
        # AR Links Utilization #
        ########################
        ax = sns.violinplot(data=glob_df[glob_df['Network'].str.contains("Large")], x="Network", y="Allreduce Upper\nLayer Traffic (%)", hue="Scenario", cut=cut_violins, linewidth=0, saturation=1, order=networks, ax=axes[1])    
        ax.get_legend().remove()
        ax_two = ax
        #ax.axhline(single_job_ar_links_l1["Hx2Large"], ls='--', linewidth=3, color='red')
        #ax.axhline(single_job_ar_links_l1["Hx4Large"], ls='--', linewidth=3, color='blue')
        overlay_markers(glob_df, scenarios, "Allreduce Upper\nLayer Traffic (%)", ax)
        plt.tight_layout()
        if show_legend:
            for ax in [ax_one, ax_two]:
                box = ax.get_position()
                ax.set_position([box.x0, box.y0 - box.height * 0.115, box.width, box.height * 0.885])
        fig.savefig('plots/opt_comparison_links_ut' + dataset + show_legend_suffix + '.pdf', format='pdf', dpi=100)
        plt.clf()

    ##########
    # Faults #
    ##########
    scenarios_small = ["unsorted", "sorted"]
    scenarios_large = ["unsorted_min_crossing", "sorted_min_crossing"]
    #for dataset in ["_", "_alibaba_"]:
    for dataset in ["_alibaba_"]:
        glob_df = pd.DataFrame()
        for net in networks:
            scenarios = []
            if "Large" in net:
                scenarios = scenarios_large
            else:
                scenarios = scenarios_small
            for scenario in scenarios:                                
                faults_list = []
                if "Small" in net:
                    faults_list = [0, 20, 40]
                else:
                    faults_list = [0, 50, 100]
                for f in faults_list:
                    faults_suffix = ""
                    if f != 0:
                        faults_suffix = "_faults-" + str(f)
                    if "min_crossing" in scenario:
                        filename = "logs/log_" + net + dataset + scenario + "_a2a" + faults_suffix + "_full.csv" # For system utilization we only consider the minimization on A2A pattern (results on AR are the same)                                
                    else:
                        filename = "logs/log_" + net + dataset + scenario + "" + faults_suffix + "_full.csv"
                    
                    if not os.path.isfile(filename):
                        print("[Not found] " + filename)
                        continue
                    df = pd.read_csv(filename, delimiter="\t")
                    df["Scenario"] = scenario_to_name[scenario]
                    df["Network"] = net
                    if "unsorted" in scenario:
                        df["NetworkAndScenario"] = net + " (Unsorted Jobs)"
                    else:
                        df["NetworkAndScenario"] = net + " (Sorted Jobs)"
                    df["Faults"] = str(f)
                    df["System Utilization (%)"] = df["Utilization"].astype(float) * 100.0   
                    glob_df = pd.concat([glob_df, df])
        
        glob_df.reset_index(inplace=True)    
        glob_df = glob_df.drop(glob_df[glob_df.Utilization == -1].index)        
        #fig, axes = plt.subplots(1, 2, sharey=True, figsize=(10,3))
        fig, axes = plt.subplots(1, 2, sharey=True, figsize=(10,2.4))
        hue_order = ["Hx2Small (Unsorted Jobs)", "Hx2Small (Sorted Jobs)", "Hx4Small (Unsorted Jobs)", "Hx4Small (Sorted Jobs)"]
        ax = sns.violinplot(data=glob_df[glob_df['Network'].str.startswith("Hx2Small") | glob_df['Network'].str.startswith("Hx4Small")], x="Faults", y="System Utilization (%)", hue="NetworkAndScenario", hue_order=hue_order, cut=0, linewidth=0, ax=axes[0], saturation=1)
        overlay_markers_faults(glob_df, False, ax)
        #ax.get_legend().set_title(None)
        ax.get_legend().remove()
        ax.set_ylim(ymin=50, ymax=102)
        ax_one = ax
        if show_legend:
            fig.legend(loc='upper center', bbox_to_anchor=(0.25, 1), ncol=2)

        hue_order = ["Hx2Large (Unsorted Jobs)", "Hx2Large (Sorted Jobs)", "Hx4Large (Unsorted Jobs)", "Hx4Large (Sorted Jobs)"]
        ax = sns.violinplot(data=glob_df[glob_df['Network'].str.startswith("Hx2Large") | glob_df['Network'].str.startswith("Hx4Large")], x="Faults", y="System Utilization (%)", hue="NetworkAndScenario", hue_order=hue_order, cut=0, linewidth=0, ax=axes[1], saturation=1)
        overlay_markers_faults(glob_df, True, ax)
        ax.set_ylabel("")
        #ax.get_legend().set_title(None)
        ax.get_legend().remove()
        ax.set_ylim(ymin=50, ymax=102)       
        ax_two = ax
        patches = [mpatches.Patch(color=sns.color_palette()[l], label=hue_order[l]) for l in range(len(hue_order))]
        if show_legend:
            fig.legend(patches, hue_order, loc='upper center', bbox_to_anchor=(0.75, 1), ncol=2)
        plt.tight_layout()
        if show_legend:
            for ax in [ax_one, ax_two]:
                box = ax.get_position()
                ax.set_position([box.x0, box.y0 - box.height * 0.115, box.width, box.height * 0.885])
        fig.savefig('plots/faults' + dataset + show_legend_suffix + '.pdf', format='pdf', dpi=100)
        plt.clf()

    #################
    # Job sizes CDF #
    #################  
    fig, axes = plt.subplots(1, 1, figsize=(5,2.4))
    #df_synthetic = pd.DataFrame()
    #df_synthetic["Job Size"] = gen_jobs([64, 64], [2, 2], 100, [32, 32], 0, False, False)
    #df_synthetic["Job Size"] *= 2*2 # 2x2 boards such that we plot the number of accelerators!
    #df_synthetic["Type"] = "Synthetic (sampled from PDF=2048/e)"
    df_sampled = pd.DataFrame()
    df_sampled["Job Size"] = gen_jobs([64, 64], [2, 2], 100, [32, 32], 0, False, True)
    df_sampled["Job Size"] *= 2*2 # 2x2 boards such that we plot the number of accelerators!
    df_sampled["Type"] = "Sampled"
    tot_nodes_sampled = np.sum(df_sampled["Job Size"])

    df_sampled_vc = pd.DataFrame()
    df_sampled_vc["Values"], df_sampled_vc["Count"] = np.unique(df_sampled["Job Size"], return_counts=True)
    df_sampled_vc["VC"] = ((df_sampled_vc["Values"] * df_sampled_vc["Count"]) / tot_nodes_sampled)*100.0
    df_sampled_vc["CumulativeVC"] = df_sampled_vc["VC"].cumsum()
    df_sampled_vc["Type"] = "Sampled"

    df_original = pd.read_csv("alibaba_raw.csv", sep=",", index_col=None, names=["Dummy", "Job Size"], header=0)
    df_original["Job Size"] *= 2*2 # 2x2 boards such that we plot the number of accelerators!
    #df_original['prob'] = df_original['perc_jobs_lte'] - df['perc_jobs_lte'].shift(1)
    #df_original.loc[0, 'prob'] = df_original.loc[0, 'perc_jobs_lte'] # Prob is the probability that a job will have that size
    df_original["Type"] = "Original"
    tot_nodes_original = np.sum(df_original["Job Size"])

    df_original_vc = pd.DataFrame()
    df_original_vc["Values"], df_original_vc["Count"] = np.unique(df_original["Job Size"], return_counts=True)
    df_original_vc["VC"] = ((df_original_vc["Values"] * df_original_vc["Count"]) / tot_nodes_original)*100.0
    df_original_vc["CumulativeVC"] = df_original_vc["VC"].cumsum()
    df_original_vc["Type"] = "Original"
    
    df = pd.concat([df_original_vc, df_sampled_vc])
    df.reset_index(inplace=True)    

    ax = sns.lineplot(data=df, x="Values", y="CumulativeVC", hue="Type", ax=axes)
    #ax = sns.ecdfplot(df, x="Values", hue="Type", ax=axes) 
    ax.set(xlabel ="Job Size - Cumulative Distribution (CDF)", ylabel = "Proportion")
    ax.get_legend().set_title(None)
    ax.set_xscale("log")
    #plt.axvline(np.max(df_original["Job Size"]), color='blue')
    #plt.axvline(np.max(df_sampled["Job Size"]), color='orange')

    '''
    bins = [0, 128, 256, 512, 1024, 2048, 4096, 8192]

    df_original_vc = pd.DataFrame()    
    values, counts = np.unique(df_original["Job Size"], return_counts=True)
    df_original_vc["Value"] = values
    df_original_vc["Count"] = counts
    df_original_vc['bin'] = pd.cut(df_original_vc['Value'], bins=bins, labels=[f'{bins[l]}-{bins[l+1]}' for l in range(0,len(bins) - 1)])   
    df_original_vc = df_original_vc.groupby(["bin"]).sum().reset_index()
    df_original_vc["ValueCount"] = ((df_original_vc["Value"]) / 32768)*100.0
    df_original_vc["Type"] = "Original"
    
    df_sampled_vc = pd.DataFrame()
    values, counts = np.unique(df_sampled["Job Size"], return_counts=True)   
    df_sampled_vc["Value"] = values
    df_sampled_vc["Count"] = counts
    #pd.set_option('display.max_rows', 100000)
    df_sampled_vc['bin'] = pd.cut(df_sampled_vc['Value'], bins=bins, labels=[f'{bins[l]}-{bins[l+1]}' for l in range(0,len(bins) - 1)])   
    df_sampled_vc = df_sampled_vc.groupby(["bin"]).sum().reset_index()
    df_sampled_vc["ValueCount"] = ((df_sampled_vc["Value"]) / 32768)*100.0
    df_sampled_vc["Type"] = "Sampled"

    df_vc = pd.concat([df_original_vc, df_sampled_vc])
    df_vc.reset_index(inplace=True)    
       
    ax = sns.barplot(data=df_vc, x="bin", y="ValueCount", hue="Type", saturation=1, ax = axes[1])    
    ax.set_xticklabels(ax.get_xticklabels(), rotation=25)
    ax.set(xlabel ="Job Size", ylabel = "Share of nodes occupied\nby jobs of that size")
    ax.get_legend().set_title(None)
    '''
    
    plt.tight_layout()
    ax.figure.savefig('plots/jobs_size_cdf.pdf', format='pdf', dpi=100)        

    #ax.set_xscale('log')
    #plt.tight_layout()
    #ax.figure.savefig('../plots/allocation/jobs_size_cdf_log.pdf', format='pdf', dpi=100)        
    #plt.clf()
    
if __name__ == "__main__":
    main()

