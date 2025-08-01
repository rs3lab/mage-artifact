#include "controller.h"
#include "cacheline_manager.h"
#include "memory_management.h"
#include "pid_ns.h"
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include "kshmem.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h> //close
#include <fcntl.h>	//socket configuration
#include <pthread.h>
#include <arpa/inet.h>
#include "cache_manager_thread.h"
#include "kshmem.h"
#include "./include/disagg/pid_disagg.h"
#include "./include/disagg/pid_ns_disagg.h"
#include "./include/disagg/debug.h"
#include "../redis_logger/redis_logger.h"	// redis logger
#include "../disagg_logging_keys.hpp"	// redis keys

extern int get_cluster_id(void);

// #ifndef MODULE_NAME
// #define MODULE_NAME "controller"
// #endif

#ifndef MAX_CONNS
#define MAX_CONNS 32
#endif

#define kmalloc(X, Y) malloc(X)
#define vmalloc(X) malloc(X)
#define kfree free
#define vfree free

#define pr_info(...) printf(__VA_ARGS__)
#define pr_err(...) printf(__VA_ARGS__)
#define kthread_should_stop(X) 1

/*
 *	For debugging connection handler
 */
#define n_debug_region 3
static long t_debug[n_debug_region] = {0}, t_debug_cnt[n_debug_region] = {0};
static void start_debug(struct timespec *t_rec)
{
	clock_gettime(CLOCK_MONOTONIC, t_rec);
}

static void end_debug(int debug_idx, struct timespec *t_rec_st, struct timespec *t_rec_end)
{
	clock_gettime(CLOCK_MONOTONIC, t_rec_end);
	t_debug[debug_idx] += (((t_rec_end->tv_sec - t_rec_st->tv_sec) * 1000 * 1000) + ((t_rec_end->tv_nsec - t_rec_st->tv_nsec) / 1000));
	t_debug_cnt[debug_idx]++;
}

// For thread management
static int tcp_listener_stopped = 0;
static int tcp_acceptor_stopped = 0;
int run_thread = 0;

struct tcp_conn_manager
{
	struct tcp_conn_info conn_data[MAX_CONNS];
	pthread_t thread[MAX_CONNS];
	int handler_is_dead[MAX_CONNS];
};

struct tcp_conn_manager *tcp_conns;
struct server_service *tcp_server;
struct server_service *cache_man_server;

char *_inet_ntoa(struct in_addr *in)
{
	char *str_ip = NULL;

	str_ip = malloc(16 * sizeof(char));
	sprintf(str_ip, "%s", inet_ntoa(*in));
	return str_ip;
}

// ========== TCP server ===========//
/* Mode selection */
extern int is_debug_mode(void);
extern int is_recording_mode(void);

/* TCP transmission functions */
int tcp_server_send(struct socket *sk, int id, const char *buf,
					const size_t length, unsigned long flags)
{
	int len, left = length; //written = 0,
	(void)id;
	int sock_fd = sk->sock_fd;

	// check for replaying mode
	if (is_debug_mode() && !is_recording_mode())
	{
		// we are replaying now, so do not send anything
		return length;
	}

repeat_send:
	len = write(sock_fd, buf, left);
	if ((!(flags & MSG_DONTWAIT) && (len == -EAGAIN)))
		goto repeat_send;

	return len;
}

int tcp_server_receive(struct socket* sk, void *buf, int size, unsigned long flags)
{
	int len;
	int sock_fd = sk->sock_fd;
	len = read(sock_fd, buf, size);
	(void)flags;
	return len;
}

