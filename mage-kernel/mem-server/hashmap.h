#ifndef RDMA_HASHMAP_H
#define RDMA_HASHMAP_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Assuming struct fault_task is defined elsewhere
typedef struct hashmap_element hashmap_element_t;
struct hashmap_element {
    uint64_t key;
    void *data;
    hashmap_element_t *next;
};

typedef struct hashmap {
    hashmap_element_t **buckets;
    size_t size;
} hashmap_t;

// Function prototypes
void init_hashmap(hashmap_t *map, size_t size);
void free_hashmap(hashmap_t *map);
size_t hash_function(uint64_t key, size_t size);
int hmap_push_task(hashmap_t *map, uint64_t key, void *data);
void *hmap_pop_task(hashmap_t *map, uint64_t key);

#endif