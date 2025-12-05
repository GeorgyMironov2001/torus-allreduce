#!/usr/bin/env python3
import numpy as np
import random
import seaborn as sns
import matplotlib.pyplot as plt
import argparse
import sys
import itertools
import math
import pandas as pd

class TreeInfo:
    def __init__(self):
        self.lvl_0_traff = 0 # All the traffic (either originated by a node connected to this tree or not)
        self.lvl_1_traff = 0 # All the traffic (either originated by a node connected to this tree or not)
        self.lvl_0_traff_indirect = 0 # Traffic originated by a node not connected to this tree 
        self.lvl_1_traff_indirect = 0 # Traffic originated by a node not connected to this tree
    
    def __add__(self, other):
        res = TreeInfo()
        res.lvl_0_traff = self.lvl_0_traff + other.lvl_0_traff
        res.lvl_1_traff = self.lvl_1_traff + other.lvl_1_traff
        res.lvl_0_traff_indirect = self.lvl_0_traff_indirect + other.lvl_0_traff_indirect
        res.lvl_1_traff_indirect = self.lvl_1_traff_indirect + other.lvl_1_traff_indirect
        return res
    
    def __truediv__(self, d : int):
        res = TreeInfo()
        res.lvl_0_traff = self.lvl_0_traff / float(d)
        res.lvl_1_traff = self.lvl_1_traff / float(d)
        res.lvl_0_traff_indirect = self.lvl_0_traff_indirect / float(d)
        res.lvl_1_traff_indirect = self.lvl_1_traff_indirect / float(d)
        return res

    def set_lvl_0_traff(self, c):
        self.lvl_0_traff = c

    def set_lvl_1_traff(self, c):
        self.lvl_1_traff = c

    def set_lvl_0_traff_indirect(self, c):
        self.lvl_0_traff_indirect = c

    def set_lvl_1_traff_indirect(self, c):
        self.lvl_1_traff_indirect = c

    def inc_lvl_0_traff(self):
        self.lvl_0_traff += 1

    def inc_lvl_1_traff(self):
        self.lvl_1_traff += 1

    def inc_lvl_0_traff_indirect(self):
        self.lvl_0_traff_indirect += 1

    def inc_lvl_1_traff_indirect(self):
        self.lvl_1_traff_indirect +=1

    def get_lvl_1_traff_ratio(self) -> float:
        if self.lvl_0_traff:
            return float(self.lvl_1_traff) / float(self.lvl_0_traff)
        else:
            return 0.0

    def get_lvl_1_traff_ratio_no_indirect(self) -> float:
        #print("1: " + str(self.lvl_1_traff) + " " + str(self.lvl_1_traff_indirect) + " " + str(self.lvl_0_traff) + " " + str(self.lvl_0_traff_indirect))
        if self.lvl_0_traff - self.lvl_0_traff_indirect:
            return float(self.lvl_1_traff - self.lvl_1_traff_indirect) / float(self.lvl_0_traff - self.lvl_0_traff_indirect)
        else:
            return 0.0

    def get_ratio_indirect(self) -> float:
        #print("2: " + str(self.lvl_1_traff) + " " + str(self.lvl_1_traff_indirect) + " " + str(self.lvl_0_traff) + " " + str(self.lvl_0_traff_indirect) + " " + str(float(self.lvl_0_traff_indirect) / float(self.lvl_0_traff)))
        if self.lvl_0_traff:
            return float(self.lvl_0_traff_indirect) / float(self.lvl_0_traff)
        else:
            return 0.0


