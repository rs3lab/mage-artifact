# Compute Node (CN) Setup Instructions

**Mage is a research prototype. The Compute Node Kernel is unstable!**
**We recommend setting the Compute Node up inside a virtual machine**. 

## Hardware Requirements

See the [main README](../README.md). 

## Software Requirements

- Base OS: we tested Mage-Linux with Ubuntu Server 18.04 LTS (of which the
  default kernel is 4.15.x). 
- Since this is a research prototype and can become unstable, we recommend
  installing the Compute Node kernel inside a VM. 
- NIC Driver Version (Mellanox OFED): MLNX_OFED_LINUX-5.8-5.1.1.2 (OFED-5.8-5.1.1)
- `chronic` command: (`apt install moreutils`). 

## Installation Instructions

### Repo Setup

Clone this repo into the VM. 

Set the environment variable `MIND_ROOT` to point to the root directory of
Mage-Linux tree (aka: the directory whose README starts with "Welcome to
Mage-Linux!"). 

Many automated scripts will use this environment variable. 
So we recommend something like: 

```
export MIND_ROOT="$HOME/mage/mage-kernel"
```

in your shell init file (`~/.bashrc`, `~/.profile`, etc). 

**Make sure that even incoming SSH processes have `$MIND_ROOT` set!**
You can check by running `ssh servername 'echo $MIND_ROOT'`; make sure it
prints the value you set in your shell config. 

### User Setup

Make sure your user (and root!) can run `sudo` commands without a password. 
Do this by adding your user to group `sudo` 
(via `sudo usermod -aG sudo $USERNAME`), then adding: 
```
%sudo   ALL=(ALL:ALL) NOPASSWD:ALL
```
to /etc/sudoers. 

*This is insecure*, which is another reason to set up the CN inside a Virtual
Machine. 


### Kernel Installation

Install pre-requisite packages: 

```
sudo apt -y update && sudo apt -y upgrade
sudo apt install -y magic-wormhole make htop moreutils expect libncurses-dev \
  bison flex libssl-dev libelf-dev fakeroot liblzma-dev \
  libzstd-dev libpci-dev gawk dkms autoconf \
  rdma-core libibverbs-dev librdmacm-dev ibverbs-utils \
  rdmacm-utils \
  moreutils
```

Then, build the kernel! We have provided some helpful scripts for you. Just
run: 
1. `cd mind_linux`
2. `make olddefconfig`
3. `sudo ./build_kernel_and_modules.sh`

Refer to https://wiki.ubuntu.com/Kernel/BuildYourOwnKernel
(or contact Yash Lala <yash.lala@yale.edu>) if you get stuck. 

Reboot the virtual machine. 

Make sure you've booted into the Mage kernel; `uname -r` should print
"4.15.0-110-generic", and `dmesg` should contain the string "shoop" when run
after boot. We recommend adding this to your `~/.bashrc`, just so you're
warned if you forget to boot into Mage. 

```
# does dmesg go back to first boot?
if dmesg | grep -q "KERNEL supported cpus:"; then
  # are we running rfbs kernel?
  if ! dmesg | grep -q "shoop"; then
    echo "WARNING: might not be running rFBS kernel!"
  fi
fi

if [[ "$(uname -r)" != '4.15.0-110-generic' ]]; then
        echo "WARNING: might not be running rFBS kernel!"
fi
```

Then, install Mellanox OFED. OFED provides network drivers that Mage uses to
get the best performance out of its RDMA NIC. Unfortunately, it is a
third-party library by Mellanox (now acquired by NVIDIA). 
1. First, download MLNX_OFED_LINUX-5.8-5.1.1.2 from NVIDIA's 
   [website](https://network.nvidia.com/products/infiniband-drivers/linux/mlnx_ofed/) or this [third party mirror](https://downloaders.azurewebsites.net/downloaders/mlnx_ofed_downloader/downloader3.html).
   You can find the correct version under the "Archives" tab. 
2. Install OFED with
   `sudo ./mlnxofedinstall --force-dkms --with-neohost-backend`. 

Reboot the VM again. 
You should see "ofed" printed in the kernel's log after boot (check via `dmesg
| grep -i ofed`). 
