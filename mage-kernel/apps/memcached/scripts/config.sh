#!/bin/bash
set -x
set -u
set -e

export CC=gcc-9
export CXX=g++-9
export SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
export SOURCE_DIR=$SCRIPT_DIR/..
export CMAKE_C_COMPILER=$CC
export CMAKE_CXX_COMPILER=$CXX