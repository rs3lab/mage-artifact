#!/bin/zsh -e

if [[ -z $MIND_ROOT ]]; then
        echo '$MIND_ROOT not set!' >/dev/stderr
        exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/apps/sequential-read-lat-breakdown

# TODO uncomment before submission to AE
#bench_branch='sosp_ae_latency_breakdown'
bench_branch='sosp_ae_lat_breakdown_yale'

echo "Execute this on the VM host only!"

if [[ "$(git rev-parse --abbrev-ref HEAD)" != $bench_branch ]]; then
        echo "Error: VM Host is not on $bench_branch branch!" >/dev/stderr
        exit 1
fi

echo "Before running this, please make sure the Compute Node is on $bench_branch branch!" 
# TODO(yash): Don't hardcode this anymore! Should be run in $MIND_ROOT!
#vm_branch="$(ssh $cn_control_sshname zsh -c 'cd /home/sslee/rfbs && git rev-parse --abbrev-ref HEAD' 2>/dev/null)"
#if [[ $vm_branch != $bench_branch ]]; then
#        echo "Error: Compute Node is not on $bench_branch branch!" >/dev/stderr
#        exit 1
#fi

# COMPILE THE BENCHMARK APPLICATION, COMPILE KERNEL, RUN TESTS
echo "Running tests!"
./test-ssh.zsh
