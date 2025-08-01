#!/bin/bash
source config.sh
source scripts/config-bench.sh

CPUS=(48)
TRY=1
TRIES=$(seq 1 ${TRY})
PREFETCHERS=(no)
MAX_MEMORY=30000
MEMORIES=(3000 6000 9000 12000 15000 18000 21000 24000 27000 30000)
#TEST_SYSTEMS=(mage hermit)
TEST_SYSTEMS=(magelibos)
#TLB_FLUSH_MODES=(patr batch)
#LRU_MODES=(static_fifo static_lru)
TLB_FLUSH_MODES=(batch)
LRU_MODES=(static_fifo)
# Different bench in different file
OUTPUT_FILE_MAP=$OUT_PATH/wrmem/wr-map.txt
touch $OUTPUT_FILE_MAP
OUTPUT_FILE_REDUCE=$OUT_PATH/wrmem/wr-reduce.txt
touch $OUTPUT_FILE_REDUCE
for P in ${PREFETCHERS[@]}; do
    # Different prefetch also in different file
    for S in ${TEST_SYSTEMS[@]}; do
        DIR=$1
        echo ${DIR}
        for C in ${CPUS[@]}; do
            for TL in ${TLB_FLUSH_MODES[@]}; do
                for L in ${LRU_MODES[@]}; do
                    echo "$S $C $P" | tee -a $OUTPUT_FILE_MAP
                    echo "$S $C $P" | tee -a $OUTPUT_FILE_REDUCE
                    for M in ${MEMORIES[@]}; do
                        RESULT_MAP=0
                        RESULT_REDUCE=0
                        for T in ${TRIES[@]}; do
                            TMP_MAP=`grep Real $DIR/wrmem-nosync-pipelined-4-selective-$TL-$L-$M-$P-$C-$T.txt | awk '{print $6}'` #Map
                            TMP_REDUCE=`grep Real $DIR/wrmem-nosync-pipelined-4-selective-$TL-$L-$M-$P-$C-$T.txt | awk '{print $10}'` #Reduce
                            echo $DIR
                            echo $TMP_MAP
                            echo $TMP_REDUCE
                            if [[ -z $TMP_MAP ]]; then
                                TMP_MAP=0
                            fi
                            if [[ -z $TMP_REDUCE ]]; then
                                TMP_REDUCE=0
                            fi
                            RESULT_MAP=$(python3 -c "print($RESULT_MAP + $TMP_MAP)")
                            RESULT_REDUCE=$(python3 -c "print($RESULT_REDUCE + $TMP_REDUCE)")
                        done
                        if [[ "$RESULT_MAP" != "0" ]]; then
                            RESULT_MAP=$(python3 -c "print(round(3600000*$TRY/$RESULT_MAP , 2))")
                        fi
                        if [[ "$RESULT_REDUCE" != "0" ]]; then
                            RESULT_REDUCE=$(python3 -c "print(round(3600000*$TRY/$RESULT_REDUCE , 2))")
                        fi
                        PERCENT=$(python3 -c "print(int(100 * $M/$MAX_MEMORY))")
                        echo "$PERCENT $RESULT_MAP" | tee -a $OUTPUT_FILE_MAP
                        echo "$PERCENT $RESULT_REDUCE" | tee -a $OUTPUT_FILE_REDUCE
                    done
                    echo "" | tee -a $OUTPUT_FILE_MAP
                    echo "" | tee -a $OUTPUT_FILE_MAP
                    echo "" | tee -a $OUTPUT_FILE_REDUCE
                    echo "" | tee -a $OUTPUT_FILE_REDUCE
                done
            done
        done
    done
done