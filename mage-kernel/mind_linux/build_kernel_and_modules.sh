#!/bin/bash
./build_kernel.sh build
./build_kernel_modules.sh
./build_kernel.sh install
