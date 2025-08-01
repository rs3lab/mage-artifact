#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <linux/mman.h>
#include <unistd.h>
#include <assert.h>

#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>
#include <arpa/inet.h>

// ---------------------------------------------
// Data Definitions
// ---------------------------------------------

#define PAGE_SIZE (1 << 12) // 4 KB
#define MIND_DEFAULT_CONTROL_PORT 58675
#define TIMEOUT_IN_MS 5000
#define MIND_QUEUE_SIZE 512	// number of in-flight msgs
#define MIND_RDMA_MAX_NUM_QP 256

// NOTE: Keep this struct in sync with the definition on the memory server side!
struct mind_mr_info
{
	uint64_t remote_addr; // y: not used when CN->MN.
	uint32_t rkey;        // y: not used when CN->MN; rdmacm does that separately
	uint64_t mem_size;    // y: During first CN->MN, CN sets this field to request a MR size.
	uint32_t num_qps;     // y: During first CN->MN, CN sets this field to set # parallel QPs.
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
	struct rdma_cm_id *conn_cm_id;
	// QP attributes; used during QP init only.
	// note: qp are accessed via `conn->qp`.
	struct ibv_cq *cq;
};

// ---------------------------------------------
// Server state
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
char *buffer;
uint64_t buffer_size;
struct mind_conn_state conn_states[MIND_RDMA_MAX_NUM_QP];
static int num_parallel_qps;

// ---------------------------------------------
// Helper Functions
// ---------------------------------------------

static void mind_set_client_info(struct mind_mr_info const *mr_info)
{
	uint64_t client_addr;
	uint32_t client_rkey;
	if (mr_info == NULL) {
		fprintf(stderr, "Client private data is NULL\n");
		exit(1);
	}

	client_addr = mr_info->remote_addr;
	client_rkey = mr_info->rkey;
	serv_buffer_size = mr_info->mem_size;
	num_parallel_qps = mr_info->num_qps;

	printf("Received information from client:\n");
	printf("  client_addr: 0x%lx\n", client_addr);
	printf("  client_rkey: %u\n", client_rkey);
	printf("  serv_buffer_size: 0x%lx\n", serv_buffer_size);
	printf("  parallel_qps: 0x%d\n", num_parallel_qps);
}

static struct ibv_mr *mind_alloc_and_register_mr(void)
{
	struct ibv_mr *ret;

