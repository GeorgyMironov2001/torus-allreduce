import subprocess
import sys
from argparse import ArgumentParser
import shutil
import os
import re
import time

# Change this to Path later
logs_folder = "logs"
sbatch_folder = "sbatch"
ember_load_folder = "/home/gera/torus-allreduce/scratch/src/sst-elements/src/sst/elements/ember/test/"
sstsim_path = "/home/gera/torus-allreduce/local/sstcore-15.0.0/libexec/sstsim.x"

EMBERTREES_SPINELEAF = (
    "/home/gera/torus-allreduce/scratch/src/sst-elements/src/sst/elements/"
    "ember/mpi/motifs/embertrees/spineleaf"
)
EMBERTREES_DRAGONFLY = (
    "/home/gera/torus-allreduce/scratch/src/sst-elements/src/sst/elements/"
    "ember/mpi/motifs/embertrees/dragonfly"
)
EMBERTREES_ROUTETABLES = (
    "/home/gera/torus-allreduce/scratch/src/sst-elements/src/sst/elements/"
    "ember/mpi/motifs/emberroutingtables"
)

# Number of allreduce trees used by TreesAllreduce for a given job size.
TREES_BY_JOB_SIZE = {
    "8x4": 2,
    "8x8": 4,
    "16x16": 4,
    "32x32": 4,
    "64x64": 4,
}


def num_ranks_from_shape(shape):
    # torus/hyperx: "8x8"; spineleaf Merlin: "4,4:16"; dragonfly: "2:4:1:9"
    if ":" in shape:
        parts = shape.split(":")
        if len(parts) == 4:
            p, a, _n, g = (int(x) for x in parts)
            return p * a * g
        # S,S:L → S * L hosts (2-level SL)
        left, leaves = shape.split(":")
        spines = int(left.split(",")[0])
        return spines * int(leaves)
    num_ranks = 1
    for part in shape.replace(",", "x").split("x"):
        if part:
            num_ranks *= int(part)
    return num_ranks


def num_trees_for_bench(bench, shape):
    if bench == "Trees":
        if shape not in TREES_BY_JOB_SIZE:
            print(
                f"Warning: unknown num_trees for job_size {shape}, using 4"
            )
        return TREES_BY_JOB_SIZE.get(shape, 4)
    if bench in ("Overlay", "DBTree"):
        return 1
    return 1


def compute_short_value(bench, count, num_ranks, num_trees=1):
    if bench == "SwingL":
        return 8 * count
    elif bench == "SwingB":
        return 4 * count
    elif bench in ("Overlay", "DBTree", "Rings", "Bucket"):
        return 4 * count
    elif bench == "Trees":
        return 2 * count
    else:
        return None


def apply_short_value_to_default_params(default_params_path, short_value):
    with open(default_params_path, "r") as f:
        content = f.read()
    new_content, replacements = re.subn(
        r"^(\s*)valueShort\s*=\s*.+$",
        rf"\1valueShort = {short_value}",
        content,
        flags=re.MULTILINE,
    )
    if replacements == 0:
        print(
            f"Warning: could not patch valueShort in {default_params_path}"
        )
        return
    with open(default_params_path, "w") as f:
        f.write(new_content)


def tree_edge_length_from_route_table(route_table_file):
    """Extract tree edge length from ember_bdmst_routing_table_8_6.json -> 6."""
    if not route_table_file:
        return None
    basename = os.path.basename(route_table_file)
    match = re.search(r"ember_bdmst_routing_table_\d+_(\d+)\.json$", basename)
    if match:
        return match.group(1)
    return None


# VCs/VN for torus_trees turn+dateline bump (from ecdg_check_torus_trees.py).
# num_vc = max_vc + 1 on full Trees_k RS+AG flow set.
NUM_VCS_TREES_16x16 = {
    1: 5,
    2: 4,
    3: 5,
    4: 4,
    5: 4,
    6: 2,
    7: 2,
    8: 3,
    9: 3,
    10: 4,
    11: 4,
    12: 2,
    13: 2,
    14: 2,
    15: 2,
}

