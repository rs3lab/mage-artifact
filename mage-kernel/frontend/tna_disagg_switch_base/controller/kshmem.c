#include "kshmem.h"
#include "list_and_hash.h"

// static vairables for shared kernel memory
static u64 kshmem_va_start = 0;

static struct task_struct *get_kern_tsk(void)
{
    return mn_get_task(DISAGG_KERN_TGID);
}

void kshmem_set_va_start(u64 va_start, int nid)
{
    if (!kshmem_va_start)
    {
        kshmem_va_start = va_start;
        printf("KSHMEM: VA initialized [cn: %d]: [0x%lx - 0x%lx]\n",
                nid, va_start, va_start + DISAGG_KERN_SHMEM_SIZE);

        // initialize task struct for kernel
        task_spin_lock();
        // To have single shared entry for kernel's dummy tsk,
        // here we use DISAGG_CONTROLLER_NODE_ID regardless of sender's ID
        if (!get_kern_tsk())
        {
            // 1) Initial for from systemd
            int ret = mn_create_dummy_task_mm(DISAGG_CONTROLLER_NODE_ID, DISAGG_KERN_TGID, DISAGG_KERN_PID_NS_ID, DISAGG_KERN_TGID);
            if (!ret)
            {
                printf("KSHMEM: Dummy task/mm inserted (exec required): sender: %u, tgid: %u\n",
                        (unsigned int)nid, (unsigned int)DISAGG_KERN_TGID);
            }
            else
            {
                printf("KSHMEM: Cannot create dummy task_struct for kernel!\n");
            }
        }else{
            printf("KSHMEM: WARN - previous task found: sender: %u, tgid: %u\n",
                  (unsigned int)nid, (unsigned int)DISAGG_KERN_TGID);
        }
        task_spin_unlock();
    }
    else if (kshmem_va_start != va_start)
    {
        printf("\n\n**ERR: VA for kernel shared memory [cn: %d]: expected [0x%lx], received [0x%lx]\n\n",
                nid, kshmem_va_start, va_start);
    }else{
        printf("KSHMEM: VA detected [cn: %d]: [0x%lx]\n", nid, va_start);
    }
}

u64 kshmem_get_va_start(void)
{
    return kshmem_va_start;
}

unsigned long kshmem_alloc(int nid, unsigned long va, unsigned long len)
{
    unsigned long addr = -ENOMEM, tmp_addr = 0, _len;
    struct task_struct *kern_tsk;
    struct mm_struct *mm;

    // get task and mm structures
    task_spin_lock();
    kern_tsk = get_kern_tsk();
    task_spin_unlock();
    mm = kern_tsk ? kern_tsk->mm : NULL;

    if (len > DISAGG_KERN_SHMEM_SIZE || !kshmem_get_va_start() || !kern_tsk || !mm)
    {
        return -ENOMEM;
    }

    // main routine for allocation (simpler mmap)
    sem_wait(&mm->mmap_sem);
    if (len % CACHELINE_MAX_SIZE)
    {
        // make it cacheline aligned
        len = ((len / (unsigned long)CACHELINE_MAX_SIZE) + 1) * CACHELINE_MAX_SIZE;
    }
    if (!va)
    {
        // if va is not given, enforce power of 2 size
        len = get_pow_of_two_req_size(len);
    }
    _len = len;

    // get address
    if (va)
    {
        addr = mn_arch_kern_check_and_get_va(kern_tsk, va, len);
    } else {
        addr = mn_arch_kern_get_unmapped_area(kern_tsk, len * 2,    // ask for doubled size
                                            kshmem_get_va_start(),
                                            kshmem_get_va_start() + DISAGG_KERN_SHMEM_SIZE);
        addr = get_next_pow_of_two_addr(addr, len);
    }
    tmp_addr = addr;
    while (_len > 0)
    {
        unsigned long remain = min(_len, DISAGG_VMA_MAX_SIZE);
        tmp_addr = mn_mmap_region(kern_tsk, tmp_addr, remain,
                                  VM_WRITE | VM_READ, 0, NULL, 0, 0);
        if (IS_ERR_VALUE(tmp_addr))
        {
            addr = tmp_addr;
            goto disagg_mmap_out;
        }
        tmp_addr += remain;
        _len -= remain;
    }
#ifdef CACHE_DIR_PRE_OPT
	if (!IS_ERR_VALUE(addr))
	{
		if (mn_populate_cache(DISAGG_KERN_TGID, DISAGG_KERN_PID_NS_ID, DISAGG_KERN_TGID, addr, len, 
                              nid, 0, 0, (void*)NULL))	// try to populate cachelines
		{
			addr |= MMAP_CACHE_DIR_POPULATION_FLAG;
			usleep(1000); // 1 ms
			barrier();
		}
	}
#endif
disagg_mmap_out:
    sem_post(&mm->mmap_sem);
    return addr;
}

