#!/bin/bash
source config.sh
source scripts/config-bench.sh

CPUS=(48)
TRY=1
TRIES=$(seq 1 ${TRY})
PREFETCHERS=(no)
MEMORIES=(2200 4400 6600 8800 11000 13200 15400 17600 19800 22000) # Kron
MAX_MEMORY=${MEMORIES[-1]}
BENCHS=(pr) 
TEST_SYSTEMS=(mage)
for B in ${BENCHS[@]}; do
    # Different bench in different file
    for P in ${PREFETCHERS[@]}; do
        # Different prefetch also in different file
        OUTPUT_FILE=$OUT_PATH/gapbs/gapbs.txt
        touch $OUTPUT_FILE
        for S in ${TEST_SYSTEMS[@]}; do
            DIR=$1
            echo ${DIR}
            for C in ${CPUS[@]}; do
                echo "$S $C $P" | tee -a $OUTPUT_FILE
                for M in ${MEMORIES[@]}; do
                    RESULT=0
                    for T in ${TRIES[@]}; do
                        #TMP=`grep Average $DIR/gapbs-$B-$M-$P-$C-$T.txt | awk '{print $3}'
                        TMP=`grep Average $DIR/gapbs-kron-nosync-pipelined-4-selective-64-batch-static_fifo-$B-$M-$P-$C-$T.txt | awk '{print $3}'`
                        echo $DIR 
                        echo $TMP
                        if [[ -z $TMP ]]; then
                            TMP=0
                        fi
                        RESULT=$(python3 -c "print($RESULT + $TMP)")
                    done
                    if [[ "$RESULT" != "0" ]]; then
                        RESULT=$(python3 -c "print(round(3600*$TRY/$RESULT , 2))")
                    fi
                    PERCENT=$(python3 -c "print(int(100 * $M/$MAX_MEMORY))")
                    echo "$PERCENT $RESULT" | tee -a $OUTPUT_FILE
                done
                echo "" | tee -a $OUTPUT_FILE
                echo "" | tee -a $OUTPUT_FILE
            done
        done
    done
done
