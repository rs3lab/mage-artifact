#include "roce_disagg.h"
#include "../../include/disagg/network_rdma_disagg.h"
#include "../../include/disagg/kshmem_disagg.h"

#ifndef MODULE_NAME
#define MODULE_NAME "roce_for_disaggregation"
#endif


MODULE_LICENSE("GPL");

static char *ip_addr = "10.10.10.202";
static char *frontend_ip_addr = "192.168.122.1";

module_param(ip_addr, charp, 0000);  // Module parameter for IP address
MODULE_PARM_DESC(ip_addr, "IP address of this blade");  // Parameter description
module_param(frontend_ip_addr, charp, 0000);  // Module parameter for IP address
MODULE_PARM_DESC(frontend_ip, "IP address of the control plane front-end");  // Parameter description

// ---------------------------------------------
// TCP Server
// ---------------------------------------------

static void print_rdma_msg_struct(struct rdma_msg_struct *msg)
{
	pr_syscall("rdma_msg_struct:\n"
			"\tret=%d\n"
			"\tnode_id=%d\n"
			"\tnode_type=%d\n"
			"\tnode_id_res=%d\n"
			"\taddr=%llx\n"
			"\tsize=%llu\n"
			"\trkey=%d\n"
			"\tlkey=%d\n"
			"\tqpn=NOT SHOWN\n"
			"\tpsn=%d\n"
			"\tip_address=%d:%d:%d:%d\n"
			"\tbase_addr=%llx\n"
			"\tkshmem_va_start=%llx\n",
			msg->ret, msg->node_id, msg->node_type,
			msg->node_id_res, msg->addr, msg->size, msg->rkey, msg->lkey,
			msg->psn,
			msg->ip_address[0], msg->ip_address[1], msg->ip_address[2], msg->ip_address[3],
			msg->base_addr, msg->kshmem_va_start);
}

// This variable is used send_rdma_meta, and nowhere else.
// I put here so I don't have to allocate it on the stack (it's very large).
static struct rdma_msg_struct send_rdma_meta_payload;

// Sends the metadata of this node to the control node.
static int send_rdma_meta(void)
{
	struct rdma_msg_struct *payload = &send_rdma_meta_payload;
	struct rdma_msg_struct *reply;
	int ret;

	reply = kzalloc(sizeof(struct rdma_msg_struct), GFP_KERNEL);
	if (!reply)
		 return -ENOMEM;

	memset(payload, 0, sizeof(*payload));
	payload->node_id = get_local_node_id();

	payload->node_type = DISAGG_RDMA_COMPUTE_TYPE;
	// y: What does this do?
	payload->kshmem_va_start = kshmem_get_local_start_va();

	// y: Load our VM IP address into the request.
	sscanf(ip_addr, "%hhu.%hhu.%hhu.%hhu",
			&payload->ip_address[0], &payload->ip_address[1],
			&payload->ip_address[2], &payload->ip_address[3]);

	pr_syscall("Sending Node RDMA information to SCP (our ip=%s, scp ip=%s)\n",
			ip_addr, frontend_ip_addr);
	print_rdma_msg_struct(payload);

	ret = send_msg_to_control(DISAGG_RDMA, payload, sizeof(*payload),
			reply, sizeof(struct rdma_msg_struct));

	pr_info("send_rdma_meta: controller reply is here.\n");
	print_rdma_msg_struct(reply);

	if (ret < sizeof(*payload)) {
		ret = -EINTR;
		goto out;
	}

	if (reply->ret) {     // only 0 is success
		ret = reply->ret; // set error
		pr_info("send_rdma_meta: Fail with err[%d]\n", ret);
		goto out;
	}
	ret = 0;

	pr_info("Successfully sent data to controller with ret | node [%u]\n", reply->node_id_res);
	pr_info("Setting local node ID to %d accordingly\n", reply->node_id_res);
	set_local_node_id(reply->node_id_res);

out:
	kfree(reply);
	return ret;
}

// ---------------------------------------------
// Module Init
// ---------------------------------------------

// Module and RoCE connection initialiazation
static int init_tcp_conn_to_controller(void *args)
{
	int ret;

	msleep(DISAGG_RDMA_INIT_SLEEP_TIME_IN_MS);

	// Connect to Controller.
	pr_info("ROCE TCP: starting TCP server connection...\n");
retry_send_meta:
	pr_info("ROCE TCP: Sending rdma metadata to controller\n");
	ret = send_rdma_meta();
	if (ret) {
		msleep(1000);
		pr_info("ROCE TCP: retry connecting to the server, err: %d\n", ret);
		goto retry_send_meta;
	}
	pr_info("ROCE TCP: TCP handshake with controller success\n");

	pr_info("ROCE TCP: TCP connection to controller has been initialized\n");

	return 0;
}

static int __init module_init_fn(void)
{
	int num_cnqps = NUM_CNTHREADS;
	int num_fhqps = NUM_FHTHREADS;

	pr_info("ROCE: Module initialized with cn_ip_addr=%s, cnqps=%d, fhqps=%d\n",
			ip_addr, num_cnqps, num_fhqps);

	pr_info("ROCE: starting tcp init\n");
	init_tcp_conn_to_controller(NULL);
	pr_info("ROCE: starting rdma init\n");
	init_rdma_conn_to_mn(num_cnqps, num_fhqps);

	// Set up callbacks so kernel can use RDMA
	set_mind_rdma_map_dma_fn(mind_rdma_map_dma);
	set_mind_rdma_unmap_dma_fn(mind_rdma_unmap_dma);
	set_mind_rdma_get_fhqp_handle_fn(mind_rdma_get_fhqp_handle);
	set_mind_rdma_get_cnqp_handle_fn(mind_rdma_get_cnqp_handle);
	set_mind_rdma_put_qp_handle_fn(mind_rdma_put_qp_handle);
	set_mind_rdma_read_fn(mind_rdma_read);
	set_mind_rdma_write_fn(mind_rdma_write);
	set_mind_rdma_read_sync_fn(mind_rdma_read_sync);
	set_mind_rdma_write_sync_fn(mind_rdma_write_sync);
	set_mind_rdma_initialize_batched_write_fn(mind_rdma_initialize_batched_write);
	set_mind_rdma_batched_write_fn(mind_rdma_batched_write);
	set_mind_rdma_check_cq_fn(mind_rdma_check_cq);
	set_mind_rdma_check_cqs_fn(mind_rdma_check_cqs);
	set_mind_rdma_poll_cq_fn(mind_rdma_poll_cq);
	set_mind_rdma_poll_cqs_fn(mind_rdma_poll_cqs);

	mind_notify_network_init_done();

	return 0;
}

static void __exit module_exit_fn(void)
{
	// TODO: Stop the cnthreads. unmap their dma pages before disabling RDMA.
	destroy_rdma_conn_to_mn();
}

module_init(module_init_fn)
module_exit(module_exit_fn)

/* vim: set sw=8 ts=8 noexpandtab */
