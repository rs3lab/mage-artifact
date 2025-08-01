#!/bin/bash
source config.sh
source scripts/config-bench.sh

CPUS=(48)
TRY=1
TRIES=$(seq 1 ${TRY})
PREFETCHERS=(no)
MEMORIES=(28800)
MAX_MEMORY=${MEMORIES[-1]}
BENCHS=(gups) 
#TEST_SYSTEMS=(mage dilos hermit)
TEST_SYSTEMS=(magelibos)
for B in ${BENCHS[@]}; do
    # Different bench in different file
    for P in ${PREFETCHERS[@]}; do
        # Different prefetch also in different file
        OUTPUT_FILE=$OUT_PATH/gups/gups.txt
        touch $OUTPUT_FILE
        for S in ${TEST_SYSTEMS[@]}; do
            DIR=$1
            echo ${DIR}
            for C in ${CPUS[@]}; do
                echo "$S $C $P" | tee -a $OUTPUT_FILE
                for M in ${MEMORIES[@]}; do
                    for T in ${TRIES[@]}; do
                        # Start printing Gups lines only after the first "Thread" line.
                        awk '
                            /^Thread 47/ { seen = 1; next }
                            seen && /^Gups:/ { printf "%d %.10f\n", ++count, $2 }
                        ' "$DIR/gups-nosync-pipelined-4-selective-batch-static_lru-$M-$P-$C-$T.txt" | tee -a "$OUTPUT_FILE"
                    done
                done
                echo "" | tee -a $OUTPUT_FILE
                echo "" | tee -a $OUTPUT_FILE
            done
        done
    done
done
