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

## 3. Dataset Preparation:
**WARNING: Takes long time**

Run below on **compute node**.

```bash
# Download dataset to /scratch
./scripts/prepare-disk.sh # Generated disk images contain dataset
```

## 4. Evaluation
### Figure 9 (a)

### Figure 9 (b)

### Figure 10 (a)

### FIgure 10 (b)

### Figure 11

### Figure 12
#### (a)
#### (b)

### Figure 13 (a)

### Figure 13 (b)

### Figure 14 (a)

### Figure 14 (b)

### Figure 16

### Figure 17 (a)

### Figure 17 (b)