class GlobalLinksInfo:
    def __init__(self, nrows, ncols):
        self.conns = 0
        self.info_rows = []
        self.info_cols = []
        for i in range(nrows):
            self.info_rows.append(TreeInfo())
        for i in range(ncols):
            self.info_cols.append(TreeInfo())
    
    def __add__(self, other):
        res = GlobalLinksInfo(len(self.info_rows), len(self.info_cols))
        res.conns = self.conns + other.conns
        for row in range(len(self.info_rows)):
            res.info_rows[row] = self.info_rows[row] + other.info_rows[row]
        for col in range(len(self.info_cols)):
            res.info_cols[col] = self.info_cols[col] + other.info_cols[col]
        return res
    
    def __truediv__(self, d : int):
        res = GlobalLinksInfo(len(self.info_rows), len(self.info_cols))
        res.conns = self.conns / float(d)
        for row in range(len(self.info_rows)):
            res.info_rows[row] = self.info_rows[row] / float(d)
        for col in range(len(self.info_cols)):
            res.info_cols[col] = self.info_cols[col] / float(d)
        return res

    def get_info_row(self, row: int) -> TreeInfo:
        return self.info_rows[row]

    def get_info_col(self, col: int) -> TreeInfo:
        return self.info_cols[col]

    def inc_conns(self):
        self.conns += 1
    
    def get_lvl_1_traff_ratio_no_indirect(self) -> float:       
        tot = 0
        cnt = 0
        if len(self.info_cols)*2 > 64: # Compute lvl 1 utilization only if there is a lvl 1
            for ir in self.info_rows:
                tot += ir.get_lvl_1_traff_ratio_no_indirect()
                cnt += 1

        if len(self.info_rows)*2 > 64: # Compute lvl 1 utilization only if there is a lvl 1
            for ic in self.info_cols:
                tot += ic.get_lvl_1_traff_ratio_no_indirect()
                cnt += 1
        if tot:
            return tot / float(cnt)
        else:
            return 0        

    def get_lvl_1_traff_ratio(self) -> float:
        tot = 0
        cnt = 0
        if len(self.info_cols)*2 > 64: # Compute lvl 1 utilization only if there is a lvl 1
            #print("Has row trees")
            for ir in self.info_rows:
                tot += ir.get_lvl_1_traff_ratio()
                cnt += 1
        if len(self.info_rows)*2 > 64: # Compute lvl 1 utilization only if there is a lvl 1                
            #print("Has col trees")
            for ic in self.info_cols:
                tot += ic.get_lvl_1_traff_ratio()
                cnt += 1
        if tot:
            return tot / float(cnt)
        else:
            return 0

    def get_ratio_indirect(self) -> float:
        tot = 0
        cnt = 0
        for ir in self.info_rows:
            tr = ir.get_ratio_indirect()
            tot += tr 
            cnt += 1
        for ic in self.info_cols:
            tr = ic.get_ratio_indirect()
            tot += tr
            cnt += 1
        if tot:
            return tot / float(cnt)
        else:
            return 0   

    def __repr__(self): # Connections Lvl1TraffRatio Lvl1TraffRatioNoIndirect RatioIndirect
        return str(self.conns) + "\t" + str(self.get_lvl_1_traff_ratio()) + "\t" + str(self.get_lvl_1_traff_ratio_no_indirect()) + "\t" + str(self.get_ratio_indirect())

    def is_better_than(self, other : TreeInfo) -> bool:
        if other == None:
            return True
        else:
            return self.get_lvl_1_traff_ratio() < other.get_lvl_1_traff_ratio()


def decompose(num, aspect_ratio):
    bestfound = [1,num]
    bestratio = 1/num
    for a in range(1,num): # iterate all possible factors
        b = int(num/a)
        if(b == num/a): # i divides num
            if args.print_nid_lists and a % b:
                continue # Skip jobs where num rows is not multiple of num cols
            ratio = min(a,b)/max(a,b) 
            #print(a, "x", b,":", ratio, " ", aspect_ratio, " ", bestratio)
            if(abs(ratio-aspect_ratio) < abs(bestratio-aspect_ratio)):
                bestratio=ratio
                bestfound=[a,b]
    return bestfound

def decompose_list(num, aspect_ratio):
    decomps = []
    for a in range(1,num): # iterate all possible factors
        b = int(num/a)
        if(b == num/a): # i divides num
            if args.print_nid_lists and a % b:
                continue # Skip jobs where num rows is not multiple of num cols
            ratio = min(a,b)/max(a,b) 
            #print(a, "x", b,":", ratio, " ", aspect_ratio, " ", bestratio)
            decomps.append([abs(ratio-aspect_ratio),a,b])
    decomps.sort()
    return decomps

