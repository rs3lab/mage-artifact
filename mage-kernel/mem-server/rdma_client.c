#include "rdma_client.h"

#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>
#include <arpa/inet.h>


// todo remove?
uint64_t server_addr;
uint32_t server_rkey;
uintptr_t next_remote_va;


#define ATOMIC_UINT_BITSIZE (sizeof(atomic_uint) * 8)
#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>


///      OLD

// ---------------------------------------------
// Data Definitions
// ---------------------------------------------

#define PAGE_SIZE (1 << 12) // 4 KB
#define MIND_DEFAULT_CONTROL_PORT 58675
#define TIMEOUT_IN_MS 5000
#define MIND_QUEUE_SIZE 4096	// number of in-flight msgs
#define MIND_RDMA_NUM_QP 4

struct mind_mr_info
{
	uint64_t remote_addr; // y: not used when CN->MN.
	uint32_t rkey;        // y: not used when CN->MN; rdmacm does that separately
	uint64_t mem_size;    // y: During CN->MN, CN sets this field to request a MR size.
};

struct mind_conn_state {
	// The index in the `conn_state` array
	int id;
	bool valid;
	// used to receive RDMA-CM events for the connection.
	struct rdma_event_channel *ec;
	// A temporary var, used to store events from ec.
	struct rdma_cm_event *event;
	// Bound by the rdma_event_channel
	struct rdma_cm_id *listener_cm_id;
	// Source address RDMA-CM binds to.
	struct sockaddr_in addr;
	// The RDMA-CM handle!
	struct rdma_cm_id *conn;
	// QP attributes; used during QP init only.
	// note: qp are accessed via `conn->qp`.
	struct ibv_cq *cq;
};

// ---------------------------------------------
// Client state
// ---------------------------------------------

const char *server_ip;
// incremented as we go through the program
// (each connection listens on a new port).
uint32_t rdma_cm_listen_port;

static size_t serv_buffer_size;
static int client_connected = 0;

struct ibv_context *dev;
struct ibv_pd *pd;
struct ibv_mr *mr;
char *buffer; // todo I don't need anymore? Or do I?
uint64_t buffer_size;
struct mind_conn_state conn_states[MIND_RDMA_NUM_QP];

static void mind_create_cq_and_qp(struct mind_conn_state *state)
{
	printf("Creating CQ...\n");
	state->cq = ibv_create_cq(dev, MIND_QUEUE_SIZE + 1, NULL, NULL, 0);

	printf("Creating QP...\n");
	struct ibv_qp_init_attr qp_attr = { 0 };
	qp_attr.qp_type = IBV_QPT_RC;
	qp_attr.send_cq = state->cq;
	qp_attr.recv_cq = state->cq;
	qp_attr.cap.max_send_wr = MIND_QUEUE_SIZE; // TODO: can I put _more_ than
											   // queue size? Let retransmit
											   // take care of it?
	qp_attr.cap.max_recv_wr = MIND_QUEUE_SIZE;
	qp_attr.cap.max_send_sge = 1;
	qp_attr.cap.max_recv_sge = 1;
	if (rdma_create_qp(state->conn, pd, &qp_attr)) {
		perror("rdma_create_qp");
		exit(1);
	}
}

enum rdma_cm_event_type mind_check_cm_event(struct mind_conn_state *state)
{
	printf("Checking CM event...\n");
	if (rdma_get_cm_event(state->ec, &state->event)) {
		perror("Failed to retrieve a cm event\n");
		exit(1);
	}
	fprintf(stdout, "Rceived event: %s\n", rdma_event_str(state->event->event));
	rdma_ack_cm_event(state->event);
	puts("ack!");
	return state->event->event;
}

static struct ibv_pd *mind_alloc_pd(struct ibv_context *dev)
{
	// Allocate Protection Domain
	printf("Allocating PD...\n");
	pd = ibv_alloc_pd(dev);
	if (!pd) {
		perror("ibv_alloc_pd");
		exit(1);
		return NULL;
	}
	return pd;
}

static struct ibv_mr *mind_alloc_and_register_mr(void)
{
	struct ibv_mr *ret;

	if (!buffer) {
		perror("Need to allocate buffer before connecting rdma\n");
		exit(1);
	}

	printf("Registering mem region...\n");
	ret = ibv_reg_mr(pd, buffer, buffer_size,
			IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE |
			IBV_ACCESS_REMOTE_READ);
	if (!ret) {
		perror("ibv_reg_mr");
		exit(1);
	}
	return ret;
}

