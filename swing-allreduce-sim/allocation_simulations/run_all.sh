#!/bin/bash

for FAULTS in 0 20 40 50 100
do
    for NET in Hx2Small Hx2Large Hx4Small Hx4Large
    do
        if [[ "$NET" == *"Small"* && "$FAULTS" == "50" ]]
        then
            continue
        fi
        if [[ "$NET" == *"Small"* && "$FAULTS" == "100" ]]
        then
            continue
        fi
        if [[ "$NET" == *"Large"* && "$FAULTS" == "20" ]]
        then
            continue
        fi
        if [[ "$NET" == *"Large"* && "$FAULTS" == "40" ]]
        then
            continue
        fi
        for PARAMS in "--no_transpose --no_change_ratio" "--no_change_ratio" "" "--sort" "--dummy" "--min_crossing_ar" "--min_crossing_a2a" "--min_crossing_ar --sort" "--min_crossing_a2a --sort"
        do
            if [[ "$NET" == *"Small"* && "$PARAMS" == *"min_crossing"* ]]
            then
                continue
            fi
            ./simulate_allocation.py --samples 1000 ${PARAMS} --shape ${NET} --faults ${FAULTS} &
            ./simulate_allocation.py --samples 1000 ${PARAMS} --shape ${NET} --faults ${FAULTS} --alibaba 
        done
    done
done

rm logs.tar.gz
tar -czvf logs.tar.gz logs/