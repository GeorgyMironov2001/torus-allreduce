import subprocess
import sys
from argparse import ArgumentParser
import shutil
import os
import time

# Change this to Path later
logs_folder = "logs"
sbatch_folder = "sbatch"
# ember_load_folder = "../../sst-elements-library-11.1.0/src/sst/elements/ember/test/"
ember_load_folder = "/home/gera/torus-allreduce/scratch/src/sst-elements/src/sst/elements/ember/test/"
# sstsim_path = "/scratch/desensi/libexec/sstsim.x"
# sstsim_path = "/home/gera/local/sstcore-15.0.0/libexec/sstsim.x"
sstsim_path = "/home/gera/torus-allreduce/local/sstcore-15.0.0/libexec/sstsim.x"

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
    # slim_string = " time /scratch/2/t2hx/dep/adaptive_openmpi/bin/mpirun -x {} -mca plm_rsh_no_tree_spawn 1 --map-by node -mca btl openib,self,sm -mca btl_openib_if_include mlx4_0 --hostfile {} -mca orte_base_help_aggregate 0  -np {} {}".format(
    #     allocation_policy, args.hostfile, node_num, sstsim_path
    # )
    # slim_string = " time /home/gera/local/packages/OpenMPI-4.1.6/bin/mpirun -x {} -mca plm_rsh_no_tree_spawn 1 --map-by node -mca btl openib,self,sm -mca btl_openib_if_include mlx4_0 --hostfile {} -mca orte_base_help_aggregate 0  -np {} {}".format(
    #     allocation_policy, args.hostfile, node_num, sstsim_path
    # )
    # slim_string = " time /home/gera/local/packages/OpenMPI-4.1.6/bin/mpirun -x {} -mca plm_rsh_no_tree_spawn 1 --map-by node -mca btl vader,self --hostfile {} -mca orte_base_help_aggregate 0  -np {} {}".format(
    #     allocation_policy, args.hostfile, node_num, sstsim_path
    # )
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
    # Write the array back to the same file
    name = name.replace("loads/", "")
    file_name = "launch{}_{}_{}".format(topo, bench, size)
    with open(sbatch_folder + "/" + file_name, "w") as file:
        file.writelines(my_sbatch_cont)

    os.system("sbatch " + sbatch_folder + "/" + file_name)
    time.sleep(2)


def run_sst(args, topo, count, bench, shape, motif_file, out_file):
    ember_load = ember_load_folder + "emberLoad.py"

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
        launch_string = '--num-threads={} --model-options="--param="nic:module=merlin.reorderlinkcontrol" --topo=hyperx --shape={} --netBW={} --loadFile={}" {} > {}'.format(
            str(args.num_threads), shape, args.netBW, motif_file, ember_load, out_file
        )
        dimensions = 2
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
        
        # launch_string = '--num-threads={} --model-options="--param="nic:module=merlin.reorderlinkcontrol"  --topo=torus --shape={} --hostsPerRtr=1 --netBW={} --loadFile={}" {} > {}'.format(
        #     str(args.num_threads), shape, args.netBW, motif_file, ember_load, out_file
        # )
        launch_string = (
            f'--num-threads={args.num_threads} '
            f'--model-options="{model_opts} '
            f' --topo=torus --shape={shape} --hostsPerRtr=1 '
            f' --netBW={args.netBW} --loadFile={motif_file}" '
            f'{ember_load} > {out_file}'
        )
        dimensions = shape.count("x") + 1
    elif topo == "dragonfly":
        # Special case for Dragonfly
        if args.num_threads > 8:
            args.num_threads = 8
        launch_string = '--num-threads={} --model-options="--param="nic:module=merlin.reorderlinkcontrol"  --topo=dragonfly --shape={} --netBW={} --loadFile={}" {} > {}'.format(
            str(args.num_threads), shape, args.netBW, motif_file, ember_load, out_file
        )
    else:
        print("Error, unknown topo")
        sys.exit(0)

    # Set defaultParams
    defaultParamsName = "defaultParams" + str(dimensions) + "D.py"
    shutil.copyfile(
        ember_load_folder + defaultParamsName, ember_load_folder + "defaultParams.py"
    )

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
