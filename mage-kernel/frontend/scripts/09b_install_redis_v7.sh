#!/bin/bash
WORKING_DIR=$PWD
cd ~/Downloads
wget https://github.com/redis/redis/archive/7.0.11.tar.gz
tar xvzf 7.0.11.tar.gz 
cd redis-7.0.11
make -j8
ln -s $PWD/src/redis-server $WORKING_DIR/../mind_redis_server
echo ""
echo "**********"
echo "Symlink to the redis server has been created at: "$WORKING_DIR/../mind_redis_server
cd $WORKING_DIR