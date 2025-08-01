#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "rdma_common.h"
#include "rdma_client.h"

// Assuming necessary includes for constants and global variable declarations

void test_allocate_and_free_buffer() {
    unsigned long initial_size = PAGE_SIZE * 10; // Request 10 pages
    void* allocated_buffer = allocate_buffer(initial_size);
    assert(allocated_buffer != NULL);
    assert(buffer_size == PAGE_SIZE * 10);
    free_buffer();
    assert(buffer == NULL);
    assert(alloc_array == NULL);
}

void test_get_and_release_buffer() {
    allocate_buffer(PAGE_SIZE * 100); // 100 pages

    // Get a free buffer and verify
    uint32_t buffer_idx = get_free_buffer();
    assert(buffer_idx != (uint32_t)-1);

    void* buffer_ptr = get_buffer(buffer_idx);
    assert(buffer_ptr != NULL);

    // Release and re-acquire the same buffer
    release_buffer(buffer_idx);
    uint32_t buffer_idx_reacquired = get_free_buffer();
    assert(buffer_idx == buffer_idx_reacquired);

    free_buffer();
}

void test_buffer_overflow() {
    allocate_buffer(PAGE_SIZE * 2); // Small buffer for testing

    // Attempt to get more buffers than available should fail
    assert(get_free_buffer() != (uint32_t)-1);
    assert(get_free_buffer() != (uint32_t)-1);
    assert(get_free_buffer() == (uint32_t)-1); // Should fail here

    free_buffer();
}

void test_buffer_boundaries() {
    allocate_buffer(PAGE_SIZE * 1); // Single page for boundary test

    get_free_buffer();
    void* buffer_ptr = get_buffer(0);
    assert(buffer_ptr != NULL);

    release_buffer(0);

    // Trying to access beyond the allocated buffer should fail assertions
    // get_buffer(1); // Uncomment to test boundary check - should fail assertion

    free_buffer();
}

int main() {
    test_allocate_and_free_buffer();
    test_get_and_release_buffer();
    test_buffer_overflow();
    test_buffer_boundaries();

    printf("All buffer tests passed.\n");
    return 0;
}