def get_random_alibaba_job(df):
    rnd = random.random() * 100
    cumul = 0
    sel = 0
    for index, row in df.iterrows():
        cumul += row['prob']
        if cumul > rnd:
            sel = row['scaled_boards']
            break
    return int(sel)

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
        if alibaba:
            jobsize = get_random_alibaba_job(df)
        else:
            #jobsize=random.randrange(1,size+1)
            #avg_job_size = size/4
            avg_job_size = size/16 # TODO (?)
            jobsize = int(np.random.exponential(1)*avg_job_size+1)
        jobsize = min(jobsize, size) # To avoid having the last job bigger than the number of remaining nodes        
        if single_job:
            jobsize = size
        [a,b] = decompose(jobsize, 1)
        # only use job shapes that fit dimensions of hxnet!
        if(single_job or (max(max_dims) >= max([a,b]) and min(max_dims) >= min(a,b))):            
            size -= jobsize
            #print(jobsize,":", a,"x", b)
            jobs.append([a,b])
    return jobs

# insertion sort jobs by size
def sort_jobs(jobs):
    #key = map(lambda a,b: a*b, jobs)
    keys = [a*b for [a,b] in jobs]
    srt_jobs=[x for _, x in sorted(zip(keys, jobs), reverse=True)]
    return srt_jobs

def add_jobid(jobs):
    id=1
    jobs_with_id = []
    for job in jobs:
        job.append(id)
        jobs_with_id.append(job)
        id += 1
    return jobs_with_id

def init_hxnet(shape):
    hxnet = np.zeros(shape, np.int)
    return hxnet

def print_hxnet(hxnet):
    #print(hxnet.shape)
    print(hxnet)

def terminate_job(hxnet, jobid):
    hxnet[hxnet == jobid] = 0
    return hxnet


def generate_partners_allreduce(pairs_matrix, X, Y, a, b):
    # For the allreduce, just with the N, S, W, E neighbors
    partners = []
    if len(pairs_matrix[b]) > 1:
        partners += [pairs_matrix[b][(a+1)%X]] # E
        partners += [pairs_matrix[b][(a-1)%X]] # W
    if len(pairs_matrix) > 1:
        partners += [pairs_matrix[(b+1)%Y][a]] # S
        partners += [pairs_matrix[(b-1)%Y][a]] # N
    return partners

def generate_partners_alltoall(pairs_matrix, X, Y, a, b):
    # For the alltoall, everyone talks to everyone
    partners = []
    for g in range(0, X):
        for h in range(0, Y):
            if g != a or h != b:
                partners += [pairs_matrix[h][g]]
    return partners


# Computes the distance between <a,b> and <s,t>
# We assume 64-ports switches used in the fat tree
def compute_distance(ab, st, tot_rows, tot_cols, gli : GlobalLinksInfo):
    a = ab[1]
    b = ab[0]
    s = st[1]
    t = st[0]
    tree_rows = False
    tree_cols = False
    # Compute the leaf switch IDs to which they are attached
    if tot_rows*2 <= 64: # *2 because each mesh requires two connections to the tree
        switch_b = 0
        switch_t = 0
        tree_rows = False
    elif tot_rows*2 <= 32*32:
        switch_b = math.floor(float(b) / 16) # 16 meshes for each 32 ports switch
        switch_t = math.floor(float(t) / 16) # 16 meshes for each 32 ports switch
        tree_rows = True
    else: # For the moment, we do not do the calculation for 3-level fat trees
        sys.exit("Code to compute link utilization for 3-level fat trees not yet implemented.")

    if tot_cols*2 <= 64:
        switch_a = 0
        switch_s = 0
        tree_cols = False
    elif tot_cols*2 <= 32*32:
        switch_a = math.floor(float(a) / 16) # 16 meshes for each 32 ports switch
        switch_s = math.floor(float(s) / 16) # 16 meshes for each 32 ports switch
        tree_cols = True
    else: # For the moment, we do not do the calculation for 3-level fat trees
        sys.exit("Code to compute link utilization for 3-level fat trees not yet implemented.")
    
    #print(str(tot_cols) + "x" + str(tot_rows))
    #print(str(a) + " " + str(b) + " -> " + str(s) + " " + str(t))
    #print(str(switch_a) + " " + str(switch_b) + " -> " + str(switch_s) + " " + str(switch_t))

    # We count number of switches crossed, not number of links (for number of links just multiply add 2 instead of 1, and 4 instead of 2)
    if a == s: # Same column 
        info = gli.get_info_col(a)
        info.inc_lvl_0_traff()        
        if switch_b != switch_t: 
            info.inc_lvl_1_traff()
    elif b == t: # Same row
        info = gli.get_info_row(b)
        info.inc_lvl_0_traff()    
        if switch_a != switch_s:             
            info.inc_lvl_1_traff()            
    else: # Different column and row
        route = random.choice(["XY", "YX"]) # Randomly do XY or YX
        indirect = None # Where we should account the indirect traffic
        if route == "XY":
            row_tree = b
            col_tree = s
            indirect = "col"
        else:
            col_tree = a
            row_tree = t
            indirect = "row"

        info_col = gli.get_info_col(col_tree)        
        info_col.inc_lvl_0_traff()
        if indirect == "col":
            info_col.inc_lvl_0_traff_indirect()
        if switch_b != switch_t: 
            info_col.inc_lvl_1_traff()
            if indirect == "col":
                info_col.inc_lvl_1_traff_indirect()

        info_row = gli.get_info_row(row_tree)
        info_row.inc_lvl_0_traff()
        if indirect == "row":
            info_row.inc_lvl_0_traff_indirect()
        if switch_a != switch_s: 
            info_row.inc_lvl_1_traff()
            if indirect == "row":
                info_row.inc_lvl_1_traff_indirect()

