/*
 * mm/rmem_disagg.c
 *
 * This file manages remote memory. It uses a primitive memory allocator to manage the remote
 * address space, and automatically updates the (CN VA) -> (MN VA) address mapping tables (which
 * are in `cnmap_disagg.h`
 */

#include <linux/kernel.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/printk.h>

#include <disagg/config.h>
#include <disagg/cnmap_disagg.h>
#include <disagg/rmem_disagg.h>
#include <disagg/print_disagg.h>
#include <disagg/profile_points_disagg.h>

#define MIND_RMEM_SIZE_B (MIND_RMEM_SIZE_MIB << 20)

struct rmem_mapping {
	struct list_head list;
	uint64_t cn_va; // Starting virtual address on the local VMA ( Compute Node)
	uint64_t mn_va; // Starting virtual address on the remote memory node.
	uint64_t size;  // Size of mapping segment.
	uint16_t tgid; // TODO: Many functions ignore this!
};

static DEFINE_MUTEX(rmem_map_lock);
// This list should remain sorted by mn_va.
static LIST_HEAD(rmem_maps);


#ifdef PRINT_RMEM_ALLOC
static void __maybe_unused print_rmaps(void)
{
	struct rmem_mapping *entry;
	mutex_lock(&rmem_map_lock);

	pr_info("rmap: Printing current rmaps:\n");
	list_for_each_entry(entry, &rmem_maps, list) {
		pr_info("rmap:\t(0x%llx - 0x%llx)->\t(0x%llx - 0x%llx) size=0x%llx\n",
				entry->cn_va, entry->cn_va + entry->size,
				entry->mn_va, entry->mn_va + entry->size, entry->size);
	}

	mutex_unlock(&rmem_map_lock);
}
#else
static void __maybe_unused print_rmaps(void) {}
#endif

static void init_cnmap_msg(struct rmem_mapping *rmem_map, struct mind_map_msg *cnmap)
{
	cnmap->valid = true;
	cnmap->va = rmem_map->cn_va;
	cnmap->mn_va = rmem_map->mn_va;
	cnmap->size = rmem_map->size;
	cnmap->tgid = rmem_map->tgid;
}

// Caller should hold rmem_table_lock.
// This function may sleep.
static void update_cnmap_layer(void)
{
	struct mind_map_msg *cnmaps = NULL;
	struct rmem_mapping *cur;
	size_t i = 0, rmem_table_size = 0;

	list_for_each_entry(cur, &rmem_maps, list)
		rmem_table_size++;

	cnmaps = kzalloc(sizeof(*cnmaps) * rmem_table_size, GFP_KERNEL);
	BUG_ON(!cnmaps);

	list_for_each_entry(cur, &rmem_maps, list) {
		struct mind_map_msg *cnmap = &cnmaps[i];
		init_cnmap_msg(cur, cnmap);
		i++;
	}

	set_cnmaps(cnmaps, rmem_table_size);
	kfree(cnmaps);
}


// The caller should hold rmem_table_lock.
static u64 find_free_rmem_chunk(size_t size)
{
	struct rmem_mapping *cur, *prev = NULL;
	const u64 lowest_allowed = PAGE_SIZE; // start default alloc at page 1

	if (list_empty(&rmem_maps))
		 return lowest_allowed; 

	cur = list_first_entry(&rmem_maps, struct rmem_mapping, list);

	// Try to allocate the new memory after the first page.
	if (cur->mn_va >= lowest_allowed + size) {
		pr_rmem_alloc("rmem alloc: shortcut path triggered!\n");
		 return lowest_allowed;
	}

	// Assume the list is sorted by mn_va (aka beginning of remote mem region)
	list_for_each_entry(cur, &rmem_maps, list) {
		u64 start;
		pr_rmem_alloc("cur:  0x%llx - 0x%llx\n", cur->mn_va, cur->mn_va + cur->size);

		// The first address we'd be allowed to start at...
		start = (prev == NULL) ? lowest_allowed : prev->mn_va + prev->size;
		if (start + size <= cur->mn_va) {
			pr_rmem_alloc("found empty slot at:  0x%llx - 0x%llx\n", start, start + size);
			return start;
		}

		prev = cur;
	}

	// Try to allocate memory immediately after our last alloc.
	if (prev->mn_va + prev->size + size < MIND_RMEM_SIZE_B) {
		 pr_rmem_alloc("rmem alloc: fallback path triggered!\n");
		 return prev->mn_va + prev->size;
	}

	pr_err("Couldn't find suitably sized memory chunk!\n");
	BUG();
}

// Caller should hold rmem_map_lock.
static void add_mapping(struct rmem_mapping *new)
{
	struct rmem_mapping *cur;

	if (list_empty(&rmem_maps)) {
		 list_add(&new->list, &rmem_maps);
		 return;
	}

	// Keep the allocation list in ascending order...
	list_for_each_entry(cur, &rmem_maps, list) {
		if (cur->mn_va > new->mn_va) {
			list_add_tail(&new->list, &cur->list);
			return;
		}
	}
	// If we got here, we're the largest element => append to list.
	list_add_tail(&new->list, &rmem_maps);
}

