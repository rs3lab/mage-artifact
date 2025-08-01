#include "rdma_common.h"
#include <stdlib.h>
#include <stdio.h>

struct sockaddr_in addr;
struct rdma_event_channel *ec = NULL;
struct ibv_qp_init_attr qp_attr;
struct rdma_cm_id *conn = NULL;
struct ibv_pd *pd;
struct ibv_mr *mr;
struct ibv_cq *cq;
char *buffer = NULL;
uint64_t buffer_size = 0;
atomic_uint *alloc_array = NULL;

struct rdma_cm_event *event;

void rdma_deinit(void)
{
	printf("Disconnecting and cleaning up\n");
	check_cm_event();

	rdma_destroy_qp(conn);

	puts("destroyed qp");

	// rdma_destroy_id(conn);

	puts("destroyed conn");

	ibv_destroy_cq(cq);

	puts("destroyed cq");

	printf("Destroying mr\n");
	ibv_dereg_mr(mr);

	ibv_dealloc_pd(pd);

	rdma_destroy_event_channel(ec);
}

enum rdma_cm_event_type check_cm_event(void)
{
	printf("Checking CM event...\n");
	if (rdma_get_cm_event(ec, &event)) {
		perror("Failed to retrieve a cm event\n");
		exit(1);
	}
	fprintf(stdout, "Rceived event: %s\n", rdma_event_str(event->event));
	rdma_ack_cm_event(event);
	puts("ack!");
	return event->event;
}

void rdma_init(void)
{
	printf("Creating event channel...\n");
	ec = rdma_create_event_channel();
	if (!ec) {
		perror("rdma_create_event_channel");
		exit(1);
	}

	printf("Creating RDMA ID...\n");
	if (rdma_create_id(ec, &conn, NULL, RDMA_PS_TCP)) {
		perror("rdma_create_id");
		exit(1);
	}
}

void rdma_init_finish(void)
{
	// Allocate Protection Domain
	printf("Allocating PD...\n");
	pd = ibv_alloc_pd(conn->verbs);
	if (!pd) {
		perror("ibv_alloc_pd");
		exit(1);
	}

	printf("Creating CQ...\n");
	cq = ibv_create_cq(conn->verbs, 3 * MIND_QUEUE_SIZE + 1, NULL, NULL, 0);

	printf("Creating QP...\n");
	memset(&qp_attr, 0, sizeof(qp_attr));
	qp_attr.qp_type = IBV_QPT_RC;
	qp_attr.send_cq = cq;
	qp_attr.recv_cq = cq;
	qp_attr.cap.max_send_wr = MIND_QUEUE_SIZE;
	qp_attr.cap.max_recv_wr = MIND_QUEUE_SIZE;
	qp_attr.cap.max_send_sge = 3;
	qp_attr.cap.max_recv_sge = 3;
	if (rdma_create_qp(conn, pd, &qp_attr)) {
		perror("rdma_create_qp");
		exit(1);
	}
}
