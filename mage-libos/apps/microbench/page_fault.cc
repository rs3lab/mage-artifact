/*
 * The benchmark to test how many pages the system can handle
 */

#include <iostream>
#include <iomanip>
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
#include "helper.h"
extern "C"{
    #include "zipf.h"
}

constexpr static uint64_t kTotalSize = 20ULL * 1024 * 1024 * 1024; /* 6GB total size */
constexpr static uint64_t kSampleGap = 4ULL; /* Sample every 16 access */
/* NOTE: We use high_resolution_clock */
constexpr static double kFrequency = 1; /* 2.5929 GHz */
/*This is used to guarantee that the expectation of the final entry being accessed is 0 */
//constexpr static uint64_t kTotalAccesses = 1572864; /* For zipf = 0 */
//constexpr static uint64_t kTotalAccesses = 3143897; /* For zipf = 0.5 */
constexpr static uint64_t kTotalAccesses = 23177824; /* For zipf = 0.999 */

#define THREAD_PINNING

static cpu_set_t cpu_set[500]; // Up to 500 threads;

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

int test(uint32_t n_threads, uint32_t stride, uint32_t total_iter, bool write_test, bool latency_test, bool minor_fault, 
bool random_access, bool exclusive, double zipfian){
    long kPageSize = sysconf(_SC_PAGE_SIZE);
    char* test_array = reinterpret_cast<char*>(memalign(kPageSize, kTotalSize * sizeof(char)));
    uint64_t *total_count = reinterpret_cast<uint64_t*>(malloc(n_threads * sizeof(uint64_t)));
    uint64_t total_cycles[64];
    uint64_t per_thread_size = kTotalSize / n_threads / kPageSize * kPageSize;
    assert(per_thread_size % kPageSize == 0);
    assert((uint64_t) test_array % kPageSize == 0);
    struct rusage start, end;
    Barrier bar(n_threads + 1);
    std::vector<std::thread> threads;
    uint64_t *latency_sample;
    if (latency_test){
        if (random_access){
            latency_sample = reinterpret_cast<uint64_t*>(memalign(64, (kTotalAccesses + kSampleGap) / kSampleGap * sizeof(uint64_t)));
        } else {
            latency_sample = reinterpret_cast<uint64_t*>(memalign(64, (per_thread_size / kPageSize * n_threads * total_iter + kSampleGap) / kSampleGap * sizeof(uint64_t)));
        }
    }

    /* Populate the how area with some value */
    if (!minor_fault){
        std::cout<<"Before memset"<<std::endl;
        //printf("%p %p\n", test_array, test_array + kTotalSize);
        for (uint32_t tid = 0; tid < n_threads; tid ++){
            threads.emplace_back([&, tid](){
                uint64_t thread_start = tid * per_thread_size;
                for (uint64_t i = thread_start; i < thread_start + per_thread_size; i++){
                    test_array[i] = i % 256;
                    // if (i % ( 4096 * 128 )== 0){
                    //     printf("%u, %llu\n", tid, i / 4096);
                    // }
                }
                //std::memset(test_array + thread_start, 1, per_thread_size);
                printf("%u, done\n", tid);
            });
        }

        for (auto& thread: threads){
            thread.join();
        }
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

    unsigned global_seed = (unsigned int)time(NULL);
    srand(global_seed);
    unsigned global_offset = rand();
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
            struct zipf_state zs;
            unsigned int local_seed = tid + (unsigned int)time(NULL);
            if (random_access) {
                // Exclusive: [thread_start, thread_end)
                // Non-exclusive: [0, n_threads * per_thread_size)
                if (exclusive){
                    // Use different seed for different threads
                    zipf_init(&zs, per_thread_size / kPageSize, zipfian, local_seed);
                }
                else {
                    // For shared, disable hash, set rand off to 0 but use different seeds so the 
                    // overall distribution follows zipfian.
                    zipf_init(&zs, n_threads * per_thread_size / kPageSize, zipfian, local_seed);
                    zipf_set_rand_off(&zs, global_offset);
                }
            }
            std::chrono::_V2::system_clock::time_point start;
            std::chrono::_V2::system_clock::time_point end;
            unsigned cycles_high_start, cycles_high_end, cycles_low_start, cycles_low_end;
            bar.Wait();
            auto cycles_start = std::chrono::high_resolution_clock::now();
            if (random_access){
                uint64_t per_thread_accesses = kTotalAccesses / n_threads;
                if (exclusive){
                    for (uint64_t i = 0; i < per_thread_accesses; i++){
                        uint64_t index = zipf_next(&zs) * kPageSize + thread_start;
                        if (write_test == true){
                            if (latency_test && (cnt % kSampleGap == 0)){
                                start = std::chrono::high_resolution_clock::now();
                            }
                            *(test_array + index) = 2;
                            if (latency_test && (cnt % kSampleGap == 0)){
                                end = std::chrono::high_resolution_clock::now();
                                latency_sample[per_thread_accesses * tid / kSampleGap + cnt / kSampleGap] = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
                            }
                        }else {
                            if (latency_test && (cnt % kSampleGap == 0)){
                                start = std::chrono::high_resolution_clock::now();
                            }
                            asm volatile ("" : : "r" (*(unsigned int *)(test_array + index)));
                            assert(test_array[index] == index % 256);
                            if (latency_test && (cnt % kSampleGap == 0)){
                                end = std::chrono::high_resolution_clock::now();
                                latency_sample[per_thread_accesses * tid / kSampleGap + cnt / kSampleGap] = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
                            }
                        }
                        cnt ++;
                        if (cnt % 16384 == 0){
                            printf("%u, %llu\n", tid, cnt);
                        }
                    }
                } else {
                    for (uint64_t i = 0; i < per_thread_accesses; i++){
                        uint64_t index = zipf_next(&zs) * kPageSize;
                        if (write_test == true){
                            if (latency_test && (cnt % kSampleGap == 0)){
                                start = std::chrono::high_resolution_clock::now();
                            }
                            *(test_array + index) = 2;
                            if (latency_test && (cnt % kSampleGap == 0)){
                                end = std::chrono::high_resolution_clock::now();
                                latency_sample[per_thread_accesses * tid / kSampleGap + cnt / kSampleGap] = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
                            }
                        }else {
                            if (latency_test && (cnt % kSampleGap == 0)){
                                start = std::chrono::high_resolution_clock::now();
                            }
                            asm volatile ("" : : "r" (*(unsigned int *)(test_array + index)));
                            assert(test_array[index] == index % 256);
                            if (latency_test && (cnt % kSampleGap == 0)){
                                end = std::chrono::high_resolution_clock::now();
                                latency_sample[per_thread_accesses * tid / kSampleGap + cnt / kSampleGap] = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
                            }
                        }
                        cnt ++;
                        if (cnt % 16384 == 0){
                            printf("%u, %llu\n", tid, cnt);
                        }
                    }
                }
            } else {
                for (uint32_t l = 0; l < total_iter; l++){
                    for (uint32_t iter = 0; iter < stride; iter++){
                        for (uint64_t i = thread_start + (iter * kPageSize); i + kPageSize < thread_end; i += ((stride) * kPageSize)){
                            if (write_test == true){
                                if (latency_test && (cnt % kSampleGap == 0)){
                                    start = std::chrono::high_resolution_clock::now();
                                }
                                *(test_array + i) = (l * iter + i) % 256;
                                if (latency_test && (cnt % kSampleGap == 0)){
                                    end = std::chrono::high_resolution_clock::now();
                                    latency_sample[per_thread_size / kPageSize * tid * total_iter / kSampleGap + cnt / kSampleGap] = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
                                }
                            }else {
                                if (latency_test && (cnt % kSampleGap == 0)){
                                    start = std::chrono::high_resolution_clock::now();
                                }
                                asm volatile ("" : : "r" (*(unsigned int *)(test_array + i)));
                                assert(test_array[i] == i % 256);
                                if (latency_test && (cnt % kSampleGap == 0)){
                                    end = std::chrono::high_resolution_clock::now();
                                    latency_sample[per_thread_size / kPageSize * tid * total_iter/ kSampleGap + cnt / kSampleGap] = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
                                }
                            }
                            cnt ++;
                            if (cnt % 16384 == 0){
                                printf("%u, %llu\n", tid, cnt);
                            }
                        }
                    }
                }
            }
            auto cycles_end = std::chrono::high_resolution_clock::now(); 
            total_count[tid] = cnt;
            total_cycles[tid] = std::chrono::duration_cast<std::chrono::nanoseconds>(cycles_end - cycles_start).count();
        }); 
    }

    /* Read the statistics before test */
    size_t start_minor = getMinor();
    size_t start_major = getMajor();
    size_t start_fault_cycles = getFaultTime();
    size_t start_rdma = getRDMAReadTime();
    size_t start_rdma_sched_cycles = getRDMASchedTime();
    std::cout<<"Before test: mem = "<<MemUsage()<<std::endl;
    
    std::cout<<"Please attach"<<std::endl;
    sleep(2);
    
    madvise((void*)test_array, 0, MADV_DDC_PRINT_STAT);

    auto start_time = std::chrono::system_clock::now();
    bar.Wait();
    for (auto& thread: threads){
        thread.join();
    }
    auto end_time = std::chrono::system_clock::now();

    std::chrono::duration<double, std::micro> elapsed_time = end_time - start_time;

    uint64_t count = 0;
    uint64_t cycles = 0;
    for (uint32_t i = 0; i < n_threads; i++){
        count += total_count[i];
        cycles += total_cycles[i];
    }

    /* Read the statistics after test */ 
    size_t end_minor = getMinor();
    size_t end_major = getMajor();
    size_t end_fault_cycles = getFaultTime();
    size_t end_rdma = getRDMAReadTime();
    size_t end_rdma_sched_cycles = getRDMASchedTime();

    double tput = count / elapsed_time.count(); 
    double maj_flt_tput = (end_major - start_major) / elapsed_time.count();
    double flt_tput = (end_major - start_major + end_minor - start_minor) / elapsed_time.count();

    std::cout<<"After test: mem = "<<MemUsage()<<std::endl;
    std::cout<<"Duration: "<<elapsed_time.count()<<" us"<<std::endl;
    std::cout<<"tput: "<<tput<<" mops"<<std::endl;
    std::cout<<"count: "<<count<<std::endl;

    std::cout<<"Total cycles: "<<cycles<<std::endl;
    std::cout<<"Faulting cycles: "<<end_fault_cycles - start_fault_cycles<<std::endl;
    std::cout<<"RDMA cycles: "<<end_rdma - start_rdma<<std::endl;
    std::cout<<"RDMA sched cycles: "<<end_rdma_sched_cycles - start_rdma_sched_cycles<<std::endl;

    if (latency_test){
        uint64_t p50_position = count / kSampleGap / 2;
        uint64_t p99_position = count / kSampleGap / 100;
        std::sort(latency_sample, latency_sample + count / kSampleGap, std::greater<uint64_t>());
        double p50_latency = latency_sample[p50_position] / kFrequency;
        double p99_latency = latency_sample[p99_position] / kFrequency;
        std::cout<<"p50-latency: "<<p50_latency<<" ns"<<std::endl;
        std::cout<<"p99-latency: "<<p99_latency<<" ns"<<std::endl;
        uint64_t sum = 0;
        //std::cout<<std::fixed;
        for(uint64_t i = 0; i <  count / kSampleGap ; i++){
            //std::cout<<std::setprecision(1)<<latency_sample[i] / kFrequency <<std::endl;
            sum += latency_sample[i];
        }
        std::cout<<"Average-latency: "<<sum / (count / kSampleGap) / kFrequency<<std::endl;
    }
    sleep(1);
    return 0;

}


