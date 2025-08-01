#include "hashmap.h"
#include <pthread.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

// add a lock to protect the hashmap
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void init_hashmap(hashmap_t *map, size_t size) {
    map->size = size;
    map->buckets = (hashmap_element_t **)calloc(size, sizeof(hashmap_element_t *)); // Allocate and zero-initialize an array of element pointers
    if (!map->buckets) {
        perror("Failed to allocate hashmap buckets");
        // TODO: graceful retry
        exit(EXIT_FAILURE);
    }
}

// @return 0 if the map exists and is locked, -1 otherwise
static int check_map_unlock(hashmap_t *map) {
    if (!map || !map->buckets)
    {
        pthread_mutex_unlock(&lock);
        return -1;
    }
    return 0;
}

void free_hashmap(hashmap_t *map) {
    pthread_mutex_lock(&lock);
    if (check_map_unlock(map))
    {
        return;
    }

    for (size_t i = 0; i < map->size; ++i) {
        hashmap_element_t *element = map->buckets[i];
        while (element) {
            hashmap_element_t *tmp = element;
            element = element->next;
            free(tmp);
        }
    }

    free(map->buckets);
    map->buckets = NULL;
    map->size = 0;
    pthread_mutex_unlock(&lock);
}

size_t hash_function(uint64_t key, size_t size) {
    return (key / PAGE_SIZE) % size;
}

static hashmap_element_t *find_element(hashmap_t *map, uint64_t key) {
    size_t index = hash_function(key, map->size);
    hashmap_element_t *element = map->buckets[index];
    while (element) {
        if (element->key == key) {
            return element;
        }
        element = element->next;
    }
    return NULL; // Not found
}

int hmap_push_task(hashmap_t *map, uint64_t key, void *data) {
    pthread_mutex_lock(&lock);
    if (check_map_unlock(map))
    {
        return -1;
    }
    hashmap_element_t *existing_element = find_element(map, key);
    if (existing_element) {
        perror("Key already exists in hashmap\n");
        pthread_mutex_unlock(&lock);
        return -1;
    } else {
        // Key does not exist, insert new element at the beginning
        size_t index = hash_function(key, map->size);
        hashmap_element_t *new_element = (hashmap_element_t *)malloc(sizeof(hashmap_element_t));
        if (!new_element) {
            perror("Failed to allocate new hashmap element\n");
            pthread_mutex_unlock(&lock);
            return -1;
        }
        new_element->key = key;
        new_element->data = data;
        new_element->next = map->buckets[index];
        map->buckets[index] = new_element;
    }
    pthread_mutex_unlock(&lock);
    return 0;
}

void *hmap_pop_task(hashmap_t *map, uint64_t key) {
    size_t index = hash_function(key, map->size);
    pthread_mutex_lock(&lock);
    if (check_map_unlock(map))
    {
        return NULL;
    }

    hashmap_element_t *element = map->buckets[index];
    hashmap_element_t *prev = NULL;
    while (element) {
        if (element->key == key) {
            void *found_data = element->data;
            // Update the next pointer of the previous element
            if (prev) {
                prev->next = element->next;
            } else {
                // This is the first element in the bucket
                map->buckets[index] = element->next;
            }
            free(element);
            pthread_mutex_unlock(&lock);
            return found_data;
        }
        prev = element;
        element = element->next;
    }
    pthread_mutex_unlock(&lock);
    return NULL; // Not found
}
