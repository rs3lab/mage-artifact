#include "controller.h"
#include "memory_management.h"
#include "request_handler.h"
#include "rbtree_ftns.h"
#include "cacheline_manager.h"
#include "cache_manager_thread.h"
#include "cacheline_def.h"
#include <time.h>
#include <sys/time.h>

u64 DEBUG_count_anon_vma(struct mm_struct *mm)
{
    u64 total_size = 0;
    struct vm_area_struct *cur = mm->mmap;
    for (; cur; cur = cur->vm_next)
    {
        // Print only allocated VMAs
        if (cur->vm_private_data)
            total_size += (cur->vm_end - cur->vm_start);
        if (cur->vm_next == cur)
        {
            fprintf(stderr, "ERROR: vm_next pointing to itself: 0x%lx - 0x%lx\n",
                    cur->vm_start, cur->vm_end);
        }
    }
    return total_size;
}

void DEBUG_print_one_vma(struct vm_area_struct *cur, int i)
{
    struct vm_area_struct *ln, *rn;
    (void)ln;
    (void)rn;
    (void)i;
    ln = cur->vm_rb.rb_left ? cur->vm_rb.rb_left->vma : 0;
    rn = cur->vm_rb.rb_right ? cur->vm_rb.rb_right->vma : 0;
    pr_vma("  *[%d, %p]: addr: 0x%lx - 0x%lx [0x%lx], alloc: %d, pgoff: 0x%lx, f: %d, l: %p, r: %p\n",
           i, (void *)cur, cur->vm_start, cur->vm_end, cur->vm_flags,
           cur->vm_private_data ? 1 : 0, // if allocated, then 1
           cur->vm_pgoff,
           cur->vm_file ? 1 : 0,
           (void *)ln, (void *)rn);
}

void DEBUG_print_vma(struct mm_struct *mm)
{
    int i = 0;
    struct vm_area_struct *cur = mm->mmap;
    for (; cur; cur = cur->vm_next)
    {
        // Print only allocated VMAs
        if (cur->vm_private_data)
            DEBUG_print_one_vma(cur, i);
        i++;
    }
}

void DEBUG_print_exec_vma(struct exec_msg_struct *exec_req)
{
    int i = 0;
    struct exec_vmainfo *exec;
    for (i = 0; i < (int)exec_req->num_vma; i++)
    {
        exec = &((&exec_req->vmainfos)[i]);
        printf("  *[%d, %p]: addr: 0x%lx - 0x%lx [%lx] (f:%d)\n",
               i, (void*)exec, exec->vm_start, exec->vm_end, exec->vm_flags,
               exec->file_id ? 1 : 0);
    }
}

void DEBUG_print_vma_diff(struct mm_struct *mm, struct exec_msg_struct *exec_req)
{
    int i = 0;
    struct vm_area_struct *prev, *vma = mm->mmap;
    struct exec_vmainfo *exec;
    unsigned long prev_end;
    unsigned long mn_only = 0, cn_only = 0;

    sem_wait(&mm->mmap_sem);

    prev = NULL;
    printf("  # of VMAs (tgid: %d, pid: %d): CN[%d]\n",
           (int)exec_req->tgid, (int)exec_req->pid, (int)exec_req->num_vma);

    for (i = 0; i < (int)exec_req->num_vma; i++)
    {
        exec = &((&exec_req->vmainfos)[i]);
        prev_end = ((i == 0) ? 0 : (&exec_req->vmainfos)[i - 1].vm_end);
        while (vma && (vma->vm_end <= exec->vm_start))
        {
            // All vma in between (prev_end) to (exec->vm_start) is mn only
            if (prev_end < vma->vm_end)
            {
                // printf("  *[%d]: addr: 0x%lx - 0x%lx (mn only)\n",
                // 		i, max(prev_end, vma->vm_start), vma->vm_end);
                mn_only += vma->vm_end - max(prev_end, vma->vm_start);
            }
            prev = vma;
            vma = vma->vm_next;
        }

        // Now, vma->vm_end >= exec->vm_start
        // Any remaining part of vma < exec->vm_start
        if (vma && vma->vm_start < exec->vm_start)
        {
            if (prev_end < exec->vm_start)
            {
                // printf("  *[%d]: addr: 0x%lx - 0x%lx (mn only)\n",
                // 		i, max(prev_end, vma->vm_start), exec->vm_start);
                mn_only += exec->vm_start - max(prev_end, vma->vm_start);
            }
            if (vma->vm_end <= exec->vm_end)
            {
                prev = vma;
                vma = vma->vm_next;
            }
        }

        // Exec regions in between (vma->vm_prev->vm_end) to (vma->vm_start)
        while (vma && (vma->vm_start >= exec->vm_start && vma->vm_start < exec->vm_end))
        {
            if (vma->vm_start > exec->vm_start)
            {
                if (!prev || (vma->vm_start > prev->vm_end))
                {
                    if (!exec->file_id)
                    {
                        printf("  *[%d]: addr: 0x%lx - 0x%lx f:%d (cn only)\n",
                               i, max(prev ? prev->vm_end : 0, exec->vm_start), vma->vm_start,
                               (exec->file_id ? 1 : 0));
                    }
                    cn_only += vma->vm_start - max(prev ? prev->vm_end : 0, exec->vm_start);
                }
            }
            if (vma->vm_end <= exec->vm_end)
            {
                prev = vma;
                vma = vma->vm_next;
            }
            else
                break;
        }

        // Now, vma->vm_start >= exec->vm_end OR vma == NULL
        //   OR (vma->vm_start < exec->vm_start && vma->vm_end >= exec->vm_end)
        // Any remaining part of vma->prev->vm_end < exec->vm_end
        if ((prev ? prev->vm_end : 0) < exec->vm_end)
        {
            if (!vma || (vma->vm_start > exec->vm_start))
            {
                if (!exec->file_id)
                {
                    printf("  *[%d]: addr: 0x%lx - 0x%lx f:%d (cn only)\n",
                           i, max(prev ? prev->vm_end : 0, exec->vm_start), exec->vm_end,
                           (exec->file_id ? 1 : 0));
                }
                cn_only += exec->vm_end - max(prev ? prev->vm_end : 0, exec->vm_start);
            }
        }
    }

    prev_end = (i > 0) ? ((&exec_req->vmainfos)[i - 1]).vm_end : 0;

    // Remaining vmas
    while (vma)
    {
        if (vma->vm_end > prev_end)
        {
            // printf("  *[%d]: addr: 0x%lx - 0x%lx (mn only)\n",
            // 		i, max(vma->vm_start, prev_end), vma->vm_end);
            mn_only += vma->vm_end - max(vma->vm_start, prev_end);
        }
        prev = vma;
        vma = vma->vm_next;
    }

    sem_post(&mm->mmap_sem);

    // Finally, print summary
    printf("  *cn only: %lu <-> mn only: %lu\n", cn_only, mn_only);
}

void get_timestamp(char *buf, unsigned int max_size)
{
    struct timeval t;
    struct tm *broken;

    if (max_size < 32 || !buf)
        return;

    memset(buf, 0, max_size);
    gettimeofday(&t, NULL);
    broken = localtime(&t.tv_sec);
    sprintf(buf, "%02d:%02d:%02d:%06ld",
            broken->tm_hour, broken->tm_min,
            broken->tm_sec, t.tv_usec);
}