NUM_VCS_TREES_32x32 = {
    1: 5,
    2: 5,
    3: 4,
    4: 4,
    5: 4,
    6: 4,
    7: 4,
    8: 3,
    9: 2,
    10: 4,
    11: 4,
    12: 4,
    13: 2,
    14: 2,
    15: 4,
    16: 4,
    17: 5,
    18: 5,
    19: 4,
    20: 2,
    21: 4,
    22: 4,
    23: 4,
    24: 2,
    25: 2,
    26: 2,
    27: 2,
    28: 2,
    29: 2,
    30: 2,
    31: 2,
    32: 1,
}

NUM_VCS_TREES_64x64 = {
    1: 5,
    2: 5,
    3: 5,
    4: 5,
    5: 4,
    6: 5,
    7: 4,
    8: 4,
    9: 4,
    10: 4,
    11: 4,
    12: 4,
    13: 3,
    14: 3,
    15: 4,
    16: 3,
    17: 4,
    18: 4,
    19: 4,
    20: 2,
    21: 3,
    22: 3,
    23: 3,
    24: 4,
    25: 4,
    26: 3,
    27: 4,
    28: 3,
    29: 2,
    30: 2,
    31: 2,
    32: 4,
    33: 4,
    34: 4,
    35: 4,
    36: 4,
    37: 2,
    38: 2,
    39: 3,
    40: 3,
    41: 2,
    42: 2,
    43: 2,
    44: 2,
    45: 2,
    46: 2,
    47: 2,
    48: 2,
    49: 2,
    50: 2,
    51: 2,
    52: 3,
    53: 2,
    54: 2,
    55: 2,
    56: 2,
    57: 2,
    58: 2,
    59: 2,
    60: 2,
    61: 2,
    62: 2,
    63: 2,
    64: 1,
}

NUM_VCS_TREES_BY_SHAPE = {
    "16x16": NUM_VCS_TREES_16x16,
    "32x32": NUM_VCS_TREES_32x32,
    "64x64": NUM_VCS_TREES_64x64,
}


def num_vcs_for_trees_route_table(route_table_file, shape="16x16"):
    """Return --numVCs for torus_trees given the BDMST route-table JSON.

    Falls back to 5 (worst case on tabulated shapes) if k/shape is unknown.
    """
    k_s = tree_edge_length_from_route_table(route_table_file)
    if k_s is None:
        return 5
    k = int(k_s)
    table = NUM_VCS_TREES_BY_SHAPE.get(shape)
    if table is not None:
        return table.get(k, 5)
    # Other shapes not tabulated yet — keep the safe upper bound.
    return 5


def tree_edge_length_from_tree_set(tree_set):
    """tree_set 0 -> d1, 1 -> d2, ... (k = tree_set + 1)."""
    edge_length = int(tree_set) + 1
    if edge_length < 1 or edge_length > 64:
        assert False, f"Invalid tree_set {tree_set}"
    return str(edge_length)


def parse_tree_sets(spec):
    """Parse tree_set list: '0', '0,2,4', '0-7', '7-0', or 'all'.

    Order is preserved (descending ranges like 7-0 stay reversed).
    """
    if spec is None or str(spec).strip() == "":
        return [0]
    spec = str(spec).strip()
    if spec.lower() == "all":
        return list(range(8))
    tree_sets = []
    for part in spec.split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            start_s, end_s = part.split("-", 1)
            start, end = int(start_s), int(end_s)
            if start <= end:
                tree_sets.extend(range(start, end + 1))
            else:
                tree_sets.extend(range(start, end - 1, -1))
        else:
            tree_sets.append(int(part))
    # unique, preserve order
    return list(dict.fromkeys(tree_sets))


def parse_counts(spec):
    """Parse message counts: 'All', '21,22', '20-23', or raw sizes."""
    if spec is None or str(spec).strip() == "" or str(spec).strip().lower() == "all":
        return [2**21, 2**22]
    counts = []
    for part in str(spec).split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            a_s, b_s = part.split("-", 1)
            a, b = int(a_s), int(b_s)
            lo, hi = (a, b) if a <= b else (b, a)
            # exponents if both small
            if hi < 40:
                counts.extend(2**e for e in range(lo, hi + 1))
            else:
                counts.extend(range(lo, hi + 1))
        else:
            v = int(part)
            counts.append(2**v if v < 40 else v)
    return list(dict.fromkeys(counts))


