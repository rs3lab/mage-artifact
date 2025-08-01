CC=gcc
CFLAGS=-lrdmacm -libverbs -lpthread -O2 -g
COMMON_FILES=rdma_common.c
FILES=clean_rdma_client clean_rdma_server
C_RDMA_LIB_NAME = mind_rdma
LIB=lib$(C_RDMA_LIB_NAME).a

all: clean_rdma_client clean_rdma_server $(LIB)

# sys_client: sys_client.c uvm_rdma_client.c rdma_client.c $(COMMON_FILES)
# 	$(CC) $^ $(CFLAGS) -o $@.o

# sys_server: sys_server.c rdma_server.c $(COMMON_FILES)
# 	$(CC) $^ $(CFLAGS) -o $@.o

# clean_fault_queue: clean_fault_queue_test.c uvm_rdma_client.c rdma_client.c $(COMMON_FILES)
# 	$(CC) $^ $(CFLAGS) -o $@.o

hashmap: hashmap.c
	$(CC) $^ $(CFLAGS) -o $@

hashmap_test: hashmap_test.c hashmap.c
	$(CC) $^ $(CFLAGS) -o $@ -g

rdma_buffer_test: rdma_buffer_test.c rdma_client.c $(COMMON_FILES)
	$(CC) $^ $(CFLAGS) -o $@ -g

clean_rdma_client: clean_rdma_client.c rdma_client.c $(COMMON_FILES)
	$(CC) $^ $(CFLAGS) -o $@

clean_rdma_server: clean_rdma_server.c rdma_server.c
	$(CC) $^ $(CFLAGS) -o $@

run_client: clean_rdma_client
	./clean_rdma_client "10.10.10.202"

# Note) server needs to be run as root to register memory as RDMA-able
run_server: clean_rdma_server
	sudo ./clean_rdma_server "10.10.10.206"

# Library for rust code
$(LIB): rdma_client.o rdma_common.o rdma_server.o hashmap.o
	$(CC) -shared -o $@ $^ -lrdmacm -libverbs -g

# for shared library 
CFLAGS += -fPIC
rdma_common.o: rdma_common.c
	$(CC) -c rdma_common.c $(CFLAGS) -o rdma_common.o

rdma_client.o: rdma_client.c rdma_client.h
	$(CC) -c rdma_client.c $(CFLAGS) -o rdma_client.o -g

rdma_server.o: rdma_server.c rdma_server.h
	$(CC) -c rdma_server.c $(CFLAGS) -o rdma_server.o

clean:
	rm -f $(FILES) *.o $(LIB) hashmap_test rdma_buffer_test

.PHONY: all clean
