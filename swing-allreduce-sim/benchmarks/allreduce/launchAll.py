import sys
sys.path.append("../")
from general_bench import *
from argparse import ArgumentParser
import pathlib
import numpy as np
shapes_fattree = {}
shapes_fattree_21 = {}
shapes_fattree_41 = {}
shapes_fattree_81 = {}

shapes_fattree[1024] = "32,32:32"
shapes_fattree_21[1024] = "32,16:32"
shapes_fattree_41[1024] = "32,8:32"
shapes_fattree_81[1024] = "32,4:32"

topologies = ["torus", "hx2", "hx4", "dragonfly", "fattree",
              "fattree21", "fattree41", "fattree81", "hyperx"]

bench_to_motif = {}
bench_to_motif["Rings"] = "RingAllreduce25D"
bench_to_motif["Bucket"] = "RingAllreduceRev"
bench_to_motif["SwingB"] = "SwingAllreduce"
bench_to_motif["SwingL"] = "SwingAllreduce"
bench_to_motif["RecDoubB"] = "RecDoubAllreduce"
bench_to_motif["RecDoubL"] = "RecDoubAllreduce"
bench_to_motif["RecDoubBM"] = "RecDoubAllreduce"
bench_to_motif["RecDoubLM"] = "RecDoubAllreduce"
bench_to_motif["Torus"] = "TorusAllreduce"
bench_to_motif["Trees"] = "TreesAllreduce"
bench_to_motif["AlltoallB"] = "AlltoallAllreduce"
bench_to_motif["AlltoallL"] = "AlltoallAllreduce"
motif_folder = os.getcwd() + "/loads"
out_folder = os.getcwd() + "/output"


def check_if_exist(topo, subpath):
    # First check if the folder we need to use exist
    path_motif = pathlib.Path(motif_folder + "/" + subpath)
    path_motif.mkdir(parents=True, exist_ok=True)
    path_out = pathlib.Path(out_folder + "/" + subpath)
    path_out.mkdir(parents=True, exist_ok=True)
    path_sbatch = pathlib.Path(sbatch_folder)
    path_sbatch.mkdir(parents=True, exist_ok=True)
    path_logs = pathlib.Path(logs_folder)
    path_logs.mkdir(parents=True, exist_ok=True)


