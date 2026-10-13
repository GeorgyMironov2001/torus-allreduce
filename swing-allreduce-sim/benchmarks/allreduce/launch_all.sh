#!/bin/bash
# Bench launcher for Spine-Leaf, HyperX, Dragonfly Overlay, and Torus.
#
# Spine-Leaf 4s16l: 4 spines, 16 leaves, 64 hosts (Merlin shape 4,4:16)
#   Ring            -> --bench Rings
#   NCCL DBTree     -> --bench DBTree
#   Trees allreduce -> --bench Overlay --overlay_k K
#   Bucket          -> --bench Bucket  (RingAllreduceRev)
#   RecDoubB        -> --bench RecDoubB
#   RecDoubL        -> --bench RecDoubL
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
# Torus 8x8 Bandwidth Impact (paper Fig. 8):
#   Fabric BW 100 Gb/s … 3.2 Tb/s; host/NIC = 1000000Gb/s
#   SwingB/L, RecDoubB/L, Rings, Bucket, Trees (tree_sets 0-7 → Trees_1..8)
#   Outputs: torus_8x8 / torus_8x8_{100,200,800,1600,3200}Gb
#
# Rectangular torus 1024 nodes (paper Fig. rectangular):
#   64x16 / 128x8 / 256x4 @ 400Gb/s
#   SwingB, RecDoubB, Rings, Bucket (+ optional Trees with *_40/_68/_130 route tables)
#   Outputs: torus_64x16 / torus_128x8 / torus_256x4
#   Trees C++ pack must match shape (bdms_tree_specs_{64x16_40,128x8_68,256x4_130}_double)
#
# Rectangular torus, balanced overlay (overlay_rect_*.json), not compiled Trees:
#   RUN_STAGE=rect_overlay
#   From the smallest meaningful count (largest packed isend >= one 8192 B
#   packet, every tree chunk non-zero) through 2**26:
#     64x16 -> 2**18..2**26
#     128x8 -> 2**19..2**26
#     256x4 -> 2**20..2**26
#   Outputs: torus_<shape>/Overlay/<count>
# 256x4 only, counts 2**12..2**19 (FLOAT elements):
#   RUN_STAGE=rect_overlay_256x4
#   Schedule has K=2048 trees (2 templates x 1024 shifts), not 4 per node.
#   2**12 floats = 16384 bytes = 8 bytes (2 floats) on each tree.
# Two-tree exact balance (64x16, 128x8), Omega = 1:
#   RUN_STAGE=rect_overlay_bfs2
#   --overlay_k bfs2 -> overlay_rect_<shape>_bfs2.json
#   The two BFS trees, each rotated, are shifted onto all 1024 nodes:
#   K = 4 * 1024 = 4096 equal shares. Every share is a positive integer
#   once count is a multiple of 4096, so the sweep starts at 2**12.
#   Outputs: torus_<shape>/Overlay_bfs2/<count>, counts 2**12..2**26.
#
# --hostfile нужен только для --env slimfly (mpirun по кластеру).
#
# Large-payload extension controls:
#   RUN_STAGE=torus8_trees_bandwidth|torus8_swingl_bandwidth|torus8_latency|torus64_trees|torus64_khop_64mib|torus_trees_to26|hyperx_tree25|remaining25|rect|rect_overlay|rect_overlay_256x4|rect_overlay_bfs2|spineleaf_radix|spineleaf_recdoub|spineleaf32_swing|spineleaf32_swingb26|spineleaf32_k1x1|recdoub_b_large|bandwidth|spineleaf|spineleaf_overlay|hyperx_trees|hyperx|large26|size32|triviance_all|all
#   size32 runs only HyperX 32x32 Trees_budget_b64 through 2**26, with no
#   timeout, then triviance_all. Completed outputs are skipped.
#   triviance_all is 8x8, 16x16, 32x32, 64x64, HyperX 32x32, rectangular
#   64x16, 128x8, 256x4, and Spine-Leaf 16s32l at 400 Gb/s, plus torus 8x8
#   fabric bandwidth (100, 200, 800, 1600, 3200 Gb/s; 400 Gb/s stays in
#   torus_8x8) and hop latency (200, 800, 1600 ns; 400 ns stays in torus_8x8).
#   TrivanceB stops at 2**26. TrivanceL follows SwingL: 2**3..2**20 on 8x8
#   and on the bandwidth sweep, 2**3..2**23 on the hop-latency sweep,
#   2**3..2**19 on the larger squares, 2**7..2**20 on Spine-Leaf 16s32l.
#   Spine-Leaf 16s32l TrivanceB follows SwingB: 2**9..2**26.
#   The outer loop is the count: every topology that includes that count
#   runs before the next count. DBTree is not included.
#   Default: spineleaf32_swingb26. One job, Spine-Leaf 16s32l SwingB at
#   count=2**26. The existing output is marked complete but its TIME lines
#   are garbage, so this stage overwrites it instead of skipping.
#   torus64_trees fills Trees_1..64 at counts 2**14..2**23, skipping every
#   output that already has a completion marker.
#   torus64_khop_64mib (default) runs torus 64x64 Trees_11..Trees_14
#   at count=2**24 FLOAT elements (64 MiB). Completed outputs are skipped.
#   torus_trees_to26 finishes missing torus Trees up to 2**26:
#     32x32 Trees_20..Trees_31 at 2**12..2**26, then
#     64x64 Trees_12..Trees_63 at 2**14..2**26.
#   A file is skipped only when it already contains "Simulation is complete".
#   32x32 needs a rebuilt libember.so that contains torus_32x32_d20;
#   the stage stops before any job if that string is absent.
#   remaining25 fills Spine-Leaf radix @2**25 and HyperX
#   Trees_budget_b16 @2**14..2**25, skipping completed outputs.
#   large26 backfills HyperX Trees @2**25, then runs all configured @2**26.
#   COUNT_EXP=26 means count=2**26 FLOAT elements = 256 MiB payload.
RUN_STAGE="${RUN_STAGE:-torus64_khop_64mib}"
COUNT_EXP="${COUNT_EXP:-26}"
COUNT_VALUE=$((1 << COUNT_EXP))
SPINELEAF_RADIX_EXP_LO="${SPINELEAF_RADIX_EXP_LO:-10}"
SPINELEAF_RADIX_EXP_HI="${SPINELEAF_RADIX_EXP_HI:-25}"
SPINELEAF_RADIX_TIMEOUT="${SPINELEAF_RADIX_TIMEOUT:-1h}"
HYPERX_TREES_EXP_LO="${HYPERX_TREES_EXP_LO:-14}"
HYPERX_TREES_EXP_HI="${HYPERX_TREES_EXP_HI:-25}"
# New sizes. Bandwidth sweeps stop at 2**26. Latency variants stop at 2**20,
# the same split as HyperX 64x64 and Spine-Leaf 16s64l.
# Spine-Leaf 16s32l has 512 hosts and 512 radix trees, so 2**9 is one FLOAT
# per tree. HyperX 32x32 Trees uses 1024 ranks * 4 trees, so 2**12.
SIZE32_EXP_HI="${SIZE32_EXP_HI:-26}"
SL32_EXP_LO="${SL32_EXP_LO:-9}"
SL32_SMALL_EXP_LO="${SL32_SMALL_EXP_LO:-7}"
SL32_LAT_EXP_HI="${SL32_LAT_EXP_HI:-20}"
HX32_EXP_LO="${HX32_EXP_LO:-12}"
HX32_SMALL_EXP_LO="${HX32_SMALL_EXP_LO:-7}"
HX32_LAT_EXP_HI="${HX32_LAT_EXP_HI:-20}"
TORUS8_TREES_EXP_LO="${TORUS8_TREES_EXP_LO:-8}"
TORUS8_TREES_EXP_HI="${TORUS8_TREES_EXP_HI:-26}"
TORUS8_SWINGL_EXP_LO="${TORUS8_SWINGL_EXP_LO:-3}"
TORUS8_SWINGL_EXP_HI="${TORUS8_SWINGL_EXP_HI:-20}"
# Hop latency sweep at fixed 400Gb/s. Alpha 400ns is already torus_8x8.
# 200/800/1600ns scale link:input:output as 1:1.5:1.5.
# SwingL runs past 2**20 so the high-latency Swing envelope is still covered.
TORUS8_LAT_EXP_LO="${TORUS8_LAT_EXP_LO:-8}"
TORUS8_LAT_EXP_HI="${TORUS8_LAT_EXP_HI:-26}"
TORUS8_LAT_SWINGL_EXP_LO="${TORUS8_LAT_SWINGL_EXP_LO:-3}"
TORUS8_LAT_SWINGL_EXP_HI="${TORUS8_LAT_SWINGL_EXP_HI:-23}"
TORUS64_TREES_EXP_LO="${TORUS64_TREES_EXP_LO:-14}"
TORUS64_TREES_EXP_HI="${TORUS64_TREES_EXP_HI:-23}"
# torus_trees_to26. 32x32 already has 2**12..2**24; 64x64 has 2**14..2**23.
# Trees_57..63 on 64x64 also have an incomplete 2**24 file, so exp 24 is
# inside the sweep and run_if_missing restarts it.
TORUS32_TREES_D_LO="${TORUS32_TREES_D_LO:-20}"
TORUS32_TREES_D_HI="${TORUS32_TREES_D_HI:-31}"
TORUS32_TREES_EXP_LO="${TORUS32_TREES_EXP_LO:-12}"
TORUS32_TREES_EXP_HI="${TORUS32_TREES_EXP_HI:-26}"
TORUS64_TREES_TO26_D_LO="${TORUS64_TREES_TO26_D_LO:-12}"
TORUS64_TREES_TO26_D_HI="${TORUS64_TREES_TO26_D_HI:-63}"
TORUS64_TREES_TO26_EXP_LO="${TORUS64_TREES_TO26_EXP_LO:-14}"
TORUS64_TREES_TO26_EXP_HI="${TORUS64_TREES_TO26_EXP_HI:-26}"
# k-hop neighbors of SwingB on 64x64 (2*log2(64) = 12 rounds).
# 64 MiB = 2**24 FLOAT elements. tree_set = d - 1.
TORUS64_KHOP_D_LO="${TORUS64_KHOP_D_LO:-11}"
TORUS64_KHOP_D_HI="${TORUS64_KHOP_D_HI:-14}"
TORUS64_KHOP_EXP="${TORUS64_KHOP_EXP:-24}"
LIBEMBER="${LIBEMBER:-/home/mipt-user/torus-allreduce/local/sstelements-15.0.0/lib/sst-elements-library/libember.so}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"
OUTPUT_ROOT="${SCRIPT_DIR}/output"