int kshmem_free(unsigned long va)
{
    return 0;
}

// Request handlers
static unsigned long kshmem_serv_base_addr[DISAGG_KSHMEM_SERV_MAX_NUM] = {0};
static int __handle_kshmem_alloc(struct mem_header *hdr, void *payload, struct kshmem_reply_struct *reply)
{
    struct kshmem_msg_struct *kmap_req = (struct kshmem_msg_struct *) payload;
    // struct task_struct *tsk = mn_get_task(hdr->sender_id, mmap_req->tgid);
    unsigned long addr = -ENOMEM;
    if (kmap_req->serv_id < DISAGG_KSHMEM_SERV_MAX_NUM)
        addr = kshmem_alloc(hdr->sender_id, 0, kmap_req->size);

    if (IS_ERR_VALUE(addr))
    {
        reply->ret = -1;
    }else{
        reply->ret = 0;
        addr &= PAGE_MASK;  // clear cache allocation flag
        // check if this is the first allocation
        if (!kshmem_serv_base_addr[kmap_req->serv_id])
            kshmem_serv_base_addr[kmap_req->serv_id] = addr;
    }
    reply->addr = addr;
    printf("KSHMEM_ALLOC: serv_id: %d, size: %lu, ret: %d, addr: 0x%lx | base: 0x%lx\n",
           kmap_req->serv_id, kmap_req->size, reply->ret, addr,
           kmap_req->serv_id < DISAGG_KSHMEM_SERV_MAX_NUM ?\
           kshmem_serv_base_addr[kmap_req->serv_id] : 0);
   return IS_ERR_VALUE(addr) ? -1 : 0;
}


int handle_kshmem_alloc(struct mem_header *hdr, void *payload, struct socket *sk, int id)
{
    struct kshmem_reply_struct reply;
    int ret = __handle_kshmem_alloc(hdr, payload, &reply);
    tcp_server_send(sk, id, (const char*)&reply, sizeof(reply), MSG_DONTWAIT);
    return ret;
}

static int __handle_kshmem_base_addr(struct mem_header *hdr, void *payload, struct kshmem_reply_struct *reply)
{
    struct kshmem_msg_struct *kmap_req = (struct kshmem_msg_struct *) payload;
    if (kmap_req->serv_id < DISAGG_KSHMEM_SERV_MAX_NUM)
    {
        reply->ret = 0;
        reply->addr = kshmem_serv_base_addr[kmap_req->serv_id];
    }else{
        reply->ret = -1;
        reply->addr = 0;
    }
    printf("KSHMEM_BASE: serv_id: %d, size: %lu, ret: %d, addr: 0x%lx\n",
           kmap_req->serv_id, kmap_req->size, reply->ret, reply->addr);
   return reply->ret;
}

int handle_kshmem_base_addr(struct mem_header *hdr, void *payload, struct socket *sk, int id)
{
    struct kshmem_reply_struct reply;
    int ret = __handle_kshmem_base_addr(hdr, payload, &reply);
    tcp_server_send(sk, id, (const char*)&reply, sizeof(reply), MSG_DONTWAIT);
    return ret;
}

static int __handle_kshmem_alloc_va(struct mem_header *hdr, void *payload, struct kshmem_reply_struct *reply)
{
    struct kshmem_va_msg_struct *kmap_req = (struct kshmem_va_msg_struct *) payload;
    // struct task_struct *tsk = mn_get_task(hdr->sender_id, mmap_req->tgid);
    unsigned long addr = -ENOMEM;
    if (kmap_req->serv_id < DISAGG_KSHMEM_SERV_MAX_NUM)
        addr = kshmem_alloc(hdr->sender_id, kmap_req->addr, kmap_req->size);

    if (IS_ERR_VALUE(addr))
    {
        reply->ret = -1;
    }else{
        reply->ret = 0;
        addr &= PAGE_MASK;  // clear cache allocation flag
        reply->addr = addr;
    }
    reply->addr = addr;
    printf("KSHMEM_ALLOC_WITH_VA: serv_id: %d, req_addr: 0x%lx, size: %lu | ret: %d, ret_addr: 0x%lx\n",
           kmap_req->serv_id, kmap_req->addr, kmap_req->size, reply->ret, addr);
   return IS_ERR_VALUE(addr) ? -1 : 0;
}

int handle_kshmem_alloc_va(struct mem_header *hdr, void *payload, struct socket *sk, int id)
{
    struct kshmem_reply_struct reply;
    int ret = __handle_kshmem_alloc_va(hdr, payload, &reply);
    tcp_server_send(sk, id, (const char*)&reply, sizeof(reply), MSG_DONTWAIT);
    return ret;
}
