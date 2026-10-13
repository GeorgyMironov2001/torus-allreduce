#!/usr/bin/env bash
# Best schemes for the visualization canvas: results_viz/<nx>x<ny>_R<R>.{json,log}
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p results_viz
THREADS=${THREADS:-10}

run() {  # nx ny R iters chains
    local tag="$1x$2_R$3"
    ./partition_sa --nx "$1" --ny "$2" --R "$3" --iters "$4" --chains "$5" --threads "$THREADS" \
        --t0factor 0.2 --seed 2026 --out "results_viz/$tag.json" > "results_viz/$tag.log" 2>&1
    echo "$tag: $(grep -E '^best L' "results_viz/$tag.log")"
}

./exact_small 5 3 4 base/exact5x3 > base/exact5x3.log

for R in 2 3 4 5 6; do run 6 6 $R 10000000 10; done
for R in 2 3 4 5; do run 8 4 $R 10000000 10; done
for R in 2 3 4 5 6 7 8; do run 12 12 $R 8000000 10; done
for R in 2 3 4 5 6 7 8; do run 16 16 $R 4000000 10; done
