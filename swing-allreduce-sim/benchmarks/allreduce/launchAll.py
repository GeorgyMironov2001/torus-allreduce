import sys
sys.path.append("../")
from general_bench import *
from argparse import ArgumentParser
import pathlib
import numpy as np
import copy
import os
import re

shapes_fattree = {}
shapes_fattree_21 = {}
shapes_fattree_41 = {}
shapes_fattree_81 = {}

shapes_fattree[1024] = "32,32:32"
shapes_fattree_21[1024] = "32,16:32"
shapes_fattree_41[1024] = "32,8:32"
shapes_fattree_81[1024] = "32,4:32"

topologies = [
    "torus", "hx2", "hx4", "dragonfly", "fattree",
    "fattree21", "fattree41", "fattree81", "hyperx", "spineleaf",
]

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
bench_to_motif["Overlay"] = "OverlayAllreduce"
bench_to_motif["DBTree"] = "OverlayAllreduce"

motif_folder = os.getcwd() + "/loads"
out_folder = os.getcwd() + "/output"

# Rectangular torus shapes where Trees headers exist today.
TREES_RECT_SHAPES = {"8x4"}


def check_if_exist(topo, subpath):
    path_motif = pathlib.Path(motif_folder + "/" + subpath)
    path_motif.mkdir(parents=True, exist_ok=True)
    path_out = pathlib.Path(out_folder + "/" + subpath)
    path_out.mkdir(parents=True, exist_ok=True)
    path_sbatch = pathlib.Path(sbatch_folder)
    path_sbatch.mkdir(parents=True, exist_ok=True)
    path_logs = pathlib.Path(logs_folder)
    path_logs.mkdir(parents=True, exist_ok=True)


def tree_sets_for_bench(bench, args, topo):
    if bench != "Trees":
        return [0]
    if topo == "hyperx":
        return [0]
    if topo == "torus":
        parts = args.job_size.split("x")
        if len(parts) == 2 and parts[0] != parts[1]:
            return [0]
        return parse_tree_sets(getattr(args, "tree_sets", "0"))
    return [0]


def resolve_tree_pack(topo, job_size):
    if topo == "hyperx":
        return "hyperx"
    if topo == "torus":
        parts = job_size.split("x")
        if len(parts) == 2 and parts[0] != parts[1]:
            return "rect"
        return "torus"
    return ""