# Кластер SlimFly:
# EXTRA="--env slimfly --nodes 32 --hostfile /home/mipt-user/hosts.cluster --num-threads 32"

# Локально:
#   spineleaf: host↔leaf = netBW = 400Gb/s
#   dragonfly: host / group / global задаются отдельно (по умолчанию = netBW)
#   hyperx/torus: host/NIC = 1000000Gb/s, fabric = netBW (и torusBW для torus)
EXTRA="--env local --num-threads 10 --netBW 400Gb/s"
EXTRA_SL="${EXTRA} --hostBW 400Gb/s"
EXTRA_HX="${EXTRA} --hostBW 1000000Gb/s"
EXTRA_DF="${EXTRA} --hostBW 400Gb/s --groupBW 400Gb/s --globalBW 400Gb/s"
EXTRA_TORUS_BASE="--env local --num-threads 10 --hostBW 1000000Gb/s"

# ---------- Spine-Leaf 16s64l: RecDoub baseline ----------
JOB_SL="--topo spineleaf --job_size 16s64l"
JOB_SL32="--topo spineleaf --job_size 16s32l"
SL_TREES="/home/mipt-user/torus-allreduce/scratch/src/sst-elements/src/sst/elements/ember/mpi/motifs/embertrees/spineleaf"

# python3 launchAll.py ${JOB_SL} ${EXTRA_SL} --bench Rings
# python3 launchAll.py ${JOB_SL} ${EXTRA_SL} --bench DBTree
# for k in 1 2 3; do
#   python3 launchAll.py ${JOB_SL} ${EXTRA_SL} --bench Overlay --overlay_k ${k}
# done
# 960x63 15x1 1x1
# for k in 960x63 480x32 240x15 120x8 60x4 30x2; do
#   python3 launchAll.py ${JOB_SL} ${EXTRA_SL} --bench Overlay --overlay_k ${k}
# done
# python3 launchAll.py ${JOB_SL} ${EXTRA_SL} --bench Bucket
# Full power-of-two range: counts = [2**i for i in range(10, 25)].
# python3 launchAll.py ${JOB_SL} ${EXTRA_SL} --bench RecDoubB \
#   --counts 10-24
# python3 launchAll.py ${JOB_SL} ${EXTRA_SL} --bench RecDoubL \
#   --counts 7-20
# 2**25 = 33554432 FLOATs: RUN_STAGE=spineleaf_recdoub

# ---------- HyperX 8x8 (params as launch.json hyperx Trees) ----------
JOB_HX="--topo hyperx --job_size 8x8"
ROUTE_HX="/home/mipt-user/torus-allreduce/scratch/src/sst-elements/src/sst/elements/ember/mpi/motifs/emberroutingtables/ember_bdmst_routing_table_hyperx_8_8_2.json"