int main(int argc, char** argv){
    uint32_t n_threads = 1;
    uint32_t stride = 1; /* The number of pages between two accesses */
    uint32_t total_iter = 1; /* Loop how many times for strided accesses */
    bool write_test = false;
    bool latency_test = false;
    bool minor_fault = false; /* Test minor fault so don't populate */
    bool random_access = false; /* If the random access is not set, the access pattern is sequential or strided */
    bool exclusive = false; /* Only useful for random access, Decides whether the memory region is partitioned or not */
    double zipfian = 0; /* zipfian distribution */  

    int c;
    while((c = getopt(argc, argv, "dt:ws:mrez:i:"))!= -1){
        switch (c)
        {
        case 't':
            n_threads = strtoul(optarg, NULL, 10);
            break;
        case 'w':
            write_test = true;
            break;
        case 's':
            stride = strtoul(optarg, NULL, 10);
            break;
        case 'd':
            latency_test = true;
            break;
        case 'm':
            minor_fault = true;
            break;
        case 'r':
            random_access = true;
            break;
        case 'e':
            exclusive = true;
            break;
        case 'z':
            zipfian = strtod(optarg, NULL);
            break;
        case 'i':
            total_iter = strtoul(optarg, NULL, 10);
            break;
        case '?':
            std::cerr<<"Unknown option"<<std::endl;
            exit(-1);
        default:
            std::cerr<<"Unknown option "<<c<<std::endl;
            exit(-1);
        }
    }

    test(n_threads, stride, total_iter, write_test, latency_test, minor_fault, random_access, exclusive, zipfian);
    return 0;
}