// y: Sends data to a memory server. Which server? The one you pass in as `sk`.
//    This function is only useful because it knows what size SCP<->MN messages
//    should be.
int send_msg_to_mem(uint32_t msg_type,
		void *buf, unsigned long tot_len,
		struct socket *sk,
		void *reply_buf, unsigned long reply_size)
{
	struct mem_header *hdr;
	int ret;
	int dummy_id = 0;
	int i = 0;

	hdr = get_header_ptr(buf);
	hdr->opcode = msg_type;
	hdr->sender_id = DISAGG_CONTROLLER_NODE_ID;

	ret = tcp_server_send(sk, dummy_id,
						  (const char *)buf, tot_len,
						  MSG_DONTWAIT);
	if (ret < (int)sizeof(struct memcpy_msg_struct))
	{
		printf("Cannot send message to memory node [%d]\n", ret);
		return -1;
	}

	while (1)
	{
		for (i = 0; i < DISAGG_NET_POLLING_SKIP_COUNTER; i++)
		{
			// check for replaying mode
			if (is_debug_mode() && !is_recording_mode())
			{
				// emulate the reply
				struct meminit_reply_struct *reply_ptr = (struct meminit_reply_struct *)reply_buf;
				reply_ptr->ret = 0;
				ret = reply_size;
			} else {
				ret = tcp_server_receive(sk, reply_buf, reply_size, MSG_DONTWAIT);
			}
			if (ret > 0)
				return ret;
		}
		usleep(1000);	// 1 ms
	}
	return -1;
}

static int send_simple_ack(struct socket *accept_socket, int id, int ret)
{
	const int len = DISAGG_NET_SIMPLE_BUFFER_LEN;
	char out_buf[DISAGG_NET_SIMPLE_BUFFER_LEN + 1];
	memset(out_buf, 0, len + 1);
	sprintf(out_buf, "ACK %d", ret);
	tcp_server_send(accept_socket, id, out_buf,
					strlen(out_buf), MSG_DONTWAIT);
	return 0;
}

/* Packet recorder interface*/
extern void record_packet(int id, char* ip_address, const void *buf, unsigned int len);
extern bool next_packet(int id, char* ip_address, void *buf, unsigned int* len);
extern void packet_served(void);

