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
#include <ddc/page.hh>
#include "helper.h"

constexpr static uint64_t kTotalSize = 30ULL * 1024 * 1024 * 1024; /* 6GB total size */
constexpr static uint64_t kSampleGap = 16ULL; /* Sample every 16 access */
constexpr static double kFrequency = 1; /* 2.6 GHz */

constexpr static size_t kSliceSize = 32;
constexpr static uint64_t kTotalOps = 10000000;

#define THREAD_PINNING

static cpu_set_t cpu_set[500]; // Up to 500 threads;

int test(uint32_t n_threads, uint32_t total_iter, bool latency_test){
    std::cout<<"Start test"<<std::endl;
    long kPageSize = sysconf(_SC_PAGE_SIZE);
    char* test_array = reinterpret_cast<char*>(memalign(kPageSize, kTotalSize * sizeof(char)));
    uint64_t *total_count = reinterpret_cast<uint64_t*>(malloc(n_threads * sizeof(uint64_t)));
    uint64_t per_thread_size = kTotalSize / n_threads / kPageSize * kPageSize;
    uint64_t per_thread_ops = kTotalOps / n_threads;
    assert(per_thread_size % kPageSize == 0);
    assert((uint64_t) test_array % kPageSize == 0);
    struct rusage start, end;
    Barrier bar(n_threads + 1);
    std::vector<std::thread> threads;
    uint64_t *latency_sample;
    if (latency_test){
        latency_sample = reinterpret_cast<uint64_t*>(memalign(64, (per_thread_ops * n_threads * total_iter + kSampleGap) / kSampleGap * sizeof(uint64_t)));
    }

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
            uint64_t cnt = 0;
            unsigned cycles_high_start, cycles_high_end, cycles_low_start, cycles_low_end;
            std::chrono::_V2::system_clock::time_point start_time, end_time;
            ddc::base_page_slice_t slice_active;
            size_t active_list_id = tid % ddc::max_queues;
            bar.Wait();
            for (uint32_t l = 0; l < total_iter; l++){
                for (uint64_t i = 0; i < kTotalOps/ n_threads; i++){
                    if (latency_test && (cnt % kSampleGap == 0)){
                        start_time = std::chrono::system_clock::now();
                    }
                    ddc::page_list.slice_pages_active(active_list_id, slice_active, kSliceSize);
                    if (latency_test && (cnt % kSampleGap == 0)){
                        end_time = std::chrono::system_clock::now();
                        std::chrono::duration<int64_t, std::nano> elapsed_time = end_time - start_time;
                        latency_sample[per_thread_ops * tid * total_iter/ kSampleGap + cnt / kSampleGap] = elapsed_time.count();
                    }
                    ddc::page_list.push_pages_active(active_list_id, slice_active, true);
                    cnt ++;
                    if (cnt % 16384 == 0){
                        printf("%u, %llu\n", tid, cnt);
                    }
                }
            }
            total_count[tid] = cnt;
        }); 
    }
    
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
    for (uint32_t i = 0; i < n_threads; i++){
        count += total_count[i];
    }
    // One slice and one push so 2 ops
    double tput = count * 2 / elapsed_time.count(); 

    std::cout<<"Duration: "<<elapsed_time.count()<<" us"<<std::endl;
    std::cout<<"tput: "<<tput<<" mops"<<std::endl;
    std::cout<<"count: "<<count<<std::endl;
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
    std::cout<<"Main Start"<<std::endl;
    uint32_t n_threads = 1;
    uint32_t total_iter = 1; /* Loop how many times for strided accesses */
    bool latency_test = false;

    int c;
    while((c = getopt(argc, argv, "dt:i:"))!= -1){
        switch (c)
        {
        case 't':
            n_threads = strtoul(optarg, NULL, 10);
            break;
        case 'd':
            latency_test = true;
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

    test(n_threads, total_iter, latency_test);
    return 0;
}

