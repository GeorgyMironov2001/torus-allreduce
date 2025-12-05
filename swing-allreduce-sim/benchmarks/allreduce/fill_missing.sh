#!/bin/bash
EXTRA_LARGE="--env slimfly --nodes 32 --hostfile /home/desensi/hosts.cluster.100 --num_threads 8"
EXTRA_SMALL="--env slimfly --nodes 8 --hostfile /home/desensi/hosts.cluster.100 --num_threads 8"


# Higher dimensional torus
#python3 launchAll.py --topo torus --job_size 8x8     ${EXTRA_SMALL} --bench RecDoubB --counts 2^29
#python3 launchAll.py --topo torus --job_size 8x8x8   ${EXTRA_SMALL} --bench RecDoubB --counts 2^29
#python3 launchAll.py --topo torus --job_size 8x8x8x8 ${EXTRA_LARGE} --bench RecDoubB --counts 2^29
#python3 launchAll.py --topo torus --job_size 8x8x8x8 ${EXTRA_LARGE} --bench SwingB,Bucket --counts 2^29

# Square torus
python3 launchAll.py --topo torus --job_size 16x16   ${EXTRA_SMALL} --bench RecDoubB,Rings,Bucket --counts 2^27
python3 launchAll.py --topo torus --job_size 32x32   ${EXTRA_LARGE} --bench SwingB,RecDoubB,Rings,Bucket --counts 2^27
#python3 launchAll.py --topo torus --job_size 128x128 ${EXTRA_LARGE} --bench RecDoubB,SwingB,Rings,Bucket --counts 2^27
#python3 launchAll.py --topo torus --job_size 128x128 ${EXTRA_LARGE} --bench SwingL --counts 2^17

# Fat trees
#python3 launchAll.py --topo fattree    --job_size 1024 ${EXTRA_LARGE} --counts 2^17 --bench RecDoubL,SwingL
#python3 launchAll.py --topo fattree21  --job_size 1024 ${EXTRA_LARGE} --counts 2^17 --bench RecDoubL,SwingL
#python3 launchAll.py --topo fattree41  --job_size 1024 ${EXTRA_LARGE} --counts 2^17 --bench RecDoubL,SwingL
#python3 launchAll.py --topo fattree81  --job_size 1024 ${EXTRA_LARGE} --counts 2^17 --bench RecDoubL,SwingL

# Other topologies
#python3 launchAll.py --topo hx2    --job_size 64x64 ${EXTRA_LARGE} --counts 2^27 --bench RecDoubB
#python3 launchAll.py --topo hx4    --job_size 64x64 ${EXTRA_LARGE} --counts 2^27 --bench RecDoubB
#python3 launchAll.py --topo hyperx --job_size 64x64 ${EXTRA_LARGE} --counts 2^27 --bench RecDoubB

