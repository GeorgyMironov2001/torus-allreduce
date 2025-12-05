#!/bin/bash
BENCHS="Torus"
EXTRA_LARGE="--env slimfly --nodes 32 --hostfile /home/gera/hosts.cluster --num-threads 32"
EXTRA_SMALL="--env local --nodes 16 --hostfile /home/gera/hosts.cluster --num-threads 1"



# # Square torus
python3 launchAll.py --topo torus --job_size 5x5   ${EXTRA_SMALL} --bench ${BENCHS} 