# python3 launchAll.py ${JOB_HX} ${EXTRA_HX} --bench Trees --route_table_file ${ROUTE_HX}
# python3 launchAll.py ${JOB_HX} ${EXTRA_HX} --bench SwingB
# # python3 launchAll.py ${JOB_HX} ${EXTRA_HX} --bench SwingL
# python3 launchAll.py ${JOB_HX} ${EXTRA_HX} --bench Bucket

# ---------- HyperX 64x64 ----------
# Fixed SP policy: budget_b16 only. tree_set=7 gives both the matching
# Trees_budget_b16 output name and route-table JSON.
# 2**25 = 33554432 FLOATs: RUN_STAGE=hyperx_tree25
JOB_HX="--topo hyperx --job_size 64x64"
JOB_HX32="--topo hyperx --job_size 32x32"

# ---------- Dragonfly p2 a4 g9 (72 hosts, Merlin shape 2:4:1:9) ----------
JOB_DF="--topo dragonfly --job_size p2a4g9"

# python3 launchAll.py ${JOB_DF} ${EXTRA_DF} --bench Rings
# python3 launchAll.py ${JOB_DF} ${EXTRA_DF} --bench Overlay --overlay_k 1x1x1
# python3 launchAll.py ${JOB_DF} ${EXTRA_DF} --bench Overlay --overlay_k 4x6x1
# 1x1x1 4x6x1 36x27x8
# for k in 36x27x8; do
#   python3 launchAll.py ${JOB_DF} ${EXTRA_DF} --bench Overlay --overlay_k ${k}
# done

# ---------- Torus 8x8 hop latency (fixed 400Gb/s) ----------
# Alpha = link_lat + input_latency + output_latency.
#   200ns  -> 50ns + 75ns + 75ns
#   800ns  -> 200ns + 300ns + 300ns
#   1600ns -> 400ns + 600ns + 600ns
# Baseline 400ns (100+150+150) stays in torus_8x8 and is not rerun.
# Swing is both SwingB and SwingL. Trees_1..8, Bucket, Rings.
# Trees_1 at 2**24 and above stays disabled (same stall as the BW sweep).
# Outputs: torus_8x8_lat{200,800,1600}ns/<bench>/<count>

# ---------- Torus 8x8 Bandwidth Impact (Fig. 8) ----------
# Goodput gain of Swing vs best-known algo across fabric BW.
# netBW=torusBW; path suffix from netBW (400Gb/s → torus_8x8, else torus_8x8_<bw>).
#
# tree_set 0..7 selects d1..d8 and the matching route-table JSON.
JOB_TORUS="--topo torus --job_size 8x8"
ROUTE_TORUS_D8="/home/mipt-user/torus-allreduce/scratch/src/sst-elements/src/sst/elements/ember/mpi/motifs/emberroutingtables/ember_bdmst_routing_table_8_8.json"
# RecDoubB and L variants are excluded from run_bandwidth. SwingL has its own
# restart-safe torus8_swingl_bandwidth stage for counts 2**3..2**20.

# ---------- Torus 64x64 Trees sweep ----------
# Compiled quadruples d1..d64; tree_set is d-1.
# RUN_STAGE=torus64_trees continues Trees_64..Trees_1 at counts 2**14..2**23.
# A file is skipped only when it already contains "Simulation is complete".
JOB_TORUS_64="--topo torus --job_size 64x64"
EXTRA_TORUS_64="${EXTRA_TORUS_BASE} --netBW 400Gb/s --torusBW 400Gb/s"
# Square 32x32. Same 400 Gb/s fabric as 64x64. tree_set = d - 1.
JOB_TORUS_32="--topo torus --job_size 32x32"
EXTRA_TORUS_32="${EXTRA_TORUS_BASE} --netBW 400Gb/s --torusBW 400Gb/s"

# ---------- Rectangular torus 1024 nodes (64x16 / 128x8 / 256x4) ----------
# Paper-style baselines + Trees. BW fixed 400Gb/s → output torus_<shape>/.
# Trees: empty JSON route tables (VC/path in tree edges); C++ selects the
# matching bdms_tree_specs_*_double pack from dimensions_sizes.
ROUTE_RT="/home/mipt-user/torus-allreduce/scratch/src/sst-elements/src/sst/elements/ember/mpi/motifs/emberroutingtables"
EXTRA_RECT="${EXTRA_TORUS_BASE} --netBW 400Gb/s --torusBW 400Gb/s"

# shape → route_table (Trees only; ignored by other benches)
declare -A ROUTE_RECT=(
  [64x16]="${ROUTE_RT}/ember_bdmst_routing_table_64_16_40.json"
  [128x8]="${ROUTE_RT}/ember_bdmst_routing_table_128_8_68.json"
  [256x4]="${ROUTE_RT}/ember_bdmst_routing_table_256_4_130.json"
)
# Rings: launcher keeps job_size/output as 64x16 but Merlin shape becomes 16x64
# so RingAllreduce25D neighbors are single-hop (see rings_px_and_merlin_shape).
# Completed latency-optimal runs, count=2**7..2**20 FLOAT elements.
# for shape in 64x16 128x8 256x4; do
#   python3 launchAll.py --topo torus --job_size ${shape} ${EXTRA_RECT} \
#     --bench SwingL,RecDoubL --counts 7-20
# done

run_if_missing() {
  local output_file="$1"
  shift
  if [[ -f "${output_file}" ]] &&
     grep -q '^Simulation is complete' "${output_file}"; then
    echo "SKIP complete: ${output_file}"
    return
  fi
  python3 launchAll.py "$@"
}

run_if_missing_radix() {
  local output_file="$1"
  shift
  if [[ -f "${output_file}" ]] &&
     grep -q '^Simulation is complete' "${output_file}"; then
    echo "SKIP complete: ${output_file}"
    return 0
  fi
  timeout --kill-after=30s "${SPINELEAF_RADIX_TIMEOUT}" \
    python3 launchAll.py "$@"
}

# Balanced overlay on rectangular tori. Lo is the packet floor; hi is 2**26.
declare -A RECT_OVERLAY_EXP_LO=(
  [64x16]=18
  [128x8]=19
  [256x4]=20
)
RECT_OVERLAY_EXP_HI=26

# 256x4 Overlay only: count = 2**12 .. 2**19 FLOAT elements.
# Output: torus_256x4/Overlay/<count>
run_rect_overlay_256x4() {
  local exp count output_file
  for ((exp = 12; exp <= 19; exp++)); do
    count=$((1 << exp))
    output_file="${OUTPUT_ROOT}/torus_256x4/Overlay/${count}"
    run_if_missing "${output_file}" \
      --topo torus --job_size 256x4 ${EXTRA_RECT} \
      --bench Overlay --counts "${exp}"
  done
}

run_rect_overlay() {
  local shape exp lo output_file
  for shape in 64x16 128x8 256x4; do
    lo="${RECT_OVERLAY_EXP_LO[${shape}]}"
    for ((exp = lo; exp <= RECT_OVERLAY_EXP_HI; exp++)); do
      output_file="${OUTPUT_ROOT}/torus_${shape}/Overlay/$((1 << exp))"
      run_if_missing "${output_file}" \
        --topo torus --job_size "${shape}" ${EXTRA_RECT} \
        --bench Overlay --counts "${exp}"
    done
  done
}

