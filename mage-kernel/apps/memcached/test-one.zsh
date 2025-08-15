#!/bin/zsh -e

if [[ -z $MIND_ROOT ]]; then
        echo '$MIND_ROOT not set!' >/dev/stderr
        exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/apps/memcached

if [[ $# -ne 5 ]]; then 
	echo 'args: cn, fh, bs, lmem_mib, sleep_time!'
	exit 1
fi

cn=$1
fh=$2
bs=$3
local_mem_mib=$4
sleep_time=$5

log_root='/tmp/logs'
output_log="$log_root/$fh.log"
mkdir -p $log_root

generate-pretest-logs $cn $fh $bs $local_mem_mib

#/usr/bin/time -v ./memcached -u root -t $fh -m20000 -v |& tee $output_log
/usr/bin/time -v ./memcached -u root -t $fh -m2000 -v |& tee $output_log

generate-posttest-logs $cn $fh $bs $local_mem_mib
echo "test-one: done."