def parse_spineleaf_job_size(job_size):
    """'4s16l' -> (spines=4, leaves=16, merlin_shape='4,4:16', nnodes=64)."""
    match = re.match(r"^(\d+)s(\d+)l$", str(job_size).strip(), re.IGNORECASE)
    if not match:
        raise ValueError(
            f"spineleaf job_size must look like '4s16l' or '2s4l', got {job_size!r}"
        )
    spines = int(match.group(1))
    leaves = int(match.group(2))
    merlin_shape = f"{spines},{spines}:{leaves}"
    nnodes = spines * leaves
    label = f"{spines}s{leaves}l"
    return spines, leaves, merlin_shape, nnodes, label


def parse_dragonfly_job_size(job_size):
    """'p2a4g9' or '2:4:1:9' -> (p, a, n, g, merlin_shape, nnodes, label).

    Merlin shape is hosts_per_router:routers_per_group:intergroup_links:num_groups.
    intergroup_links defaults to 1 (balanced p=2,a=4,g=9 → shape 2:4:1:9).
    """
    spec = str(job_size).strip()
    match = re.match(
        r"^p(\d+)a(\d+)(?:n(\d+))?g(\d+)$", spec, re.IGNORECASE
    )
    if match:
        p = int(match.group(1))
        a = int(match.group(2))
        n = int(match.group(3) or 1)
        g = int(match.group(4))
    else:
        parts = spec.split(":")
        if len(parts) == 4 and all(part.isdigit() for part in parts):
            p, a, n, g = (int(x) for x in parts)
        else:
            raise ValueError(
                f"dragonfly job_size must look like 'p2a4g9' or '2:4:1:9', "
                f"got {job_size!r}"
            )
    merlin_shape = f"{p}:{a}:{n}:{g}"
    nnodes = p * a * g
    label = f"p{p}_a{a}_g{g}"
    return p, a, n, g, merlin_shape, nnodes, label


def resolve_route_table_for_tree_set(route_table_file, tree_set, job_size="8x8"):
    """Pick route table JSON matching tree_set (k = tree_set + 1)."""
    edge_length = tree_edge_length_from_tree_set(tree_set)
    if route_table_file:
        match = re.search(
            r"(ember_bdmst_routing_table_)(\d+_)(\d+)(\.json)$",
            os.path.basename(route_table_file),
        )
        if match:
            prefix, job_part, _, suffix = match.groups()
            dirname = os.path.dirname(route_table_file)
            return os.path.join(
                dirname,
                f"{prefix}{job_part}{edge_length}{suffix}",
            )
        return route_table_file
    torus_dim = job_size.split("x")[0]
    return os.path.join(
        EMBERTREES_ROUTETABLES,
        f"ember_bdmst_routing_table_{torus_dim}_{edge_length}.json",
    )


def resolve_overlay_trees_file(
    args, spines=None, leaves=None, topo="spineleaf", p=None, a=None, g=None
):
    """Resolve Overlay / DBTree JSON under embertrees/{spineleaf,dragonfly}/."""
    explicit = getattr(args, "trees_file", "") or ""
    if explicit:
        return explicit
    which = getattr(args, "overlay_kind", "Overlay")
    k = getattr(args, "overlay_k", "") or ""
    if topo == "dragonfly":
        if which == "DBTree":
            return os.path.join(
                EMBERTREES_DRAGONFLY,
                f"overlay_nccl_dbtree_p{p}_a{a}_g{g}.json",
            )
        if k != "" and str(k) != "0":
            return os.path.join(
                EMBERTREES_DRAGONFLY,
                f"overlay_df_p{p}_a{a}_g{g}_k{k}.json",
            )
        return os.path.join(
            EMBERTREES_DRAGONFLY,
            f"overlay_df_p{p}_a{a}_g{g}_k3.json",
        )
    if which == "DBTree":
        return os.path.join(
            EMBERTREES_SPINELEAF,
            f"overlay_nccl_dbtree_anneal_s{spines}_l{leaves}.json",
        )
    if k != "" and str(k) != "0":
        return os.path.join(
            EMBERTREES_SPINELEAF,
            f"overlay_sl_s{spines}_l{leaves}_k{k}.json",
        )
    return os.path.join(
        EMBERTREES_SPINELEAF,
        f"overlay_sl_s{spines}_l{leaves}.json",
    )