def generate_simulations(args, bench):
    counts = parse_counts(getattr(args, "counts", "All"))
    # counts = [2**i for i in range(14, 21)]
    # counts = [2**i for i in range(6, 25)]
    # counts = [2**24]
    # counts = [2**i for i in range(7, 15)]
    counts = [2**i for i in range(7, 23)]
    # counts = [2**i for i in range(6, 7)]
    for topo in topologies:
        if args.topo != "" and args.topo != topo:
            continue

        # Spine-leaf comparison set; Overlay/DBTree on spineleaf and dragonfly.
        if topo == "spineleaf" and bench not in (
            "Overlay", "Rings", "DBTree", "AlltoallB", "AlltoallL"
        ):
            continue
        if topo == "dragonfly" and bench not in ("Overlay", "Rings"):
            continue
        if bench in ("Overlay", "DBTree") and topo not in (
            "spineleaf", "dragonfly"
        ):
            continue

        for tree_set in tree_sets_for_bench(bench, args, topo):
            run_args = copy.copy(args)
            run_args.tree_set = tree_set
            run_args.overlay_kind = bench
            run_args.tree_pack = resolve_tree_pack(topo, args.job_size)

            for count in counts:
                if topo == "spineleaf":
                    spines, leaves, shape, nnodes, label = parse_spineleaf_job_size(
                        args.job_size
                    )
                    dimensions = 1
                    dimensions_sizes = str(nnodes)
                    generateNidList = "generateNidListRange(0,{})".format(nnodes)
                    subpath_shape = label
                    run_args.trees_file = resolve_overlay_trees_file(
                        run_args, spines, leaves, topo="spineleaf"
                    )
                    if bench in ("Overlay", "DBTree") and not os.path.isfile(
                        run_args.trees_file
                    ):
                        print(
                            f"Warning: missing trees_file {run_args.trees_file}, skip"
                        )
                        continue
                    print(f"spineleaf {label} shape={shape} n={nnodes}")
                elif topo == "dragonfly":
                    p, a, _n, g, shape, nnodes, label = parse_dragonfly_job_size(
                        args.job_size
                    )
                    dimensions = 1
                    dimensions_sizes = str(nnodes)
                    generateNidList = "generateNidListRange(0,{})".format(nnodes)
                    subpath_shape = label
                    run_args.trees_file = resolve_overlay_trees_file(
                        run_args, topo="dragonfly", p=p, a=a, g=g
                    )
                    if bench in ("Overlay", "DBTree") and not os.path.isfile(
                        run_args.trees_file
                    ):
                        print(
                            f"Warning: missing trees_file {run_args.trees_file}, skip"
                        )
                        continue
                    print(
                        f"dragonfly {label} shape={shape} n={nnodes} "
                        f"trees={run_args.trees_file}"
                    )
                else:
                    if "x" in args.job_size:
                        nnodes = int(
                            np.prod([int(x) for x in args.job_size.split("x")])
                        )
                        dimensions_sizes = ",".join(args.job_size.split("x"))
                        dimensions = args.job_size.count("x") + 1
                    else:
                        nnodes = int(args.job_size)
                        dimensions_sizes = str(nnodes)
                        dimensions = 1

                    print(dimensions_sizes)
                    print(topo)

                    shape = args.job_size
                    if "hx2" in topo:
                        assert dimensions == 2
                        shape = (
                            str(int(int(args.job_size.split("x")[0]) / 2))
                            + "x"
                            + str(int(int(args.job_size.split("x")[1]) / 2))
                        )
                        generateNidList = "generateNidListHx(2x2x{})".format(shape)
                    elif "hx4" in topo:
                        assert dimensions == 2
                        shape = (
                            str(int(int(args.job_size.split("x")[0]) / 4))
                            + "x"
                            + str(int(int(args.job_size.split("x")[1]) / 4))
                        )
                        generateNidList = "generateNidListHx(4x4x{})".format(shape)
                    elif "torus" in topo or "hyperx" in topo:
                        shape = args.job_size
                        generateNidList = "generateNidListRange(0,{})".format(nnodes)
                    else:
                        assert topo != "dragonfly"
                        assert "x" not in args.job_size
                        if topo == "fattree":
                            shape = shapes_fattree[nnodes]
                        elif topo == "fattree21":
                            shape = shapes_fattree_21[nnodes]
                        elif topo == "fattree41":
                            shape = shapes_fattree_41[nnodes]
                        elif topo == "fattree81":
                            shape = shapes_fattree_81[nnodes]
                        generateNidList = "generateNidListRange(0,{})".format(nnodes)
                    subpath_shape = shape

                if bench == "Trees":
                    if (
                        run_args.tree_pack == "rect"
                        and args.job_size not in TREES_RECT_SHAPES
                    ):
                        print(
                            f"Warning: Trees on rectangular {args.job_size} "
                            f"not supported yet (have {sorted(TREES_RECT_SHAPES)}), skip"
                        )
                        continue
                    if run_args.tree_pack == "hyperx" and args.job_size not in (
                        "8x8",
                        "64x64",
                    ):
                        print(
                            f"Warning: HyperX Trees only for 8x8/64x64, "
                            f"got {args.job_size}, skip"
                        )
                        continue
                    if run_args.tree_pack == "torus":
                        run_args.route_table_file = resolve_route_table_for_tree_set(
                            args.route_table_file, tree_set, args.job_size
                        )
                        print(
                            f"tree_set={tree_set} -> "
                            f"Trees_{tree_edge_length_from_tree_set(tree_set)} "
                            f"route_table={run_args.route_table_file}"
                        )
                    elif run_args.tree_pack == "hyperx":
                        # Motif needs route_table_file (even if JSON is []).
                        # tree_pack is only an internal launcher hint — not a motif arg.
                        if args.route_table_file:
                            run_args.route_table_file = args.route_table_file
                        else:
                            hx_rt = {
                                "8x8": "ember_bdmst_routing_table_hyperx_8_8_2.json",
                                "64x64": "ember_bdmst_routing_table_hyperx_64_64_2.json",
                            }[args.job_size]
                            run_args.route_table_file = os.path.join(
                                EMBERTREES_ROUTETABLES,
                                hx_rt,
                            )
                        print(
                            f"Trees hyperx route_table={run_args.route_table_file}"
                        )
                    else:
                        run_args.route_table_file = args.route_table_file or ""
                        print(
                            f"Trees job_size={args.job_size} "
                            f"route_table={run_args.route_table_file or '(none)'}"
                        )

                bw = ""
                if args.netBW != "400Gb/s":
                    bw = "_" + args.netBW.split("/")[0]
                if topo == "dragonfly":
                    host_bw = args.hostBW or args.netBW
                    group_bw = getattr(args, "groupBW", "") or args.netBW
                    global_bw = getattr(args, "globalBW", "") or args.netBW
                    extras = []
                    if host_bw != args.netBW:
                        extras.append("h" + host_bw.split("/")[0])
                    if group_bw != args.netBW:
                        extras.append("grp" + group_bw.split("/")[0])
                    if global_bw != args.netBW:
                        extras.append("glb" + global_bw.split("/")[0])
                    if extras:
                        bw += "_" + "_".join(extras)

                if topo == "spineleaf":
                    topo_label = "fattree_spineleaf"
                else:
                    topo_label = topo

                bench_name = output_bench_name(bench, run_args)
                subpath = topo_label + "_" + subpath_shape + bw + "/" + bench_name
                check_if_exist(topo, subpath)
                output = out_folder + "/" + subpath + "/" + str(count)

                if bench in ("SwingL", "RecDoubL", "RecDoubLM"):
                    latency_optimal = "1"
                else:
                    latency_optimal = "0"

                if bench in ("RecDoubB", "RecDoubL"):
                    ports = "1"
                elif topo == "spineleaf" or "fattree" in topo or "dragonfly" in topo:
                    ports = "1"
                else:
                    ports = str(int(dimensions) * 2)

                motif_name = bench_to_motif[bench]
                if bench == "Rings" and int(dimensions) == 1:
                    motif_name = "RingAllreduce05D"

                if motif_name == "OverlayAllreduce":
                    motif_args = [
                        "[MOTIF]",
                        motif_name,
                        f"count={count}",
                        f"ports={ports}",
                        "aggregation_cost_ns=0",
                        "blocking=true",
                        "sync=true",
                        f"trees_file={run_args.trees_file}",
                    ]
                    if getattr(args, "validate", 0):
                        motif_args.append(f"validate={args.validate}")
                else:
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
                        "sync=true",
                    ]
                    if motif_name == "TreesAllreduce":
                        # Same as old generator: only route_table_file when set.
                        # Do not emit tree_set/tree_pack — motif ignores them.
                        if run_args.route_table_file:
                            motif_args.append(
                                f"route_table_file={run_args.route_table_file}"
                            )

                motif_main_args = " ".join(motif_args) + "\n"
                motif_content = [
                    "[JOB_ID] 10\n",
                    "[NID_LIST] generateNidList={}\n".format(generateNidList),
                    "[MOTIF] Init\n",
                    motif_main_args,
                    "[MOTIF] Fini",
                ]

                motif_file = motif_folder + "/" + subpath + "/" + str(count)
                with open(motif_file, "w") as outfile:
                    outfile.writelines(motif_content)

                run_sst(
                    run_args, topo, count, bench, shape, motif_file, output
                )