/* Main controller logic */
static int _debug_replay_cnt = 0;
void *tcp_connection_handler(void *data)
{
	struct tcp_conn_info *conn_data = (struct tcp_conn_info *)data;
	struct socket *accept_socket = &conn_data->accept_socket;
	int id = conn_data->thread_id;
	char *client_ip_str = NULL;

	// Header for message
	struct mem_header *incoming_msg_header;
	int ret;

	// Timers
	struct timespec t_st, t_end, t_debug_st[2], t_debug_end[2];
	long t_elapse = 0;
	unsigned long local_cnt = 0;
	void *incoming_msg_buf = malloc(DISAGG_NET_MAXIMUM_BUFFER_LEN);
	if (!incoming_msg_buf) {
		ret = -ENOMEM;
		goto out;
	}
	accept_socket->sock_is_done = 0;

	client_ip_str = _inet_ntoa(&conn_data->address->sin_addr);

	tcp_conns->handler_is_dead[id] = 0;	// started
	// Start timer: it just have a TCP connection
	clock_gettime(CLOCK_MONOTONIC, &t_st);
	// Set timeout
	fcntl(accept_socket->sock_fd, F_SETFL, O_NONBLOCK);
	memset(incoming_msg_buf, 0, DISAGG_NET_MAXIMUM_BUFFER_LEN);

	while (run_thread)
	{
		// y: Receive the next message.
		{
		if (accept_socket->sock_is_done) {
			usleep(1000000);	// 1 sec
			continue;
		}

		// y: Read the incoming message into a buffer.
		ret = tcp_server_receive(accept_socket, incoming_msg_buf,
				DISAGG_NET_MAXIMUM_CTRL_LEN, MSG_DONTWAIT);
		}
		if (ret <= 0) {
			usleep(10);	// Penalty for control plane compared to data plane (UDP)
			continue;
		}

		if (ret < (int)sizeof(*incoming_msg_header)) {
			pr_err("Cannot retrieve a header from %s\n", client_ip_str);
			goto reset_for_next_message;
		}

		// y: Get a node ID number based on the IP address of the source.
		//    XXX: Isn't this always the same, because the source is always
		//    the same?
		//    XXX(yash): Why do we set "is-memory-node" as 0 here? What if
		//    it is?
		int client_nid = get_nid_from_ip_array((u8 *) client_ip_str, 0);
		int recv = ret, expected_size = 0;
		incoming_msg_header = get_header_ptr(incoming_msg_buf);

		// y: If the incoming MSG's self-proclaimed client ID is out of range, fall back to
		//    using the source IP to derive the NID.
		//    This will get the wrong IP if the blade
		if (incoming_msg_header->sender_id < 0 || incoming_msg_header->sender_id > 2 * MAX_NUMBER_COMPUTE_NODE)
			 incoming_msg_header->sender_id = client_nid;
		// ^ NOTE: it will get wrong IP if the blade was not registered
		// TODO(yash): What does "registered" mean here?

		expected_size = incoming_msg_header->size;
		// receive remaining chunks
		while (recv < expected_size)
		{
			// continue to recv
			ret = tcp_server_receive(accept_socket, (void *)((char *)incoming_msg_buf + recv),
					DISAGG_NET_MAXIMUM_BUFFER_LEN - recv,
					MSG_DONTWAIT);
			if (ret > 0)
				 recv += ret;
			else
				 usleep(1000000);	// debug: 1 s
			pr_info("OPCode[%d]: received %d, expected %d\n", incoming_msg_header->opcode, recv, expected_size);
		}
		ret = recv;

		if (is_debug_mode() && is_recording_mode())
		{
			// record the packet
			record_packet(id, client_ip_str, incoming_msg_buf, expected_size);
		}

		// pr_info("TCP: Received opcode: %u, size: %d, from: %s\n", hdr->opcode, ret, client_ip_str);
		// end_debug(1, &t_debug_st[0], &t_debug_end[0]);

		switch (incoming_msg_header->opcode) {
		case DISAGG_FORK:
			// pr_info("Fork here 0\n");
			if (ret >= (int)(sizeof(*incoming_msg_header) + sizeof(struct fork_msg_struct)))
			{
				pr_info("Handler: received FORK! (%d bytes)...\n", ret);
				ret = handle_fork(incoming_msg_header, get_payload_ptr(incoming_msg_buf), accept_socket, id);
			}
			else
			{
				pr_info("FORK ERR\n");
				ret = -1;
				send_simple_ack(accept_socket, id, ret);
			}
			break;

		// TODO(yash): Didn't send mmaps back for this one.
		case DISAGG_CONN_USAGE_REMOTE_FORK:
			if (ret >= (int)(sizeof(*incoming_msg_header)))
			{
				pr_info("Handler: received REMOTE_FORK! (%d bytes)...\n", ret);
				ret = handle_remote_fork(incoming_msg_header, get_payload_ptr(incoming_msg_buf), accept_socket, id);
				// logging for success
				if (!ret)
					redis_add_message_with_blade(get_cluster_id(), incoming_msg_header->sender_id, redis_init_class, disagg_log_compute_daemon, "success");
			}
			else
			{
				ret = -1;
				pr_info("REMOTE FORK: received %d\n", ret);
				send_simple_ack(accept_socket, id, ret);
			}
			// For replaying, we need to mark it is processed
			if (is_debug_mode() && !is_recording_mode())
				packet_served();
			goto end;
			break;

		case DISAGG_EXEC:
			if (ret >= (int)(sizeof(*incoming_msg_header) + sizeof(struct exec_msg_struct)))
			{
				// TODO: Apply the same 'check and receive more' scheme for all the other buf
				int recv = ret;
				void *payload = get_payload_ptr(incoming_msg_buf);
				struct exec_msg_struct *exec_req = (struct exec_msg_struct *)payload;
				int expected_size = sizeof(struct exec_msg_struct);	// Should be short enough
				expected_size += (exec_req->num_vma - 1) * sizeof(struct exec_vmainfo);	// Size of VMA list
				expected_size += sizeof(*incoming_msg_header);	// Size of header

				pr_info("Handler: received first fragment of exec...\n");
				while (recv < expected_size)
				{
					// Continue to recv
					ret = tcp_server_receive(accept_socket, (void *)((char *)incoming_msg_buf + recv),
							DISAGG_NET_MAXIMUM_BUFFER_LEN - recv,
							MSG_DONTWAIT);
					if (ret > 0)
					{
						recv += ret;
					}else{
						usleep(1000000);	// 1 sec
					}
					pr_info("EXEC: received %d, expected %d\n", recv, expected_size);
				}

				pr_info("Handler: Received EXEC! (%d bytes)\n", ret);
				ret = handle_exec(incoming_msg_header, payload, accept_socket, id);
			}
			else
			{
				ret = -1;
				send_simple_ack(accept_socket, id, ret);
			}
			break;

			// Exit message
		// TODO(yash): Didn't send mmaps back for this one.
		case DISAGG_EXIT:
			if (ret >= (int)(sizeof(*incoming_msg_header) + sizeof(struct exit_msg_struct)))
			{
				pr_info("Handler: Received EXIT! (%d bytes)\n", ret);
				ret = handle_exit(incoming_msg_header, get_payload_ptr(incoming_msg_buf), accept_socket, id);
			}
			else
			{
				ret = -1;
				send_simple_ack(accept_socket, id, ret);
			}
			break;

		case DISAGG_NEW_TGID:
			if (ret >= (int)(sizeof(*incoming_msg_header) + sizeof(struct tgid_msg_struct)))
			{
				pr_info("New TGID: received %d\n", ret);
				ret = handle_get_new_tgid(incoming_msg_header, get_payload_ptr(incoming_msg_buf), accept_socket, id);
			}
			else
			{
				ret = -1;
				pr_info("New TGID: received %d\n", ret);
				send_simple_ack(accept_socket, id, ret);
			}
			// goto end;
			break;

		case DISAGG_NEW_PID_NS:
			if (ret >= (int)(sizeof(*incoming_msg_header) + sizeof(struct pid_ns_msg_struct)))
			{
				pr_info("New PID NS: received %d\n", ret);
				ret = handle_create_new_pid_ns(incoming_msg_header, get_payload_ptr(incoming_msg_buf), accept_socket, id);
			}
			else
			{
				ret = -1;
				pr_info("New PID NS: received %d\n", ret);
				send_simple_ack(accept_socket, id, ret);
			}
			// goto end;
			break;

		case DISAGG_GET_TGID_IN_NS:
			if (ret >= (int)(sizeof(*incoming_msg_header) + sizeof(struct get_tgid_ns_msg_struct)))
			{
				pr_info("Get TGID in NS: received %d\n", ret);
				ret = handle_get_tgid_in_ns(incoming_msg_header, get_payload_ptr(incoming_msg_buf), accept_socket, id);
			}
			else
			{
				ret = -1;
				pr_info("Get TGID in NS: received %d\n", ret);
				send_simple_ack(accept_socket, id, ret);
			}
			// goto end;
			break;

			// ALLOCATION - mmap
		case DISAGG_MMAP:
			if (ret >= (int)(sizeof(*incoming_msg_header) + sizeof(struct mmap_msg_struct)))
			{
				pr_info("Handler: Received MMAP! (%d bytes)\n", ret);
				// Send a reply inside the handler
				ret = handle_mmap(incoming_msg_header, get_payload_ptr(incoming_msg_buf), accept_socket, id);
			}
			else
			{
				ret = -1;
				send_simple_ack(accept_socket, id, -1);
			}
			break;

			// ALLOCATION - brk
		case DISAGG_BRK:
			if (ret >= (int)(sizeof(*incoming_msg_header) + sizeof(struct brk_msg_struct)))
			{
				pr_info("Handler: Received BRK! (%d bytes)\n", ret);
				// Send a reply inside the handler
				ret = handle_brk(incoming_msg_header, get_payload_ptr(incoming_msg_buf), accept_socket, id);
			}
			else
			{
				ret = -1;
				send_simple_ack(accept_socket, id, -1);
			}
			break;

			// MUNMAP
		case DISAGG_MUNMAP:
			if (ret >= (int)(sizeof(*incoming_msg_header) + sizeof(struct munmap_msg_struct)))
			{
				pr_info("Handler: Received MUNMAP! (%d bytes)\n", ret);
				// Send a reply inside the handler
				ret = handle_munmap(incoming_msg_header, get_payload_ptr(incoming_msg_buf), accept_socket, id);
			}
			else
			{
				ret = -1;
				send_simple_ack(accept_socket, id, -1);
			}
			break;

			// MREMAP
		case DISAGG_MREMAP:
			if (ret >= (int)(sizeof(*incoming_msg_header) + sizeof(struct mremap_msg_struct)))
			{
				pr_info("Handler: Received MREMAP! (%d bytes)\n", ret);
				// Send a reply inside the handler
				ret = handle_mremap(incoming_msg_header, get_payload_ptr(incoming_msg_buf), accept_socket, id);
			}
			else
			{
				ret = -1;
				send_simple_ack(accept_socket, id, -1);
			}
			break;

			// RDMA: New nodes send this message to tell the controller abt
			// their rdma parameters (rkey, MR size, base address, etc).
		case DISAGG_RDMA:
			if (ret >= (int)(sizeof(*incoming_msg_header)))
			{
				int recv = ret;
				void *payload = get_payload_ptr(incoming_msg_buf);
				int expected_size = sizeof(*incoming_msg_header) + sizeof(struct rdma_msg_struct);
				int updated_nid = 0;

				// y: Fully receive the incoming message.

				pr_info("RDMA_INIT: received %d, expected %d\n", recv, expected_size);
				while (recv < expected_size)
				{
					// Continue to recv
					ret = tcp_server_receive(accept_socket, (void *)((char *)incoming_msg_buf + recv),
							DISAGG_NET_MAXIMUM_BUFFER_LEN - recv,
							MSG_DONTWAIT);
					if (ret > 0)
					{
						recv += ret;
					}else{
						usleep(1000000);	// wait for the next packet(s)
					}
					pr_info("RDMA_INIT: received %d, expected %d\n", recv, expected_size);
				}

				// Send a reply inside the handler
				ret = handle_client_rdma_init(incoming_msg_header, payload, accept_socket, id, client_ip_str, &updated_nid);

				if (ret)
				{
					send_simple_ack(accept_socket, id, -1);
				} else {
					if (updated_nid > MAX_NUMBER_COMPUTE_NODE)
					{
						redis_add_message_with_blade(
								get_cluster_id(), updated_nid - MAX_NUMBER_COMPUTE_NODE,
								redis_init_class, disagg_log_memory_roce, "success");
					} else {
						redis_add_message_with_blade(
								get_cluster_id(), updated_nid,
								redis_init_class, disagg_log_compute_roce, "success");
					}
				}
			}
			else
			{
				ret = -1;
				send_simple_ack(accept_socket, id, -1);
			}
			break;
			// Kernel side va alloc
		// TODO(yash): Didn't send mmaps back for this one.
		case DISAGG_KSHMEM_ALLOC:
		case DISAGG_KSHMEM_BASE_ADDR:	// sharing the same request/reponse type
			if (ret >= (int)(sizeof(*incoming_msg_header)))
			{
				int recv = ret;
				void *payload = get_payload_ptr(incoming_msg_buf);
				int expected_size = sizeof(*incoming_msg_header) + sizeof(struct kshmem_msg_struct);
				pr_info("KSHMEM_ALLOC/BASE ADDR (%d): received %d, expected %d\n",
						(incoming_msg_header->opcode), recv, expected_size);
				while (recv < expected_size)
				{
					// Continue to recv
					ret = tcp_server_receive(accept_socket, (void *)((char *)incoming_msg_buf + recv),
							DISAGG_NET_MAXIMUM_BUFFER_LEN - recv,
							MSG_DONTWAIT);
					if (ret > 0) {
						recv += ret;
					}else{
						usleep(1000000);	// wait for the next packet(s)
						pr_info("KSHMEM_ALLOC: received %d, expected %d\n", recv, expected_size);
					}
				}
				// Send a reply inside the handler
				if (incoming_msg_header->opcode == DISAGG_KSHMEM_ALLOC)
				{
					ret = handle_kshmem_alloc(incoming_msg_header, get_payload_ptr(incoming_msg_buf), accept_socket, id);
					redis_add_message_with_blade(get_cluster_id(), incoming_msg_header->sender_id, redis_init_class, disagg_log_compute_kshmem, "success");
				}
				else if (incoming_msg_header->opcode == DISAGG_KSHMEM_BASE_ADDR)
					 ret = handle_kshmem_base_addr(incoming_msg_header, get_payload_ptr(incoming_msg_buf), accept_socket, id);
			}
			else
			{
				ret = -1;
				send_simple_ack(accept_socket, id, -1);
			}
			break;

		// TODO(yash): Didn't send mmaps back for this one.
		case DISAGG_KSHMEM_ALLOC_VA:
			if (ret >= (int)(sizeof(*incoming_msg_header)))
			{
				int recv = ret;
				void *payload = get_payload_ptr(incoming_msg_buf);
				int expected_size = sizeof(*incoming_msg_header) + sizeof(struct kshmem_va_msg_struct);
				pr_info("KSHMEM_ALLOC with VA (%d): received %d, expected %d\n",
						(incoming_msg_header->opcode), recv, expected_size);
				while (recv < expected_size)
				{
					// Continue to recv
					ret = tcp_server_receive(accept_socket, (void *)((char *)incoming_msg_buf + recv),
							DISAGG_NET_MAXIMUM_BUFFER_LEN - recv,
							MSG_DONTWAIT);
					if (ret > 0)
					{
						recv += ret;
					}else{
						usleep(1000000);	// wait for the next packet(s)
						pr_info("KSHMEM_ALLOC with VA: received %d, expected %d\n", recv, expected_size);
					}
				}
				// Send a reply inside the handler
				ret = handle_kshmem_alloc_va(incoming_msg_header, get_payload_ptr(incoming_msg_buf), accept_socket, id);
			}
			else
			{
				ret = -1;
				send_simple_ack(accept_socket, id, -1);
			}
			break;

			// DEBUG functions
		case DISAGG_COPY_VMA:
			if (ret >= (int)(sizeof(*incoming_msg_header) + sizeof(struct exec_msg_struct)))
			{
				pr_info("COPY_VMA - received: %d\n", ret);
				ret = handle_exec(incoming_msg_header, get_payload_ptr(incoming_msg_buf), accept_socket, id);
			}
			else
			{
				ret = -1;
			}
			send_simple_ack(accept_socket, id, ret);
			break;

		case DISAGG_CHECK_VMA:
			printf("Error: Recieved unsupported msg type: DISAGG_CHECK_VMA\n");
			ret = -1;
			send_simple_ack(accept_socket, id, ret);
			break;

		case DISAGG_DEBUG_KERN_LOG:
			{
				void *payload = get_payload_ptr(incoming_msg_buf);
				struct err_msg_struct *res_alloc_req = (struct err_msg_struct *)payload;
				printf("\n**KERN_LOG::\n%s\n==END of LOG==\n\n", res_alloc_req->debug_str);
				send_simple_ack(accept_socket, id, 0);
			}
			break;

		case DISAGG_DEBUG_KERN_STACK_LOG:
			{
				void *payload = get_payload_ptr(incoming_msg_buf);
				struct err_stack_msg_struct *res_alloc_req = (struct err_stack_msg_struct *)payload;
				printf("\n**KERN_LOG_STACK::\n%s\n==END of LOG==\n\n", res_alloc_req->debug_str);
				send_simple_ack(accept_socket, id, 0);
			}
			break;

		case DISAGG_FIN_CONN:
			// Finish connection and release
			clock_gettime(CLOCK_MONOTONIC, &t_end);
			t_elapse = (t_end.tv_sec - t_st.tv_sec) * 1000 * 1000;
			t_elapse += (t_end.tv_nsec - t_st.tv_nsec) / 1000;

			// Currently do not need to send another msg
			pr_info("FINISH connection: thread %d, %ld usec\n", id, t_elapse);
			goto out;

		default:
			pr_err("TCP: Cannot recognize opcode: %u from %s\n", incoming_msg_header->opcode, client_ip_str);
			goto out;
		}

reset_for_next_message:
		// Reset used buffer space
		memset(incoming_msg_buf, 0, DISAGG_NET_MAXIMUM_BUFFER_LEN);

		// For replaying, we need to mark it is processed
		if (is_debug_mode() && !is_recording_mode())
			packet_served();

		// To avoid watchdog
		local_cnt ++;
		if (local_cnt >= DISAGG_NET_POLLING_SKIP_COUNTER) {
			local_cnt = 0;
			usleep(10);
			continue;
		}
	}

out:
	close(tcp_conns->conn_data[id].accept_socket.sock_fd);

	// Clean up data structures of this accepted connection
	if (tcp_conns->conn_data[id].address)
	{
		free(tcp_conns->conn_data[id].address);
		tcp_conns->conn_data[id].address = NULL;
	}
	memset(&tcp_conns->conn_data[id], 0, sizeof(struct tcp_conn_info));

	if (client_ip_str)
		free(client_ip_str);

	if (incoming_msg_buf)
		free(incoming_msg_buf);
	incoming_msg_buf = NULL;
	tcp_conns->handler_is_dead[id] = 1;
end:
	return NULL;
}