// Creates a new memory mapping. This function may sleep.
// TODO check for overlapping allocations, and destroy them.
int rmem_alloc(uint16_t tgid, u64 va, size_t size)
{
	struct rmem_mapping *new_map = kzalloc(sizeof(*new_map), GFP_KERNEL);
	struct mind_map_msg cnmap;
	BUG_ON(!new_map);

	new_map->tgid = tgid;
	new_map->cn_va = va;
	new_map->size = size;
	new_map->mn_va = find_free_rmem_chunk(size);
	INIT_LIST_HEAD(&new_map->list);

	BUG_ON(!PAGE_ALIGNED(new_map->cn_va));
	BUG_ON(!PAGE_ALIGNED(new_map->mn_va));
	BUG_ON(new_map->size % PAGE_SIZE != 0);

	mutex_lock(&rmem_map_lock);
	add_mapping(new_map);

	init_cnmap_msg(new_map, &cnmap); // used to update lower layer;
	add_one_cnmap(&cnmap); // update cnmap layer

	mutex_unlock(&rmem_map_lock);

	pr_rmem_alloc("Adding cnmapping (0x%llx (len %lu))!\n", va, size);
	print_cnmaps();
	print_rmaps();

	return 0;
}

// Split a mapping into two, smaller, mappings.
// Assumes:
// - The hole must be fully enclosed by the "map"
// - `map` should already be in the rmem_table.
// - Caller holds rmem_map_lock.
static void split_mapping(struct rmem_mapping *map, u64 hole_start, u64 hole_end)
{
	struct rmem_mapping old_map = *map;
	struct rmem_mapping *new_map_1 = map;
	struct rmem_mapping *new_map_2 = kzalloc(sizeof(*map), GFP_KERNEL);
	u64 offset;
	BUG_ON(!new_map_2);

	// First, shrink the first new mapping.
	new_map_1->size = hole_start - old_map.cn_va;

	// Then, shrink the second new mapping. This is tricker, we need to push the start up.
	offset = hole_end - old_map.cn_va;
	new_map_2->cn_va += offset;
	new_map_2->mn_va += offset;
	new_map_2->size -= offset;

	// Add second map back into list, after the first map (which is already there). 
	list_add(&new_map_2->list, &new_map_1->list);
}


// Given a `mapping`, free a "hole" in that mapping. So if our mapping is
// (0, 100) and we call (punch_hole_in_mapping(map, 20, 30)), our final map table will read
// [0, 20) and [30, 100).
//
// This function assumes:
// - `map` should already be in the rmem_table.
// - The caller holds rmem_map_lock.
static void punch_hole_in_mapping(struct rmem_mapping *map, u64 hole_start, u64 hole_end)
{
	u64 start = map->cn_va;
	u64 end = map->cn_va + map->size;

	// Skip non-overlapping intervals.
	if (hole_start >= end || hole_end <= start)
		 return;

	// If this alloc is a subinterval of our dealloc, destroy it.
	if (hole_start <= start && end <= hole_end) {
		list_del(&map->list);
		kfree(map);
		return;
	}

	// Split containing allocs in two.
	if (hole_start >= start && hole_end <= end) {
		split_mapping(map, hole_start, hole_end);
		return;
	}

	// Shrink overlapping allocations.
	if (hole_start < end) {
		map->size = hole_start - map->cn_va;
		return;
	}
	if (hole_end > start) {
		u64 offset = hole_end - start;
		map->cn_va += offset;
		map->mn_va += offset;
		map->size -= offset;
	}
	WARN_ON_ONCE(true);
}

// Free the specified region of remote memory. This function "punches a hole" in existing mappings;
// so if we have mapping [0, 100) and call free(tgid, 50, 10), our resulting cnmap table will be:
// [0, 50), [60, 100).
int rmem_free(u16 tgid, u64 va, size_t size)
{
	struct rmem_mapping *cur, *tmp;
	u64 hole_start = va;
	u64 hole_end = va + size;

	mutex_lock(&rmem_map_lock);

	list_for_each_entry_safe(cur, tmp, &rmem_maps, list) {
		if (cur->tgid != tgid)
			 continue;

		punch_hole_in_mapping(cur, hole_start, hole_end);
	}

	update_cnmap_layer();
	mutex_unlock(&rmem_map_lock);

	pr_rmem_alloc("Removing cnmapping (0x%llx (len 0x%lx)!\n", va, size);
	print_cnmaps();

	return 0;
}

int rmem_free_tgid(uint16_t tgid)
{
	struct rmem_mapping *cur, *tmp;
	mutex_lock(&rmem_map_lock);

	list_for_each_entry_safe(cur, tmp, &rmem_maps, list) {
		if (cur->tgid != tgid)
			 continue;
		list_del(&cur->list);
		kfree(cur);
	}

	update_cnmap_layer();
	mutex_unlock(&rmem_map_lock);
	return 0;
}

int rmem_free_all(void)
{
	struct rmem_mapping *cur, *tmp;
	mutex_lock(&rmem_map_lock);

	list_for_each_entry_safe(cur, tmp, &rmem_maps, list) {
		list_del(&cur->list);
		kfree(cur);
	}

	update_cnmap_layer();
	mutex_unlock(&rmem_map_lock);
	return 0;
}

/* vim: set tw=99 ts=8 sw=8 noexpandtab */
