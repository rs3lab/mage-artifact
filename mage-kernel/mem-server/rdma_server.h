#ifndef RDMA_CLIENT_H
#define RDMA_CLIENT_H

#include "rdma_common.h"

void server_init(char *server_ip, uint32_t server_start_port);

void wait_for_server_done(void);

void server_shutdown(void);

#endif
