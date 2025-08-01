#!/bin/bash

if [[ -z $MIND_ROOT ]]; then
        echo '$MIND_ROOT not set!' >/dev/stderr
        exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/frontend

echo "Building MIND control (no output => success)"
chronic -e cmake .
chronic -e make
