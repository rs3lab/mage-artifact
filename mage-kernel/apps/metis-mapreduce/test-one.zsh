#!/bin/zsh -e

if [[ -z $MIND_ROOT ]]; then
        echo '$MIND_ROOT not set!' >/dev/stderr
        exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/apps/metis-mapreduce

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

# PERFORMANCE TUNING: 
# Mage-Linux has unoptimized system call path, so let's reduce number of
# memory-allocation related syscalls. 
# 
# See: https://www.gnu.org/software/libc/manual/html_node/Malloc-Tunable-Parameters.html

# When extending heap, add 128 MiB padding for future allocations. 
export MALLOC_TOP_PAD_='134217728' # 128 MiB
# Shrink heap only if topmost 256 MiB is free. 
export MALLOC_TRIM_THRESHOLD_='268435456' # 256 MiB
# force new mmap (instead of brk memory) for allocs greater than this size. 
export MALLOC_MMAP_THRESHOLD_='268435456' # 256 MiB

/usr/bin/time -v ./metis/obj/app/wrmem -s 5000 -p $fh |& tee $output_log

generate-posttest-logs $cn $fh $bs $local_mem_mib
echo "test-one: done."
