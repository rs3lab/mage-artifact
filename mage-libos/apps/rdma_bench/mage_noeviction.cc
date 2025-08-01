/*
 * The benchmark test how much RDMA bandwidth DiLOS can achieve
 */
#include <atomic>
#include <unistd.h>
#include <iostream>
#include <stdlib.h>
#include <malloc.h>
#include <vector>
#include <thread>
#include <string.h>
#include <atomic>
#include "helper.h"
#include <ddc/remote.hh>
#include <ddc/mman.h>
#include <osv/pagealloc.hh>
#include <osv/virt_to_phys.hh>

/* Must pin threads */
#define THREAD_PINNING

constexpr static uint64_t kTotalSize = 30UL * 1024 * 1024 * 1024; /* 30GB total size */
constexpr static uint64_t kTotalPages = kTotalSize / 4096;
constexpr static uint64_t kMaxCPUs = 64;
constexpr static uint64_t kSampleGap = 8;
constexpr static double kFrequency = 2.6; /* 2.6GHz */
constexpr static uintptr_t kDefaultVec = 64;

static cpu_set_t cpu_set[500]; /* Up to 500 threads */

inline uint64_t rdtsc()
{
    uint32_t lo, hi;
    asm("rdtsc" : "=a"(lo), "=d"(hi));
    return lo | (uint64_t(hi) << 32);
}

size_t getUserTime(){
  FILE *fp = fopen("/proc/self/stat", "r");
  if(fp == nullptr) {
    fprintf(stderr, "Could not open /proc/self/stat to read memory usage\n");
    exit(1);
  }

  unsigned long unused;

  unsigned long utime;
  char buff[20];
  if (fscanf(fp, "%d %s %c %d %d %d %d %d %lu %lu %lu %lu %lu %lu %lu ...", 
    &unused, &buff, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &utime) != 14) {
	  perror("");
	  exit(1);
  }

  (void)unused;
  fclose(fp);
  return utime;
}

size_t getRDMASchedTime(){
  FILE *fp = fopen("/proc/self/stat", "r");
  if(fp == nullptr) {
    fprintf(stderr, "Could not open /proc/self/stat to read memory usage\n");
    exit(1);
  }

  unsigned long unused;

  unsigned long cutime;
  char buff[20];
  if (fscanf(fp, "%d %s %c %d %d %d %d %d %lu %lu %lu %lu %lu %lu %lu %ld ...", 
    &unused, &buff, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &cutime) != 16) {
	  perror("");
	  exit(1);
  }

  (void)unused;
  fclose(fp);
  return cutime;
}

size_t getRDMAReadTime(){
  FILE *fp = fopen("/proc/self/stat", "r");
  if(fp == nullptr) {
    fprintf(stderr, "Could not open /proc/self/stat to read memory usage\n");
    exit(1);
  }

  unsigned long unused;

  unsigned long cstime;
  char buff[20];
  if (fscanf(fp, "%d %s %c %d %d %d %d %d %lu %lu %lu %lu %lu %lu %lu %ld %ld ...", 
    &unused, &buff, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &cstime) != 17) {
	  perror("");
	  exit(1);
  }

  (void)unused;
  fclose(fp);
  return cstime;
}

