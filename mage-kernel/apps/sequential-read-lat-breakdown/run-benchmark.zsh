#!/bin/zsh -e

if [[ -z $MIND_ROOT ]]; then
        echo '$MIND_ROOT not set!' >/dev/stderr
        exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/apps/sequential-read-lat-breakdown

echo "Execute this on the VM host only!"

if [[ "$(git rev-parse --abbrev-ref HEAD)" != 'sosp_ae_latency_breakdown' ]]; then
        echo "Please run this on sosp_ae_latency_breakdown branch!" >/dev/stderr
        exit 1
fi

# COMPILE THE BENCHMARK APPLICATION, COMPILE KERNEL, RUN TESTS
echo "Running tests!"
./test-ssh.zsh

# TODO: Parse the data
