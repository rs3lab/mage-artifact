#ifndef __H_RDMA_MONITOR__
#define __H_RDMA_MONITOR__

#define MONITOR_INT 1 /* 1s  */
const char *RX_PACKETS_STR="/sys/class/infiniband/mlx5_0/ports/1/counters/unicast_rcv_packets";
const char *RX_BYTES_STR="/sys/class/infiniband/mlx5_0/ports/1/counters/port_rcv_data";
const char *TX_PACKETS_STR="/sys/class/infiniband/mlx5_0/ports/1/counters/unicast_xmit_packets";
const char *TX_BYTES_STR="/sys/class/infiniband/mlx5_0/ports/1/counters/port_xmit_data";
const char *READ_REQ_STR="/sys/class/infiniband/mlx5_0/ports/1/hw_counters/rx_read_requests";
const char *WRITE_REQ_STR="/sys/class/infiniband/mlx5_0/ports/1/hw_counters/rx_write_requests";

#endif