void tcp_server_accept(void)
{
	int acc_sock_fd;
	int id = 0;
	int debug_thread_accpected = 0;

	for (id = 0; id < MAX_CONNS; id++)
		tcp_conns->handler_is_dead[id] = 1;

	printf("Ready to accept new socket.\n");
	while (run_thread)
	{
		struct sockaddr_in *client = NULL;
		int addr_len = sizeof(struct sockaddr_in);

		// y: Accept a new connection from the socket.

		client = malloc(sizeof(struct sockaddr_in));
		if (is_debug_mode() && !is_recording_mode() && debug_thread_accpected < 5)
		{
			debug_thread_accpected ++;
		} else {
			if ((acc_sock_fd = accept(tcp_server->listen_socket.sock_fd,
									(struct sockaddr *)client,
									(socklen_t *)&addr_len)) < 0)
			{
				printf("Error - cannot accept\n");
				goto err;
			} else {
				printf("New socket accepted\n");
			}
		}

		// y: Try to accept this connection, slotting it into our list of
		// active connections (`tcp_conn_manager`)

		for (id = 1; id < MAX_CONNS; id++)
		{
			if (tcp_conns->handler_is_dead[id]) {
				printf("new connection accepted\n");
				break;
			}
		}
		if (id == MAX_CONNS)
		{
			// cannot accept this connection
			close(acc_sock_fd);
			fprintf(stderr, "Cannot accept new conn (%d)\n", MAX_CONNS);
			continue;
		}

		// y: Put the connection information into our `tcp_conns` array.

		tcp_conns->handler_is_dead[id] = 0;
		tcp_conns->conn_data[id].accept_socket.sock_fd = acc_sock_fd;
		tcp_conns->conn_data[id].thread_id = id;
		tcp_conns->conn_data[id].address = client;
		tcp_conns->conn_data[id].is_send_conn = 0;
		pthread_spin_init(&(tcp_conns->conn_data[id].send_msg_lock), PTHREAD_PROCESS_PRIVATE);
		tcp_conns->conn_data[id].accept_socket.sk_lock = &(tcp_conns->conn_data[id].send_msg_lock);

		// y: Initialize a handler for this incoming TCP connection.
		pthread_create(&tcp_conns->thread[id], NULL, tcp_connection_handler,
					   (void *)&tcp_conns->conn_data[id]);
		usleep(1000);
	}

	if (tcp_conns->conn_data[id].address)
	{
		free(tcp_conns->conn_data[id].address);
		tcp_conns->conn_data[id].address = NULL;
	}

err:
	tcp_acceptor_stopped = 1;
	return;
}

