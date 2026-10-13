#!/usr/bin/env bash
# Second pass for large tori, chains seeded from block digit schemes: results_viz/<tag>_b.{json,log}
set -euo pipefail
cd "$(dirname "$0")"
THREADS=${THREADS:-10}

run() {  # nx ny R iters chains
    local tag="$1x$2_R$3_b"
    ./partition_sa --nx "$1" --ny "$2" --R "$3" --iters "$4" --chains "$5" --threads "$THREADS" \
        --t0factor 0.2 --seed 2027 --out "results_viz/$tag.json" > "results_viz/$tag.log" 2>&1
    echo "$tag: $(grep -E 'schemes evaluated' "results_viz/$tag.log") | $(grep -E '^best L' "results_viz/$tag.log")"
}

for R in 6 7 8; do run 12 12 $R 8000000 10; done
for R in 4 5 6 7 8; do run 16 16 $R 4000000 10; done
