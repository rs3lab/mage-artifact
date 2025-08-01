#!/bin/bash

if [[ -z $MIND_ROOT ]]; then
	echo '$MIND_ROOT not set!' >/dev/stderr
	exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/mind_linux/roce_modules

# Linux kernel module for small initrd
export INSTALL_MOD_STRIP=1
chronic make clean
chronic make
