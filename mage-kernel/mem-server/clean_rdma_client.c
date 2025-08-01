#include "rdma_client.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define PAGE_SIZE (1 << 12)

// Minimal RDMA client to test the server
int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <batch size>\n", argv[0]);
		return 1;
	}
	int batch_size = atoi(argv[1]);

	// INIT PHASE

	int buf_size = MIND_RDMA_BUFFER_SIZE;
	char *buf = calloc(buf_size, sizeof(*buf));
	if (!buf) {
		perror("Buf out of memory!");
		return 1;
	}
	uint64_t server_addr = client_init(buf, buf_size, "10.10.10.206", MIND_DEFAULT_CONTROL_PORT);

	// TESTING PHASE

	buf[0] = 'Z';
	write_page(0, 0, server_addr);
	buf[0] = '0';
	read_page(0, 0, server_addr);
	if (buf[0] != 'Z') {
		printf("Value test fail!\n");
		return 1;
	} else {
		 printf("Value matched test pass!\n");
	}

	// BENCH PHASE

    struct timespec start_time, current_time;
    unsigned long long iteration_count = 0;

    clock_gettime(CLOCK_MONOTONIC, &start_time);


    do {
		for (int i=0; i < batch_size; i++) {
			read_page_async(
					i % MIND_RDMA_NUM_QP,
					i % (MIND_RDMA_BUFFER_SIZE / PAGE_SIZE),
					server_addr + (i * PAGE_SIZE));
		}

		for (int i=0; i < batch_size; i++) {
			while (try_check_cq(i % MIND_RDMA_NUM_QP) != -1) {
				;
			}
		}

        iteration_count++;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
    } while ((current_time.tv_sec - start_time.tv_sec) < 10 ||
             (current_time.tv_nsec - start_time.tv_nsec) < 0);

    // Print the number of iterations
    printf("Number of iterations in 10 seconds: %llu\n", iteration_count);

	// BENCHMARK PHASE

	client_disconnect();
	free(buf);
	return 0;
}
