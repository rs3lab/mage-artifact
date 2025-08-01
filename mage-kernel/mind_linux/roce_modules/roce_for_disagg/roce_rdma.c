#include "roce_disagg.h"
#include "../../include/disagg/print_disagg.h"
#include "../../include/disagg/profile_points_disagg.h"
#include "../../include/linux/dma-contiguous.h"

#include <linux/inet.h>

// GLOBAL DATA STRUCTURES
// extern struct ib_client mind_rdma_ib_client;
struct mind_rdma_state *rdma_state = NULL;

// FIXME: must be synchronized with server-side definition
struct mind_mr_info
{
	uint64_t remote_addr; // y: not used when CN->MN.
	uint32_t rkey;        // y: not used when CN->MN; rdmacm does that separately
	uint64_t mem_size;    // y: During first CN->MN, CN sets this field to request a MR size.
	uint32_t num_qps;     // y: During first CN->MN, CN sets this field to set # parallel QPs.
};

char* mn_server_ip = "10.10.10.202";
char* mn_server_port_start = "58675";
// Every QP sets up its connection on a different port. This var tracks the "current" (max) used
// port, so subsequent connections know which port to use.
u32 current_mem_server_port = 0;

// y: This callback (dma_init_body, of type page_init_callback) takes in
// a virtual, kmapped, region of memory. It should create an ib MR region for
// that address, and return the _DMA_ address of that region.
//
// This means that the RDMA stack can DMA directly to/from that memory region
// using the returned address.
// Don't dereference the address directly! It's for DMA only.
//
// Arguments:
//   - `len`: size in bytes.
u64 mind_rdma_map_dma(struct page *page, size_t len)
{
	// `len` is in bytes.
	u64 dma_addr = ib_dma_map_page(rdma_state->dev, page, 0, len,
			DMA_BIDIRECTIONAL);
	int error = ib_dma_mapping_error(rdma_state->dev, dma_addr);

	if (error) {
		pr_err("ib_dma_map_page(0x%px) failed (ret = %d)\n", page, error);
		return 0;
	}
	return dma_addr;
}

// Arguments:
//   - `len`: size in bytes.
void mind_rdma_unmap_dma(u64 laddr_dma, size_t len)
{
	ib_dma_unmap_page(rdma_state->dev, laddr_dma, len, DMA_BIDIRECTIONAL);
}

// Given an outstanding RDMA request, this function polls the RDMA CQs for the
// remote server's response. The response data arrives within the outstanding
// request struct itself. This function destroys the provided `mind_rdma_req`.
//
// Returns 0 on success, nonzero on error.
int mind_rdma_poll_cq(void *qp_handle, struct mind_rdma_req *target)
{
	return mind_rdma_poll_cqs(qp_handle, target, 1);
}
EXPORT_SYMBOL(mind_rdma_poll_cq);

// Returns -1 for error, else returns "# popped".
int mind_rdma_check_cq(void *qp_handle, struct mind_rdma_req *target)
{
	struct mind_rdma_cm_state *cm = qp_handle;
	struct ib_wc wc; 
	struct mind_rdma_req *req;
	int ret;

	ret = ib_poll_cq(cm->cq, 1, &wc);
	if (ret < 0) {
		pr_err("ib_poll_cq(ret=%d) failed\n", ret);
		return -1;
	}

	if (ret == 0)
		 return 0;

	if (unlikely(wc.status != IB_WC_SUCCESS)) {
		pr_err("ib_poll_cq failed with status(%d)\n", wc.status);
		return -1;
	}

	req = (void *) wc.wr_id;
	if (unlikely(req->owning_cpu != smp_processor_id())) {
		pr_warn("mind_rdma_poll_cq: running on CPU%d, but recved CQE from CPU %d!\n",
				smp_processor_id(), req->owning_cpu);
		return -1;
	}
	return 1;
}

int mind_rdma_check_cqs(void *qp_handle, int num_targets, struct ib_wc *wc_arr)
{
	struct mind_rdma_cm_state *cm = qp_handle;
	int i, ret;

	// Wait for RDMA requests.
	ret = ib_poll_cq(cm->cq, num_targets, wc_arr);
	if (ret < 0) {
		pr_err("ib_poll_cq(%d) failed (%d)\n", num_targets, ret);
		return -1;
	}

	for (i = 0; i < ret; i++) {
		struct ib_wc *wc = &wc_arr[i];
		struct mind_rdma_req *req = (void *) wc->wr_id;

		if (unlikely(wc->status != IB_WC_SUCCESS)) {
			pr_err("ib_poll_cq failed with status(%d)\n", wc->status);
			return -1;
		}
		if (unlikely(req->owning_cpu != smp_processor_id())) {
			pr_warn("mind_rdma_poll_cqs: received request sent from another CPU!\n");
			return -1;
		}
	}
	return ret;
}

// MIND processes pinned to cores => per-CPU is OK.

DEFINE_PER_CPU(struct ib_wc *, percpu_wc_cache);