void *tcp_server_listen(void* args)
{
	struct sockaddr_in address;
	int sock_fd;
	int opt = 1;
	int error;
	(void)args;

	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd <= 0)
	{
		printf(" *** mtp | Error: %d while creating tcp server "
				"listen socket | tcp_server_listen *** \n",
				sock_fd);
		goto err;
	}

	error = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
					   &opt, sizeof(opt));
	if (error)
	{
		printf("Error: cannot setsockopt - %d\n", error);
		exit(EXIT_FAILURE);
	}
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(DEFAULT_PORT);

	// Bind
	error = bind(sock_fd, (struct sockaddr *)&address,
				 sizeof(address));
	if (error < 0)
	{
		printf("Error: cannot bind - %d\n", error);
		exit(EXIT_FAILURE);
	}

	// Listen
	error = listen(sock_fd, MAX_CONNS);
	if (error < 0)
	{
		printf("Error: cannot listen - %d\n", error);
		exit(EXIT_FAILURE);
	}

	tcp_server->listen_socket.sock_fd = sock_fd;
	tcp_server->address = &address;

	tcp_server_accept();
	run_thread = 0;

	close(sock_fd); // Listening socket
	printf("Terminate: %s\n", __func__);
	tcp_listener_stopped = 1;
	exit(0);
