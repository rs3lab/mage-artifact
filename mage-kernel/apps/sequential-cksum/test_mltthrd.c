#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>

// Define the array size and a global array
size_t array_size;
uint8_t *data;

pthread_mutex_t bench_start_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  bench_start_cond  = PTHREAD_COND_INITIALIZER;
bool            bench_start       = false;

pthread_mutex_t finished_threads_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  finished_threads_cond  = PTHREAD_COND_INITIALIZER;
int             finished_threads       = 0;

pthread_mutex_t bench_end_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  bench_end_cond  = PTHREAD_COND_INITIALIZER;
bool            bench_end       = false;

// Give N seconds for thread activity to reach a steady state before
// benchmarking the system.
const int warmup_duration = 30;
const int benchmark_duration = 30;
const int heartbeat_secs = 1;

struct worker_args {
    int thread_id; // 1 indexed
    int num_workers;
    bool thread_leader;
};

static struct timespec diff_timespec(struct timespec *t1, struct timespec *t0)
{
    assert(t1);
    assert(t0);
    struct timespec diff = {
        .tv_sec = t1->tv_sec - t0->tv_sec,
        .tv_nsec = t1->tv_nsec - t0->tv_nsec
    };

    if (diff.tv_nsec < 0) {
        diff.tv_nsec += 1000000000;
        diff.tv_sec--;
    }
    return diff;
}

static uint8_t frobnicate(size_t index, uint64_t iteration)
{
    return (index ^ 'f') + iteration;
}

static void workload_function(struct worker_args *args)
{
    int thread_id = args->thread_id;
    int num_threads = args->num_workers;

    // Calculate the chunk size for each thread
    size_t chunk_size = array_size / num_threads;
    size_t start_index = chunk_size * (thread_id - 1);
    size_t end_index = (thread_id == num_threads)
        ? array_size : chunk_size * thread_id;

    struct timespec start_time, last_heartbeat_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    last_heartbeat_time = start_time;

    // Frobnicate the data.
    uint64_t iteration = 0;
    int iterations_since_check = 0;
    bool started_benchmarks = false;
    while(true) { 
        iteration++;

        for (size_t i = start_index; i < end_index; i += 1024) {
            if (data[i] != frobnicate(i, iteration - 1)) {
                 fprintf(stderr, "ERROR: Thread %d saw memory corruption! (expected %d, got %d)\n",
                         thread_id, frobnicate(i, iteration-1), data[i]);
                 goto done;
            }

            data[i] = frobnicate(i, iteration);

            iterations_since_check++;
            if (iterations_since_check >= 512) {
                iterations_since_check = 0;
                struct timespec current_time, thread_runtime;

                clock_gettime(CLOCK_MONOTONIC, &current_time);
                thread_runtime = diff_timespec(&current_time, &start_time);

                if (diff_timespec(&current_time, &last_heartbeat_time).tv_sec >= heartbeat_secs) {
                    printf("Heartbeat: thread%d still alive\n", thread_id);
                    last_heartbeat_time = current_time;
                }

                if (thread_runtime.tv_sec >= warmup_duration + benchmark_duration) {
                    if (args->thread_leader)
                         assert(started_benchmarks == true);
                    goto done;
                }

                if (args->thread_leader && !started_benchmarks
                        && thread_runtime.tv_sec >= warmup_duration) {
                    printf("BEGIN_BENCHMARK\n"); // signal to test framework
                    fflush(stdout);
                    started_benchmarks = true;
                }

            }
        }
    }

done:
    printf("Thread %d stopping benchmarks, %d seconds have passed\n",
            thread_id, benchmark_duration);
    if (args->thread_leader) {
        printf("END_BENCHMARK\n"); // signal to test framework
        fflush(stdout);
    }
}

