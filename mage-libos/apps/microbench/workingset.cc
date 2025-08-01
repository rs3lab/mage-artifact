/*
 * The benchmark to test how many pages the system can handle
 */

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <cstring>
#include <cassert>
#include <chrono>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <malloc.h>
#include <time.h>
#include <algorithm>
#include <ddc/mman.h>
#include "numa-config.h"

/* Working set test. The local size is 1/2 of the TotalSize */
/* 1/6 hot1 1/6 hot2 2/3 one-time cold */
constexpr static uint64_t kTotalSize = 6ULL * 1024 * 1024 * 1024; /* 16GB total size */
constexpr static double kFrequency = 2.6; /* 2.6 GHz */
constexpr static uint64_t kHot1 = 6; /* 1/6 hot 1 */
constexpr static uint64_t kHot2 = 6; /* 1/6 hot 2 */

#define THREAD_PINNING

class Barrier {
public:
    Barrier(int numThreads) : count(numThreads), initialCount(numThreads), generation(0) {}

    void Wait() {
        std::unique_lock<std::mutex> lock(mutex);
        int gen = generation;

        if (--count == 0) {
            generation++;
            count = static_cast<int>(initialCount);
            cv.notify_all();
        } else {
            cv.wait(lock, [this, gen]() { return gen != generation; });
        }
    }

private:
    std::mutex mutex;
    std::condition_variable cv;
    int count;
    int generation;
    const size_t initialCount;
};

static cpu_set_t cpu_set[500]; // Up to 500 threads;

int getCPUid(int index, bool reset)
{
  static int cur_socket = 0;
  static int cur_physical_cpu = 0;
  static int cur_smt = 0;

  if(reset){
          cur_socket = 0;
          cur_physical_cpu = 0;
          cur_smt = 0;
          return 1;
  }

  int ret_val = OS_CPU_ID[cur_socket][cur_physical_cpu][cur_smt];
  cur_physical_cpu++;

  if(cur_physical_cpu == NUM_PHYSICAL_CPU_PER_SOCKET){
          cur_physical_cpu = 0;
          cur_socket++;
          if(cur_socket == NUM_SOCKET){
                  cur_socket = 0;
                  cur_smt++;
                  if(cur_smt == SMT_LEVEL)
                          cur_smt = 0;
          }
  }

  return ret_val;
                        
}

inline void timer_start(unsigned *cycles_high_start, unsigned *cycles_low_start) {
  asm volatile("xorl %%eax, %%eax\n\t"
               "CPUID\n\t"
               "RDTSC\n\t"
               "mov %%edx, %0\n\t"
               "mov %%eax, %1\n\t"
               : "=r"(*cycles_high_start), "=r"(*cycles_low_start)::"%rax",
                 "%rbx", "%rcx", "%rdx");
}

inline void timer_end(unsigned *cycles_high_end, unsigned *cycles_low_end) {
  asm volatile("RDTSCP\n\t"
               "mov %%edx, %0\n\t"
               "mov %%eax, %1\n\t"
               "xorl %%eax, %%eax\n\t"
               "CPUID\n\t"
               : "=r"(*cycles_high_end), "=r"(*cycles_low_end)::"%rax", "%rbx",
                 "%rcx", "%rdx");
}

inline uint64_t get_elapsed_cycles(unsigned cycles_high_start, unsigned cycles_low_start, unsigned cycles_high_end, unsigned cycles_low_end) {
  uint64_t start, end;
  start = ((static_cast<uint64_t>(cycles_high_start) << 32) | cycles_low_start);
  end = ((static_cast<uint64_t>(cycles_high_end) << 32) | cycles_low_end);
  return end - start;
}

size_t getMajor(){
  FILE *fp = fopen("/proc/self/stat", "r");
  if(fp == nullptr) {
    fprintf(stderr, "Could not open /proc/self/stat to read memory usage\n");
    exit(1);
  }

  unsigned long unused;

  unsigned long major;
  char buff[20];
  if (fscanf(fp, "%d %s %c %d %d %d %d %d %lu %lu %lu %lu ...", &unused, &buff, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &major) != 12) {
	  perror("");
	  exit(1);
  }

  (void)unused;
  fclose(fp);
  return major;
}

