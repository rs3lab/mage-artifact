#!/bin/zsh -e

if [[ -z $MIND_ROOT ]]; then
        echo '$MIND_ROOT not set!' >/dev/stderr
        exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/apps/gups

echo "Execute this on the VM host only!"

# COMPILE THE BENCHMARK APPLICATION.
ssh $cn_control_sshname 'cd $MIND_ROOT/apps/gups/gups && make'

# SET UP THE KERNEL W CORRECT PARAMS
function set-params () {
	local lmem_mib=$1

	# rebuild kernel w/ new params
	ssh $cn_control_sshname set-params 'gups-hotset-move' $cn $bs $lmem_mib 0.8
	manager cn allonly
	sleep 1s

	# reset the cluster (updating the kernel as well)
	reset-fbs
	sleep 5s
}

# A single test "run".
function run-test () {
  local cn=$1
  local fh=$2
  local bs=$3
  local lmem_mib=$4

  # run test-one; time to start the application!
  ssh $cn_control_sshname "zsh \$MIND_ROOT/apps/gups/test-one.zsh $cn $fh $bs $lmem_mib"
}

# RUN THE TESTS

cn=4
fh=48
bs=256
lmem_mib=28800
if (( fh + cn > 52 )); then
	echo 'too many threads!' >/dev/stderr
	continue
fi

set-params $lmem_mib
run-test $cn $fh $bs $lmem_mib
fetch-test-logs "cn$cn-fh$fh-bs$bs-lmem_mib$lmem_mib-logs.1"
