#include "hashmap.h"
#include <assert.h>
#include <string.h>

void test_init_and_free_hashmap() {
    hashmap_t map;
    init_hashmap(&map, 10);
    assert(map.size == 10);
    assert(map.buckets != NULL);
    free_hashmap(&map);
    assert(map.buckets == NULL);
}

void test_push_and_hmap_pop_task() {
    hashmap_t map;
    init_hashmap(&map, 10);

    char data1[] = "data1";
    char data2[] = "data2";

    // Push task
    hmap_push_task(&map, 1, data1);
    hmap_push_task(&map, 2, data2);

    // Pop and verify
    char* result1 = (char*)hmap_pop_task(&map, 1);
    assert(result1 != NULL);
    assert(strcmp(result1, data1) == 0);

    char* result2 = (char*)hmap_pop_task(&map, 2);
    assert(result2 != NULL);
    assert(strcmp(result2, data2) == 0);

    // Verify pop non-existing key
    assert(hmap_pop_task(&map, 3) == NULL);

    free_hashmap(&map);
}

void test_collision_and_chain() {
    hashmap_t map;
    init_hashmap(&map, 10);

    // Push tasks with colliding keys
    char data1[] = "data1";
    char data2[] = "data2";
    hmap_push_task(&map, 1, data1);
    hmap_push_task(&map, 11, data2); // This should collide with key 1 in a size 10 map

    // Pop and verify
    assert(strcmp((char*)hmap_pop_task(&map, 1), data1) == 0);
    assert(strcmp((char*)hmap_pop_task(&map, 11), data2) == 0);

    free_hashmap(&map);
}

void test_push_existing_key() {
    hashmap_t map;
    init_hashmap(&map, 10);

    char data1[] = "data1";
    int res = hmap_push_task(&map, 1, data1);
    assert(res == 0);
    res = hmap_push_task(&map, 1, data1);
    assert(res != 0);
    free_hashmap(&map);
}

int main() {
    test_init_and_free_hashmap();
    test_push_and_hmap_pop_task();
    test_collision_and_chain();
    test_push_existing_key();

    printf("All tests passed.\n");
    return 0;
}