static void mind_resolve_route(struct mind_conn_state *state)
{
	memset(&state->addr, 0, sizeof(state->addr));
	state->addr.sin_family = AF_INET;
	state->addr.sin_port = htons(rdma_cm_listen_port);
	inet_pton(AF_INET, server_ip, &state->addr.sin_addr);

	printf("Creating event channel...\n");
	state->ec = rdma_create_event_channel();
	if (!state->ec) {
		perror("rdma_create_event_channel");
		exit(1);
	}

	printf("Creating RDMA ID...\n");
	if (rdma_create_id(state->ec, &state->conn, NULL, RDMA_PS_TCP)) {
		perror("rdma_create_id");
		exit(1);
	}

	printf("Resolving address...\n");
	if (rdma_resolve_addr(state->conn, NULL, (struct sockaddr *)&state->addr,
			      TIMEOUT_IN_MS)) {
		perror("rdma_resolve_addr");
		exit(1);
	}
	mind_check_cm_event(state);

	printf("Resolving route...\n");
	if (rdma_resolve_route(state->conn, TIMEOUT_IN_MS)) {
		perror("rdma_resolve_route");
		exit(1);
	}
	mind_check_cm_event(state);

	rdma_cm_listen_port++;
}

void mind_setup_connection(struct mind_conn_state *state)
{
	printf("Client: key %u\n", mr->rkey);
	printf("Client: addr %lx\n", (uintptr_t)buffer);

	printf("Connecting...\n");
	struct rdma_conn_param cm_params = { 0 };
	struct mind_mr_info mr_info = { (uintptr_t)buffer, mr->rkey, MIND_RDMA_REMOTE_MEM_SIZE };
	cm_params.private_data = &mr_info;
	cm_params.private_data_len = sizeof(mr_info);
	cm_params.responder_resources = 16;
	cm_params.initiator_depth = 16;
	if (rdma_connect(state->conn, &cm_params)) {
		perror("rdma_connect: --");
		exit(1);
	}

	printf("Getting CM event...\n");
	if (rdma_get_cm_event(state->ec, &state->event)) {
		perror("rdma_get_cm_event");
		exit(1);
	}

	if (state->event->event == RDMA_CM_EVENT_ESTABLISHED) {
		struct mind_mr_info *server_mr =
			(struct mind_mr_info *)state->event->param.conn.private_data;
		if (server_mr == NULL) {
			perror("Private data is NULL\n");
			exit(1);
		}
		// Extract server keys
		memcpy(&server_addr, &server_mr->remote_addr,
		       sizeof(server_addr));
		memcpy(&server_rkey, &server_mr->rkey, sizeof(server_rkey));

		printf("server_addr: %lx\n", server_addr);
		next_remote_va = server_addr;
		printf("server_rkey: %u\n", server_rkey);
	} else {
		fprintf(stderr, "Unexpected event: %s\n",
			rdma_event_str(state->event->event));
		exit(1);
	}
	printf("connected\n");
}


uint64_t client_init(void *buf, int buf_size, char *__server_ip, uint32_t server_port)
{
	server_ip = __server_ip; // TODO danger after stack dealloc!
	rdma_cm_listen_port = server_port;
	buffer = buf;
	buffer_size = buf_size;

	// Initialize structs that manage connections.
	for (int i = 0; i < MIND_RDMA_NUM_QP; i++) {
		 conn_states[i].id = i;
		 conn_states[i].valid = false;
	}

	// PHASE 1: HANDLE THE FIRST CONNECTION
	//
	// In the first server connection, we need to specify the MR size we
	// want.

	mind_resolve_route(&conn_states[0]);

	dev = conn_states[0].conn->verbs;
	pd = mind_alloc_pd(dev);
	mr = mind_alloc_and_register_mr();

	mind_create_cq_and_qp(&conn_states[0]);
	mind_setup_connection(&conn_states[0]);

	printf("Successfully set up connection 0 on port %d\n", server_port);


	for (int i = 1; i < MIND_RDMA_NUM_QP; i++)
	{
		printf("Setting up connection %u on port %d\n", i, rdma_cm_listen_port);
		mind_resolve_route(&conn_states[i]);
		mind_create_cq_and_qp(&conn_states[i]);
		mind_setup_connection(&conn_states[i]);
		printf("Successfully set up connection %u on port %d\n", i, rdma_cm_listen_port);
	}

	return (uint64_t) server_addr;
}


void client_disconnect(void)
{
	printf("TODO TODO TODO\n");
	// TODO
	// rdma_disconnect(state->conn);
	// rdma_deinit();
}

