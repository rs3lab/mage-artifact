#!/bin/bash
sudo apt-get install automake bison flex g++ git libboost-all-dev libevent-dev libssl-dev libtool make pkg-config
cd ~/Downloads
wget https://dlcdn.apache.org/thrift/0.18.1/thrift-0.18.1.tar.gz
tar xf thrift-0.18.1.tar.gz
cd thrift-0.18.1
./configure --without-python --with-cpp --with-c_glib --without-java --without-erlang --without-nodejs --without-lua --without-ruby --without-perl --without-php --without-php_extension --without-dart --without-go --without-rs --without-haskell --without-swift --without-rust --without-csharp --without-d --without-qt5 --without-qt4 --without-electron --without-qt --without-qt --without-qt --without-qt
make -j 8
sudo make install
