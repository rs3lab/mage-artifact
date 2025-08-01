#!/bin/zsh -e

if [[ -z $MIND_ROOT ]]; then
        echo '$MIND_ROOT not set!' >/dev/stderr
        exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/apps/xsbench

if [[ $# -ne 4 ]]; then 
	echo 'args: cn, fh, bs, lmem_mib!'
	exit 1
fi

cn=$1
fh=$2
bs=$3
local_mem_mib=$4

log_root='/tmp/logs'
output_log="$log_root/$fh.log"
mkdir -p $log_root

generate-pretest-logs $cn $fh $bs $local_mem_mib

export OMP_NUM_THREADS=$fh
/usr/bin/time -v \
    ./XSBench/openmp-threading/XSBench -t $fh -m history -s XL -l 34 -p 5000000 -G unionized -g 30000 \
    |& tee $output_log

generate-posttest-logs $cn $fh $bs $local_mem_mib
echo "test-one: done."
