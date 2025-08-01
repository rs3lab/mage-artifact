/*
 * mm/mind_map_disagg.c
 *
 * This file manages the MIND (CN VA) -> (MN VA) address mapping table.
 */

#include <linux/kernel.h>
#include <linux/mm_types.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/rwlock.h>
#include <linux/printk.h>
#include <linux/interval_tree_generic.h>
#include <linux/types.h>

#include <disagg/config.h>
#include <disagg/cnmap_disagg.h>
#include <disagg/print_disagg.h>
#include <disagg/profile_points_disagg.h>

struct cnmap_entry {
	u64 va; // aka "addr"
	u64 mn_va; // aka "mind addr"
	u64 size;
	uint16_t tgid;
	struct rb_node rb;
	u64 __last; // used in rbtree
	struct list_head list;
};

static u64 get_cnmap_entry_start(struct cnmap_entry *e)
{
	return e->va;
}

// Remember, this is inclusive.
static u64 get_cnmap_entry_last(struct cnmap_entry *e)
{
	return e->va + e->size - 1;
}

INTERVAL_TREE_DEFINE(struct cnmap_entry, rb, uint64_t, __last,
		get_cnmap_entry_start, get_cnmap_entry_last, static, cnmap_rb);

// Guards: cnmap table, cnmap table size, and mn_base_addr.
static DEFINE_RWLOCK(cnmap_table_lock);

static LIST_HEAD(cnmap_table);
static size_t cnmap_table_size;
// For quick lookup.
struct rb_root_cached cnmap_rb_root = RB_ROOT_CACHED;

// This function may sleep.
void set_cnmaps(struct mind_map_msg *maps, size_t size)
{
	size_t i;
	struct cnmap_entry *cur, *tmp;
	struct list_head new_cnmaps;

	// pr_maps("Receiving new cnmaps:\n");

	// Pre-allocate new mappings.
	INIT_LIST_HEAD(&new_cnmaps);
	for (i = 0; i < size; i++) {
		struct cnmap_entry *new = kmalloc(size * sizeof(*new), GFP_KERNEL);
		INIT_LIST_HEAD(&new->list);
		list_add(&new->list, &new_cnmaps);
	}

	write_lock(&cnmap_table_lock);

	// Clear the existing map table.
	list_for_each_entry_safe(cur, tmp, &cnmap_table, list) {
		cnmap_rb_remove(cur, &cnmap_rb_root);
		list_del(&cur->list);
		kfree(cur);
	}
	cnmap_table_size = 0;

	// Import new mappings.
	for (i = 0; i < size; i++) {
		struct mind_map_msg *map = &maps[i];
		struct cnmap_entry *new_cnmap =
			list_first_entry(&new_cnmaps, struct cnmap_entry, list);

		if (!map->valid) {
			 continue;
			 // pr_maps("\tmaps: Skipping invalid entry\n");
		}

		// pr_maps("\tcopying cnmap: va=0x%llx, mn_va=0x%llx, size=%llu, tgid=0x%hu\n",
		// 		map->va, map->mn_va, map->size, map->tgid);
		new_cnmap->va = map->va;
		new_cnmap->mn_va = map->mn_va;
		new_cnmap->size = map->size;
		new_cnmap->tgid = map->tgid;

		list_move_tail(&new_cnmap->list, &cnmap_table);
		cnmap_rb_insert(new_cnmap, &cnmap_rb_root);
		cnmap_table_size++;
	}

	if (cnmap_table_size == 0) {
		pr_warn("WARNING: reset cnmaps to size zero!\n");
		dump_stack();
	}

	write_unlock(&cnmap_table_lock);

	// If we preallocated too many entries, free them.
	list_for_each_entry_safe(cur, tmp, &new_cnmaps, list) {
		list_del(&cur->list);
		kfree(cur);
	}
}

void read_lock_cnmap_table(void)
{
	read_lock(&cnmap_table_lock);
}

void read_unlock_cnmap_table(void)
{
	read_unlock(&cnmap_table_lock);
}

// The compiler complains when cnmap_rb_iter_next isn't used :P
static void __maybe_unused __dummy_call_cnmap_rb_iter_next(void)
{
	struct cnmap_entry *entry;
	entry = cnmap_rb_iter_first(&cnmap_rb_root, 0, 1);
	entry = cnmap_rb_iter_next(entry, 0, 1);
}

// Translate the virtual address (provided) to an RDMAable address.
// The CN will use this to RDMA request the MN directly.
uint64_t __get_cnmapped_addr(unsigned long addr)
{
	struct cnmap_entry *entry = NULL;
	unsigned long last_addr = addr + PAGE_SIZE - 1;
	BUILD_BUG_ON(sizeof(unsigned long) != sizeof(uint64_t));

	// Theoretically, there should be only one RB tree entry with this mapping.
	entry = cnmap_rb_iter_first(&cnmap_rb_root, addr, last_addr);
	if (unlikely(!entry)) {
		pr_err("cnmap: invalid address translation 0x%lx!\n", addr);
		BUG();
	}

	// MN-specific base address offset is handled by the RoCE module.
	return entry->mn_va + (addr - entry->va);
}

// Translate the virtual address (provided) to an RDMAable address.
// The CN will use this to RDMA request the MN directly.
uint64_t get_cnmapped_addr(unsigned long addr)
{
	uint64_t ret;
	read_lock(&cnmap_table_lock);
	ret = __get_cnmapped_addr(addr);
	read_unlock(&cnmap_table_lock);
	return ret;
}

#ifdef PRINT_MAPS
void print_cnmaps(void)
{
	size_t i = 0;
	struct cnmap_table_entry *entry;

	read_lock(&cnmap_tree_lock);

	pr_info("cnmap: Printing current cnmap table (size %zu):\n", cnmap_table_size);
	list_for_each_entry(entry, &cnmap_table, list) {
		pr_info("\tcnmap[%zu]: va=0x%llx, mn_va=0x%llx, size=%llu, tgid=0x%u\n",
				i, entry->va, entry->mn_va, entry->size, entry->tgid);
		i++;
	}

	read_unlock(&cnmap_tree_lock);
}
#else
void print_cnmaps(void) {}
#endif

/* vim: set tw=99 ts=8 sw=8 noexpandtab */
