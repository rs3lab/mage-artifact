#!/bin/zsh -e

if [[ -z $MIND_ROOT ]]; then
        echo '$MIND_ROOT not set!' >/dev/stderr
        exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/apps/webfrontend

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
  ssh $cn_control_sshname zsh \
  	'/home/sslee/rfbs/apps/webfrontend/test-one.zsh' \
	$cnthreads $fhthreads $bs $lmem_mib
}


local cn=4
local fh=48
local bs=256

# full 
local lmems_mib=(34000 30600 27200 23800 20400 17000 13600 12000)
for lmem_mib in $lmems_mib; do 
	set-params $cn $bs $lmem_mib
	if (( fh + cn > 52 )); then
		continue
	fi

	run-test $cn $fh $bs $lmem_mib
	fetch-test-logs "cn$cn-fh$fh-bs$bs-lmem_mib$lmem_mib-logs"
done
