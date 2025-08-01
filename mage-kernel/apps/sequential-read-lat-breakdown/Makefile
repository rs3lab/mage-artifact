# Makefile
IDIR = .
CC=gcc
CFLAGS=-I$(IDIR) -static -static-libgcc -static-libstdc++ -lrt -pthread -Wl,--whole-archive -lpthread -Wl,--no-whole-archive 
PROG_NAME := test_mltthrd

obj-y += $(PROG_NAME).o

test_mltthrd: $(obj-y)
	@mkdir -p bin
	@$(CC) -o bin/$@ $^ $(CFLAGS) $(LIBS)

# Test 16 threads across 8 compute blades (or any arbitrary blade by default)
run_test: test_mltthrd
	@bin/$(PROG_NAME) $(NUM_THREAD)

clean:
	@rm -f *.o
	@rm -f bin/*