# Two BFS trees (overlay_rect_*_bfs2.json). K = 4096 equal shares, so the
# smallest power of two with a non-zero integer chunk per tree is 2**12.
# Through 2**26. Output stays in Overlay_bfs2, separate from Overlay/.
declare -A RECT_BFS2_EXP_LO=(
  [64x16]=12
  [128x8]=12
)
run_rect_overlay_bfs2() {
  local shape exp lo output_file
  for shape in 64x16 128x8; do
    lo="${RECT_BFS2_EXP_LO[${shape}]}"
    for ((exp = lo; exp <= RECT_OVERLAY_EXP_HI; exp++)); do
      output_file="${OUTPUT_ROOT}/torus_${shape}/Overlay_bfs2/$((1 << exp))"
      run_if_missing "${output_file}" \
        --topo torus --job_size "${shape}" ${EXTRA_RECT} \
        --bench Overlay --overlay_k bfs2 --counts "${exp}"
    done
  done
}

run_rect() {
  local shape bench output_file
  # Global light-to-heavy order. RecDoubB and L variants are excluded.
  for bench in Rings Bucket SwingB Trees; do
    for shape in 64x16 128x8 256x4; do
      output_file="${OUTPUT_ROOT}/torus_${shape}/${bench}/${COUNT_VALUE}"
      run_if_missing "${output_file}" \
        --topo torus --job_size "${shape}" ${EXTRA_RECT} \
        --bench "${bench}" --counts "${COUNT_EXP}" \
        --route_table_file "${ROUTE_RECT[${shape}]}"
    done
  done
}

run_bandwidth() {
  local bench bw topo_dir output_bench output_file
  # At 8x8: Rings < Bucket < Trees_8 < SwingB by configured shortValue.
  for bench in Rings Bucket Trees SwingB; do
    for bw in 100Gb/s 200Gb/s 400Gb/s 800Gb/s 1600Gb/s 3200Gb/s; do
      if [[ "${bw}" == "400Gb/s" ]]; then
        topo_dir="torus_8x8"
      else
        topo_dir="torus_8x8_${bw%/s}"
      fi
      if [[ "${bench}" == "Trees" ]]; then
        output_bench="Trees_8"
      else
        output_bench="${bench}"
      fi
      output_file="${OUTPUT_ROOT}/${topo_dir}/${output_bench}/${COUNT_VALUE}"
    EXTRA_T="${EXTRA_TORUS_BASE} --netBW ${bw} --torusBW ${bw}"
      run_if_missing "${output_file}" \
        ${JOB_TORUS} ${EXTRA_T} --bench "${bench}" \
        --counts "${COUNT_EXP}" --tree_sets 7 \
        --route_table_file "${ROUTE_TORUS_D8}"
    done
  done
}

run_torus8_trees_bandwidth() {
  local tree_set d exp count bw topo_dir output_file extra_t

  # Trees_8 is already complete; sweep Trees_1..7 only.
  for tree_set in 0 1 2 3 4 5 6; do
    d=$((tree_set + 1))
    for ((exp = TORUS8_TREES_EXP_LO; exp <= TORUS8_TREES_EXP_HI; exp++)); do
      if ((tree_set == 0 && exp >= 24)); then
        echo "SKIP Trees_1 count=2**${exp}: disabled after stalled large run"
        continue
      fi
      count=$((1 << exp))
      for bw in 100Gb/s 200Gb/s 400Gb/s 800Gb/s 1600Gb/s 3200Gb/s; do
        if [[ "${bw}" == "400Gb/s" ]]; then
          topo_dir="torus_8x8"
        else
          topo_dir="torus_8x8_${bw%/s}"
        fi
        output_file="${OUTPUT_ROOT}/${topo_dir}/Trees_${d}/${count}"
        extra_t="${EXTRA_TORUS_BASE} --netBW ${bw} --torusBW ${bw}"
        run_if_missing "${output_file}" \
          ${JOB_TORUS} ${extra_t} --bench Trees \
          --counts "${exp}" --tree_sets "${tree_set}" \
          --route_table_file "${ROUTE_TORUS_D8}"
      done
    done
  done
}

run_torus8_latency() {
  local lat_ns bench exp count tree_set d lo hi output_file

  for lat_ns in 200 800 1600; do
    for bench in Rings Bucket SwingB SwingL; do
      if [[ "${bench}" == "SwingL" ]]; then
        lo="${TORUS8_LAT_SWINGL_EXP_LO}"
        hi="${TORUS8_LAT_SWINGL_EXP_HI}"
      else
        lo="${TORUS8_LAT_EXP_LO}"
        hi="${TORUS8_LAT_EXP_HI}"
      fi
      for ((exp = lo; exp <= hi; exp++)); do
        count=$((1 << exp))
        output_file="${OUTPUT_ROOT}/torus_8x8_lat${lat_ns}ns/${bench}/${count}"
        run_if_missing "${output_file}" \
          ${JOB_TORUS} ${EXTRA_TORUS_BASE} \
          --netBW 400Gb/s --torusBW 400Gb/s \
          --hopLat "${lat_ns}ns" --bench "${bench}" --counts "${exp}"
      done
    done

    # Trees_8 first: that is the SP schedule used in the bandwidth figure.
    for ((tree_set = 7; tree_set >= 0; tree_set--)); do
      d=$((tree_set + 1))
      for ((exp = TORUS8_LAT_EXP_LO; exp <= TORUS8_LAT_EXP_HI; exp++)); do
        if ((tree_set == 0 && exp >= 24)); then
          echo "SKIP Trees_1 count=2**${exp} lat=${lat_ns}ns: disabled after stalled large run"
          continue
        fi
        count=$((1 << exp))
        output_file="${OUTPUT_ROOT}/torus_8x8_lat${lat_ns}ns/Trees_${d}/${count}"
        run_if_missing "${output_file}" \
          ${JOB_TORUS} ${EXTRA_TORUS_BASE} \
          --netBW 400Gb/s --torusBW 400Gb/s \
          --hopLat "${lat_ns}ns" --bench Trees \
          --counts "${exp}" --tree_sets "${tree_set}" \
          --route_table_file "${ROUTE_TORUS_D8}"
      done
    done
  done
}