def generate_simulations(args, bench):

    counts = [2**i for i in range(8, 20)]
    # counts = [2**i for i in range(4, 8)]

    dimensions_sizes = ','.join(args.job_size.split("x"))
    dimensions = args.job_size.count("x") + 1
    print(dimensions_sizes)

    for count in counts:
        for topo in topologies:
            if (args.topo != "" and args.topo != topo):
                continue
            nnodes = np.prod([int(x) for x in args.job_size.split("x")])
            print(topo)

            shape = args.job_size
            if "hx2" in topo:
                assert dimensions == 2
                shape = str(int(int(args.job_size.split("x")[
                            0]) / 2)) + "x" + str(int(int(args.job_size.split("x")[1]) / 2))
                generateNidList = "generateNidListHx(2x2x{})".format(shape)
            elif "hx4" in topo:
                assert dimensions == 2
                shape = str(int(int(args.job_size.split("x")[
                            0]) / 4)) + "x" + str(int(int(args.job_size.split("x")[1]) / 4))
                generateNidList = "generateNidListHx(4x4x{})".format(shape)
            elif "torus" in topo or "hyperx" in topo:
                shape = args.job_size
                generateNidList = "generateNidListRange(0,{})".format(nnodes)
            else:
                assert topo != "dragonfly"
                # Fat tree
                assert not "x" in args.job_size
                if topo == "fattree":
                    shape = shapes_fattree[nnodes]
                elif topo == "fattree21":
                    shape = shapes_fattree_21[nnodes]
                elif topo == "fattree41":
                    shape = shapes_fattree_41[nnodes]
                elif topo == "fattree81":
                    shape = shapes_fattree_81[nnodes]

                generateNidList = "generateNidListRange(0,{})".format(nnodes)

            bw = ""
            if args.netBW != "400Gb/s":
                bw = "_" + args.netBW.split("/")[0]
            subpath = topo + "_" + shape + bw + "/" + bench
            check_if_exist(topo, subpath)
            output = out_folder + "/" + subpath + "/" + str(count)

            if bench == "SwingL" or bench == "RecDoubL" or bench == "RecDoubLM":
                latency_optimal = "1"
            else:
                latency_optimal = "0"

            if bench == "RecDoubB" or bench == "RecDoubL":
                ports = "1"
            elif "fattree" in topo or "dragonfly" in topo:
                ports = "1"
            else:
                ports = str(int(dimensions) * 2)

            motif_name = bench_to_motif[bench]
            if bench == "Rings" and int(dimensions) == 1:
                motif_name = "RingAllreduce05D"

            motif_args = [
                "[MOTIF]",
                motif_name,
                f"count={count}",
                f"ports={ports}",
                f"dimensions={dimensions}",
                f"dimensions_sizes={dimensions_sizes}",
                f"px={dimensions_sizes.split(',')[-1]}",
                f"latency_optimal={latency_optimal}",
                "aggregation_cost_ns=0",
                "blocking=true",
                "sync=false",
            ]
            if (motif_name == "TreesAllreduce") and (args.route_table_file != ""):
                motif_args.append(f"route_table_file={args.route_table_file}")

            motif_main_args = " ".join(motif_args) + "\n"

            motif_content = ["[JOB_ID] 10\n",
                             "[NID_LIST] generateNidList={}\n".format(
                                 generateNidList),
                             "[MOTIF] Init\n",
                             motif_main_args,
                             "[MOTIF] Fini"]
            # motif_content = ["[JOB_ID] 10\n",
            #                  "[NID_LIST] generateNidList={}\n".format(
            #                      generateNidList),
            #                  "[MOTIF] Init\n",
            #                  "[MOTIF] {} count={} ports={} dimensions={} dimensions_sizes={} px={} latency_optimal={} blocking=true sync=false\n".format(
            #                      motif_name, count, ports, dimensions, dimensions_sizes, dimensions_sizes.split(",")[-1], latency_optimal),
            #                  "[MOTIF] Fini"]

            motif_file = motif_folder + "/" + subpath + "/" + str(count)
            with open(motif_file, 'w') as outfile:
                outfile.writelines(motif_content)
            run_sst(args, topo, count, bench, shape, motif_file, output)


def main(args):
    benchmarks = ["SwingB", "SwingL", "RecDoubB",
                  "RecDoubL", "Rings", "Bucket", "Torus"]  # RecDoubMirrored?
    if args.bench != "All":
        benchmarks = args.bench.split(",")
    for bench in benchmarks:
        dimensions = args.job_size.count("x") + 1
        if dimensions > 2 and bench == "Rings":
            continue
        if dimensions == 1 and bench == "Bucket":
            continue
        generate_simulations(args, bench)


if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument("--topo", type=str, help="Topology to run", default="", choices=[
                        "hx4", "hx2", "fattree", "fattree21", "fattree41", "fattree81", "torus", "dragonfly", "hyperx"])
    parser.add_argument("--num-threads", type=int,
                        help="Number of threads to use for SST", default=8)
    parser.add_argument("--env", type=str, help="Local or Cluster",
                        default="", choices=["cluster", "daint", "ault", "local", "slimfly"])
    parser.add_argument("--nodes", type=int,
                        help="Number of nodes for cluster", default="8")
    parser.add_argument("--cpus_per_task", type=str,
                        help="Number of cores per node for cluster", default="8")
    parser.add_argument("--mem", type=str,
                        help="Memory per Node for cluster", default="16G")
    parser.add_argument("--hostfile", type=str,
                        help="Hostfile name for Slimfly", default="hostfile")
    parser.add_argument("--job_size", type=str,
                        help="Size of the job", default="8x8")
    parser.add_argument("--bench", type=str,
                        help="Benchmark to run", default="All")
    parser.add_argument("--counts", type=str, help="Counts", default="All")
    parser.add_argument("--netBW", type=str,
                        help="Link bandwidth", default="400Gb/s")
    parser.add_argument("--hostBW", type=str,
                        help="BW NIC<->router (host ports), e.g. 1600Gb/s",
                        default="")
    parser.add_argument("--torusBW", type=str,
                        help="BW router<->router (torus links), e.g. 400Gb/s",
                        default="")
    parser.add_argument("--route_table_file", type=str,
                        help="Route table file", default="")
    args = parser.parse_args()
    main(args)