# Pairs matrix is a matrix containing the coordinates where the job was
# allocated. E.g.
# <0,0> <0,1> <0,3>
# <2,0> <2,1> <2,3>
def estimate_tree_links_ut(pairs_matrix, X, Y, tot_rows, tot_cols):
    #print(pairs_matrix)
    gli_a2a = GlobalLinksInfo(tot_rows, tot_cols)
    gli_ar = GlobalLinksInfo(tot_rows, tot_cols)
    for a in range(0, X):
        for b in range(0, Y):
            # Now we are on a specific board. Let's see with whom it communicates.            
            partners = generate_partners_alltoall(pairs_matrix, X, Y, a, b)
            for p in partners:
                gli_a2a.inc_conns()
                compute_distance(pairs_matrix[b][a], p, tot_rows, tot_cols, gli_a2a)

            partners = generate_partners_allreduce(pairs_matrix, X, Y, a, b)            
            for p in partners:
                gli_ar.inc_conns()
                compute_distance(pairs_matrix[b][a], p, tot_rows, tot_cols, gli_ar)
    return (gli_a2a, gli_ar)

def print_nid_list(hxnet, jobid, board_shape):
    if board_shape[0] == 0 or board_shape[1] == 0:
        sys.exit("Unknown board shape")
    nodes_per_board = board_shape[0]*board_shape[1]
    nid_list = []
    start = 0
    num_cols = 0
    num_cols_ready = 0
    first_job_row_i = -1
    first_job_row_ii = -1
    for i in range(hxnet.shape[0]):
        for ii in range(board_shape[1]):
            for j in range(hxnet.shape[1]):
                start = (i*hxnet.shape[1] + j)*nodes_per_board + (ii*board_shape[0])
                if hxnet[i,j] == jobid:                            
                    if first_job_row_i == -1 and first_job_row_ii == -1:
                        #print("First job row set to " + str(i))
                        first_job_row_i = i
                        first_job_row_ii = ii
                    if i == first_job_row_i and ii == first_job_row_ii:
                        num_cols += board_shape[0]        
                    for id in range(start, start+board_shape[0]):
                        nid_list += [id]

    print("[JOB_ID] " + str(10 + jobid))
    print("[NID_LIST] " + ",".join([str(element) for element in nid_list]))
    print("[NUM_COLS] " + str(num_cols) + "\n")
    #print_hxnet(hxnet)
    return nid_list