run_torus8_swingl_bandwidth() {
  local exp count bw topo_dir output_file extra_t

  for ((exp = TORUS8_SWINGL_EXP_LO; exp <= TORUS8_SWINGL_EXP_HI; exp++)); do
    count=$((1 << exp))
    for bw in 100Gb/s 200Gb/s 400Gb/s 800Gb/s 1600Gb/s 3200Gb/s; do
      if [[ "${bw}" == "400Gb/s" ]]; then
        topo_dir="torus_8x8"
      else
        topo_dir="torus_8x8_${bw%/s}"
      fi
      output_file="${OUTPUT_ROOT}/${topo_dir}/SwingL/${count}"
      extra_t="${EXTRA_TORUS_BASE} --netBW ${bw} --torusBW ${bw}"
      run_if_missing "${output_file}" \
        ${JOB_TORUS} ${extra_t} --bench SwingL --counts "${exp}"
    done
  done
}

run_torus64_trees() {
  local d tree_set exp count output_file

  for ((d = 64; d >= 1; d--)); do
    tree_set=$((d - 1))
    for ((exp = TORUS64_TREES_EXP_LO; exp <= TORUS64_TREES_EXP_HI; exp++)); do
      count=$((1 << exp))
      output_file="${OUTPUT_ROOT}/torus_64x64/Trees_${d}/${count}"
      run_if_missing "${output_file}" \
        ${JOB_TORUS_64} ${EXTRA_TORUS_64} --bench Trees \
        --counts "${exp}" --tree_sets "${tree_set}"
    done
  done
}

# Finish missing square-torus Trees through count=2**26.
# 32x32 Trees_20..31 first (2**12..2**26), then 64x64 Trees_12..63
# (2**14..2**26). Completed outputs are skipped.
run_torus_trees_range() {
  local job="$1"
  local extra="$2"
  local topo_dir="$3"
  local d_lo="$4"
  local d_hi="$5"
  local exp_lo="$6"
  local exp_hi="$7"
  local d tree_set exp count output_file

  for ((d = d_lo; d <= d_hi; d++)); do
    tree_set=$((d - 1))
    for ((exp = exp_lo; exp <= exp_hi; exp++)); do
      count=$((1 << exp))
      output_file="${OUTPUT_ROOT}/${topo_dir}/Trees_${d}/${count}"
      run_if_missing "${output_file}" \
        ${job} ${extra} --bench Trees \
        --counts "${exp}" --tree_sets "${tree_set}"
    done
  done
}

run_torus_trees_to26() {
  if ! grep -a -q 'torus_32x32_d20' "${LIBEMBER}"; then
    echo "libember.so has no torus 32x32 packs (missing torus_32x32_d20): ${LIBEMBER}" >&2
    echo "Rebuild sst-elements ember, then rerun RUN_STAGE=torus_trees_to26." >&2
    echo "64x64 was not started, so it still runs after 32x32." >&2
    exit 1
  fi

  echo "torus 32x32 Trees_${TORUS32_TREES_D_LO}..Trees_${TORUS32_TREES_D_HI} counts 2**${TORUS32_TREES_EXP_LO}..2**${TORUS32_TREES_EXP_HI}"
  run_torus_trees_range \
    "${JOB_TORUS_32}" "${EXTRA_TORUS_32}" "torus_32x32" \
    "${TORUS32_TREES_D_LO}" "${TORUS32_TREES_D_HI}" \
    "${TORUS32_TREES_EXP_LO}" "${TORUS32_TREES_EXP_HI}"

  echo "torus 64x64 Trees_${TORUS64_TREES_TO26_D_LO}..Trees_${TORUS64_TREES_TO26_D_HI} counts 2**${TORUS64_TREES_TO26_EXP_LO}..2**${TORUS64_TREES_TO26_EXP_HI}"
  run_torus_trees_range \
    "${JOB_TORUS_64}" "${EXTRA_TORUS_64}" "torus_64x64" \
    "${TORUS64_TREES_TO26_D_LO}" "${TORUS64_TREES_TO26_D_HI}" \
    "${TORUS64_TREES_TO26_EXP_LO}" "${TORUS64_TREES_TO26_EXP_HI}"
}

# Torus 64x64 k-hop trees at 64 MiB: Trees_11..Trees_14, count=2**24.
# Outputs: torus_64x64/Trees_{11,12,13,14}/16777216
run_torus64_khop_64mib() {
  echo "torus 64x64 Trees_${TORUS64_KHOP_D_LO}..Trees_${TORUS64_KHOP_D_HI} count 2**${TORUS64_KHOP_EXP} (64 MiB)"
  run_torus_trees_range \
    "${JOB_TORUS_64}" "${EXTRA_TORUS_64}" "torus_64x64" \
    "${TORUS64_KHOP_D_LO}" "${TORUS64_KHOP_D_HI}" \
    "${TORUS64_KHOP_EXP}" "${TORUS64_KHOP_EXP}"
}

# RecDoubB on Spine-Leaf 16s64l at count=2**25 (33554432 FLOATs).
run_spineleaf_recdoub() {
  local exp count output_file
  exp=25
  count=$((1 << exp))
  output_file="${OUTPUT_ROOT}/fattree_spineleaf_16s64l/RecDoubB/${count}"
  run_if_missing "${output_file}" \
    ${JOB_SL} ${EXTRA_SL} --bench RecDoubB --counts "${exp}"
}

# RecDoubB only. Rectangular tori (1024 nodes) at 2**25 and 2**26,
# then HyperX 64x64 at 2**25. Completed outputs are skipped.
run_recdoub_b_large() {
  local shape exp count output_file
  for exp in 25 26; do
    count=$((1 << exp))
    for shape in 64x16 128x8 256x4; do
      output_file="${OUTPUT_ROOT}/torus_${shape}/RecDoubB/${count}"
      run_if_missing "${output_file}" \
        --topo torus --job_size "${shape}" ${EXTRA_RECT} \
        --bench RecDoubB --counts "${exp}"
    done
  done

  exp=25
  count=$((1 << exp))
  output_file="${OUTPUT_ROOT}/hyperx_64x64/RecDoubB/${count}"
  run_if_missing "${output_file}" \
    ${JOB_HX} ${EXTRA_HX} --bench RecDoubB --counts "${exp}"
}

run_spineleaf() {
  local output_file
  output_file="${OUTPUT_ROOT}/fattree_spineleaf_16s64l/Rings/${COUNT_VALUE}"
  run_if_missing "${output_file}" \
    ${JOB_SL} ${EXTRA_SL} --bench Rings --counts "${COUNT_EXP}"

  output_file="${OUTPUT_ROOT}/fattree_spineleaf_16s64l/DBTree/${COUNT_VALUE}"
  run_if_missing "${output_file}" \
    ${JOB_SL} ${EXTRA_SL} --bench DBTree --counts "${COUNT_EXP}"

  # k60x4 is the best completed Overlay variant at count=2**24.
  output_file="${OUTPUT_ROOT}/fattree_spineleaf_16s64l/Overlay_k60x4/${COUNT_VALUE}"
  run_if_missing "${output_file}" \
    ${JOB_SL} ${EXTRA_SL} --bench Overlay --overlay_k 60x4 \
    --counts "${COUNT_EXP}"
}

