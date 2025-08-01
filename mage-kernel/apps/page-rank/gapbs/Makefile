# See LICENSE.txt for license details.
#CXX = clang++-14
PASS_DIR = /home/yupan/mage-compiler/build/pass

CXX_FLAGS += -static -static-libgcc -static-libstdc++ -lrt -pthread -Wl,--whole-archive -lpthread -Wl,--no-whole-archive -std=c++11 -O3 -Wall
# LLVM pass
#CXX_FLAGS += -flegacy-pass-manager -Xclang -load -Xclang $(PASS_DIR)/Mage.so
#CXX_FLAGS += -fno-plt -fpie -I../../../dilos/include -I../../../dilos/arch/x64 -I../../../dilos  -L../../../dilos/build/last
PAR_FLAG = -fopenmp

ifneq (,$(findstring icpc,$(CXX)))
	PAR_FLAG = -openmp
endif

ifneq (,$(findstring sunCC,$(CXX)))
	CXX_FLAGS = -std=c++11 -xO3 -m64 -xtarget=native
	PAR_FLAG = -xopenmp
endif

ifneq ($(SERIAL), 1)
	CXX_FLAGS += $(PAR_FLAG)
endif

KERNELS = bc bfs cc cc_sv pr pr_spmv sssp tc
SUITE = $(KERNELS) converter

.PHONY: all
all: $(SUITE)

% : src/%.cc src/*.h
##  clang for coop tlb
#	$(CXX) $(CXX_FLAGS) $< mage.cc -losv -o $@
##  gcc for general
	$(CXX) $(CXX_FLAGS) $< -o $@

# Testing
include test/test.mk

# Benchmark Automation
include benchmark/bench.mk


.PHONY: clean
clean:
	rm -f $(SUITE) test/out/*
