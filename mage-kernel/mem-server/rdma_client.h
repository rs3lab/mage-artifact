#include <rdma/rdma_cma.h>

#ifndef RDMA_CLIENT_H
#define RDMA_CLIENT_H

extern uint64_t server_addr;
extern uint32_t server_rkey;
extern uintptr_t next_remote_va;

struct rdma_poll_status {
    uint64_t address;   // the address of the struct fault_task
    uint32_t buffer_idx;    // the index of the buffer used for this rdma operation
};

// #include "../kernel-open/nvidia-uvm/uvm_rdma.h"

#ifndef MIND_RDMA_REMOTE_MEM_SIZE
#define MIND_RDMA_REMOTE_MEM_SIZE (16LL * (1 << 30)) // 16 GB
#endif

#ifndef MIND_RDMA_BUFFER_SIZE
#define MIND_RDMA_BUFFER_SIZE (4 << 20) // 4 MB
#endif

#ifndef MIND_DEFAULT_CONTROL_PORT
#define MIND_DEFAULT_CONTROL_PORT 58675
#endif

#define MIND_RDMA_NUM_QP 4

uint64_t client_init(void *buf, int buf_size, char *server_ip, uint32_t server_port);

void client_disconnect(void);

void read_page(int qp_id, uint32_t buffer_pg_idx, uintptr_t addr); // read addr to buffer

// Read page over RDMA but do not wait for cq
// Caller must call check_cq() to get the result
// @return the wr_id
uint64_t read_page_async(int qp_id, uint32_t buffer_pg_idx, uintptr_t addr);

void write_page(int qp_id, uint32_t buffer_pg_idx, uintptr_t addr); // write buffer to addr

// Read page over RDMA but do not wait for cq
// Caller must call check_cq() to get the result
// @return the wr_id
uint64_t write_page_async(int qp_id, uint32_t buffer_pg_idx, uintptr_t addr);

// Try to poll cq
// @return the wr_id if success, -1 otherwise
uint64_t try_check_cq(int qp_id);

#endif
