#!/bin/bash
function install_timeout() {
    rm -f build/bench-sleep
    ln -s /bin/sleep build/bench-sleep
    build/bench-sleep $1 && echo "TIMEOUT!!" && (
        sudo pkill -f $2
    ) &
}

function stop_timeout() {
    sudo pkill -f "bench-sleep"
}

TRIES=1
TRIES=$(seq 1 ${TRIES})

export AUTO_POWEROFF=y

FULL_CLEAN=y

FULL_MB=60000

function recover() {
    sed -i "s/constexpr size_t max_tokens = 16;/constexpr size_t max_tokens = 64;/g" ${ROOT_PATH}/dilos/core-ddc/tlb.hh
    sed -i "s/constexpr size_t current_max_evict = [0-9]\+;/constexpr size_t current_max_evict = 256;/g" ${ROOT_PATH}/dilos/include/ddc/remote.hh
    sed -i "s/constexpr size_t max_queues = [0-9]\+;/constexpr size_t max_queues = 4;/g" ${ROOT_PATH}/dilos/include/ddc/page.hh
    sed -i "s/static constexpr size_t max = [0-9]\+;/static constexpr size_t max = 512;/g" ${ROOT_PATH}/dilos/core/mempool.cc
}