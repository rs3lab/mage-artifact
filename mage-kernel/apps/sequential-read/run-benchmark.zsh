#!/bin/zsh -e

if [[ -z $MIND_ROOT ]]; then
        echo '$MIND_ROOT not set!' >/dev/stderr
        exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/apps/sequential-read

echo "Execute this on the VM host only!"


# COMPILE THE BENCHMARK APPLICATION.

# Compile Benchmark + Kernel, RUN THE TESTS
echo "Running tests!"
./test-ssh.zsh