run_spineleaf_overlay() {
  local border exp count trees_file output_file
  local borders=(16 8)

  # Fail before launching anything if the requested schedule set is incomplete.
  for border in "${borders[@]}"; do
    trees_file="${SL_TREES}/overlay_sl_s16_l64_b${border}_constructive.json"
    if [[ ! -f "${trees_file}" ]]; then
      echo "Missing Spine-Leaf schedule: ${trees_file}" >&2
      return 1
    fi
  done

  # For every payload, run the least restrictive schedule first: b63 -> b8.
  for ((exp = 10; exp <= 24; exp++)); do
    count=$((1 << exp))
    for border in "${borders[@]}"; do
      trees_file="${SL_TREES}/overlay_sl_s16_l64_b${border}_constructive.json"
      output_file="${OUTPUT_ROOT}/fattree_spineleaf_16s64l/Overlay_b${border}_constructive/${count}"
      run_if_missing "${output_file}" \
        ${JOB_SL} ${EXTRA_SL} --bench Overlay \
        --trees_file "${trees_file}" \
        --overlay_k "b${border}_constructive" \
        --counts "${exp}"
    done
  done
}

run_spineleaf_radix() {
  local name exp count trees_file output_file
  # Fewer stages first: 4 -> 5 -> 7 -> 10.
  local names=(radix4x8 radix4 radix2x4 radix2)
  declare -A blocked=()

  for name in "${names[@]}"; do
    trees_file="${SL_TREES}/overlay_sl_s16_l64_${name}_constructive.json"
    if [[ ! -f "${trees_file}" || ! -f "${trees_file%.json}.pack.json" ]]; then
      echo "Missing Spine-Leaf radix schedule or pack audit: ${trees_file}" >&2
      return 1
    fi
  done

  for ((exp = SPINELEAF_RADIX_EXP_LO; exp <= SPINELEAF_RADIX_EXP_HI; exp++)); do
    count=$((1 << exp))
    for name in "${names[@]}"; do
      if [[ "${blocked[${name}]:-0}" == 1 ]]; then
        echo "SKIP ${name} at ${count}: failed or timed out at a smaller count"
        continue
      fi
      trees_file="${SL_TREES}/overlay_sl_s16_l64_${name}_constructive.json"
      output_file="${OUTPUT_ROOT}/fattree_spineleaf_16s64l/Overlay_${name}_constructive/${count}"
      if ! run_if_missing_radix "${output_file}" \
        ${JOB_SL} ${EXTRA_SL} --bench Overlay \
        --trees_file "${trees_file}" \
        --overlay_k "${name}_constructive" \
        --counts "${exp}"; then
        blocked["${name}"]=1
        echo "BLOCK ${name} after count=${count}"
      fi
    done
  done
}

run_hyperx() {
  local bench output_bench output_file
  # HyperX Rings and RecDoubB remain deferred.
  for bench in Bucket Trees SwingB; do
    if [[ "${bench}" == "Trees" ]]; then
      output_bench="Trees_budget_b16"
    else
      output_bench="${bench}"
    fi
    output_file="${OUTPUT_ROOT}/hyperx_64x64/${output_bench}/${COUNT_VALUE}"
    run_if_missing "${output_file}" \
      ${JOB_HX} ${EXTRA_HX} --bench "${bench}" \
      --counts "${COUNT_EXP}" --tree_sets 7
  done
}

run_hyperx_tree_exp() {
  local exp="$1"
  local value=$((1 << exp))
  local output_file="${OUTPUT_ROOT}/hyperx_64x64/Trees_budget_b16/${value}"
  run_if_missing "${output_file}" \
    ${JOB_HX} ${EXTRA_HX} --bench Trees \
    --counts "${exp}" --tree_sets 7
}

run_hyperx_trees() {
  local exp
  for ((exp = HYPERX_TREES_EXP_LO; exp <= HYPERX_TREES_EXP_HI; exp++)); do
    run_hyperx_tree_exp "${exp}"
  done
}

# Spine-Leaf 16 spines x 32 leaves, 512 hosts.
# Bucket, recursive doubling, Ring. DBTree is omitted: the 16s32l anneal
# is not ready. Radix schedules are separate, below.
# Swing on 16s32l. Merlin shape 16,16:32 numbers hosts as
# nid = leaf * 16 + host_on_leaf, and generateNidListRange keeps rank == nid.
# A leaf is a block of 16 consecutive ranks, so the ring steps of length
# 1, 2, 4 and 8 stay on that leaf except at the block boundary.
# There is no cheaper order across leaves: every leaf pair is one spine hop.
# SwingL is 2**7..2**20, SwingB is 2**9..2**26. For one count, B then L.
run_spineleaf32_swing() {
  local exp count output_dir
  output_dir="${OUTPUT_ROOT}/fattree_spineleaf_16s32l"
  for ((exp = SL32_SMALL_EXP_LO; exp <= SIZE32_EXP_HI; exp++)); do
    count=$((1 << exp))
    echo "Spine-Leaf 16s32l Swing count=2**${exp}"
    if ((exp >= SL32_EXP_LO && exp <= SIZE32_EXP_HI)); then
      run_if_missing "${output_dir}/SwingB/${count}" \
        ${JOB_SL32} ${EXTRA_SL} --bench SwingB --counts "${exp}"
    fi
    if ((exp <= SL32_LAT_EXP_HI)); then
      run_if_missing "${output_dir}/SwingL/${count}" \
        ${JOB_SL32} ${EXTRA_SL} --bench SwingL --counts "${exp}"
    fi
  done
}

# One point: SwingB at 2**26 (256 MiB). The file already contains
# "Simulation is complete", so run_if_missing would keep the bad TIME lines.
# This call always overwrites output/fattree_spineleaf_16s32l/SwingB/67108864.
run_spineleaf32_swingb26() {
  local exp=26
  local count=$((1 << exp))
  local output_file="${OUTPUT_ROOT}/fattree_spineleaf_16s32l/SwingB/${count}"
  echo "Spine-Leaf 16s32l SwingB count=2**${exp} -> ${output_file}"
  python3 launchAll.py ${JOB_SL32} ${EXTRA_SL} --bench SwingB --counts "${exp}"
}

run_spineleaf32_baselines() {
  local bench lo hi exp count output_file
  for bench in Bucket Rings RecDoubL RecDoubB; do
    case "${bench}" in
      RecDoubL)
        lo="${SL32_SMALL_EXP_LO}"
        hi="${SL32_LAT_EXP_HI}"
        ;;
      RecDoubB)
        lo="${SL32_EXP_LO}"
        hi="${SIZE32_EXP_HI}"
        ;;
      *)
        lo="${SL32_SMALL_EXP_LO}"
        hi="${SIZE32_EXP_HI}"
        ;;
    esac
    for ((exp = lo; exp <= hi; exp++)); do
      count=$((1 << exp))
      output_file="${OUTPUT_ROOT}/fattree_spineleaf_16s32l/${bench}/${count}"
      run_if_missing "${output_file}" \
        ${JOB_SL32} ${EXTRA_SL} --bench "${bench}" --counts "${exp}"
    done
  done
}