// Function to be executed by each thread
static void *worker_thread_fn(void* arg) {
    struct worker_args *args = arg;
    int thread_id = args->thread_id;
    int num_threads = args->num_workers;

    printf("Thread %d waiting for permission to start\n", thread_id);
    pthread_mutex_lock(&bench_start_mutex);
    while (!bench_start)
         pthread_cond_wait(&bench_start_cond, &bench_start_mutex);
    pthread_mutex_unlock(&bench_start_mutex);
    printf("Thread %d starting\n", thread_id);

    workload_function(args);

    printf("Thread %d signalling completion\n", thread_id);
    pthread_mutex_lock(&finished_threads_mutex);
    finished_threads++;
    pthread_cond_signal(&finished_threads_cond);
    pthread_mutex_unlock(&finished_threads_mutex);

    printf("Thread %d waiting for permission to die\n", thread_id);
    pthread_mutex_lock(&bench_end_mutex);
    while (!bench_end)
         pthread_cond_wait(&bench_end_cond, &bench_end_mutex);
    pthread_mutex_unlock(&bench_end_mutex);

    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <number of threads> <array size>\n", argv[0]);
        return 1;
    }
    int num_workers = atoi(argv[1]);
    array_size = strtoul(argv[2], NULL, 10);

    printf("Allocating %ld bytes\n", array_size);
    data = malloc(array_size * sizeof(*data)); // sizeof(char) = 1
    if (data == NULL) {
        printf("Memory allocation failed.\n");
        return 1;
    }
    printf("Allocated %ld bytes\n", array_size);
    printf("ALLOC_DONE\n");

    // Initialize shared workload buffer
    for (size_t i = 0; i < array_size; i++)
         data[i] = frobnicate(i, 0);

    bench_start = false;
    finished_threads = 0;

    // Initialize thread arguments.
    struct worker_args worker_args[num_workers];
    for (int i = 0; i < num_workers; i++) {
        worker_args[i].thread_id = i + 1;
        worker_args[i].thread_leader = (i == 0);
        worker_args[i].num_workers = num_workers;
    }

    // Spawn worker threads, don't let them start the workload though.
    printf("Spawning %d threads.\n", num_workers);
    pthread_t threads[num_workers - 1];
    for (int i = 0; i < num_workers - 1; i++)
        pthread_create(&threads[i], NULL, worker_thread_fn, &worker_args[i+1]);

    // Signal 'benchmark begin' to worker threads!
    printf("Starting worker threads...\n");
    pthread_mutex_lock(&bench_start_mutex);
    bench_start = true;
    pthread_cond_broadcast(&bench_start_cond);
    pthread_mutex_unlock(&bench_start_mutex);

    // Perform work on main thread. This function signals the test framework when complete.
    workload_function(&worker_args[0]);

    sleep(2);

    // Wait for worker threads to finish
    printf("Waiting on worker threads to finish...\n");
    pthread_mutex_lock(&finished_threads_mutex);
    while (finished_threads < num_workers - 1)
         pthread_cond_wait(&finished_threads_cond, &finished_threads_mutex);
    pthread_mutex_unlock(&finished_threads_mutex);

    sleep(2);

    // Allow workers to die
    printf("Commanding workers to die...\n");
    pthread_mutex_lock(&bench_end_mutex);
    bench_end = true;
    pthread_cond_broadcast(&bench_end_cond);
    pthread_mutex_unlock(&bench_end_mutex);

    // Wait for threads to finish
    printf("Joining with workers...\n");
    for (int i = 0; i < num_workers - 1; i++) {
        pthread_join(threads[i], NULL);
    }
    printf("Threads done.\n");

    // Write to dev null. Do this so the array math isn't optimized out.
    FILE *null_file = fopen("/dev/null", "w");
    size_t elements_written = fwrite(data, 1, array_size, null_file);
    if (elements_written != array_size) {
        perror("Failed to write to /dev/null");
        fclose(null_file);
        return 1;
    }
    fclose(null_file);

    free(data);
    return 0;
}
