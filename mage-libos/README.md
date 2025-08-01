# `Mage-LibOS`

## 1. Software Installation
### MLNX OFED
Install it on both **compute node** and **memory node**
NOTE: We have already installed it both compute node and memory node
1. Download `MLNX_OFED_LINUX-5.9-0.5.6.0-ubuntu22.04-x86_64.tgz` to `/root/src/NVidia-Mellanox/MLNX_OFED`
2. `cd /root/src/NVidia-Mellanox/MLNX_OFED && tar -xvf MLNX_OFED_LINUX-5.9-0.5.6.0-ubuntu22.04-x86_64.tgz`
3. `cd /root/src/NVidia-Mellanox/MLNX_OFED/MLNX_OFED_LINUX-5.9-0.5.6.0-ubuntu22.04-x86_64`
4. `./uninstall.sh`
5. `./mlnxofedinstall`
6. `/etc/init.d/openibd restart`
7. `sudo systemctl restart openibd`
8. **NOTE: Only execute this on compute node** `sudo systemctl opensm`
### Other software
On compute node execute:
```bash
sudo ./install-deps.sh
```

## 2. Configuration
1. Modify `config.sh`, if you need. Make sure `config.sh` is the same on both nodes

2. Run below on **compute node**

```bash
sudo ./setup-compute.sh
```

3. Run below on **memory node**

```bash
sudo ./setup-remote.sh
```

4. Make sure your compute node and memory node can `ssh` each other without password using 
your current account

## 3. Dataset Preparation:
**WARNING: Takes long time**

Run below on **compute node**.

```bash
# Download dataset to /scratch
./scripts/prepare-disk.sh # Generated disk images contain dataset
```

## 4. Evaluation
### Figure 9 (a)
On compute node:
```
./scripts/bench-gapbs.sh
```
This script will compile gapbs, run it and generate one column 
starting with `magelibos 48 no` in 
`~/benchmark-out-ae/gapbs/gapbs-pr-kron.txt`

### Figure 9 (b)
On compute node:
```
./scripts/bench-xsbench.sh
```
This script will compile xsbench, run it and generate one column 
starting with `magelibos 48 no` in 
`~/benchmark-out-ae/xsbench/xsbench.txt`

### Figure 10
On compute node:
```
./scripts/bench-seq-scan.sh
```
This script will compile sequential scan, run it and generate two columns 
starting with `magelibos 48 no` and `magelibos 48 readahead` in 
`~/benchmark-out-ae/seq-scan/seq-scan.txt`

`magelibos 48 no` is for Figure 10 (a) and `magelibos 48 readahead` is for 
Figure 10 (b)

### Figure 11
On compute node:
```
./scripts/bench-gups.sh
```
This script will compile gups, run it and generate the time series graph
starting with `magelibos 48 no` in 
`~/benchmark-out-ae/gups/gups.txt`

### Figure 12
On compute node:
```
./scripts/bench-wr-new.sh
```
This script will compile wr, run it and generate one column
starting with `magelibos 48 no` in 
`~/benchmark-out-ae/wrmem/wr-map.txt` and one column starting 
with `magelibos 48 readahead` in 
`~/benchmark-out-ae/wrmem/wr-reduce.txt`
The former is for Figure 12 (a) and the latter is for Figure 12 (b)

### Figure 13 (a)
On compute node:
```
./scripts/bench-memcached-a.sh
```
This script will compile memcached, run it and generate 
one column starting with `magelibos 24 no` in 
`~/benchmark-out-ae/memcached/memcached-a.txt`

### Figure 13 (b)
On compute node:
```
./scripts/bench-memcached-b.sh
```
This script will compile memcached, run it and generate 
one column starting with `magelibos 24 no` in 
`~/benchmark-out-ae/memcached/memcached-b.txt`

### Figure 14
On compute node:
```
./scripts/bench-microbench.sh
```
This script will compile microbench, run it and generate 
one column starting with `magelibos` in 
`~/benchmark-out-ae/microbench/microbench-lat.txt` 
and one column starting with `magelibos` in 
`~/benchmark-out-ae/microbench/microbench-tput.txt` 
The former is for Figure 14 (a) and the latter is for Figure 14 (b)

### Figure 16
On compute node:
```
./scripts/bench-breakdown.sh
```
This script will compile gapbs, run it and generate 
four columns starting with `l2` `lru` `pipelined` and 
`baseline` in 
`~/benchmark-out-ae/gapbs/gapbs-breakdown.txt`

### Figure 17 (a)
On compute node:
```
./scripts/bench-gapbs-batch.sh
```
This script will compile gapbs, run it and generate four columns
starting with `Pipelined 256` `Pipelined 128` `Pipelined 64` and `SYNC Best` in 
`~/benchmark-out-ae/gapbs/gapbs-batch.txt`

### Figure 17 (b)
On compute node:
```
./scripts/bench-gapbs-4cores.sh
```
This script will compile gapbs, run it and generate one column 
starting with `magelibos 4 no` in 
`~/benchmark-out-ae/gapbs/gapbs-4cores.txt`