def output_bench_name(bench, args):
    if bench == "Trees":
        # HyperX / rect: keep plain Trees (no torus k suffix)
        tree_pack = getattr(args, "tree_pack", "") or ""
        if tree_pack in ("hyperx", "rect"):
            return bench
        edge_length = tree_edge_length_from_route_table(
            getattr(args, "route_table_file", "")
        )
        if edge_length is None:
            edge_length = tree_edge_length_from_tree_set(
                getattr(args, "tree_set", 0)
            )
        if edge_length:
            return f"Trees_{edge_length}"
    if bench == "Overlay":
        k = getattr(args, "overlay_k", "") or ""
        if k != "" and str(k) != "0":
            return f"Overlay_k{k}"
    return bench


def resolve_output_path(out_file, bench, args):
    bench_name = output_bench_name(bench, args)
    if bench_name == bench:
        return out_file
    resolved = out_file.replace(f"/{bench}/", f"/{bench_name}/")
    os.makedirs(os.path.dirname(resolved), exist_ok=True)
    return resolved


def allocate_logic(bench, topo):
    # We allocate only for GPT or Cosmo, not needed for other benchmarks
    if "DLRM" in bench and topo == "hx4":
        return "SST_NO_MEM=1 IS_XH4=1"
    if "gpt" in bench.lower() or "cosmo" in bench.lower():
        return "SST_NO_MEM=0"
    else:
        return "SST_NO_MEM=1"


def run_slim(args, launch_string, bench, topo):
    if topo == "dragonfly" and args.nodes > 4:
        node_num = 128
    else:
        node_num = args.nodes
    allocation_policy = allocate_logic(bench, topo)
    slim_string = " time /home/gera/torus-allreduce/local/packages/OpenMPI-4.1.6/bin/mpirun -x {} -mca plm_rsh_no_tree_spawn 1 --map-by slot -mca btl vader,self --hostfile {}  -mca orte_base_help_aggregate 0  --oversubscribe -np {} {}".format(
        allocation_policy, args.hostfile, node_num, sstsim_path
    )
    print(slim_string + " " + launch_string)
    os.system(slim_string + " " + launch_string)


def run_cluster(args, launch_string, size, bench, topo):

    # Special case if we are running on SlimFly
    if args.env == "slimfly":
        return run_slim(args, launch_string, bench, topo)

    allocation_policy = allocate_logic(bench, topo)

    my_sbatch_cont = [
        "#!/bin/bash\n",
        "#SBATCH -N {}\n".format(args.nodes),
        "#SBATCH -n {}\n".format(args.nodes),
        "#SBATCH --time=03:59:59\n",
        "#SBATCH -A g34\n",
        "#SBATCH --mem={}\n".format(args.mem),
        "#SBATCH --cpus-per-task={}\n".format(args.cpus_per_task),
        "#SBATCH -C mc\n",
        "#SBATCH --output=logs/slurm-%A.out\n",
        "# Load the module environment suitable for the job\n",
        "module load openmpi\n",
        "{}\n".format(allocation_policy),
        "# And finally run the job\n",
    ]

    if args.env == "ault":
        my_sbatch_cont = [x for x in my_sbatch_cont if x != "#SBATCH -C mc\n"]
        my_sbatch_cont = [x for x in my_sbatch_cont if x != "#SBATCH -A g34\n"]

    my_sbatch_cont.append(
        "SST_NO_MEM=1 srun --mem={} -N {} -n {} --cpus-per-task={} sstsim.x ".format(
            args.mem, args.nodes, args.nodes, args.cpus_per_task
        )
        + launch_string
    )
    file_name = "launch{}_{}_{}".format(topo, bench, size)
    with open(sbatch_folder + "/" + file_name, "w") as file:
        file.writelines(my_sbatch_cont)

    os.system("sbatch " + sbatch_folder + "/" + file_name)
    time.sleep(2)