def try_alloc(hxnet, job, permute, opt_rows_order, show_failures):
    #print("allocating job", job)
    a,b,id=job
    # for first row get available indexes!
    #for row in range(0,hxnet.shape[0]):
    idx = np.arange(hxnet.shape[1], dtype=np.int)    
    rows_identifiers = range(0, hxnet.shape[0])
    good_rows = []
    good_rows_zeros = []
    for row in rows_identifiers:
        if len(np.where(hxnet[row,] == 0)[0]) >= a:
            good_rows.append(row)
            good_rows_zeros.append(len(np.where(hxnet[row,] == 0)[0]))
    #print("Found " + str(len(good_rows)) + " good rows")
    if len(good_rows) < b:
        return (0, None, None)

    pairs = list(zip(good_rows_zeros, good_rows))
    pairs.sort(reverse=True)
    optimal_rows_order = [list(t) for t in zip(*pairs)][1]
    
    permutations_list = []
    permutations_list.append(good_rows.copy())
    if permute:
        permutations_list.append(optimal_rows_order.copy())
        #permutations_list = itertools.permutations(good_rows)
        # Better to permute the rows randomly since we only consider a few permutations
        for i in range(0, 100):
            random.shuffle(good_rows)
            #print(good_rows)
            permutations_list.append(good_rows.copy())
    elif opt_rows_order:
        if False:
            for i in range(0, 100):
                random.shuffle(optimal_rows_order)
                #print(good_rows)
                permutations_list.append(optimal_rows_order.copy())
        else:
            permutations_list = [optimal_rows_order]        
    else:
        permutations_list = [good_rows]
    found = False
    for rows_order in permutations_list:
        rows = []
        for cand_row in rows_order: # candidate row
            cand_idx = np.where(hxnet[cand_row,] == 0)[0]            
            cand_idx = np.intersect1d(idx, cand_idx) 
            # accept if we still have enough indexes to cover a
            if(len(cand_idx) >= a): 
                #print(len(cand_idx))
                #print(len(rows))
                #print(cand_idx)
                rows.append(cand_row)
                idx = cand_idx
                #print("adding row", cand_row, idx)
            if(len(rows) == b):
                break
        
        if(len(rows) == b):
            pairs_matrix = []
            rows = np.sort(rows)
            #print("success", job, idx, rows)            
            for i in range(0,b):
                pairs_matrix_row = []
                for j in range(0,a):
                    hxnet[rows[i], idx[j]] = id
                    pairs_matrix_row += [(rows[i], idx[j])]
                pairs_matrix += [pairs_matrix_row]            
            #print(id)
            #print_hxnet(hxnet)
            gli_a2a, gli_ar = estimate_tree_links_ut(pairs_matrix, a, b, hxnet.shape[0], hxnet.shape[1])
            #print(pairs_matrix)
            #print("successfully allocated", job)
            return (1, gli_a2a, gli_ar)

    if show_failures:
        print_hxnet(hxnet)
        print("failed to allocate", job)
    return (0, None, None)

    # find a rows with b indexes that are the same!

parser = argparse.ArgumentParser(description='Generate random jobs and computes allocations.')
parser.add_argument('--sort', action='store_true', help='Sorts the jobs by size before allocating them.')
parser.add_argument('--dummy', action='store_true', help='Dummy allocation of the jobs (just row by row as they arrive).')
parser.add_argument('--shape', help='HxNet shape (XxY, or Hx2Small/Hx2Large/Hx4Small/Hx4Large). default=Hx2Small', default="Hx2Small")
parser.add_argument('--samples', help='Number of samples.', default=1000)

parser.add_argument('--faults', help='Number of faulted boards.', default="0")
parser.add_argument('--permute', action='store_true', help='When looking for an allocation tries different rows permutations.')
parser.add_argument('--no_transpose', action='store_true', help='Also considers the transpose.')
parser.add_argument('--no_change_ratio', action='store_true', help='Also considers changes in the aspect ratio.')
parser.add_argument('--opt_rows_order', action='store_true', help='When looking for an allocation uses the optimal rows order (sorted by num of zero elements).')
parser.add_argument('--full_output', action='store_true', help='If specified, it also plots job sizes and utilization distribution.')
parser.add_argument('--show_failures', action='store_true', help='If specified, it shows what happens when an allocation fails.')
parser.add_argument('--no_log', action='store_true', help='If specified, it does not write logs.')
parser.add_argument('--single_job', action='store_true', help='If specified, allocates all the network to a single job.')
parser.add_argument('--min_crossing_a2a', action='store_true', help='If specified, tries to minimize the crossing of the upper levels of the trees (assuming a2a communication pattern).')
parser.add_argument('--min_crossing_ar', action='store_true', help='If specified, tries to minimize the crossing of the upper levels of the trees (assuming ar communication pattern).')
parser.add_argument('--print_nid_lists', action='store_true', help='If specified, prints NID lists for the jobs.')
parser.add_argument('--alibaba', action='store_true', help='If specified, uses alibaba traces instead of generating job sizes randomly.') # MLaaS in the Wild: Workload Analysis and Scheduling in Large-Scale Heterogeneous GPU Clusters