/*
 * Read a page from the remove memory but not call cq polling
 * addr: remote memory address
 * Note) always a single page, read into `buffer` (static)
 */
uint64_t read_page_async(int qp_id, uint32_t buffer_pg_idx, uintptr_t addr)
{
	struct ibv_send_wr send_wr, *bad_send_wr = NULL;
	struct ibv_sge send_sge;

	// Initialize the send work request
	memset(&send_wr, 0, sizeof(send_wr));
	send_wr.wr_id = addr;
	send_wr.opcode = IBV_WR_RDMA_READ;
	send_wr.send_flags = IBV_SEND_SIGNALED;
	send_wr.wr.rdma.remote_addr = addr;
	send_wr.wr.rdma.rkey = server_rkey;
	send_sge.addr = (uint64_t) &buffer[buffer_pg_idx * PAGE_SIZE];
	send_sge.length = PAGE_SIZE;
	send_sge.lkey = mr->lkey;
	send_wr.sg_list = &send_sge;
	send_wr.num_sge = 1;

	// Post the RDMA read request
	if (ibv_post_send(conn_states[qp_id].conn->qp, &send_wr, &bad_send_wr)) {
		perror("ibv_post_send");
		exit(1);
	}
	return send_wr.wr_id;
}

/*
 * Read a page from the remove memory
 * addr: remote memory address
 * Note) always a single page, read into `buffer` (static)
 */
void read_page(int qp_id, uint32_t buffer_pg_idx, uintptr_t addr)
{
	struct ibv_wc wc;
	uint64_t wr_id = read_page_async(qp_id, buffer_pg_idx, addr);
	// Wait for send completion
	while (ibv_poll_cq(conn_states[qp_id].cq, 1, &wc) < 1) {
	}
	if (wc.status != IBV_WC_SUCCESS) {
		fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
			ibv_wc_status_str(wc.status), wc.status, (int)wc.wr_id);
		exit(1);
	}
	// read
	assert(wc.wr_id == wr_id);
}

uint64_t write_page_async(int qp_id, uint32_t buffer_pg_idx, uintptr_t addr)
{
	struct ibv_send_wr send_wr, *bad_send_wr = NULL;
	struct ibv_sge send_sge;

	// printf("writing to %lx\n", addr);

	// Initialize the send work request
	memset(&send_wr, 0, sizeof(send_wr));
	send_wr.wr_id = addr;
	send_wr.opcode = IBV_WR_RDMA_WRITE;
	send_wr.send_flags = IBV_SEND_SIGNALED;
	send_wr.wr.rdma.remote_addr = addr;
	send_wr.wr.rdma.rkey = server_rkey;
	send_sge.addr = (uint64_t) &buffer[buffer_pg_idx * PAGE_SIZE];
	send_sge.length = PAGE_SIZE;
	send_sge.lkey = mr->lkey;
	send_wr.sg_list = &send_sge;
	send_wr.num_sge = 1;

	// Post the RDMA write request
	if (ibv_post_send(conn_states[qp_id].conn->qp, &send_wr, &bad_send_wr)) {
		perror("ibv_post_send");
		exit(1);
	}
	return send_wr.wr_id;
}

/*
 * Write a page to the remove memory
 * addr: remote memory address
 * Note) always a single page, write from `buffer` (static)
 */
void write_page(int qp_id, uint32_t buffer_pg_idx, uintptr_t addr)
{
	struct ibv_wc wc;
	uint64_t wr_id = write_page_async(qp_id, buffer_pg_idx, addr);
	// Wait for send completion
	while (ibv_poll_cq(conn_states[qp_id].cq, 1, &wc) < 1) {
	}
	if (wc.status != IBV_WC_SUCCESS) {
		fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
			ibv_wc_status_str(wc.status), wc.status, (int)wc.wr_id);
		exit(1);
	}
	//puts("wrote");

	// wrote
	assert(wc.wr_id == wr_id);
}

// Check the completion queue for a single event
// @return the wr_id of the completed event, or -1 if no event
uint64_t try_check_cq(int qp_id)
{
	struct ibv_wc wc;
	if (ibv_poll_cq(conn_states[qp_id].cq, 1, &wc) < 1)
		return (u_int64_t)-1;	//0xffff..

	if (wc.status != IBV_WC_SUCCESS) {
		fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
			ibv_wc_status_str(wc.status), wc.status, (int)wc.wr_id);
		return (u_int64_t)-1;	//0xffff..
	}
	return wc.wr_id;
}
