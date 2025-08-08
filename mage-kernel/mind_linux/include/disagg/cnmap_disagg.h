#ifndef __MIND_MAP_DISAGGREGATION_H__
#define __MIND_MAP_DISAGGREGATION_H__

#include <linux/sched.h>

// NOTE !!!!!
//
// If you change this file, change it in the control plane too!
//
// NOTE !!!!!

#ifndef __packed
#define __packed __attribute__((packed))
#endif

// NOTE: If you change this macro, change it in the CN too!
#define MAX_MAPS_IN_REPLY 128
// NOTE: If you change this struct, change it in the CN too!
struct mind_map_msg { 
    bool valid;
    uint64_t va;
    uint64_t mn_va;
    uint64_t size;
    uint16_t tgid;
} __packed;

void set_cnmaps(struct mind_map_msg *maps, size_t size);
void add_one_cnmap(struct mind_map_msg *map);
uint64_t get_cnmapped_addr(unsigned long addr);
void read_lock_cnmap_table(void);
void read_unlock_cnmap_table(void);
void write_lock_cnmap_table(void);
void write_unlock_cnmap_table(void);
uint64_t __get_cnmapped_addr(unsigned long addr);
void print_cnmaps(void);
void clear_cnmaps(void);

#endif /* __MIND_MAP_DISAGGREGATION_H__ */