args = parser.parse_args()

if args.print_nid_lists and (int(args.samples) != 1 or not args.no_log):
    sys.exit("To use --print_nid_lists please also specify --samples 1 and --no_log")

if args.permute and args.opt_rows_order:
    sys.exit("You can just use one between --permute and --opt_rows_order")

if args.min_crossing_a2a and args.min_crossing_ar:
    sys.exit("You can just use one between --min_crossing_a2a and --min_crossing_ar")

np.set_printoptions(threshold=sys.maxsize,linewidth=np.inf)

shape = [64, 128] # HxNet dimensions
board_shape = [0, 0]
if args.shape:
    # Networks defined in paper Table
    if args.shape == 'Hx2Small':
        shape[1] = 16
        shape[0] = 16
        board_shape[0] = 2
        board_shape[1] = 2
    elif args.shape == 'Hx2Large':
        shape[1] = 64
        shape[0] = 64
        board_shape[0] = 2
        board_shape[1] = 2
    elif args.shape == 'Hx4Small':
        shape[1] = 8
        shape[0] = 8
        board_shape[0] = 4
        board_shape[1] = 4
    elif args.shape == 'Hx4Large':
        shape[1] = 32
        shape[0] = 32
        board_shape[0] = 4
        board_shape[1] = 4
    else:
        shape[1] = int(args.shape.split('x')[0])
        shape[0] = int(args.shape.split('x')[1])
        sys.exit("Unknown board shape")

if '%' in args.faults:
    broken = int((int(args.faults.split("%")[0])/100.0) * shape[0] * shape[1])
else:
    broken = int(args.faults)

max_dims = [shape[0]/2, shape[1]/2] # maximum job dimensions (1/4 of the system)