# Depth-2 overlay, k1=1 and k2=1. One FLOAT per tree at 2**9.
# A same-leaf isend packs 32 chunks, so shortValue is count/4 bytes
# (general_bench.py). Output: Overlay_k1x1/<count>, counts 2**9..2**26.
run_spineleaf32_k1x1() {
  local exp count trees_file output_file
  trees_file="${SL_TREES}/overlay_sl_s16_l32_k1x1.json"
  if [[ ! -f "${trees_file}" ]]; then
    echo "Missing Spine-Leaf 16s32l k1x1 schedule: ${trees_file}" >&2
    return 1
  fi

  for ((exp = SL32_EXP_LO; exp <= SIZE32_EXP_HI; exp++)); do
    count=$((1 << exp))
    output_file="${OUTPUT_ROOT}/fattree_spineleaf_16s32l/Overlay_k1x1/${count}"
    run_if_missing "${output_file}" \
      ${JOB_SL32} ${EXTRA_SL} --bench Overlay \
      --trees_file "${trees_file}" \
      --overlay_k 1x1 \
      --counts "${exp}"
  done
}

# Wider fan-in first: radix4x8 (4 stages) through radix2 (9 stages).
# A schedule that times out or fails is not retried at larger payloads.
run_spineleaf32_radix() {
  local name exp count trees_file output_file
  local names=(radix4x8 radix4 radix2x4 radix2)
  declare -A blocked=()

  for name in "${names[@]}"; do
    trees_file="${SL_TREES}/overlay_sl_s16_l32_${name}_constructive.json"
    if [[ ! -f "${trees_file}" || ! -f "${trees_file%.json}.pack.json" ]]; then
      echo "Missing Spine-Leaf 16s32l radix schedule or pack audit: ${trees_file}" >&2
      return 1
    fi
  done

  for ((exp = SL32_EXP_LO; exp <= SIZE32_EXP_HI; exp++)); do
    count=$((1 << exp))
    for name in "${names[@]}"; do
      if [[ "${blocked[${name}]:-0}" == 1 ]]; then
        echo "SKIP ${name} at ${count}: failed or timed out at a smaller count"
        continue
      fi
      trees_file="${SL_TREES}/overlay_sl_s16_l32_${name}_constructive.json"
      output_file="${OUTPUT_ROOT}/fattree_spineleaf_16s32l/Overlay_${name}_constructive/${count}"
      if ! run_if_missing_radix "${output_file}" \
        ${JOB_SL32} ${EXTRA_SL} --bench Overlay \
        --trees_file "${trees_file}" \
        --overlay_k "${name}_constructive" \
        --counts "${exp}"; then
        blocked["${name}"]=1
        echo "BLOCK ${name} after count=${count}"
      fi
    done
  done
}

# HyperX 32x32. Bucket, Swing, Ring. SwingL is the small-message half of
# Swing and stops at 2**20; SwingB continues through 2**26.
run_hyperx32_baselines() {
  local bench lo hi exp count output_file
  for bench in Bucket Rings SwingL SwingB; do
    if [[ "${bench}" == "SwingL" ]]; then
      lo="${HX32_SMALL_EXP_LO}"
      hi="${HX32_LAT_EXP_HI}"
    else
      lo="${HX32_EXP_LO}"
      hi="${SIZE32_EXP_HI}"
    fi
    for ((exp = lo; exp <= hi; exp++)); do
      count=$((1 << exp))
      output_file="${OUTPUT_ROOT}/hyperx_32x32/${bench}/${count}"
      run_if_missing "${output_file}" \
        ${JOB_HX32} ${EXTRA_HX} --bench "${bench}" --counts "${exp}"
    done
  done
}

# HyperX 32x32 Trees_budget_b64 only (tree_set 9, S=4). No timeout: the
# 2**26 run exceeds the 1h limit used by run_if_missing_radix.
# Requires the Ember rebuild that includes the 32x32 budget headers.
run_hyperx32_budgets() {
  local exp count output_file header route
  local name=budget_b64
  local tree_set=9

  header="${SL_TREES}/../emberbdmstrees_hyperx_32_32_${name}.h"
  route="${ROUTE_RT}/ember_bdmst_routing_table_hyperx_32_32_${name}.json"
  if [[ ! -f "${header}" || ! -f "${route}" ]]; then
    echo "Missing HyperX 32x32 budget pack: ${header} or ${route}" >&2
    return 1
  fi

  for ((exp = HX32_EXP_LO; exp <= SIZE32_EXP_HI; exp++)); do
    count=$((1 << exp))
    output_file="${OUTPUT_ROOT}/hyperx_32x32/Trees_${name}/${count}"
    run_if_missing "${output_file}" \
      ${JOB_HX32} ${EXTRA_HX} --bench Trees \
      --counts "${exp}" --tree_sets "${tree_set}"
  done
}

run_size32() {
  run_hyperx32_budgets
}

# TrivanceB/L next to SwingB/L. Rectangular job_size is passed unchanged:
# dimensions_sizes=64,16 is reversed inside the motif to [16, 64], which is
# Merlin's idToLocation for shape 64x16 (fast axis 64). The Rings px swap
# does not apply.
# One call launches only the variants whose Swing range contains this count.
run_trivance_one() {
  local exp="$1"
  local output_dir="$2"
  local job="$3"
  local extra="$4"
  local lo_b="$5"
  local hi_b="$6"
  local lo_l="$7"
  local hi_l="$8"
  local count=$((1 << exp))

  if ((exp >= lo_b && exp <= hi_b)); then
    run_if_missing "${output_dir}/TrivanceB/${count}" \
      ${job} ${extra} --bench TrivanceB --counts "${exp}"
  fi
  if ((lo_l <= hi_l && exp >= lo_l && exp <= hi_l)); then
    run_if_missing "${output_dir}/TrivanceL/${count}" \
      ${job} ${extra} --bench TrivanceL --counts "${exp}"
  fi
}

