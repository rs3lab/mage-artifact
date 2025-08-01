#!/bin/zsh -e

if [[ -z $MIND_ROOT ]]; then
        echo '$MIND_ROOT not set!' >/dev/stderr
        exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/apps/sequential-write

echo "Execute this on the VM host only!"

function set-params () {
	local cnthreads=$1
	local bs=$2

	ssh $cn_control_sshname set-param NUM_CNTHREADS $cnthreads
	ssh $cn_control_sshname set-param CNTHREAD_RECLAIM_BATCH_SIZE $bs

	# build and install the new kernel
	manager cn allonly
	sleep 1s
}


# A single test "run".
function run-test () {
  local cnthreads=$1
  local fhthreads=$2
  local bs=$3

  # reset the cluster (installing the kernel as well)
  reset-fbs
  sleep 5s

  # Not using it for evals. 
  ssh $cn_control_sshname disable-patr

  # run test-one; time to start the application!
  ssh $cn_control_sshname zsh \
  	'/home/sslee/rfbs/apps/sequential-write/test-ssh-wrapper.zsh' \
	$cnthreads $fhthreads $bs
}

local bs=256
for cn in 4; do
	set-params $cn $bs
	for fh in 1 2 4 8 16 32 40 48; do
		if (( fh + cn > 52 )); then
			continue
		fi

		run-test $cn $fh $bs
	done
done
