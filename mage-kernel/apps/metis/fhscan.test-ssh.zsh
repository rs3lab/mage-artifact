#!/bin/zsh -e

if [[ -z $MIND_ROOT ]]; then
        echo '$MIND_ROOT not set!' >/dev/stderr
        exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/apps/metis

echo "Execute this on the VM host only!"

function set-params () {
	local cn=$1
	local bs=$2
	local lmem_mib=$3

	ssh $cn_control_sshname set-params $cn $bs $lmem_mib

	manager cn allonly
	sleep 1s
}

# A single test "run".
function run-test () {
  local cnthreads=$1
  local fhthreads=$2
  local bs=$3
  local lmem_mib=$4

  # reset the cluster (updating the kernel as well)
  reset-fbs
  sleep 5s

  # run test-one; time to start the application!
  ssh $cn_control_sshname "zsh \$MIND_ROOT/apps/metis/test-one.zsh $cnthreads $fhthreads $bs $lmem_mib"
}


local cn=4
local bs=256

# 30 GiB is full memory. (Here shown as 30000)
# This code looks like it confuses GiB and GB...But let's follow to be consistent w/ Pan. 
local lmems_mib=( 7500 15000 )

for lmem_mib in $lmems_mib; do 
	set-params $cn $bs $lmem_mib
	for fh in 1 2 4 8 16 24 32 40 48; do
		if (( fh + cn > 52 )); then
			continue
		fi

		run-test $cn $fh $bs $lmem_mib
		fetch-test-logs "cn$cn-fh$fh-bs$bs-lmem_mib$lmem_mib-logs"
	done
done