size_t getMinor(){
  FILE *fp = fopen("/proc/self/stat", "r");
  if(fp == nullptr) {
    fprintf(stderr, "Could not open /proc/self/stat to read memory usage\n");
    exit(1);
  }

  unsigned long unused;

  unsigned long minor;
  char buff[20];
  if (fscanf(fp, "%d %s %c %d %d %d %d %d %lu %lu ...", &unused, &buff, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &minor) != 10) {
	  perror("");
	  exit(1);
  }

  (void)unused;
  fclose(fp);
  return minor;
}

/*
 * MemUsage() - Reads memory usage from /proc file system
 */

size_t MemUsage() {
  FILE *fp = fopen("/proc/self/stat", "r");
  if(fp == nullptr) {
    fprintf(stderr, "Could not open /proc/self/stat to read memory usage\n");
    exit(1);
  }

  unsigned long unused;

  unsigned long rss;
  char buff[20];
  if (fscanf(fp, "%d %s %c %d %d %d %d %d %lu %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld %ld %ld %lu %lu %lu ...", &unused, &buff, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &rss) != 24) {
	  perror("");
	  exit(1);
  }

  (void)unused;
  fclose(fp);
  return rss / 1024; // in KiB (not kB)
}

int test(uint32_t n_threads, bool write_test){
    long kPageSize = sysconf(_SC_PAGE_SIZE);
    char* test_array = reinterpret_cast<char*>(memalign(kPageSize, kTotalSize * sizeof(char)));
    uint64_t *total_count = reinterpret_cast<uint64_t*>(malloc(n_threads * sizeof(uint64_t)));
    uint64_t per_thread_size = kTotalSize / n_threads / kPageSize * kPageSize;
    assert(per_thread_size % kPageSize == 0);
    assert((uint64_t) test_array % kPageSize == 0);
    struct rusage start, end;
    Barrier bar(n_threads + 1);
    std::vector<std::thread> threads;

    /* Populate the how area with some value */
    std::cout<<"Before memset"<<std::endl;
    for (uint32_t tid = 0; tid < n_threads; tid ++){
        threads.emplace_back([&, tid](){
            uint64_t thread_start = tid * per_thread_size;
            for (uint64_t i = thread_start; i < thread_start + per_thread_size; i++){
                test_array[i] = i % 256;
            }
            //std::memset(test_array + thread_start, 1, per_thread_size);
            printf("%u, done\n", tid);
        });
    }

    for (auto& thread: threads){
        thread.join();
    }


    /* Test */
    threads.clear();
    //madvise((void*)test_array, 0, MADV_DDC_PRINT_STAT);
    #ifndef THREAD_PINNING
    uint32_t j = 0;
    CPU_ZERO(&cpu_set[0]);
    for(j = 0; j < n_threads; j++){
      int cpuid = getCPUid(j,0);
      CPU_SET(cpuid, &cpu_set[0]);
    }
    getCPUid(0, 1);
    #endif

    for (uint32_t tid = 0; tid < n_threads; tid++){
        #ifdef THREAD_PINNING
        int cpuid = getCPUid(tid,0);
        CPU_ZERO(&cpu_set[tid]);
        CPU_SET(cpuid, &cpu_set[tid]);
        #endif
        threads.emplace_back([&, tid](){
            #ifdef THREAD_PINNING
            sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set[tid]);
            #else
            sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set[0]);
            #endif
            /* [thread_start, thread_end) */
            uint64_t thread_start = tid * per_thread_size;
            uint64_t thread_end = (tid + 1) * per_thread_size; 
            assert(thread_start % kPageSize == 0);
            assert(thread_end % kPageSize == 0);
            uint64_t cnt = 0;
            std::chrono::_V2::system_clock::time_point start;
            std::chrono::_V2::system_clock::time_point end;
            bar.Wait();
            // 1/6 hot1 1/6 hot2 2/3 cold
            // hot1 hot1 hot1 cold hot2 cold hot2 cold hot2 cold hot2
            uint64_t hot1_start = thread_start;
            uint64_t hot2_start = (thread_start + per_thread_size / kHot1) / kPageSize * kPageSize;
            uint64_t cold_start = (hot2_start + per_thread_size / kHot2) / kPageSize * kPageSize;

            // 3x hot1. So linux will promote the pages into the active list
            for (uint32_t iter = 0; iter < 3; iter++){
                for (uint64_t i = hot1_start; i < hot2_start; i += kPageSize){
                    if(write_test){
                        *(test_array + i) = iter;
                    }else{
                        asm volatile ("" : : "r" (*(unsigned int *)(test_array + i)));
                        assert(test_array[i] == i % 256);
                    }
                    cnt ++;
                }
            }

            // cold hot2
            for (uint32_t iter = 0; iter < 4; iter++){
                uint64_t tmp_cold_start = (cold_start + iter * per_thread_size / kHot2) / kPageSize * kPageSize;
                uint64_t tmp_cold_end = (cold_start + (iter + 1) * per_thread_size / kHot2) / kPageSize * kPageSize;
                for (uint64_t i = tmp_cold_start; i < tmp_cold_end; i += kPageSize){
                    if(write_test){
                        *(test_array + i) = iter;
                    }else{
                        asm volatile ("" : : "r" (*(unsigned int *)(test_array + i)));
                        assert(test_array[i] == i % 256);
                    }
                    cnt ++;
                }
                for (uint64_t i = hot2_start; i < cold_start; i += kPageSize){
                    if(write_test){
                        *(test_array + i) = iter;
                    }else{
                        asm volatile ("" : : "r" (*(unsigned int *)(test_array + i)));
                        assert(test_array[i] == i % 256);
                    }
                    cnt ++;
                }
                
            }

            total_count[tid] = cnt;
        }); 
    }

    /* Read the statistics before test */
    size_t start_minor = getMinor();
    size_t start_major = getMajor();
    std::cout<<"Before test: mem = "<<MemUsage()<<std::endl;
    
    madvise((void*)test_array, 0, MADV_DDC_PRINT_STAT);

    auto start_time = std::chrono::system_clock::now();
    bar.Wait();
    for (auto& thread: threads){
        thread.join();
    }
    auto end_time = std::chrono::system_clock::now();

    std::chrono::duration<double, std::micro> elapsed_time = end_time - start_time;

    uint64_t count = 0;
    for (uint32_t i = 0; i < n_threads; i++){
        count += total_count[i];
    }
    double tput = count / elapsed_time.count(); 

    /* Read the statistics after test */ 
    size_t end_minor = getMinor();
    size_t end_major = getMajor();

    std::cout<<"After test: mem = "<<MemUsage()<<std::endl;
    std::cout<<"Duration: "<<elapsed_time.count()<<" us"<<std::endl;
    std::cout<<"tput: "<<tput<<" mops"<<std::endl;
    std::cout<<"count: "<<count<<std::endl;
    std::cout<<"major page faults: "<<end_major - start_major<<std::endl;
    std::cout<<"minor page faults: "<<end_minor - start_minor<<std::endl;
    sleep(1);
    return 0;

}


int main(int argc, char** argv){
    uint32_t n_threads = 1;
    bool write_test = false;

    int c;
    while((c = getopt(argc, argv, "t:w"))!= -1){
        switch (c)
        {
        case 't':
            n_threads = strtoul(optarg, NULL, 10);
            break;
        case 'w':
            write_test = true;
            break;
        case '?':
            std::cerr<<"Unknown option"<<std::endl;
            exit(-1);
        default:
            std::cerr<<"Unknown option "<<c<<std::endl;
            exit(-1);
        }
    }

    test(n_threads, write_test);
    return 0;
}