def run_sst(args, topo, count, bench, shape, motif_file, out_file):
    ember_load = ember_load_folder + "emberLoad.py"
    out_file = resolve_output_path(out_file, bench, args)

    dimensions = 1
    if topo == "hx4":
        launch_string = '--num-threads={} --model-options="--param="nic:module=merlin.reorderlinkcontrol" --topo=hx --boardShape=4x4 --globalShape={} --fatTreeShape=1:1,64 --hostsPerRtr=1 --netBW={} --loadFile={}" {} > {}'.format(
            str(args.num_threads), shape, args.netBW, motif_file, ember_load, out_file
        )
        dimensions = 2
    elif topo == "hx2":
        launch_string = '--num-threads={} --model-options="--param="nic:module=merlin.reorderlinkcontrol" --topo=hx --boardShape=2x2 --globalShape={} --fatTreeShape=1:1,64 --hostsPerRtr=1 --netBW={} --loadFile={}" {} > {}'.format(
            str(args.num_threads), shape, args.netBW, motif_file, ember_load, out_file
        )
        dimensions = 2
    elif topo == "hx1":
        launch_string = '--num-threads={} --model-options="--param="nic:module=merlin.reorderlinkcontrol" --topo=hx --boardShape=1x1 --globalShape={} --fatTreeShape=1:1,64 --hostsPerRtr=1 --netBW={} --loadFile={}" {} > {}'.format(
            str(args.num_threads), shape, args.netBW, motif_file, ember_load, out_file
        )
        dimensions = 2
    elif topo == "hyperx":
        # host/NIC 1000000Gb/s; fabric hyperx links = netBW (default 400Gb/s).
        # --hostBW=400 нужен только для spineleaf, не для hyperx/torus.
        host_bw = args.hostBW if args.hostBW else "1000000Gb/s"
        model_opts = (
            '--param="nic:module=merlin.reorderlinkcontrol" '
            f'--param="merlin:hyperx.shape={shape}" '
            '--param="merlin:hyperx.width=1x1" '
            '--param="merlin:hyperx.local_ports=1" '
            f'--param="merlin:link_bw:host={host_bw}" '
            f'--param="merlin:link_bw:hyperx={args.netBW}" '
            f'--param="nic:link_bw={host_bw}"'
        )
        launch_string = (
            f'--num-threads={args.num_threads} '
            f'--model-options="{model_opts} '
            f'--topo=hyperx --shape={shape} --hostsPerRtr=1 '
            f'--netBW={args.netBW} --loadFile={motif_file}" '
            f'{ember_load} > {out_file}'
        )
        dimensions = 2
    elif topo == "spineleaf":
        # host↔leaf и leaf↔spine одинаковые: netBW (default 400Gb/s),
        # либо явный --hostBW если нужен другой host-линк.
        host_bw = args.hostBW if args.hostBW else args.netBW
        model_opts = (
            '--param="nic:module=merlin.reorderlinkcontrol" '
            f'--param="merlin:fattree.shape={shape}" '
            f'--param="merlin:link_bw={args.netBW}" '
            f'--param="merlin:link_bw:host={host_bw}" '
            f'--param="merlin:link_bw:network0={args.netBW}" '
            f'--param="nic:link_bw={host_bw}"'
        )
        launch_string = (
            f'--num-threads={args.num_threads} '
            f'--model-options="{model_opts} '
            f'--topo=fattree --shape={shape} --netBW={args.netBW} '
            f'--loadFile={motif_file}" '
            f'{ember_load} > {out_file}'
        )
    elif "fattree" in topo:
        launch_string = '--num-threads={} --model-options="--param="nic:module=merlin.reorderlinkcontrol" --topo=fattree --shape={} --netBW={} --loadFile={}" {} > {}'.format(
            str(args.num_threads), shape, args.netBW, motif_file, ember_load, out_file
        )
    elif topo == "torus":
        model_opts = '--param="nic:module=merlin.reorderlinkcontrol"'
        if args.hostBW or args.torusBW:
            model_opts += f' --param="merlin:torus.shape={shape}"'
            model_opts += f' --param="merlin:torus.width=1x1"'
            model_opts += f' --param="merlin:torus.local_ports=1"'
            if args.hostBW:
                model_opts += f' --param="merlin:link_bw:host={args.hostBW}"'
            if args.torusBW:
                model_opts += f' --param="merlin:link_bw:torus={args.torusBW}"'
        # Merlin torus_trees needs the route table at the network layer.
        route_table = getattr(args, "route_table_file", "") or ""
        if route_table and bench == "Trees":
            model_opts += f' --routeTableFile={route_table}'
            num_vcs = num_vcs_for_trees_route_table(route_table, shape)
            model_opts += f' --numVCs={num_vcs}'
            print(
                f"torus_trees numVCs={num_vcs} "
                f"(shape={shape}, route_table={os.path.basename(route_table)})"
            )

        launch_string = (
            f'--num-threads={args.num_threads} '
            f'--model-options="{model_opts} '
            f' --topo=torus --shape={shape} --hostsPerRtr=1 '
            f' --netBW={args.netBW} --loadFile={motif_file}" '
            f'{ember_load} > {out_file}'
        )
        dimensions = shape.count("x") + 1
    elif topo == "dragonfly":
        # Dragonfly BW is per logical port group (unlike fattree/torus/hyperx):
        #   host   — NIC ↔ router
        #   group  — intra-group (local router↔router)
        #   global — inter-group
        # hr_router looks up link_bw:<group>; empty flags fall back to netBW.
        if args.num_threads > 8:
            args.num_threads = 8
        host_bw = args.hostBW if getattr(args, "hostBW", "") else args.netBW
        group_bw = getattr(args, "groupBW", "") or args.netBW
        global_bw = getattr(args, "globalBW", "") or args.netBW
        model_opts = (
            '--param="nic:module=merlin.reorderlinkcontrol" '
            '--param="merlin:dragonfly.global_route_mode=relative" '
            f'--param="merlin:link_bw={args.netBW}" '
            f'--param="merlin:link_bw:host={host_bw}" '
            f'--param="merlin:link_bw:group={group_bw}" '
            f'--param="merlin:link_bw:global={global_bw}" '
            f'--param="nic:link_bw={host_bw}"'
        )
        print(
            f"dragonfly BW host={host_bw} group={group_bw} global={global_bw}"
        )
        launch_string = (
            f'--num-threads={args.num_threads} '
            f'--model-options="{model_opts} '
            f'--topo=dragonfly --shape={shape} --netBW={args.netBW} '
            f'--loadFile={motif_file}" '
            f'{ember_load} > {out_file}'
        )
    else:
        print("Error, unknown topo")
        sys.exit(0)

    # Set defaultParams and patch shortMsgLength threshold for this count/bench.
    defaultParamsName = "defaultParams" + str(dimensions) + "D.py"
    default_params_src = ember_load_folder + defaultParamsName
    default_params_dst = ember_load_folder + "defaultParams.py"
    shutil.copyfile(default_params_src, default_params_dst)

    num_ranks = num_ranks_from_shape(shape)
    num_trees = num_trees_for_bench(bench, shape)
    short_value = compute_short_value(bench, count, num_ranks, num_trees)
    if short_value is not None:
        print(
            "shortValue={} (bench={}, count={}, ranks={}, trees={})".format(
                short_value, bench, count, num_ranks, num_trees
            )
        )
        apply_short_value_to_default_params(default_params_dst, short_value)

    if args.env == "local" or args.env == "":
        allocation_policy = allocate_logic(bench, topo)
        print("{} {} ".format(allocation_policy, sstsim_path) + launch_string)
        process = subprocess.Popen(
            ["{} {} ".format(allocation_policy, sstsim_path) + launch_string],
            shell=True,
            stderr=sys.stderr
        )
        process.wait()
    elif (
        args.env == "slurm"
        or args.env == "cluster"
        or args.env == "ault"
        or args.env == "slimfly"
        or args.env == "daint"
    ):
        run_cluster(args, launch_string, count, bench, topo)
    else:
        print("Error, unknown env")
        sys.exit(0)
