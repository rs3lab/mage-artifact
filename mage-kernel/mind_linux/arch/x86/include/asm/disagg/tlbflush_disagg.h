#ifndef _ASM_X86_TLBFLUSH_DISAGG_H
#define _ASM_X86_TLBFLUSH_DISAGG_H

#include <linux/mm.h>
#include <linux/hashtable.h>

#define TLB_NR_MIND_ASID_START	8
#define TLB_NR_MIND_ASID_END	2040

// struct tlb_asid_disagg {
//     struct hlist_node hnode;
// 	struct mm_struct *mm;
// 	u16 asid;
// };  // 16 + 8 + 2 B

u16 find_next_avail_disagg_asid(struct mm_struct *mm);
void put_disagg_asid(u16 old_asid);

#endif