size_t getFaultTime(){
  FILE *fp = fopen("/proc/self/stat", "r");
  if(fp == nullptr) {
    fprintf(stderr, "Could not open /proc/self/stat to read memory usage\n");
    exit(1);
  }

  unsigned long unused;

  unsigned long stime;
  char buff[20];
  if (fscanf(fp, "%d %s %c %d %d %d %d %d %lu %lu %lu %lu %lu %lu %lu ...", 
    &unused, &buff, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &stime) != 15) {
	  perror("");
	  exit(1);
  }

  (void)unused;
  fclose(fp);
  return stime;
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
  if (fscanf(fp, "%d %s %c %d %d %d %d %d %lu %lu %lu %lu ...", 
    &unused, &buff, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &major) != 12) {
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
  if (fscanf(fp, "%d %s %c %d %d %d %d %d %lu %lu ...", 
    &unused, &buff, &unused, &unused, &unused, &unused, &unused, &unused, &unused, &minor) != 10) {
	  perror("");
	  exit(1);
  }

  (void)unused;
  fclose(fp);
  return minor;
}

void print_latency(uint64_t *latency_sample, uint64_t count, uint64_t sample_gap, double frequency){
    uint64_t p50_position = count / sample_gap / 2;
    uint64_t p99_position = count / sample_gap / 100;
    std::sort(latency_sample, latency_sample + count / sample_gap, std::greater<uint64_t>());
    double p50_latency = latency_sample[p50_position] / frequency;
    double p99_latency = latency_sample[p99_position] / frequency;
    double max_latency = latency_sample[0] / frequency;
    std::cout<<"p50-latency: "<<p50_latency<<" ns"<<std::endl;
    std::cout<<"p99-latency: "<<p99_latency<<" ns"<<std::endl;
    std::cout<<"max-latency: "<<max_latency<<" ns"<<std::endl;
    uint64_t sum = 0;
    //std::cout<<std::fixed;
    for(uint64_t i = 0; i <  count / sample_gap ; i++){
        //std::cout<<std::setprecision(1)<<latency_sample[i] / frequency <<std::endl;
        sum += latency_sample[i];
    }
    std::cout<<"Average-latency: "<<sum / (count / sample_gap) / frequency<<std::endl;

}


/* The benchmark first populates the region with some data */
/* Then write to the remote side */
/* Then read from the remote side */
/* Then interleave read and write */
int test(uint32_t n_threads, uint32_t n_iter, bool latency_test){
    uint64_t total_cycles[64];
    long kPageSize = sysconf(_SC_PAGE_SIZE);
    char* test_array = reinterpret_cast<char*>(memalign(kPageSize, kTotalSize * sizeof(char)));
    uint64_t per_thread_size = kTotalPages / n_threads;
    uint64_t latency_sample_size = 
        (per_thread_size * n_threads * n_iter + kSampleGap) / kSampleGap * sizeof(uint64_t);
    uint64_t *latency_sample;
    //uint64_t *latency_sample_verify;
    if (latency_test){
        latency_sample = reinterpret_cast<uint64_t*>(memalign(64, latency_sample_size));
        //latency_sample_verify = reinterpret_cast<uint64_t*>(memalign(64, latency_sample_size));
    }
    std::vector<std::thread> threads;
    Barrier bar(n_threads + 1);

    /* populate the region */
    std::cout<<"Before memset"<<std::endl;
    for (uint32_t tid = 0; tid < n_threads; tid ++){
        threads.emplace_back([&, tid](){
            uint64_t thread_start = tid * per_thread_size;
            for (uint64_t i = thread_start; i < thread_start + per_thread_size; i++){
                for (uint64_t j = 0; j < kPageSize; j += sizeof(char)){
                    test_array[kPageSize * i + j] = (i + j) % 256;
                }
            }
            //std::memset(test_array + thread_start, 1, per_thread_size);
            printf("%u, done\n", tid);
        });
    }

    for (auto& thread: threads){
        thread.join();
    }
    
    #ifndef THREAD_PINNING
    uint32_t j = 0;
    CPU_ZERO(&cpu_set[0]);
    for(j = 0; j < n_threads; j++){
      int cpuid = getCPUid(j,0);
      CPU_SET(cpuid, &cpu_set[0]);
    }
    #endif
    getCPUid(0, 1);
    if (latency_test) {
        memset(latency_sample, 0, latency_sample_size);
        //memset(latency_sample_verify, 0, latency_sample_size);
    }

    /* First write back */
    /* Following the same batching policy in eviction */
    threads.clear();    
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
            uint64_t thread_start = tid * per_thread_size;
            int ret = madvise(test_array + thread_start * kPageSize, per_thread_size * kPageSize, MADV_DDC_PAGEOUT);
            assert(ret == 0);
        });
    }
    auto start_time = std::chrono::system_clock::now();
    for (auto& thread: threads){
        thread.join();
    }
    auto end_time = std::chrono::system_clock::now();
    std::chrono::duration<double, std::micro> elapsed_time = end_time - start_time;
    uint64_t count = per_thread_size * n_threads;
    double tput = count / elapsed_time.count(); 
    std::cout<<"Total time: "<<elapsed_time.count()<<std::endl;
    std::cout<<"tput w: "<<tput<<" mops"<<std::endl;
    /* Finish write back */

    if (latency_test)
        memset(latency_sample, 0, latency_sample_size);
    #ifndef THREAD_PINNING
    j = 0;
    CPU_ZERO(&cpu_set[0]);
    for(j = 0; j < n_threads; j++){
      int cpuid = getCPUid(j,0);
      CPU_SET(cpuid, &cpu_set[0]);
    }
    #endif
    getCPUid(0, 1);
    std::cout<<"Please attach"<<std::endl;
    sleep(2);

    size_t start_minor = getMinor();
    size_t start_major = getMajor();
    size_t start_fault_cycles = getFaultTime();
    size_t start_utime = getUserTime();
    size_t start_rdma = getRDMAReadTime();
    size_t start_rdma_sched = getRDMASchedTime();
    /* Second read back */
    threads.clear();    
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
            uint64_t poll_cnt = 0;
            uint64_t cnt = 0;
            uint64_t fetched = 0;
            uint64_t thread_start = tid * per_thread_size;
            unsigned cycles_high_start, cycles_high_end, cycles_low_start, cycles_low_end;
            std::chrono::_V2::system_clock::time_point start_time, end_time;
            bar.Wait();
            //auto cycles_start = std::chrono::high_resolution_clock::now();
            auto cycle_start = rdtsc();
            for (uint32_t iter = 0; iter < n_iter; iter ++){
                for (register uint64_t i = thread_start; i < thread_start + per_thread_size; i++){
                    if (latency_test && (cnt % kSampleGap == 0)) {
                        //timer_start(&cycles_high_start, &cycles_low_start);
                        start_time = std::chrono::system_clock::now();
                    }
                    // asm volatile ("" : : "r" (*(unsigned int *)(test_array + i * kPageSize)));
                    // (void)*(volatile unsigned int *)(test_array + i * kPageSize);
                    // volatile char *testarray = test_array;
                    test_array[i * kPageSize] = (i + kPageSize) % 256;
                    // assert(test_array[i * kPageSize] == i % 256);
                    //char expected = i % 256;
                    // Try out the atomic theory
                    //reinterpret_cast<std::atomic<char>*>(&test_array[i * kPageSize])->compare_exchange_strong(expected, i % 256 + 1);
                    //expected = (i + 1) % 256;
                    //reinterpret_cast<std::atomic<char>*>(&test_array[i * kPageSize + 1])->compare_exchange_strong(expected, i % 256 + 2);
                    if (latency_test && (cnt % kSampleGap == 0)){
                        //timer_end(&cycles_high_end, &cycles_low_end);
                        //latency_sample[n_iter * per_thread_size * tid / kSampleGap + cnt / kSampleGap] = get_elapsed_cycles(cycles_high_start, cycles_low_start, cycles_high_end, cycles_low_end);
                        end_time = std::chrono::system_clock::now();
                        std::chrono::duration<int64_t, std::nano> elapsed_time = end_time - start_time;
                        latency_sample[n_iter * per_thread_size * tid / kSampleGap + cnt / kSampleGap] = elapsed_time.count();
                    }
                    cnt++;
                }
            }
            //auto cycle_end = std::chrono::high_resolution_clock::now();
            auto cycle_end = rdtsc();
            //total_cycles[tid] = std::chrono::duration_cast<std::chrono::nanoseconds>(cycle_end - cycles_start).count();
            total_cycles[tid] = cycle_end - cycle_start;
        });
    }
    start_time = std::chrono::system_clock::now();
    std::cout<<"Start"<<std::endl;
    bar.Wait();
    for (auto& thread: threads){
        thread.join();
    }
    end_time = std::chrono::system_clock::now();
    size_t end_minor = getMinor();
    size_t end_major = getMajor();
    size_t end_fault_cycles = getFaultTime();
    size_t end_utime = getUserTime();
    size_t end_rdma = getRDMAReadTime();
    size_t end_rdma_sched = getRDMASchedTime();

    uint64_t cycles = 0;
    for (uint32_t i = 0; i < n_threads; i++){
        cycles += total_cycles[i];
    }

    elapsed_time = end_time - start_time;
    double maj_flt_tput = (end_major - start_major) / elapsed_time.count();
    double flt_tput = (end_major - start_major + end_minor - start_minor) / elapsed_time.count();
    tput = count * n_iter / elapsed_time.count(); 
    std::cout<<"Duration: "<<elapsed_time.count()<<" us"<<std::endl;
    std::cout<<"tput r: "<<tput<<" mops"<<std::endl;
    std::cout<<"maj_tput: "<<maj_flt_tput<<" mops"<<std::endl;
    std::cout<<"flt_tput: "<<flt_tput<<" mops"<<std::endl;
    std::cout<<"major page faults: "<<end_major - start_major<<std::endl;
    std::cout<<"minor page faults: "<<end_minor - start_minor<<std::endl;

    std::cout<<"Total cycles: "<<cycles<<std::endl;
    std::cout<<"Faulting cycles: "<<end_fault_cycles - start_fault_cycles<<std::endl;
    std::cout<<"RDMA cycles: "<<end_rdma - start_rdma<<std::endl;
    std::cout<<"RDMA sched cycles: "<<end_rdma_sched - start_rdma_sched<<std::endl;
    std::cout<<"utime diff: " << end_utime - start_utime << std::endl;

    if (latency_test){
        // print_latency(latency_sample, count, kSampleGap, kFrequency);
        print_latency(latency_sample, count, kSampleGap, 1);
    }
    /* Finish read back */
    return 0;
}

int main(int argc, char **argv){
    uint32_t n_threads = 1;
    uint32_t n_iter = 1;
    bool latency_test = false;
    int c;
    while ((c = getopt(argc, argv, "dt:i:")) != -1){
        switch (c)
        {
        case 't':
            n_threads = strtoul(optarg, NULL, 10);
            break;
        case 'i':
            n_iter = strtoul(optarg, NULL, 10);
            break;
        case 'd':
            latency_test = true;
            break;
        default:
            std::cerr<<"Unknown option "<<c<<std::endl;
            exit(-1);
        } 
    }
    test(n_threads, n_iter, latency_test);
    return 0;
}