	printf("Allocating buffer and registering memory...\n");
	buffer = mmap(NULL, serv_buffer_size, PROT_READ | PROT_WRITE,
		      MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_1GB , -1, 0);
	if (buffer == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	printf("server addr: %lx\n", (uintptr_t)buffer);
	memset(buffer, 0, serv_buffer_size);
	ret = ibv_reg_mr(pd, buffer, serv_buffer_size,
			IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE |
				IBV_ACCESS_REMOTE_READ);
	if (!ret) {
		perror("ibv_reg_mr");
		exit(1);
	}
	printf("server key: %u\n", ret->rkey);
	return ret;
}

// Set max number of outstanding reads we can handle from the remote side.
static int get_device_max_recv_reads(struct ibv_context *ibv_ctx)
{
	struct ibv_device_attr attr;
	int ret = 0;

	if (!ibv_ctx) {
		fprintf(stderr, "get_device_max_recv_reads: no device!\n");
		exit(1);
	}

	if ((ret = ibv_query_device(ibv_ctx, &attr))) {
		perror("ibv_query_device");
		exit(1);
	}
	return attr.max_qp_rd_atom;
}

static void mind_accept_conn(struct mind_conn_state *conn)
{
	rdma_ack_cm_event(conn->event);

	// Accept RDMA connection
	printf("Accepting RDMA connection...\n");
	struct rdma_conn_param cm_params = { 0 };
	struct mind_mr_info mr_info = { (uintptr_t)buffer, mr->rkey };
	cm_params.private_data = &mr_info;
	cm_params.private_data_len = sizeof(mr_info);

	// max number of concurrent reqs we accept from remote
	cm_params.responder_resources = get_device_max_recv_reads(mr->context);
	assert(cm_params.responder_resources == 16);
	// max outstanding reqs _we_ will emit.
	cm_params.initiator_depth = 1;

	if (!conn) {
		printf("conn is NULL\n");
		return;
	}
	if (dev) {
		printf("Verbs context: %p\n", dev);
	} else {
		printf("Verbs context: NULL\n");
	}

	if (conn->conn_cm_id->qp) {
		printf("QP Number: %d\n", conn->conn_cm_id->qp->qp_num);
	} else {
		printf("QP: NULL\n");
	}

	// Printing Port Space
	printf("Port Space: %d\n", conn->conn_cm_id->ps);

	// Printing Port Number
	printf("Port Number: %u\n", conn->conn_cm_id->port_num);
	if (rdma_accept(conn->conn_cm_id, &cm_params)) {
		perror("rdma_accept");
		exit(1);
	}

	// Get CM event
	printf("Getting CM event...\n");
	if (rdma_get_cm_event(conn->ec, &conn->event)) {
		perror("rdma_get_cm_event");
		exit(1);
	}

	if (conn->event->event != RDMA_CM_EVENT_ESTABLISHED) {
		fprintf(stderr, "Unexpected event: %s\n",
			rdma_event_str(conn->event->event));
		exit(1);
	}
	rdma_ack_cm_event(conn->event);
	printf("connected\n");
	client_connected = 1;
}

static struct ibv_pd *mind_alloc_pd(struct ibv_context *dev)
{
	struct ibv_pd *ret = ibv_alloc_pd(dev);
	if (!ret) {
		perror("ibv_alloc_pd");
		exit(1);
		return NULL;
	}
	return ret;
}

static void mind_create_cq_and_qp(struct mind_conn_state *state)
{
	printf("Creating CQ...\n");
	state->cq = ibv_create_cq(dev, MIND_QUEUE_SIZE, NULL, NULL, 0);

	printf("Creating QP...\n");
	struct ibv_qp_init_attr qp_attr = { 0 };
	qp_attr.qp_type = IBV_QPT_RC;
	qp_attr.send_cq = state->cq;
	qp_attr.recv_cq = state->cq;
	qp_attr.cap.max_send_wr = MIND_QUEUE_SIZE;
	qp_attr.cap.max_recv_wr = MIND_QUEUE_SIZE;
	qp_attr.cap.max_send_sge = 1;
	qp_attr.cap.max_recv_sge = 1;
	if (rdma_create_qp(state->conn_cm_id, pd, &qp_attr)) {
		perror("rdma_create_qp");
		exit(1);
	}
}

static void mind_init_conn_listener(struct mind_conn_state *state)
{
	state->valid = true;

	memset(&state->addr, 0, sizeof(state->addr));
	state->addr.sin_family = AF_INET;
	state->addr.sin_port = htons(rdma_cm_listen_port);
	inet_pton(AF_INET, server_ip, &state->addr.sin_addr);

	// Create event channel
	printf("Creating event channel...\n");
	state->ec = rdma_create_event_channel();
	if (!state->ec) {
		perror("rdma_create_event_channel");
		exit(1);
	}

	// Create RDMA ID for listening
	printf("Creating RDMA ID...\n");
	if (rdma_create_id(state->ec, &state->listener_cm_id, NULL, RDMA_PS_TCP)) {
		perror("rdma_create_id");
		exit(1);
	}

	// Bind address to RDMA ID
	printf("Binding address...\n");
	if (rdma_bind_addr(state->listener_cm_id, (struct sockaddr *)&state->addr)) {
		perror("rdma_bind_addr");
		exit(1);
	}

	// Start listening for incoming connections
	printf("Listening...\n");
	if (rdma_listen(state->listener_cm_id, 10)) {
		perror("rdma_listen");
		exit(1);
	}

	printf("Server is listening at %s:%u\n", server_ip, rdma_cm_listen_port);

	rdma_cm_listen_port++; // <- NOTE: Side effects here!
}

static void mind_listen_for_conn(struct mind_conn_state *state)
{
	if (rdma_get_cm_event(state->ec, &state->event)) {
		perror("rdma_get_cm_event");
		exit(1);
	}
	if (state->event->event != RDMA_CM_EVENT_CONNECT_REQUEST) {
		fprintf(stderr, "Unexpected event: %s\n",
			rdma_event_str(state->event->event));
		exit(1);
	} 

	state->conn_cm_id = state->event->id;
}

// ---------------------------------------------
// Public API for Memory Server
// ---------------------------------------------

void server_init(char *ip, uint32_t start_port)
{
	// Initialize parameters
	server_ip = ip;
	rdma_cm_listen_port = start_port;

	// Initialize structs that manage connections.
	for (int i = 0; i < MIND_RDMA_MAX_NUM_QP; i++) {
		 conn_states[i].id = i;
		 conn_states[i].valid = false;
	}

	// PHASE 1: HANDLE THE FIRST CLIENT CONNECTION
	//
	// Unfortunately, we can't alloc the server PD and memory buffer right
	// away, because we don't know what infiniband device (or what buf size) to
	// bind to until the first RDMA request comes in.
	// So we handle the first server init specially, and set global state as
	// we go.

	printf("Setting up connection 0 on port %d...\n", start_port);
	mind_init_conn_listener(&conn_states[0]);

	// Accept incoming connection and process client requests
	printf("Waiting for first connection on port %d...\n", start_port);
	mind_listen_for_conn(&conn_states[0]);

	// Read the first CN's message to set our own client information
	// (rkey, requested buffer size, etc).
	mind_set_client_info(conn_states[0].event->param.conn.private_data);

	// Get the Infiniband device handle (opened device), pd, and server buf mr
	dev = conn_states[0].conn_cm_id->verbs;
	pd = mind_alloc_pd(dev);
	mr = mind_alloc_and_register_mr();

	mind_create_cq_and_qp(&conn_states[0]);
	mind_accept_conn(&conn_states[0]);
	printf("Successfully set up connection 0 on port %d\n", start_port);

	// PHASE 2: HANDLE SUBSEQUENT CLIENT CONNECTIONS.

	for (int i = 1; i < num_parallel_qps; i++)
	{
		printf("Setting up connection %u on port %d\n", i, rdma_cm_listen_port);
		mind_init_conn_listener(&conn_states[i]);
		mind_listen_for_conn(&conn_states[i]);
		mind_create_cq_and_qp(&conn_states[i]);
		mind_accept_conn(&conn_states[i]);
		printf("Successfully set up connection %u on port %d\n", i, rdma_cm_listen_port);
	}
}

void wait_for_server_done()
{
	// TODO
	pause();
}

void server_shutdown(void)
{
	if (!client_connected) {
		return;
	}
	client_connected = 0;

	printf("server Disconnecting and cleaning up\n");

	for (int i = 0; i < num_parallel_qps; i++) {
		struct mind_conn_state *conn = &conn_states[i];
		if (!conn->valid)
			 continue;

		rdma_destroy_qp(conn->conn_cm_id);
		rdma_destroy_id(conn->conn_cm_id);
		ibv_destroy_cq(conn->cq);
	}

	ibv_dereg_mr(mr);
	printf("Destroyed mr\n");
	ibv_dealloc_pd(pd);
	printf("Destroyed pd\n");

	for (int i = 0; i < num_parallel_qps; i++) {
		struct mind_conn_state *conn = &conn_states[i];
		if (!conn->valid)
			 continue;

		rdma_destroy_id(conn->listener_cm_id);
		rdma_destroy_event_channel(conn->ec);
	}

	if (buffer != NULL) {
		munmap(buffer, serv_buffer_size);
		buffer = NULL;
	}
}

/* vim: set ts=4 sw=4 noexpandtab */
