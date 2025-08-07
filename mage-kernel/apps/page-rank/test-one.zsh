#!/bin/zsh -e

if [[ -z $MIND_ROOT ]]; then
        echo '$MIND_ROOT not set!' >/dev/stderr
        exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/apps/page-rank

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

local graph_trial=1
export OMP_NUM_THREADS=$fh

#/usr/bin/time -v ./gapbs/gapbs_pr -f ~/twitter.sg -i1000 -t1e-4 -n$graph_trial |& tee $output_log
/usr/bin/time -v ./gapbs/gapbs_pr -f /scratch/kron.sg -i1000 -t1e-4 -n$graph_trial |& tee $output_log

generate-posttest-logs $cn $fh $bs $local_mem_mib

echo "test-one: done."