err:
	printf("Terminate (Err): %s\n", __func__);
	tcp_listener_stopped = 1;
	exit(0);
}

static int add_dummy_memory_node(void)
{
	int node_id = DUMMY_MEM_NODE_ID;
	int lid = 0;
	u8 mac[ETH_ALEN] = {0};
	unsigned int qpn[NUM_PARALLEL_CONNECTION] = {0};
	int psn = 0;
	char gid[sizeof(DISAGG_RDMA_GID_FORMAT)] = {0};
	u32 lkey = 0;
	u32 rkey = 0;
	u64 addr = 0x100000000; // y: Set a 4 GiB memory offset. The CN should
				// subtract this.
	u64 size = 34359738368UL;
	u64 *ack_buf = calloc(NUM_PARALLEL_CONNECTION, sizeof(*ack_buf));;
	struct socket *sk = calloc(1, sizeof(*sk));
	struct dest_info *added_mn = ctrl_set_node_info(node_id, lid,
			mac, qpn, psn, gid, lkey, rkey, addr, size, ack_buf, sk, true);

	increase_mem_node_count();
	return 0;
}


static int server_start(void)
{
	run_thread = 1;
	tcp_server->running = 1;
	pthread_create(&tcp_server->thread, &tcp_server->pth_attr, &tcp_server_listen, NULL);

	cache_man_server->running = 1;
	// pthread_create(&cache_man_server->thread, &cache_man_server->pth_attr, &cache_manager_main, NULL);
	
	return 0;
}

