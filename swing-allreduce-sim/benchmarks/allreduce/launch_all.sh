#!/bin/bash
# BENCHS="RecDoubL,RecDoubB,SwingB,SwingL,RecDoubBM,RecDoubLM"
BENCHS="Bucket,SwingB"
BENCH_EVEN="SwingB,SwingL,Bucket"
# BENCHS="SwingB"
EXTRA_LARGE="--env slimfly --nodes 32 --hostfile /home/gera/hosts.cluster --num-threads 32"
EXTRA_SMALL="--env local --nodes 64 --hostfile /home/gera/hosts.cluster --num-threads 1"

python3 launchAll.py --topo torus --job_size 8x8 --netBW 1000000Gb/s --hostBW 1000000Gb/s --torusBW 400Gb/s --env local --bench SwingL 

# python3 launchAll.py \
#   --topo torus \
#   --job_size 8x8 \
#   --bench Trees \
#   --env slimfly \
#   --nodes 32 \
#   --hostfile /home/gera/hosts.cluster \
#   --num-threads 32 \
#   --netBW 1600Gb/s \
#   --hostBW 1600Gb/s \
#   --torusBW 400Gb/s


# python3 launchAll.py --topo torus   --job_size 4x4 ${EXTRA_SMALL} --bench SwingB 
# python3 launchAll.py --topo torus   --job_size 16x16 ${EXTRA_SMALL} --bench SwingB
# python3 launchAll.py --topo torus   --job_size 32x32 ${EXTRA_SMALL} --bench SwingB
# python3 launchAll.py --topo torus   --job_size 64x64 ${EXTRA_SMALL} --bench SwingB

# python3 launchAll.py --topo torus   --job_size 8x8 ${EXTRA_SMALL} --bench Bucket 
# python3 launchAll.py --topo torus   --job_size 16x16 ${EXTRA_SMALL} --bench Bucket
# python3 launchAll.py --topo torus   --job_size 32x32 ${EXTRA_SMALL} --bench Bucket
# python3 launchAll.py --topo torus   --job_size 64x64 ${EXTRA_SMALL} --bench Bucket

# python3 launchAll.py --topo torus   --job_size 9x9 ${EXTRA_SMALL} --bench Torus
# python3 launchAll.py --topo torus   --job_size 17x17 ${EXTRA_SMALL} --bench Torus
# python3 launchAll.py --topo torus   --job_size 33x33 ${EXTRA_SMALL} --bench Torus 
# python3 launchAll.py --topo torus   --job_size 65x65 ${EXTRA_SMALL} --bench Torus 

# python3 launchAll.py --topo torus   --job_size 8x8 ${EXTRA_SMALL} --bench SwingL 
# python3 launchAll.py --topo torus   --job_size 16x16 ${EXTRA_SMALL} --bench SwingL
# python3 launchAll.py --topo torus   --job_size 32x32 ${EXTRA_SMALL} --bench SwingL

# python3 launchAll.py --topo torus   --job_size 8x8 ${EXTRA_SMALL} --bench Trees 
# python3 launchAll.py --topo torus   --job_size 16x16 ${EXTRA_SMALL} --bench Trees 

# python3 launchAll.py --topo torus   --job_size 8x8 ${EXTRA_SMALL} --bench AlltoallL



# wait


# python3 launchAll.py --topo torus   --job_size 9x9 ${EXTRA_SMALL} --bench Torus

# python3 launchAll.py --topo torus   --job_size 5x5 ${EXTRA_SMALL} --bench Torus

# python3 launchAll.py --topo torus   --job_size 4x4 ${EXTRA_SMALL} --bench Bucket

# max_jobs=5
# running=0
# for N in 3 4 5 6 7 8 9 10 11 12 13 14 15; do
#   if (( N % 2 == 0 )); then
#     BENCH="${BENCH_EVEN}"
#   else
#     BENCH="${BENCHS_ODD}"
#   fi
#   ( python3 launchAll.py --topo torus --job_size ${N}x${N} ${EXTRA_SMALL} --bench "${BENCH}" ) &
#   (( ++running >= max_jobs )) && { wait -n; ((running--)); }
# done
# wait

## Fat Trees
# python3 launchAll.py --topo fattree   --job_size 1024 ${EXTRA_LARGE} 
# python3 launchAll.py --topo fattree21 --job_size 1024 ${EXTRA_LARGE} 
# python3 launchAll.py --topo fattree41 --job_size 1024 ${EXTRA_LARGE} 
# python3 launchAll.py --topo fattree81 --job_size 1024 ${EXTRA_LARGE} 

# # Square torus
# python3 launchAll.py --topo torus --job_size 3x3   ${EXTRA_SMALL} --bench ${BENCHS}
# python3 launchAll.py --topo torus --job_size 5x5   ${EXTRA_SMALL} --bench ${BENCHS}
# python3 launchAll.py --topo torus --job_size 7x7   ${EXTRA_SMALL} --bench ${BENCHS} 
# python3 launchAll.py --topo torus --job_size 9x9   ${EXTRA_SMALL} --bench ${BENCHS}
# python3 launchAll.py --topo torus --job_size 11x11 ${EXTRA_SMALL} --bench ${BENCHS}
# python3 launchAll.py --topo torus --job_size 13x13 ${EXTRA_SMALL} --bench ${BENCHS}
# python3 launchAll.py --topo torus --job_size 15x15 ${EXTRA_SMALL} --bench ${BENCHS}



# python3 launchAll.py --topo torus --job_size 16x16   ${EXTRA_SMALL} --bench ${BENCHS} 
# python3 launchAll.py --topo torus --job_size 32x32   ${EXTRA_LARGE} --bench ${BENCHS} 
# python3 launchAll.py --topo torus --job_size 64x64   ${EXTRA_LARGE} --bench ${BENCHS} 
# python3 launchAll.py --topo torus --job_size 128x128 ${EXTRA_LARGE} --bench ${BENCHS} 

# # Rectangular torus
# python3 launchAll.py --topo torus --job_size 64x16 ${EXTRA_LARGE} --bench ${BENCHS} 
# python3 launchAll.py --topo torus --job_size 128x8 ${EXTRA_LARGE} --bench ${BENCHS} 
# python3 launchAll.py --topo torus --job_size 256x4 ${EXTRA_LARGE} --bench ${BENCHS} 

# # Higher dimensional torus
# python3 launchAll.py --topo torus --job_size 8x8x8   ${EXTRA_SMALL} --bench ${BENCHS} 
# python3 launchAll.py --topo torus --job_size 8x8x8x8 ${EXTRA_SMALL} --bench ${BENCHS} 

# # Other topologies
# python3 launchAll.py --topo hx2    --job_size 64x64 ${EXTRA} --bench ${BENCHS}
# python3 launchAll.py --topo hx4    --job_size 64x64 ${EXTRA} --bench ${BENCHS}
# python3 launchAll.py --topo hyperx --job_size 64x64 ${EXTRA} --bench ${BENCHS}

