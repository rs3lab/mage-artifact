#!/bin/zsh -e

# Runs in the VM. Assumes the kernel is correctly installed, and all
# kernel-side parameters are left.

if [[ -z $MIND_ROOT ]]; then
        echo '$MIND_ROOT not set!' >/dev/stderr
        exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/apps/sequential-read-latency-breakdown

if [[ $# -ne 4 ]]; then 
	echo 'args: cn, fh, bs, lmem_mib!'
	exit 1
fi

cn=$1
fh=$2
bs=$3
local_mem_mib=$4
array_size='4294967296' # 4 GiB

log_root='/tmp/logs'
output_log="$log_root/$fh.log"
mkdir -p $log_root


# PROGRAM SETUP

echo "making program. no output = success."
chronic make clean
chronic make

# PROGRAM START

echo "test-one: spawning test_mltthrd with $fh and $array_size bytes."
nohup unbuffer \
        ./bin/test_mltthrd $n_threads $array_size &> $output_log &
bench_pid=$!

# PROGRAM WARMUP

function sleep-until-prints () {
        local flag=$1
        while true; do
                if grep -q $flag $output_log; then
                        break;
                fi
                sleep 0.5
        done
}

echo 'test-one: waiting for array allocation.'
sleep-until-prints 'ALLOC_DONE'
echo 'test-one: waiting for benchmark to begin'
sleep-until-prints 'BEGIN_BENCHMARK'

# PROGRAM RUN!

generate-pretest-logs $cn $fh $bs $local_mem_mib

sleep-until-prints 'END_BENCHMARK'

generate-posttest-logs $cn $fh $bs $local_mem_mib

echo "test-one: done."