int network_server_init(void)
{
	tcp_server = malloc(sizeof(struct server_service));
	memset(tcp_server, 0, sizeof(struct server_service));
	tcp_conns = malloc(sizeof(struct tcp_conn_manager));
	memset(tcp_conns, 0, sizeof(struct tcp_conn_manager));

	// Cache management server (dynamic cache resizing)
	cache_man_server = malloc(sizeof(struct server_service));
	memset(cache_man_server, 0, sizeof(struct server_service));

	initialize_node_list();
	init_mn_man();
	init_remote_thread_man();
	cacheline_init(); // data structure for cacheline manager
	cache_man_init();
	pid_ns_init();

	printf("Adding dummy memory node\n");
	add_dummy_memory_node();

	printf("Starting server\n");
	server_start();
	return 0;
}

static void print_exit_stat(void)
{
	int i = 0;
	for (i = 0; i < n_debug_region; i++)
	{
		if (t_debug_cnt[i] == 0)
			t_debug_cnt[i] = 1;
		pr_info("Debuging routine #%d: Total %ld usec, Avg %ld usec, Cnt %ld\n",
				i, t_debug[i], t_debug[i] / t_debug_cnt[i], t_debug_cnt[i]);
	}
}

void network_server_exit(void)
{
	int ret;
	int id;

	run_thread = 0;
	print_bfrt_addr_trans_rule_counters();

	if (!tcp_server->running)
		pr_info(" *** mtp | No kernel thread to kill | "
				"network_server_exit *** \n");
	else
	{
		for (id = 0; id < MAX_CONNS; id++)
		{
			if (!tcp_conns->handler_is_dead[id])
			{
				ret = pthread_cancel(tcp_conns->thread[id]);
				if (!ret)
					pr_info("[STOP] connection handler thread: %d\n", id);
			}
		}

		if (!tcp_listener_stopped)
		{
			ret = pthread_cancel(tcp_server->thread);
			if (!ret)
				pr_info("[STOP] tcp-server listening thread\n");
		}

		if (tcp_server != NULL && !tcp_listener_stopped)
		{
			close(tcp_server->listen_socket.sock_fd);
		}
	}
	if (tcp_conns)
		free(tcp_conns);
	tcp_conns = NULL;
	if (tcp_server)
		free(tcp_server);
	tcp_server = NULL;

	if (!cache_man_server->running){
		;
	}
	else
	{
		ret = pthread_cancel(cache_man_server->thread);
		if (!ret)
			pr_info(" *** mtp | tcp server listening thread"
					" stopped | network_server_exit *** \n");
	}
	if (cache_man_server)
		free(cache_man_server);
	cache_man_server = NULL;

	// Clear data structures
	cacheline_clear();
	clear_mn_man();
	clear_remote_thread_man();
	print_exit_stat();

	pr_info("network_server_exit *** \n");
}