all_job_sizes = []
utilizations = []
glis_a2a = []
glis_ar = []
for sample in range(0, int(args.samples)):
    gli_a2a_sample = GlobalLinksInfo(shape[0], shape[1])
    gli_ar_sample = GlobalLinksInfo(shape[0], shape[1])
    jobs = gen_jobs(shape, board_shape, 100, max_dims, broken, args.single_job, args.alibaba)
    
    if args.sort:
        jobs = sort_jobs(jobs)
    jobs = add_jobid(jobs)

    hxnet = init_hxnet(shape)
    
    brokenidx = np.random.permutation(np.arange(0,shape[0]*shape[1]))[0:broken]
    hxnet[np.unravel_index(brokenidx, shape)]=-1
    #print_hxnet(hxnet)

    failed = 0
    for job in jobs:
        all_job_sizes.append(job[0]*job[1])
        a,b,id = job
        feasible_allocations = []



        if args.dummy:
            res = 1
            pairs_matrix = []
            pairs_matrix_row = []
            allocated = 0
            for i in range(0,hxnet.shape[0]):                
                for j in range(0,hxnet.shape[1]):
                    if hxnet[i, j] == 0:
                        hxnet[i, j] = id
                        allocated += 1
                        pairs_matrix_row += [(i, j)]
                        if len(pairs_matrix_row) == a:
                            pairs_matrix += [pairs_matrix_row]
                            pairs_matrix_row = []
                    if allocated == a*b:
                        break                
                if allocated == a*b:
                    break
            #print(str(a) + "x" + str(b))
            #print(pairs_matrix)
            gli_a2a, gli_ar = estimate_tree_links_ut(pairs_matrix, a, b, hxnet.shape[0], hxnet.shape[1])
            feasible_allocations += [(np.copy(hxnet), gli_a2a, gli_ar)]
            #print_hxnet(hxnet)
        else:
            hxnet_copy = np.copy(hxnet)
            (res, gli_a2a, gli_ar) = try_alloc(hxnet_copy, job, args.permute, args.opt_rows_order, args.show_failures)
            if res:
                feasible_allocations += [(np.copy(hxnet_copy), gli_a2a, gli_ar)]
                #print("0 Pushed allocation " + str(job) + " with quality " + str(used_switches_per_level_alltoall[1]) + " " + str(used_switches_per_level_allreduce[1]))
        if not args.no_transpose and (args.min_crossing_a2a or args.min_crossing_ar or res == 0):
            # try transposed!
            transposed_job = [b,a,id] 
            hxnet_copy = np.copy(hxnet)
            (res, gli_a2a, gli_ar) = try_alloc(hxnet_copy, transposed_job, args.permute, args.opt_rows_order, args.show_failures)
            if res:
                feasible_allocations += [(np.copy(hxnet_copy), gli_a2a, gli_ar)]
                #print("1 Pushed allocation " + str(transposed_job) + " with quality " + str(used_switches_per_level_alltoall[1]) + " " + str(used_switches_per_level_allreduce[1]))
        
        # try to allocate another decomposition within the limits
        accepted_aspect=1-1/8 # max 1:8 aspect ratio
        if not args.no_change_ratio and (args.min_crossing_a2a or args.min_crossing_ar or res == 0):
            #print("trying to change shape of job", job)
            for decomp in decompose_list(job[0]*job[1], 1):
                if(decomp[0] > accepted_aspect): continue
                new_job = [decomp[1], decomp[2], id]
                if args.show_failures:
                    print("Aspect ratio changed from ", job[0], " ", job[1], " to ", decomp[1], " ", decomp[2])
                hxnet_copy = np.copy(hxnet)
                (res, gli_a2a, gli_ar) = try_alloc(hxnet_copy, new_job, args.permute, args.opt_rows_order, args.show_failures)
                if res: 
                    #print("succeeded with new shape", new_job)
                    feasible_allocations += [(np.copy(hxnet_copy), gli_a2a, gli_ar)]
                    #print("2 Pushed allocation " + str(new_job) + " with quality " + str(used_switches_per_level_alltoall[1]) + " " + str(used_switches_per_level_allreduce[1]))
                    if not (args.min_crossing_a2a or args.min_crossing_ar):
                        break
            #if(res == 0): print("failed to allocate job finally", new_job)
            #print("decomp_opts:", job, decompose_list(job[0]*job[1], 1))

        # all attempts failed ...
        if len(feasible_allocations) == 0:
            failed += 1

        else:
            best_hxnet_copy = None
            best_gli_a2a = None
            best_gli_ar = None
            #print("== " + str(len(feasible_allocations)))
            for (hxnet_copy, gli_a2a, gli_ar) in feasible_allocations:                
                if (args.min_crossing_a2a and gli_a2a.is_better_than(best_gli_a2a)) or \
                   (args.min_crossing_ar and gli_ar.is_better_than(best_gli_ar)) or \
                   (not args.min_crossing_a2a and not args.min_crossing_ar):
                    best_gli_a2a = gli_a2a
                    best_gli_ar = gli_ar
                    best_hxnet_copy = np.copy(hxnet_copy)
            hxnet = np.copy(best_hxnet_copy)
            if args.print_nid_lists:
                print_nid_list(hxnet, id, board_shape)
            gli_a2a_sample += best_gli_a2a
            gli_ar_sample += best_gli_ar
            #print_hxnet(hxnet)


    #print(jobs)
    print_hxnet(hxnet)
    non_failed_boards = (hxnet.shape[0]*hxnet.shape[1] - len(np.where(hxnet == -1)[0]))
    if non_failed_boards:
        utilization=1-len(np.where(hxnet == 0)[0])/non_failed_boards
        #print("[",sample,"] succeeded: ", len(jobs)-failed, "failed:", failed, "utilization:",utilization )
        utilizations.append(utilization)
        glis_a2a += [gli_a2a_sample]
        glis_ar += [gli_ar_sample]

#print(sorted(all_job_sizes))

suffix = "_" + args.shape

if args.alibaba:
    suffix += "_alibaba"

