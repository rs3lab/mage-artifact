OFA_KDIR := /usr/src/ofa_kernel/default

EXTRA_CFLAGS	+= -D__CN_ROCE__ #-D__CN_ROCE_TEST__
ccflags-y := -I$(OFA_KDIR)/include

obj-m += roce4disagg.o

roce4disagg-objs := ./roce_for_disagg/roce_disagg.o \
	./roce_for_disagg/roce_rdma.o

# KBUILD_EXTRA_SYMBOLS := /usr/src/linux-headers-$(shell uname -r)/Module.symvers
KBUILD_EXTRA_SYMBOLS := $(shell pwd)/Module.symvers \
		/home/sslee/mind_internal/mind_linux/roce_modules \
		/usr/src/ofa_kernel/default/Module.symvers

all:
	make -C ../. M=$(PWD) modules

clean:
	make -C ../. M=$(PWD) clean