// Given an outstanding set of RDMA requests, this function polls the RDMA CQs for the
// remote server's response. The response data arrives within the outstanding
// request structs themselves. This function destroys the provided `mind_rdma_req`s.
//
// Requirements:
// - All requests should be on the same QP!
// - num_targets > 0
//
// Returns 0 on success, nonzero on error.
int mind_rdma_poll_cqs(void *qp_handle, struct mind_rdma_req *targets, int num_targets)
{
	struct mind_rdma_cm_state *cm = qp_handle;
	struct ib_wc *wc_arr = per_cpu(percpu_wc_cache, smp_processor_id());
	u64 retry_count = 0;
	int batch_size = (cm->cm_type == MIND_RDMA_CN_CM)
		? MIND_RDMA_CNCQ_POLL_BATCH_SIZE
		: MIND_RDMA_FHCQ_POLL_BATCH_SIZE;
	int i, num_recved;

	// Local "wc_arr" cache size is limited. Don't overflow the cache.
	BUG_ON(num_targets > batch_size);

	// Wait for RDMA requests.
	num_recved = 0;
	while (num_recved < num_targets) {
		int ret, num_to_poll;

		num_to_poll = min(batch_size, num_targets - num_recved);

		ret = ib_poll_cq(cm->cq, num_to_poll, wc_arr + num_recved);
		if (ret < 0) {
			pr_err("ib_poll_cq(%d) failed (%d)\n", num_to_poll, num_recved);
			return -1;
		}
		if (ret == 0) { // no ACK popped
			ndelay(MIND_RDMA_POLL_CQS_BACKOFF_NS);
			retry_count++;
			if (retry_count > ((MIND_RDMA_POLL_CQS_TIMEOUT_MS * 1000 * 1000) / MIND_RDMA_POLL_CQS_BACKOFF_NS)) {
				pr_info("ROCE_RDMA: timeout\n");
				return -1;
			}
			continue;
		}

		// Check that recved acks are valid :)
		for (i = num_recved; i < num_recved + ret; i++) {
			struct ib_wc *wc = &wc_arr[i];
			// struct mind_rdma_req *req = (void *) wc->wr_id;

			if (unlikely(wc->status != IB_WC_SUCCESS)) {
				pr_err("ib_poll_cq failed with status(%d)\n", wc->status);
				return -1;
			}
		}

		num_recved += ret;
		retry_count = 0;
	}

	return 0;
}
EXPORT_SYMBOL(mind_rdma_poll_cqs);

// Asynchronous read. Requires a pre-allocated "out" object.
// TODO: Refactor lbuf_dma into a u64.
int mind_rdma_read(void *qp_handle, void *lbuf_dma, unsigned long addr, unsigned long len,
		struct mind_rdma_req *out)
{
	struct mind_rdma_cm_state *cm = qp_handle;
	struct ib_rdma_wr *rdma_wr;
	struct ib_send_wr *wr;
	struct ib_sge sge;
	int ret;

	pr_rdma("mind_rdma_read: sending read (qp=%d, rdma_addr=0x%lx)\n", qp_num, addr);

	if (!rdma_state || !out)
		 return -1;

	// Initialize convenience variables
	rdma_wr = &out->rdma_wr;
	wr = &rdma_wr->wr;

	// prepare rdma wr
	memset(out, 0, sizeof(*out));
	out->owning_cpu = smp_processor_id();

	memset(rdma_wr, 0, sizeof(*rdma_wr));
	rdma_wr->remote_addr = rdma_state->mem_server_base_addr + addr;
	rdma_wr->rkey = rdma_state->mem_server_rkey;
	// sge
	// TODO: match the lifespan of sge and wr
	sge.addr = (u64)lbuf_dma;
	sge.length = len;	                 // mind_req->sglist.length;
	// So registering 1000 MRs doesn't matter! I think...

	// y: `local_dma_lkey` is populated with a Giga-MR at PD init. This MR allows access to all
	//    of physical memory through DMA addresses. See:
	//    https://patchwork.kernel.org/project/linux-rdma/patch/1437608083-22898-2-git-send-email-jgunthorpe@obsidianresearch.com/
	//    Apparently this should be recently performant, as of:
	//    https://patchwork.kernel.org/project/linux-rdma/patch/1443080064-28760-4-git-send-email-sagig@mellanox.com/
	sge.lkey = rdma_state->pd->local_dma_lkey;    // mind_req->mr->lkey;
	memset(wr, 0, sizeof(*wr));
	// wr
	wr->wr_id = (__u64) out;
	wr->opcode = IB_WR_RDMA_READ;
	wr->sg_list = &sge;
	wr->num_sge = 1;
	wr->send_flags = IB_SEND_SIGNALED;
	// pr_info("READ - buf: 0x%lx, buf (dma): 0x%lx, length: %lu, lkey: %u\n",
	// 	(unsigned long)buf, (unsigned long)mind_req->sglist.dma_address, 
	// 	len, queue->dev->pd->local_dma_lkey);

