#!/bin/bash
WORKING_DIR=$PWD
mkdir -p ~/Downloads
cd ~/Downloads
git clone https://github.com/redis/hiredis.git
cd hiredis
make -j7
sudo make install
cd $WORKING_DIR
