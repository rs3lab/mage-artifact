#include "pid_ns.h"
#include "list_and_hash.h"
#include <stdio.h>
#include "task_management.h"
#include "memory_management.h"
#include <stdlib.h>

static struct hash_table tgidns_table;
u16 max_pid_ns_id = 0; // FIRST FEW PID_NS_ID is reserved for kernel stuff
struct pid_namespace *pid_namespace_id_to_struct[MAX_NUMBER_PID_NS + 1]  = {NULL};
u16 next_tgid = 0xffff - 1;

u32 generate_nstgid(u16 tgid, u16 pid_ns_id)
{
    u32 res = (pid_ns_id << 16);
    return (res | (u32)tgid);
}

struct pid_namespace *get_pid_ns_ptr(struct task_struct *t) 
{
    return t->nsproxy->pid_ns_for_children;
}

u16 get_tgid_in_ns(u16 tgid, u16 pid_ns_id) {

    u32 tgid_ns_id_combined = generate_nstgid(tgid, pid_ns_id);
    struct upid *upid = (struct upid *)ht_get(&tgidns_table, hash_ftn_u16(tgid_ns_id_combined), tgid_ns_id_combined);
    printf("get_tgid_in_ns: upid->nr: %u, tgid: %u, pid_ns_id: %u\n", upid->nr, tgid, pid_ns_id);
    if (!upid)
        printf("upid not found!!!");
    return upid->nr;
}

/** Initialize the pid namespace id to tgid mapping */
void pid_ns_init()
{
    ht_create(&tgidns_table, 1 << 16);      // hash table size
    printf("pid_ns.c: starting pid_ns_id %d, %d\n", START_PID_NS_ID, MAX_NUMBER_PID_NS);

    pid_namespace_id_to_struct[START_PID_NS_ID] = (struct pid_namespace *) malloc(sizeof(struct pid_namespace));
    pid_namespace_id_to_struct[START_PID_NS_ID]->level = 0; // ZIMING: TODO
    pid_namespace_id_to_struct[START_PID_NS_ID]->parent = NULL; // ZIMING: TODO
    pid_namespace_id_to_struct[START_PID_NS_ID]->max_nr = START_TGID_NS; // ZIMING: TODO
}

/** Get new tgid for the pid namespace */
u16 get_new_tgid(u16 sender_id, u16 pid_ns_id)
{
    u16 return_tgid = next_tgid;
    next_tgid = next_tgid - 1;

    printf("pid_ns.c: get new tgid for pid_ns_id %d: %d\n", pid_ns_id, return_tgid);
    put_tgid_to_ns_table(return_tgid, pid_ns_id);
    return return_tgid;
}

struct pid_namespace *get_pid_ns_from_id(u16 pid_ns_id) {
    return pid_namespace_id_to_struct[pid_ns_id];
}

/** Create new pid ns and return the id of the new ns */
u16 create_new_pid_ns(u16 parent_pid_ns_id) 
{
    max_pid_ns_id += 1;
    
    printf("pid_ns.c: parent pid_ns level: %d, new_pid_ns_id level: %d\n", get_pid_ns_from_id(parent_pid_ns_id)->level, get_pid_ns_from_id(parent_pid_ns_id)->level + 1);

    pid_namespace_id_to_struct[max_pid_ns_id] = (struct pid_namespace *) malloc(sizeof(struct pid_namespace));
    get_pid_ns_from_id(max_pid_ns_id)->level = get_pid_ns_from_id(parent_pid_ns_id)->level + 1; // ZIMING: TODO
    get_pid_ns_from_id(max_pid_ns_id)->parent = get_pid_ns_from_id(parent_pid_ns_id); // ZIMING: TODO
    get_pid_ns_from_id(max_pid_ns_id)->max_nr = START_TGID_NS; // ZIMING: TODO

    printf("pid_ns.c: max_pid_ns_id: %d\n", max_pid_ns_id);

    return max_pid_ns_id;
}


// Remember, PIDs are namespaced too in the kernel. This function helps
// maintain that mapping.
int put_tgid_to_ns_table(u16 tgid, u16 ns_id)
{
    u32 tgid_ns_id_combined = generate_nstgid(tgid, ns_id);
    // printf("pid_ns.c: Putting %d to hash table, ns_id: %d, tgid: %d\n", tgid_ns_id_combined, ns_id, tgid);
    // Need dummy value so that we can later check for null

    struct upid *upid = (struct upid *) malloc(sizeof(struct upid));
    get_pid_ns_from_id(ns_id)->max_nr += 1;
    upid->nr = get_pid_ns_from_id(ns_id)->max_nr;
    upid->pid_ns = get_pid_ns_from_id(ns_id);

    return ht_put(&tgidns_table, hash_ftn_u16(tgid_ns_id_combined), tgid_ns_id_combined, upid);
}

int remove_tgid_from_ns_table(u16 tgid, u16 ns_id)
{
    u32 tgid_ns_id_combined = generate_nstgid(tgid, ns_id);
    // printf("pid_ns.c: Removing %u to hash table, ns_id: %d, tgid: %u\n", tgid_ns_id_combined, ns_id, tgid);
    hlist *node = ht_get_node(&tgidns_table, hash_ftn_u16(tgid_ns_id_combined), tgid_ns_id_combined);
    if (node == NULL)
    {
        printf("pid_ns.c: couldn't remove %d\n", tgid);
        return -1;
    }
    ht_free_node(&tgidns_table, hash_ftn_u16(tgid_ns_id_combined), node);
    return 0;
}

// Check if the hash table contains given tgid 
int contains_tgid_ns(u16 tgid, u16 ns_id)
{
    u32 tgid_ns_id_combined = generate_nstgid(tgid, ns_id);
    // printf("pid_ns.c: Checking %d to hash table, ns_id: %d, tgid: %d\n", tgid_ns_id_combined, ns_id, tgid);
    int res = ht_get(&tgidns_table, hash_ftn_u16(tgid_ns_id_combined), tgid_ns_id_combined) != NULL;
    return res;
}