	ret = ib_post_send(cm->qp, wr, NULL);
	if (ret) {
		pr_err("mind_rdma_read::ib_post_send failed (%d)\n", ret);
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL(mind_rdma_read);

// Asynchronous write. Returns a handle to the ongoing request.
int mind_rdma_write(
		void *qp_handle,         // y: which QP/CQ to use
		void *lbuf_dma,         // y: data to read (kmapped pointer)
		unsigned long addr, // y: remote addr to write to
		unsigned long len,
		struct mind_rdma_req *out)  // y: return variable to the request
{
	struct mind_rdma_cm_state *cm = qp_handle;
	struct ib_rdma_wr *rdma_wr;
	struct ib_send_wr *wr;
	struct ib_sge sge;
	int ret;

	pr_rdma("mind_rdma_write: sending write (qp=%d, rdma_addr=0x%lx)\n", qp_num, addr);

	if (!rdma_state || !out)
		 return -1;

	// Initialize convenience variables
	rdma_wr = &out->rdma_wr;
	wr = &rdma_wr->wr;

	// prepare wr
	memset(out, 0, sizeof(*out));
	out->owning_cpu = smp_processor_id();
	memset(rdma_wr, 0, sizeof(*rdma_wr));
	rdma_wr->remote_addr = rdma_state->mem_server_base_addr + addr;
	rdma_wr->rkey = rdma_state->mem_server_rkey;
	// sge
	// TODO: match the lifespan of sge and wr
	sge.addr = (u64) lbuf_dma;
	sge.length = len;	// out->sglist.length;
	sge.lkey = rdma_state->pd->local_dma_lkey;	// out->mr->lkey;
							// send wr
	memset(wr, 0, sizeof(*wr));
	wr->wr_id = (__u64) out;
	wr->opcode = IB_WR_RDMA_WRITE;
	wr->sg_list = &sge;
	wr->num_sge = 1;
	wr->send_flags = IB_SEND_SIGNALED;
	// pr_info("WRITE - buf: 0x%lx, buf (dma): 0x%lx, length: %lu, lkey: %u\n",
	// 	(unsigned long)buf, (unsigned long)out->sglist.dma_address, 
	// 	len, queue->dev->pd->local_dma_lkey);

	ret = ib_post_send(cm->qp, wr, NULL);
	if (ret) {
		pr_err("mind_rdma_write::ib_post_send failed (%d)\n", ret);
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL(mind_rdma_write);

int mind_rdma_read_sync(void *qp_handle, void *lbuf_dma, u64 size, unsigned long remote_addr)
{
	struct mind_rdma_cm_state *cm = qp_handle;
	struct ib_wc wc;
	struct mind_rdma_req req;
	int ret;

	ret = mind_rdma_read(qp_handle, lbuf_dma, remote_addr, size, &req);
	if (ret) {
		pr_err("ROCE_RDMA: failed to issue read from raddr=0x%lx\n", remote_addr);
		BUG();
	}

	do {
		ret = ib_poll_cq(cm->cq, 1, &wc);
		if (ret < 0) {
			pr_err("ib_poll_cq failed (%d)\n", ret);
			return -1;
		}
		if (ret == 0)
			 ndelay(MIND_RDMA_READ_SYNC_POLL_BACKOFF_NS);
	} while (!ret);

	if (wc.status != IB_WC_SUCCESS) {
		pr_err("ib_poll_cq failed with status(%d)\n", wc.status);
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL(mind_rdma_read_sync);

int mind_rdma_write_sync(void *qp_handle, void *lbuf_dma, u64 size, unsigned long remote_addr)
{
	struct mind_rdma_cm_state *cm = qp_handle;
	struct mind_rdma_req req;
	struct ib_wc wc;
	int ret;

	pr_rdma("ROCE_RDMA: sending write (remote_addr=0x%lx)\n", remote_addr);
	ret = mind_rdma_write(qp_handle, lbuf_dma, remote_addr, size, &req);
	if (ret) {
		pr_err("ROCE_RDMA: failed to issue write to raddr=0x%lx\n", remote_addr);
		BUG();
	}
	pr_rdma("ROCE_RDMA: sent write (remote_addr=0x%lx, lbuf_dma=0x%px)\n",
			remote_addr, lbuf_dma);


	do {
		ret = ib_poll_cq(cm->cq, 1, &wc);
		if (ret < 0) {
			pr_err("ib_poll_cq failed (%d)\n", ret);
			return -1;
		}
		if (ret == 0)
			 ndelay(MIND_RDMA_WRITE_SYNC_POLL_BACKOFF_NS);
	} while (!ret);

	if (wc.status != IB_WC_SUCCESS) {
		pr_err("ib_poll_cq failed with status(%d)\n", wc.status);
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL(mind_rdma_write_sync);


// Initializes a `struct mind_rdma_reqs` for later batched sending.
//
// We don't initialize:
//  - out->reqs[i].sge.addr: Caller should set this to local DMA addr. 
//  - out->reqs[i].rdma_wr.remote_addr: Caller should set this to remote raddr, _then_ remember to
//                                       add `out->mem_server_base_addr` before sending.
//  - out->reqs[i].sge.length: Caller should set to RDMA size (bytes).
void mind_rdma_initialize_batched_write(struct mind_rdma_reqs *out)
{
	int i;
	out->mem_server_base_raddr = rdma_state->mem_server_base_addr;

	for (i = 0; i < out->num_reqs; i++) {
		struct mind_rdma_req *req = &out->reqs[i];
		struct ib_rdma_wr *rdma_wr = &req->rdma_wr;

		memset(req, 0, sizeof(*req));

		rdma_wr->rkey = rdma_state->mem_server_rkey;

		req->sge.length = PAGE_SIZE;
		req->sge.lkey = rdma_state->pd->local_dma_lkey;    // mind_req->mr->lkey;

		rdma_wr->wr.wr_id = (__u64) req;
		rdma_wr->wr.opcode = IB_WR_RDMA_WRITE;
		rdma_wr->wr.sg_list = &req->sge;
		rdma_wr->wr.num_sge = 1;
	}

	// Assemble the batch into a linked list of requests.
	// Linked list is "reversed"; reqs[0] is the tail.
	for (i = out->num_reqs - 1; i > 0; i--)
		out->reqs[i].rdma_wr.wr.next = &out->reqs[i-1].rdma_wr.wr;
	out->reqs[0].rdma_wr.wr.next = NULL;

	// Only the last request should create a CQE => less load on NIC.
	out->reqs[0].rdma_wr.wr.send_flags = IB_SEND_SIGNALED;
}

// Asynchronous batched write.
// Requires a pre-initialized "reqs" object: see `mind_rdma_init_batched_write()`.
//
// Always request at least 1 RDMA.
// `req_offset` doesn't do anything anymore. TODO remove it later.
int mind_rdma_batched_write(void *qp_handle, struct mind_rdma_reqs *reqs, int req_offset)
{
	struct mind_rdma_cm_state *cm = qp_handle;
	struct mind_rdma_req *first_req = &reqs->reqs[reqs->num_reqs - 1];
	int ret;
	
	pr_rdma("mind_rdma_batched_write: sending batched write (qp=0x%px)\n", qp_handle);
	ret = ib_post_send(cm->qp, &first_req->rdma_wr.wr, NULL);
	if (ret) {
		pr_err("mind_rdma_read::ib_post_send failed (%d)\n", ret);
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL(mind_rdma_batched_write);

void *mind_rdma_get_fhqp_handle(void) {
	int i;
	for (i = 0; i < rdma_state->num_fhqps; i++) {
		struct mind_rdma_cm_state *cm = rdma_state->fhcm[i];
		spin_lock(&cm->qp_lock);
		if (!cm->is_qp_used) {
			cm->is_qp_used = true;
			spin_unlock(&cm->qp_lock);
			return cm;
		}
		spin_unlock(&cm->qp_lock);
	}
	pr_warn("WARNING: ROCE_RDMA: no more RDMA FHQPs are available!\n");
	return NULL;
}
EXPORT_SYMBOL(mind_rdma_get_fhqp_handle);

void *mind_rdma_get_cnqp_handle(void) {
	int i;
	for (i = 0; i < rdma_state->num_cnqps; i++) {
		struct mind_rdma_cm_state *cm = rdma_state->cncm[i];
		spin_lock(&cm->qp_lock);
		if (!cm->is_qp_used) {
			cm->is_qp_used = true;
			spin_unlock(&cm->qp_lock);
			return cm;
		}
		spin_unlock(&cm->qp_lock);
	}
	pr_warn("WARNING: ROCE_RDMA: no more RDMA CNQPs are available!\n");
	return NULL;
}
EXPORT_SYMBOL(mind_rdma_get_cnqp_handle);

int mind_rdma_put_qp_handle(void *qp_handle) {
	struct mind_rdma_cm_state *cm = qp_handle;

	spin_lock(&cm->qp_lock);
	if (!cm->is_qp_used) {
		spin_unlock(&cm->qp_lock);
		pr_err("ERROR: ROCE_RDMA: releasing already-free QP handle!\n");
		return -1;
	}

	cm->is_qp_used = false;
	spin_unlock(&cm->qp_lock);
	return 0;
}
EXPORT_SYMBOL(mind_rdma_put_qp_handle);


static void mind_rdma_init_test(void)
{
	struct mind_rdma_req req;
	int ret;
	void *buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	u64 buf_dma;
	unsigned long addr = PAGE_SIZE;
	unsigned long len = PAGE_SIZE;
	int err_line = 0;

	BUG_ON(!buf);
	buf_dma = mind_rdma_map_dma(virt_to_page(buf), PAGE_SIZE);
	BUG_ON(!buf_dma);

	if (rdma_state->num_fhqps < 1) {
		pr_info("Skipping all RDMA tests (not enough FHQPs)\n");
		goto success;
	}

	// read from the second page
	((unsigned long *)buf)[0] = 0x12;

	// Test: Can we read from a page? (and does memory start as zeroed?)

	// y: Send a request
	ret = mind_rdma_read(rdma_state->fhcm[0], (void *) buf_dma, addr, len, &req);
	if (ret) {
		err_line = __LINE__;
		goto err_rw;
	}
	// y: Wait for the response to arrive.
	if (mind_rdma_poll_cq(rdma_state->fhcm[0], &req)) {
		err_line = __LINE__;
		goto err_poll;
	}
	if (((unsigned long *)buf)[0] != 0x0) {
		err_line = __LINE__;
		pr_info("Test 1: RDMA read failed:: 0x%lx\n", ((unsigned long *)buf)[0]);
		goto err_data;
	}
	
	// Test: Writes to the page.

	((unsigned long *)buf)[0] = 0x42;
	ret = mind_rdma_write(rdma_state->fhcm[0], (void *) buf_dma, addr, len, &req);
	if (ret) {
		err_line = __LINE__;
		goto err_rw;
	}
	if (mind_rdma_poll_cq(rdma_state->fhcm[0], &req)) {
		err_line = __LINE__;
		goto err_poll;
	}

	((unsigned long *)buf)[0] = 0x0;
	ret = mind_rdma_read(rdma_state->fhcm[0], (void *) buf_dma, addr, len, &req);
	if (ret) {
		err_line = __LINE__;
		goto err_rw;
	}
	if (mind_rdma_poll_cq(rdma_state->fhcm[0], &req)) {
		err_line = __LINE__;
		goto err_poll;
	}
	// y: Did we see our prior write?
	if (((unsigned long *)buf)[0] != 0x42) {
		err_line = __LINE__;
		pr_info("Test 2: RDMA write failed:: 0x%lx\n", ((unsigned long *)buf)[0]);
		goto err_data;
	}

	if (rdma_state->num_fhqps < 4) {
		pr_info("Skipping some tests (not enough QPs)\n");
		goto success;
	}

	// Test: Read from other QPs

	((unsigned long *)buf)[0] = 0x0;
	ret = mind_rdma_read(rdma_state->fhcm[1], (void *) buf_dma, addr, len, &req);
	if (ret) {
		err_line = __LINE__;
		goto err_rw;
	}
	if (mind_rdma_poll_cq(rdma_state->fhcm[1], &req)) {
		err_line = __LINE__;
		goto err_poll;
	}
	// y: Did we see our prior write?
	if (((unsigned long *)buf)[0] != 0x42) {
		err_line = __LINE__;
		pr_info("Test 3: RDMA read failed:: 0x%lx\n", ((unsigned long *)buf)[0]);
		goto err_data;
	}

	// Test: Write from other QPs

	// XXX: This might cause trouble later when we make our FHQP read only...
	((unsigned long *)buf)[0] = 0x99;
	ret = mind_rdma_write(rdma_state->fhcm[2], (void *) buf_dma, addr, len, &req);
	if (ret) {
		 err_line = __LINE__;
		 goto err_rw; 
	}
	if (mind_rdma_poll_cq(rdma_state->fhcm[2], &req)) {
		err_line = __LINE__;
		goto err_poll;
	}

	((unsigned long *)buf)[0] = 0x0;
	ret = mind_rdma_read(rdma_state->fhcm[3], (void *) buf_dma, addr, len, &req);
	if (ret) {
		 err_line = __LINE__;
		 goto err_rw; 
	}
	if (mind_rdma_poll_cq(rdma_state->fhcm[3], &req)) {
		err_line = __LINE__;
		goto err_poll;
	}
	if (((unsigned long *)buf)[0] != 0x99) {
		err_line = __LINE__;
		pr_info("Test 4: RDMA read/write failed:: 0x%lx\n", ((unsigned long *)buf)[0]);
		goto err_data;
	}

success:
	pr_info("ROCE_RDMA: RDMA tests succeeded!\n");
	mind_rdma_unmap_dma(buf_dma, PAGE_SIZE);
	kfree(buf);
	return;

err_data:
err_rw:
err_poll:
	pr_err("ROCE_RDMA: RDMA tests failed at line %d!\n", err_line);
	mind_rdma_unmap_dma(buf_dma, PAGE_SIZE);
	kfree(buf);
	return;
}

// ----------------------------------------
// RDMA CM and RDMA Init Related Code
// ----------------------------------------

static int mind_rdma_create_cqs(struct mind_rdma_state *state)
{
	int i;

	for (i = 0; i < state->num_fhcqs; i++) {
		/* Polling queues need direct cq polling context */
		state->fhcqs[i] = ib_alloc_cq(state->dev, NULL, MIND_RDMA_FHCQ_SIZE, 0, IB_POLL_DIRECT);
		if (IS_ERR(state->fhcqs[i]))
			 return PTR_ERR(state->fhcqs[i]);
	}

	for (i = 0; i < state->num_cncqs; i++) {
		/* Polling queues need direct cq polling context */
		state->cncqs[i] = ib_alloc_cq(state->dev, NULL, MIND_RDMA_CNCQ_SIZE, 0, IB_POLL_DIRECT);
		if (IS_ERR(state->cncqs[i]))
			 return PTR_ERR(state->cncqs[i]);
	}

	return 0;
}

static void mind_rdma_destroy_cqs(void)
{
	int i;
	for (i = 0; i < rdma_state->num_fhcqs; i++) {
		if (rdma_state->fhcqs[i])
			 BUG_ON(ib_destroy_cq(rdma_state->fhcqs[i]));
	}
	for (i = 0; i < rdma_state->num_cncqs; i++) {
		if (rdma_state->cncqs[i])
			 BUG_ON(ib_destroy_cq(rdma_state->cncqs[i]));
	}
}

static int mind_rdma_create_qp(struct mind_rdma_cm_state *cm)
{
	struct ib_qp_init_attr init_attr;
	int ret;
	u32 max_wr = (cm->cm_type == MIND_RDMA_CN_CM) ? MIND_RDMA_CNQP_SIZE
		                                      : MIND_RDMA_FHQP_SIZE;
	struct ib_cq *cq = (cm->cm_type == MIND_RDMA_CN_CM)
		? rdma_state->cncqs[cm->cm_num]
		: rdma_state->fhcqs[cm->cm_num];

	memset(&init_attr, 0, sizeof(init_attr));

	// init_attr.event_handler = nvme_rdma_qp_event;    // debugging purposes
	/* +1 for drain */
	init_attr.cap.max_send_wr = max_wr;
	/* +1 for drain */
	init_attr.cap.max_recv_wr = 0;
	init_attr.cap.max_recv_sge = 1;
	init_attr.cap.max_send_sge = 1;
	// WQEs will create CQEs only if explicitly requested.
	init_attr.sq_sig_type = IB_SIGNAL_REQ_WR; 
	init_attr.qp_type = IB_QPT_RC; // reliable connection
	init_attr.send_cq = cq;
	init_attr.recv_cq = cq;
	init_attr.qp_context = cm;

	ret = rdma_create_qp(cm->cm_id, rdma_state->pd, &init_attr);
	if (ret)
		 return ret;

	cm->qp = cm->cm_id->qp;
	return 0;
}

static int mind_rdma_create_queues(struct mind_rdma_cm_state *cm)
{
	struct mind_rdma_state *rdma_state = cm->rdma_state;
	int ret;

	ret = mind_rdma_create_qp(cm);
	if (ret) {
		pr_err("mind_rdma_create_qps failed (%d)\n", ret);
		goto out_destroy_ib_cq;
	}
	return 0;

out_destroy_ib_cq:
	// TODO: ref checking for multiple queues sharing the PD + CQ
	mind_rdma_destroy_cqs();
	ib_dealloc_pd(rdma_state->pd);
	return ret;
}

static int rdma_cm_handler_addr_resolved(struct mind_rdma_cm_state *cm)
{
	int ret;

	// Create CQ, QP; ref: nvme_rdma_addr_resolved, nvme_rdma_create_queue_ib
	ret = mind_rdma_create_queues(cm);
	if (ret) {
		pr_info("nvme_rdma_create_queue_ib failed (%d)\n", ret);
		return ret;
	}

	// Initiate resolving route; ref: rdma_resolve_route
	ret = rdma_resolve_route(cm->cm_id, MIND_RDMA_CM_TIMEOUT_MS);
	if (ret) {
		pr_info("rdma_resolve_route failed (%d)\n", ret);
		return ret;
	}

	return 0;
}

static int rdma_cm_handler_route_resolved(struct mind_rdma_cm_state *cm)
{
	struct rdma_conn_param param = { };
	struct mind_mr_info priv = { 0 };
	int ret;

	// param.qp_num = queue->qp->qp_num;
	param.flow_control = 0;	// 1;

	// Copied from user-space client.
	// HARD CODED to NIC max under assumption remote NIC is the same.
	param.responder_resources = 1; // max number of outstanding reqs we accept from remote
	param.initiator_depth = 16;     // max outstanding reqs _we_ will emit.

	// param.responder_resources = queue->dev->dev->attrs.max_qp_rd_atom;
	/* maximum retry count */
	param.retry_count = 7;
	param.rnr_retry_count = 7;
	param.private_data = &priv;
	param.private_data_len = sizeof(priv);

	priv.mem_size = rdma_state->mem_server_mem_size;
	// XXX: Will the memory server care if we provide QPs of different types here?
	priv.num_qps = rdma_state->num_cnqps + rdma_state->num_fhqps;

	pr_rdma("Initiating rdma_connect\n");
	ret = rdma_connect_locked(cm->cm_id, &param);
	if (ret) {
		pr_err("rdma_connect_locked failed (%d).\n", ret);
		return ret;
	}

	return 0;
}

static int rdma_cm_handler_rdma_established(struct mind_rdma_cm_state *cm, struct rdma_cm_event *ev)
{
	struct mind_mr_info *server_info = (struct mind_mr_info *)ev->param.conn.private_data;
	if (!server_info) {
		pr_err("server_info is NULL\n");
		return -EINVAL;
	}

	if (cm->rdma_state->mem_server_base_addr == 0) {
		cm->rdma_state->mem_server_base_addr = server_info->remote_addr;
		pr_rdma("ROCE_RDMA: Setting memory server base address to 0x%llx\n",
				server_info->remote_addr);
	}
	if (cm->rdma_state->mem_server_rkey == 0) {
		cm->rdma_state->mem_server_rkey = server_info->rkey;
		pr_rdma("ROCE_RDMA: Setting memory server rkey to %u\n", server_info->rkey);
	} else {
		if (cm->rdma_state->mem_server_rkey != server_info->rkey)
			 pr_warning("ROCE_RDMA: WARNING: memory server rkey has changed!\n");
	}

	switch (cm->cm_type) {
	case MIND_RDMA_CN_CM:
		pr_rdma("ROCE_RDMA: RDMA CN connection %d (out of %d) established.\n",
				cm->cm_num + 1, rdma_state->num_cnqps);
		break;
	case MIND_RDMA_FH_CM:
		pr_rdma("ROCE_RDMA: RDMA FH connection %d (out of %d) established.\n",
				cm->cm_num + 1, rdma_state->num_fhqps);
		break;
	}
	return 0;
}

static int wait_for_rdma_cm_done(struct mind_rdma_cm_state *cm)
{
	int ret;
	ret = wait_for_completion_interruptible(&cm->cm_done);
	if (ret)
		 return ret;
	WARN_ON_ONCE(cm->cm_error > 0);
	return cm->cm_error;
}

static int mind_rdma_cm_handler(struct rdma_cm_id *cm_id, struct rdma_cm_event *ev)
{
	struct mind_rdma_cm_state *cm = cm_id->context;
	int cm_error = 0;
	if (!cm)
	{
		pr_err("cm is NULL\n");
		return -EINVAL;
	}

	pr_rdma("ROCE_RDMA: mind_rdma_cm_handler was triggered\n");
	pr_rdma("%s (%d): status %d id %p\n",
			rdma_event_msg(ev->event), ev->event,
			ev->status, cm_id);

	switch (ev->event) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:
		cm_error = rdma_cm_handler_addr_resolved(cm);
		break;
	case RDMA_CM_EVENT_ROUTE_RESOLVED:
		cm_error = rdma_cm_handler_route_resolved(cm);
		break;
	case RDMA_CM_EVENT_ESTABLISHED:
		cm_error = rdma_cm_handler_rdma_established(cm, ev);
		complete(&cm->cm_done);
		return 0;
		// case RDMA_CM_EVENT_REJECTED:
		// 	cm_error = nvme_rdma_conn_rejected(queue, ev);
		// 	break;
		// case RDMA_CM_EVENT_ROUTE_ERROR:
		// case RDMA_CM_EVENT_CONNECT_ERROR:
		// case RDMA_CM_EVENT_UNREACHABLE:
		// case RDMA_CM_EVENT_ADDR_ERROR:
		// 	dev_dbg(queue->ctrl->ctrl.device,
		// 		"CM error event %d\n", ev->event);
		// 	cm_error = -ECONNRESET;
		// 	break;
	case RDMA_CM_EVENT_DISCONNECTED:
		// case RDMA_CM_EVENT_ADDR_CHANGE:
		// case RDMA_CM_EVENT_TIMEWAIT_EXIT:
		// 	dev_dbg(queue->ctrl->ctrl.device,
		// 		"disconnect received - connection closed\n");
		// 	nvme_rdma_error_recovery(queue->ctrl);
		break;
		// case RDMA_CM_EVENT_DEVICE_REMOVAL:
		// 	/* device removal is handled via the ib_client API */
		// 	break;
	default:
		// 	dev_err(queue->ctrl->ctrl.device,
		// 		"Unexpected RDMA CM event (%d)\n", ev->event);
		// 	nvme_rdma_error_recovery(queue->ctrl);
		pr_err("Unexpected RDMA CM event (%d)\n", ev->event);
		break;
	}

	pr_rdma("ROCE_RDMA: mind_rdma_cm_handler finished: %s (%d): status %d id %p\n",
			rdma_event_msg(ev->event), ev->event, ev->status, cm_id);

	if (cm_error) {
		cm->cm_error = cm_error;
		complete(&cm->cm_done);
	}

	return 0;
}

static int mind_rdma_create_pd(struct mind_rdma_state *state)
{
	struct ib_device *ibdev = state->dev;

	state->pd = ib_alloc_pd(ibdev, 0);
	// queue->dev->pd = ib_alloc_pd(ibdev, IB_PD_UNSAFE_GLOBAL_RKEY);
	// ^for unsafe, use IB_PD_UNSAFE_GLOBAL_RKEY instead of 0
	if (IS_ERR(state->pd)) {
		pr_err("ib_alloc_pd failed (%ld)\n", PTR_ERR(state->pd));
		return -1;
	}
	return 0;
}

// This function sets up a RDMA CM connection starting from just a device and a PD.
// If you're looking for a high level overview of the RDMA setup process, this is the place.
static int initialize_rdma_cm_conn(struct mind_rdma_cm_state *cm)
{
	char sock_port[14];
	int ret;

	pr_rdma("ROCE_RDMA: initializing %d rdma cm conn to %s:%u\n",
			cm->cm_type, mn_server_ip, cm->rdma_state->mem_server_port);

	cm->cq = (cm->cm_type == MIND_RDMA_CN_CM) ? rdma_state->cncqs[cm->cm_num]
		                                  : rdma_state->fhcqs[cm->cm_num];

	pr_rdma("ROCE_RDMA: creating socket\n");

	ret = snprintf(sock_port, sizeof(sock_port), "%u", rdma_state->mem_server_port);
	sock_port[sizeof(sock_port)-1] = '\0';
	ret = inet_pton_with_scope(&init_net, AF_UNSPEC, rdma_state->mem_server_ip,
			sock_port, &rdma_state->server_addr);
	if (ret) {
		pr_err("malformed address passed: %s:%s\n",
				rdma_state->mem_server_ip, sock_port);
		return -1;
	}

	// The next RDMA-CM connection should use the next TCP port up.
	rdma_state->mem_server_port++;

	pr_rdma("ROCE_RDMA: creating rdma_cm_id from socket\n");

	cm->cm_id = rdma_create_id(&init_net, mind_rdma_cm_handler, cm, RDMA_PS_TCP, IB_QPT_RC);
	if (IS_ERR(cm->cm_id)) {
		pr_err("failed to create CM ID: %ld\n", PTR_ERR(cm->cm_id));
		return PTR_ERR(cm->cm_id);
	}

	pr_rdma("ROCE_RDMA: resolving ip->rdma addr from socket\n");
	// Translates IP addresses to RDMA addresses. Pass in our server
	// address, it'll tell the RDMA-CM handle to connect to that addr.
	ret = rdma_resolve_addr(cm->cm_id, NULL, (struct sockaddr *)&rdma_state->server_addr,
			MIND_RDMA_CM_TIMEOUT_MS);
	if (ret) {
		pr_info("rdma_resolve_addr failed (%d).\n", ret);
		goto out_destroy_cm_id;
	}

	// NOTE) mind_rdma_cm_handler will trigger route resolution and final initialization
	//       so wait...
	// wait for cm handler to finish.
	pr_rdma("ROCE_RDMA: waiting for cm handler to finish\n");
	ret = wait_for_rdma_cm_done(cm);
	if (ret) {
		pr_err("ROCE_RDMA: ERR: rdma connection establishment failed (%d)\n", ret);
		goto out_destroy_cm_id;
	}

	pr_rdma("ROCE_RDMA: successfully initialized rdma cm conn to %s:%u!\n", mn_server_ip,
			cm->rdma_state->mem_server_port);

	return 0;

out_destroy_cm_id:
	rdma_destroy_id(cm->cm_id);
	return -1;
}

static void destroy_rdma_cm_conn(struct mind_rdma_cm_state *cm)
{
	rdma_disconnect(cm->cm_id);

	rdma_destroy_qp(cm->cm_id);
	rdma_destroy_id(cm->cm_id);
	cm->cm_id = NULL;
}

static void mind_rdma_initialize_conns(void)
{
	int i;

	for (i = 0; i < rdma_state->num_cnqps; i++)
	{
		struct mind_rdma_cm_state *cm_state = kzalloc(sizeof(*cm_state), GFP_KERNEL);
		if (!cm_state) {
			pr_err("ROCE_RDMA: Failed to allocate memory for rdma_state->cncm[i]\n");
			BUG();
		}
		rdma_state->cncm[i] = cm_state;

		cm_state->cm_num = i;
		cm_state->rdma_state = rdma_state;
		cm_state->cm_type = MIND_RDMA_CN_CM;
		init_completion(&cm_state->cm_done);
		spin_lock_init(&cm_state->qp_lock);
	}

	for (i = 0; i < rdma_state->num_fhqps; i++)
	{
		struct mind_rdma_cm_state *cm_state = kzalloc(sizeof(*cm_state), GFP_KERNEL);
		if (!cm_state) {
			pr_err("ROCE_RDMA: Failed to allocate memory for rdma_state->fhcm[i]\n");
			BUG();
		}
		rdma_state->fhcm[i] = cm_state;

		cm_state->cm_num = i;
		cm_state->rdma_state = rdma_state;
		cm_state->cm_type = MIND_RDMA_FH_CM;
		init_completion(&cm_state->cm_done);
		spin_lock_init(&cm_state->qp_lock);
	}


	for (i = 0; i < rdma_state->num_cnqps; i++) {
		initialize_rdma_cm_conn(rdma_state->cncm[i]);
		msleep(100);
	}
	for (i = 0; i < rdma_state->num_fhqps; i++) {
		initialize_rdma_cm_conn(rdma_state->fhcm[i]);
		msleep(100);
	}
}

static int add_ib_client(struct ib_device *ib_device)
{
	int ret = 0;
	pr_rdma("ROCE_RDMA: ib_register_client called on device %s\n",
			ib_device->name);
	if (!rdma_state) {
		pr_err("rdma_state is NULL\n");
		return 1;
	}
	if (strcmp(ib_device->name, MIND_RDMA_IB_DEVNAME) != 0) {
		pr_rdma("ROCE_RDMA: skipping unrecognized ib device (%s)\n",
				ib_device->name);
		return 0;
	}

	rdma_state->dev = ib_device;

	pr_rdma("ROCE_RDMA: creating PD\n");
	ret = mind_rdma_create_pd(rdma_state);
	if (ret)
		return 1;

	ret = mind_rdma_create_cqs(rdma_state);
	if (ret) {
		pr_err("couldn't create CQ: error code %d\n", ret);
		return 1;
	}

	pr_rdma("ROCE_RDMA: initializing RDMA CM connections\n");
	mind_rdma_initialize_conns();

	pr_info("ROCE_RDMA: CM handlers finished! Testing RDMA...\n");
	mind_rdma_init_test();

	complete(&rdma_state->init_done);
	return 0;

}

static void remove_ib_client(struct ib_device *ib_device, void *client_data)
{
	pr_rdma("ROCE_RDMA: Removing RDMA client from ib dev %s\n",
			ib_device->name);
	// TODO: proper cleanup so we don't have the hard driver restart problem
}

struct ib_client mind_rdma_ib_client = {
	.name   = "mind_rdma_to_mn",
	.add	= add_ib_client,
	.remove = remove_ib_client
};


int init_rdma_conn_to_mn(int num_cnqps, int num_fhqps)
{
	int i, ret = 0;
	size_t ib_wc_cache_size = max(MIND_RDMA_FHCQ_POLL_BATCH_SIZE,
			MIND_RDMA_CNCQ_POLL_BATCH_SIZE);

	// Initialize the per-CPU wc cache.
	for_each_possible_cpu(i) {
		void *tmp = kmalloc(sizeof(struct ib_wc) * ib_wc_cache_size, GFP_KERNEL);

		BUG_ON(!tmp);
		per_cpu(percpu_wc_cache, i) = tmp;
	}

	rdma_state = kzalloc(sizeof(*rdma_state), GFP_KERNEL);
	if (!rdma_state) {
		pr_err("ROCE_RDMA: Failed to allocate memory for rdma_state\n");
		BUG();
	}

	// setup general params
	rdma_state->num_cnqps = num_cnqps;
	rdma_state->num_fhqps = num_fhqps;
	rdma_state->num_cncqs = num_cnqps;
	rdma_state->num_fhcqs = num_fhqps;

	rdma_state->mem_server_mem_size = MIND_RDMA_RMEM_SIZE_MIB << 20; // convert MiB to bytes
	rdma_state->mem_server_ip = mn_server_ip;
	ret = kstrtou32(mn_server_port_start, 10, &rdma_state->mem_server_port_start);
	if (ret) {
		pr_err("ROCE_RDMA: invalid port supplied!\n");
		return -1;
	}
	rdma_state->mem_server_port = rdma_state->mem_server_port_start;
	init_completion(&rdma_state->init_done);

	// Attach to the Infiniband device.
	pr_rdma("ROCE_RDMA: registering ib client\n");
	ret = ib_register_client(&mind_rdma_ib_client);
	if (ret) {
		pr_err("failed to register IB client: ret=%d\n", ret);
		BUG();
	}

	pr_rdma("ROCE_RDMA: waiting for ib client registration to complete\n");
	ret = wait_for_completion_interruptible(&rdma_state->init_done);
	if (ret) {
		pr_err("rdma initialization failed: ret=%d\n", ret);
		BUG();
	}
	pr_rdma("ROCE_RDMA: done with ib registration\n");

	return 0;
}

int destroy_rdma_conn_to_mn(void)
{
	int i;

	for (i = 0; i < rdma_state->num_cnqps; i++) {
		destroy_rdma_cm_conn(rdma_state->cncm[i]);
		kfree(rdma_state->cncm[i]);
	}
	for (i = 0; i < rdma_state->num_fhqps; i++) {
		destroy_rdma_cm_conn(rdma_state->fhcm[i]);
		kfree(rdma_state->fhcm[i]);
	}

	mind_rdma_destroy_cqs();
	ib_dealloc_pd(rdma_state->pd);
	ib_unregister_client(&mind_rdma_ib_client);

	kfree(rdma_state);
	rdma_state = NULL;
	return 0;
}

/* vim: set ts=8 sw=8 tw=99 noexpandtab */