def main(args):
    benchmarks = [
        "SwingB", "SwingL", "RecDoubB", "RecDoubL", "Rings", "Bucket", "Torus",
    ]
    if args.bench != "All":
        benchmarks = args.bench.split(",")
    for bench in benchmarks:
        if args.topo in ("spineleaf", "dragonfly"):
            dimensions = 1
        elif "x" in args.job_size:
            dimensions = args.job_size.count("x") + 1
        else:
            dimensions = 1
        if dimensions > 2 and bench == "Rings":
            continue
        if (
            dimensions == 1
            and bench == "Bucket"
            and args.topo != "spineleaf"
            and "x" not in args.job_size
        ):
            continue
        generate_simulations(args, bench)


if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument(
        "--topo",
        type=str,
        help="Topology to run",
        default="",
        choices=[
            "hx4",
            "hx2",
            "fattree",
            "fattree21",
            "fattree41",
            "fattree81",
            "torus",
            "dragonfly",
            "hyperx",
            "spineleaf",
        ],
    )
    parser.add_argument(
        "--num-threads",
        type=int,
        help="Number of threads to use for SST",
        default=1,
    )
    parser.add_argument(
        "--env",
        type=str,
        help="Local or Cluster",
        default="",
        choices=["cluster", "daint", "ault", "local", "slimfly"],
    )
    parser.add_argument(
        "--nodes", type=int, help="Number of nodes for cluster", default=8
    )
    parser.add_argument(
        "--cpus_per_task",
        type=str,
        help="Number of cores per node for cluster",
        default="8",
    )
    parser.add_argument(
        "--mem", type=str, help="Memory per Node for cluster", default="16G"
    )
    parser.add_argument(
        "--hostfile", type=str, help="Hostfile name for Slimfly", default="hostfile"
    )
    parser.add_argument(
        "--job_size",
        type=str,
        help="Job size: NxM torus/hyperx, N fattree hosts, SsLl spineleaf, or pPaAgG dragonfly",
        default="8x8",
    )
    parser.add_argument("--bench", type=str, help="Benchmark to run", default="All")
    parser.add_argument(
        "--counts",
        type=str,
        help="Counts: All | 21,22 | 20-23 | raw sizes",
        default="All",
    )
    parser.add_argument(
        "--netBW", type=str, help="Link bandwidth", default="400Gb/s"
    )
    parser.add_argument(
        "--hostBW",
        type=str,
        help="BW NIC<->router (host ports), e.g. 1600Gb/s",
        default="",
    )
    parser.add_argument(
        "--torusBW",
        type=str,
        help="BW router<->router (torus links), e.g. 400Gb/s",
        default="",
    )
    parser.add_argument(
        "--groupBW",
        type=str,
        help="Dragonfly intra-group (local) router<->router BW, e.g. 400Gb/s",
        default="",
    )
    parser.add_argument(
        "--globalBW",
        type=str,
        help="Dragonfly inter-group (global) router<->router BW, e.g. 400Gb/s",
        default="",
    )
    parser.add_argument(
        "--route_table_file",
        type=str,
        help="Route table file (d in name is adjusted per tree_set)",
        default="",
    )
    parser.add_argument(
        "--tree_sets",
        type=str,
        help="Trees tree_set values: '0', '0,2,4', '0-7', '7-0', or 'all'",
        default="0",
    )
    parser.add_argument(
        "--trees_file",
        type=str,
        help="Overlay/DBTree JSON (overrides auto path under embertrees/{spineleaf,dragonfly})",
        default="",
    )
    parser.add_argument(
        "--overlay_k",
        type=str,
        help="Overlay k suffix: N, k1xk2 (spineleaf), or k1xk2xk3 (dragonfly)",
        default="",
    )
    parser.add_argument(
        "--validate",
        type=int,
        help="Overlay validate flag (0/1)",
        default=0,
    )
    args = parser.parse_args()
    args.tree_set = parse_tree_sets(args.tree_sets)[0]
    args.tree_pack = ""
    main(args)
