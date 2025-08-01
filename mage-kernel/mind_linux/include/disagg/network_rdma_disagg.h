/*
 * network_rdma_disagg.h
 *
 * Don't be fooled by the name of this file. It has little to do with the RDMA
 * stack. It's actually a header file w/ definitions for the `DISAGG_RDMA`
 * CN<->SCP message type, which is sent over _TCP_. 
 *
 * This file is also included by the controller's source tree.
 * So adding more definitions or includes will fail
 * (unless you do ifndef __CONTROLLER__).
 */

#ifndef __NETWORK_RDMA_DISAGGREGATION_H__
#define __NETWORK_RDMA_DISAGGREGATION_H__

#ifndef __CONTROLLER__
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/types.h>

#endif  // __CONTROLLER__
#include "network_fit_disagg.h"

#ifndef __packed
#define __packed __attribute__((packed))
#endif

// Populates the "u32 node_type" field of struct `rdma_msg_struct`.
#define DISAGG_RDMA_COMPUTE_TYPE    1
// We send "DISAGG_RDMA"
#define DISAGG_RDMA_MEMORY_TYPE     2

#define _IP_LEN_U8  4

#define DISAGG_RDMA_GID_FORMAT "00000000000000000000000000000000"
#define DISAGG_RDMA_INIT_SLEEP_TIME_IN_MS   10000    // 10 sec
#define DISAGG_RDMA_POLLING_SKIP_COUNTER    250000

/* send information of this machine, same data will be sent back from the server */
// This does _not_ refer to an RDMA message! It's a TCP message that's confusingly
// called "DISAGG_RDMA". Should be "DISAGG_RDMA_INFO" or smth, really.
struct rdma_msg_struct {
    int ret;
    // The sender of this message (if compute node) sets this field to its node
    // ID.
    u32	node_id;
    // Are we compute or memory? Uses `DISAGG_RDMA_COMPUTE_TYPE`.
    u32	node_type;
    // The receiver of this message (after sending `DISAGG_RDMA` to announce
    // itself to controller) uses this field to update its own node id.
    u32 node_id_res;    // when the receiver of this msg needs to update its node id
    u64 addr;
    u64 size;
    u32 rkey;
    u32 lkey;
    // CPU blade: DISAGG_QP_PER_COMPUTE, memory blade: DISAGG_MAX_COMPUTE_NODE * DISAGG_QP_PER_COMPUTE
    u32 qpn[DISAGG_MAX_COMPUTE_NODE * DISAGG_QP_PER_COMPUTE];
    // Only used by CPU blade
    u64 ack_buf[DISAGG_QP_PER_COMPUTE];    
    // y: Packet sequence number
    u32 psn;
    // y: Link ID. Not needed for RoCE.
    u32 lid;
    u8 ip_address[_IP_LEN_U8];
    u8 mac[ETH_ALEN];
    // only first half will be used (carrying raw data not string)
    char gid[sizeof(DISAGG_RDMA_GID_FORMAT)];
    u64 base_addr;
    // kernel shared memory. XXX(yash): What is this "kernel shared memory"?
    u64 kshmem_va_start;
} __packed;

struct dest_info;
#endif