if args.no_transpose:
    suffix += "_notranspose"

if args.no_change_ratio:
    suffix += "_nochangeratio"

if args.dummy:
    suffix += "_dummy"
elif args.sort:
    suffix += "_sorted"
elif args.single_job:
    suffix += "_single_job"
else:
    suffix += "_unsorted"

if args.min_crossing_a2a:
    suffix += "_min_crossing_a2a"

if args.min_crossing_ar:
    suffix += "_min_crossing_ar"

if args.permute:
    suffix += "_perm"
elif args.opt_rows_order:
    suffix += "_optrowsorder"

if int(args.faults[0]) != 0:
    suffix += "_faults-" + args.faults


if len(utilizations):
    util_avg = sum(utilizations)/len(utilizations)
    util_99p = sorted(utilizations, reverse=True)[int(len(utilizations)*0.99)]
else:
    util_avg = -1
    util_99p = -1
if not args.print_nid_lists:
    print("average utilization", util_avg)
    print("99 percentile:", util_99p)

if not args.no_log:
    log = open('logs/log' + suffix + '.csv', 'w')
    log.write("AverageUtilization\t99pUtilization\n")
    #print(str(util_avg) + "\t" + str(util_99p) + "\n")
    log.write(str(util_avg) + "\t" + str(util_99p) + "\n")
    log.close()

if args.full_output:
    ax = sns.ecdfplot(all_job_sizes)
    ax.set_xscale('log')
    ax.figure.savefig('out/job_sizes/ecdf' + suffix + '.pdf', format='pdf', dpi=100)        
    plt.clf()

    ax = sns.ecdfplot(all_job_sizes, complementary=True)
    ax.set_xlim(left=1)        
    ax.figure.savefig('out/job_sizes/eccdf' + suffix + '.pdf', format='pdf', dpi=100)        
    plt.clf()

    ax = sns.histplot(all_job_sizes)
    ax.figure.savefig('out/job_sizes/' + suffix + '.pdf', format='pdf', dpi=100)
    plt.clf()

    ax = sns.histplot(utilizations)
    ax.figure.savefig('out/utilizations/' + suffix + '.pdf', format='pdf', dpi=100)
    plt.clf()

    values, counts = np.unique(all_job_sizes, return_counts=True)
    vc = values*counts
    print(str((np.sum(all_job_sizes)/(shape[0]*shape[1]))*100.0))
    print(values)
    print(counts)
    print(vc)
    vc = (vc / (int(args.samples)*shape[0]*shape[1]))*100
    print(vc)
    vc_binned = []
    last_start = 0
    exponential_bins = True
    if exponential_bins:
        last_end = 8 
    else:
        last_end = 1
    next_acc = 0
    bins = []
    #vc = counts
    for v in range(len(vc)):
        if values[v] >= last_start and values[v] <= last_end:
            next_acc += vc[v]
        else:
            vc_binned += [next_acc]
            bins += ["<" + str(last_end)]                
            next_acc = vc[v]
            while True:
                last_start = last_end + 1
                if exponential_bins:
                    last_end *= 2 # Exponentially sized bins
                else:
                    last_end += 1
                if last_end > values[v]:
                    break                    
    vc_binned += [next_acc]
    bins += ["<" + str(last_end)]

    ax = sns.barplot(x=bins,y=vc_binned)
    ax.set_xticklabels(ax.get_xticklabels(), rotation=90)
    ax.figure.savefig('out/job_val_cnt/' + suffix + '.pdf', format='pdf', dpi=100)
    plt.clf()
#print(suffix)

if not args.no_log:
    log = open('logs/log' + suffix + '_full.csv', 'w')
    log.write("Utilization\tConnections(A2A)\tLvl1TraffRatio(A2A)\tLvl1TraffRatioNoIndirect(A2A)\tRatioIndirect(A2A)\tConnections(AR)\tLvl1TraffRatio(AR)\tLvl1TraffRatioNoIndirect(AR)\tRatioIndirect(AR)\n")
    for i in range(0, len(utilizations)):
        log.write(str(utilizations[i]) + "\t" + str(glis_a2a[i]) + "\t" + str(glis_ar[i]) + "\n")
    log.close()

