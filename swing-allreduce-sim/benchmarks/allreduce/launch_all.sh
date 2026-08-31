#!/bin/bash
# Bench launcher for Spine-Leaf, HyperX, and Dragonfly Overlay.
#
# Spine-Leaf 4s16l: 4 spines, 16 leaves, 64 hosts (Merlin shape 4,4:16)
#   Ring            -> --bench Rings
#   NCCL DBTree     -> --bench DBTree
#   Trees allreduce -> --bench Overlay --overlay_k K
#     K = N          → overlay_sl_sS_lL_kN.json      (intra0 / split_half)
#     K = k1xk2      → overlay_sl_sS_lL_k{k1}x{k2}.json  (split: same×k1, cross×k2)
#                      e.g. --overlay_k 2x1
#
# Dragonfly p2a4g9: 72 hosts (Merlin shape 2:4:1:9)
#   Overlay --overlay_k K
#     K = N            → overlay_df_pP_aA_gG_kN.json
#     K = k1xk2xk3     → overlay_df_pP_aA_gG_k{k1}x{k2}x{k3}.json
#   BW отдельно от остальных топологий:
#     --hostBW    NIC↔router
#     --groupBW   intra-group (local)
#     --globalBW  inter-group
#
# HyperX 8x8: 64 hosts
#   Trees  -> --bench Trees --route_table_file .../ember_bdmst_routing_table_hyperx_8_8_2.json
#            (C++ trees: bdms_tree_specs_bdms_hyperx_8x8_2_quadruple)
#   SwingB -> --bench SwingB
#   Bucket -> --bench Bucket  (RingAllreduceRev)
#
# --hostfile нужен только для --env slimfly (mpirun по кластеру).

# Кластер SlimFly:
# EXTRA="--env slimfly --nodes 32 --hostfile /home/gera/hosts.cluster --num-threads 32"

# Локально:
#   spineleaf: host↔leaf = netBW = 400Gb/s
#   dragonfly: host / group / global задаются отдельно (по умолчанию = netBW)
#   hyperx/torus: host/NIC = 1000000Gb/s, fabric = netBW 400Gb/s
EXTRA="--env local --num-threads 12 --netBW 400Gb/s"
EXTRA_SL="${EXTRA} --hostBW 400Gb/s"
EXTRA_HX="${EXTRA} --hostBW 1000000Gb/s"
EXTRA_DF="${EXTRA} --hostBW 400Gb/s --groupBW 400Gb/s --globalBW 400Gb/s"

# ---------- Spine-Leaf 4s16l ----------
JOB_SL="--topo spineleaf --job_size 4s16l"

# python3 launchAll.py ${JOB_SL} ${EXTRA_SL} --bench Rings
# python3 launchAll.py ${JOB_SL} ${EXTRA_SL} --bench DBTree
# for k in 1 2 3; do
#   python3 launchAll.py ${JOB_SL} ${EXTRA_SL} --bench Overlay --overlay_k ${k}
# done
# for k in 48x15; do
#   python3 launchAll.py ${JOB_SL} ${EXTRA_SL} --bench Overlay --overlay_k ${k}
# done

# ---------- HyperX 8x8 (params as launch.json hyperx Trees) ----------
JOB_HX="--topo hyperx --job_size 8x8"
ROUTE_HX="/home/gera/torus-allreduce/scratch/src/sst-elements/src/sst/elements/ember/mpi/motifs/emberroutingtables/ember_bdmst_routing_table_hyperx_8_8_2.json"

# python3 launchAll.py ${JOB_HX} ${EXTRA_HX} --bench Trees --route_table_file ${ROUTE_HX}
# python3 launchAll.py ${JOB_HX} ${EXTRA_HX} --bench SwingB
# # python3 launchAll.py ${JOB_HX} ${EXTRA_HX} --bench SwingL
# python3 launchAll.py ${JOB_HX} ${EXTRA_HX} --bench Bucket

# ---------- HyperX 64x64 (params as launch.json hyperx Trees) ----------
JOB_HX="--topo hyperx --job_size 64x64"
ROUTE_HX="/home/gera/torus-allreduce/scratch/src/sst-elements/src/sst/elements/ember/mpi/motifs/emberroutingtables/ember_bdmst_routing_table_hyperx_64_64_2.json"

# python3 launchAll.py ${JOB_HX} ${EXTRA_HX} --bench Trees --route_table_file ${ROUTE_HX}
# python3 launchAll.py ${JOB_HX} ${EXTRA_HX} --bench SwingB
# # python3 launchAll.py ${JOB_HX} ${EXTRA_HX} --bench SwingL
# python3 launchAll.py ${JOB_HX} ${EXTRA_HX} --bench Bucket

# ---------- Dragonfly p2 a4 g9 (72 hosts, Merlin shape 2:4:1:9) ----------
JOB_DF="--topo dragonfly --job_size p2a4g9"

python3 launchAll.py ${JOB_DF} ${EXTRA_DF} --bench Rings
# python3 launchAll.py ${JOB_DF} ${EXTRA_DF} --bench Overlay --overlay_k 1x1x1
# python3 launchAll.py ${JOB_DF} ${EXTRA_DF} --bench Overlay --overlay_k 4x6x1
# 1x1x1 4x6x1 36x27x8
# for k in 36x27x8; do
#   python3 launchAll.py ${JOB_DF} ${EXTRA_DF} --bench Overlay --overlay_k ${k}
# done