run_trivance_at_count() {
  local exp="$1"
  local shape extra output_dir job

  echo "Trivance count=2**${exp}"

  # Square tori at 400 Gb/s. TrivanceB stops at 2**26.
  # TrivanceL is 2**3..2**20 on 8x8 and 2**3..2**19 on the larger squares.
  # Bandwidth and hop-latency variants of 8x8 are below.
  for shape in 8x8 16x16 32x32 64x64; do
    job="--topo torus --job_size ${shape}"
    extra="${EXTRA_TORUS_BASE} --netBW 400Gb/s --torusBW 400Gb/s"
    output_dir="${OUTPUT_ROOT}/torus_${shape}"
    if [[ "${shape}" == "8x8" ]]; then
      run_trivance_one "${exp}" "${output_dir}" "${job}" "${extra}" 8 26 3 20
    else
      run_trivance_one "${exp}" "${output_dir}" "${job}" "${extra}" 8 26 3 19
    fi
  done

  # HyperX 32x32. Logical 32x32 coordinates, same as Swing.
  run_trivance_one "${exp}" \
    "${OUTPUT_ROOT}/hyperx_32x32" \
    "--topo hyperx --job_size 32x32" \
    "${EXTRA_HX}" \
    12 26 7 20

  # Spine-Leaf 16 spines x 32 leaves, 512 hosts. Same ranges as Swing:
  # TrivanceB is 2**9..2**26, TrivanceL is 2**7..2**20.
  run_trivance_one "${exp}" \
    "${OUTPUT_ROOT}/fattree_spineleaf_16s32l" \
    "${JOB_SL32}" \
    "${EXTRA_SL}" \
    9 26 7 20

  # Rectangular tori. job_size stays 64x16 / 128x8 / 256x4.
  for shape in 64x16 128x8 256x4; do
    job="--topo torus --job_size ${shape}"
    extra="${EXTRA_TORUS_BASE} --netBW 400Gb/s --torusBW 400Gb/s"
    output_dir="${OUTPUT_ROOT}/torus_${shape}"
    if [[ "${shape}" == "64x16" ]]; then
      run_trivance_one "${exp}" "${output_dir}" "${job}" "${extra}" 8 26 7 20
    else
      run_trivance_one "${exp}" "${output_dir}" "${job}" "${extra}" 12 26 7 20
    fi
  done

  # Torus 8x8 fabric bandwidth. 400 Gb/s is the square-torus case above.
  # TrivanceL stops at 2**20, same as the SwingL bandwidth stage.
  job="--topo torus --job_size 8x8"
  for bw in 100Gb/s 200Gb/s 800Gb/s 1600Gb/s 3200Gb/s; do
    extra="${EXTRA_TORUS_BASE} --netBW ${bw} --torusBW ${bw}"
    output_dir="${OUTPUT_ROOT}/torus_8x8_${bw%/s}"
    run_trivance_one "${exp}" "${output_dir}" "${job}" "${extra}" 8 26 3 20
  done

  # Torus 8x8 hop latency at 400 Gb/s. 400 ns stays in torus_8x8.
  # TrivanceL continues through 2**23, same as SwingL on this sweep.
  for lat_ns in 200 800 1600; do
    extra="${EXTRA_TORUS_BASE} --netBW 400Gb/s --torusBW 400Gb/s --hopLat ${lat_ns}ns"
    output_dir="${OUTPUT_ROOT}/torus_8x8_lat${lat_ns}ns"
    run_trivance_one "${exp}" "${output_dir}" "${job}" "${extra}" 8 26 3 23
  done
}

run_trivance_all() {
  local exp
  # Smallest SwingL point is 2**3. TrivanceB never goes past 2**26.
  for ((exp = 3; exp <= 26; exp++)); do
    run_trivance_at_count "${exp}"
  done
}

run_remaining25() {
  run_spineleaf_radix
  run_hyperx_trees
}

run_large26() {
  # The existing HyperX Trees @2**25 file is incomplete; run_if_missing
  # detects the missing completion marker and overwrites it with a fresh run.
  echo "Backfill HyperX 64x64 Trees_budget_b16 at count=2**25"
  run_hyperx_tree_exp 25

  COUNT_EXP=26
  COUNT_VALUE=$((1 << COUNT_EXP))
  echo "Run configured experiment suite at count=2**26 (${COUNT_VALUE} FLOATs)"

  # Small to large topologies. All functions are restart-safe.
  run_bandwidth
  run_rect

  # Only the new 2**26 Overlay points; do not walk the older range here.
  local shape output_file
  for shape in 64x16 128x8 256x4; do
    output_file="${OUTPUT_ROOT}/torus_${shape}/Overlay/${COUNT_VALUE}"
    run_if_missing "${output_file}" \
      --topo torus --job_size "${shape}" ${EXTRA_RECT} \
      --bench Overlay --counts "${COUNT_EXP}"
  done

  run_spineleaf
  run_hyperx
}

case "${RUN_STAGE}" in
  torus8_trees_bandwidth)
    run_torus8_trees_bandwidth
    ;;
  torus8_swingl_bandwidth)
    run_torus8_swingl_bandwidth
    ;;
  torus8_latency)
    run_torus8_latency
    ;;
  torus64_trees)
    run_torus64_trees
    ;;
  torus64_khop_64mib)
    run_torus64_khop_64mib
    ;;
  torus_trees_to26)
    run_torus_trees_to26
    ;;
  hyperx_tree25)
    run_hyperx_tree_exp 25
    ;;
  remaining25)
    run_remaining25
    ;;
  rect)
    run_rect
    ;;
  rect_overlay)
    run_rect_overlay
    ;;
  rect_overlay_256x4)
    run_rect_overlay_256x4
    ;;
  rect_overlay_bfs2)
    run_rect_overlay_bfs2
    ;;
  bandwidth)
    run_bandwidth
    ;;
  spineleaf)
    run_spineleaf
    ;;
  spineleaf_recdoub)
    run_spineleaf_recdoub
    ;;
  spineleaf32_swing)
    run_spineleaf32_swing
    ;;
  spineleaf32_swingb26)
    run_spineleaf32_swingb26
    ;;
  spineleaf32_k1x1)
    run_spineleaf32_k1x1
    ;;
  recdoub_b_large)
    run_recdoub_b_large
    ;;
  spineleaf_overlay)
    run_spineleaf_overlay
    ;;
  spineleaf_radix)
    run_spineleaf_radix
    ;;
  hyperx_trees)
    run_hyperx_trees
    ;;
  hyperx)
    run_hyperx
    ;;
  large26)
    run_large26
    ;;
  size32)
    run_size32
    run_trivance_all
    ;;
  triviance_all)
    run_trivance_all
    ;;
  all)
    run_rect
    run_rect_overlay
    run_bandwidth
    run_spineleaf
    run_hyperx
    ;;
  *)
    echo "Unknown RUN_STAGE=${RUN_STAGE}; expected torus8_trees_bandwidth, torus8_swingl_bandwidth, torus8_latency, torus64_trees, torus64_khop_64mib, torus_trees_to26, hyperx_tree25, remaining25, rect, rect_overlay, rect_overlay_256x4, rect_overlay_bfs2, spineleaf_radix, spineleaf_recdoub, spineleaf32_swing, spineleaf32_swingb26, spineleaf32_k1x1, recdoub_b_large, bandwidth, spineleaf, spineleaf_overlay, hyperx_trees, hyperx, large26, size32, triviance_all, or all" >&2
    exit 2
    ;;
esac

