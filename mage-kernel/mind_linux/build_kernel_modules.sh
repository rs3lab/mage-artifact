#!/bin/bash
export INSTALL_MOD_STRIP=1
make modules -j $(nproc --all) && sudo -E make -j $(nproc --all) modules_install
