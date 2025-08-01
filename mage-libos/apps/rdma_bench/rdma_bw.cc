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
#include "helper.h"
#include <ddc/remote.hh>
#include <osv/pagealloc.hh>
#ifdef NONOSV
#include <remote/nonosv.hh>
#else
#include <osv/virt_to_phys.hh>
#endif

/* Must pin threads */
#define THREAD_PINNING

const static std::string kIBDeviceName = "mlx5_0";
constexpr static int kIBPort = 1;
const static std::string kRDMASrvIP = "19.18.1.1";
constexpr static int kRDMASrvPort = 12346;
constexpr static int kIBGidIdx = 1;

constexpr static uint64_t kTotalSize = 30UL * 1024 * 1024 * 1024; /* 30GB total size */
constexpr static uint64_t kTotalPages = kTotalSize / 4096;
constexpr static uint64_t kMaxCPUs = 64;
constexpr static uint64_t kSampleGap = 8;
constexpr static double kFrequency = 2.6; /* 2.6GHz */
constexpr static uintptr_t kDefaultVec = 64;

/* The batch size follows what we have in the application */
constexpr static size_t kFetchBatchSize = 128; 
constexpr static size_t kPushBatchSize = 32;
static cpu_set_t cpu_set[500]; /* Up to 500 threads */
static uint8_t *PagesPtr[kTotalPages];

/* Because in our code, the page fault and reclamation are using different queueus */
/* We also use two different vectors here */
std::array<ddc::remote_queue, kMaxCPUs> write_queues;
std::array<ddc::remote_queue, kMaxCPUs> read_queues;

std::atomic<uint64_t> total_write_poll;
std::atomic<uint64_t> total_read_poll;
std::atomic<uint64_t> total_write_cnt;
std::atomic<uint64_t> total_read_cnt;
std::atomic<uint64_t> total_write_poll_while;

size_t poll_once(ddc::remote_queue &rq, size_t &pushed, uint64_t &cnt){
    if (!pushed) return 0;
    uintptr_t tokens[pushed];
    int polled = rq.poll(tokens, pushed);
    cnt++;
    pushed -= polled;
    assert(pushed >= 0);
    return polled;
}

void poll_until_one(ddc::remote_queue &rq, size_t &pushed, uint64_t &cnt){
    size_t polled = poll_once(rq, pushed, cnt);
    while ((pushed != 0 ) && (!polled)) {
        __asm volatile ("pause" ::: );
        polled = poll_once(rq, pushed, cnt);
    }
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
    #ifdef NONOSV
    memory::non_osv_init(kIBDeviceName, kIBPort, kRDMASrvIP, kRDMASrvPort, kIBGidIdx, kTotalSize);
    ddc::remote_init();
    #endif
    long kPageSize = sysconf(_SC_PAGE_SIZE);
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

    /* Prepare the queues */
    for (uint32_t tid = 0; tid < n_threads; tid ++){
        assert(tid < kMaxCPUs);
        write_queues[tid].add_push(kPushBatchSize);
        read_queues[tid].add_fetch(kFetchBatchSize);
    }
    for (uint32_t tid = 0; tid < n_threads; tid ++){
        write_queues[tid].setup();
        read_queues[tid].setup();
    }

    /* populate the region */
    std::cout<<"Before memset"<<std::endl;
    for (uint32_t tid = 0; tid < n_threads; tid ++){
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
            for (uint64_t i = thread_start; i < thread_start + per_thread_size; i++){
                PagesPtr[i] = (uint8_t *)memory::alloc_page();
                for (uint64_t j = 0; j < kPageSize; j += sizeof(char)){
                    *(PagesPtr[i] + j) = (i + j) % 256;
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
            uint64_t while_cnt = 0;
            uint64_t poll_cnt = 0;
            uint64_t cnt = 0;
            uint64_t pushed = 0;
            uint64_t thread_start = tid * per_thread_size;
            unsigned cycles_high_start, cycles_high_end, cycles_low_start, cycles_low_end;
            std::chrono::_V2::system_clock::time_point start_time, end_time;
            ddc::remote_queue &write_queue = write_queues[tid];
            bar.Wait();
            //start_time = std::chrono::system_clock::now();
            //timer_start(&cycles_high_start, &cycles_low_start);
            for (uint32_t iter = 0; iter < n_iter; iter ++){
                for (uint64_t i = thread_start; i < thread_start + per_thread_size; i++){
                    if (latency_test && (cnt % kSampleGap == 0)) {
                        //timer_start(&cycles_high_start, &cycles_low_start);
                        start_time = std::chrono::system_clock::now();
                    }
                    #ifndef NONOSV
                    mmu::phys paddr = mmu::virt_to_phys((void*)PagesPtr[i]);
                    #endif
                    uint64_t offset = i << 12;
                    while_cnt += (pushed >= kPushBatchSize);
                    while (pushed >= kPushBatchSize){
                        poll_until_one(write_queue, pushed, poll_cnt);
                    }
                    assert(pushed < kPushBatchSize);
                    #ifndef NONOSV
                    write_queue.push_vec(0, paddr, PagesPtr[i], offset, kDefaultVec);
                    #else
                    write_queue.push_vec(0, i, PagesPtr[i], offset, kDefaultVec);
                    #endif

                    pushed ++;
                    //assert(pushed == 1);
                    //poll_until_one(write_queue, pushed, poll_cnt);
                    if (latency_test && (cnt % kSampleGap == 0)){
                        //timer_end(&cycles_high_end, &cycles_low_end);
                        end_time = std::chrono::system_clock::now();
                        std::chrono::duration<int64_t, std::nano> elapsed_time = end_time - start_time;
                        latency_sample[n_iter * per_thread_size * tid / kSampleGap + cnt / kSampleGap] = elapsed_time.count();
                        //latency_sample_verify[n_iter * per_thread_size * tid / kSampleGap + cnt / kSampleGap] = get_elapsed_cycles(cycles_high_start, cycles_low_start, cycles_high_end, cycles_low_end);
                    }
                    cnt ++;
                }
            }
            //end_time = std::chrono::system_clock::now();
            //timer_end(&cycles_high_end, &cycles_low_end);
            while_cnt += (pushed >= kPushBatchSize);
            while (pushed >= kPushBatchSize){
                poll_until_one(write_queue, pushed, poll_cnt);
            }
            total_write_poll.fetch_add(poll_cnt, std::memory_order_relaxed);
            total_write_cnt.fetch_add(cnt, std::memory_order_relaxed);
            total_write_poll_while.fetch_add(while_cnt, std::memory_order_relaxed);
            //std::chrono::duration<double, std::micro> elapsed_time = end_time - start_time;
            //std::cout<<"Total time inside (sys): "<<elapsed_time.count()<<std::endl;
            //std::cout<<"Total time inside (rdtsc): "<<get_elapsed_cycles(cycles_high_start, cycles_low_start, cycles_high_end, cycles_low_end) / kFrequency / 1000 << std::endl;
        });
    }
    auto start_time = std::chrono::system_clock::now();
    bar.Wait();
    for (auto& thread: threads){
        thread.join();
    }
    auto end_time = std::chrono::system_clock::now();
    std::chrono::duration<double, std::micro> elapsed_time = end_time - start_time;
    uint64_t count = per_thread_size * n_threads * n_iter;
    double tput = count / elapsed_time.count(); 
    std::cout<<"Total time: "<<elapsed_time.count()<<std::endl;
    std::cout<<"tput w: "<<tput<<" mops"<<std::endl;
    if (latency_test){
        print_latency(latency_sample, count, kSampleGap, 1);
        //print_latency(latency_sample_verify, count, kSampleGap, kFrequency);
    }
    std::cout<<"write poll cnt: "<<total_write_poll<<std::endl;
    std::cout<<"write cnt: "<<total_write_cnt<<std::endl;
    std::cout<<"write while cnt: "<<total_write_poll_while<<std::endl;
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
            ddc::remote_queue &read_queue = read_queues[tid];
            bar.Wait();
            for (uint32_t iter = 0; iter < n_iter; iter ++){
                for (uint64_t i = thread_start; i < thread_start + per_thread_size; i++){
                    if (latency_test && (cnt % kSampleGap == 0)) {
                        //timer_start(&cycles_high_start, &cycles_low_start);
                        start_time = std::chrono::system_clock::now();
                    }
                    #ifndef NONOSV
                    mmu::phys paddr = mmu::virt_to_phys((void*)PagesPtr[i]);
                    #endif
                    uint64_t offset = i << 12;
                    assert(fetched < kFetchBatchSize);
                    #ifndef NONOSV
                    read_queue.fetch_vec(0, paddr, PagesPtr[i], offset, kDefaultVec);
                    #else
                    read_queue.fetch_vec(0, i, PagesPtr[i], offset, kDefaultVec);
                    #endif

                    fetched ++;
                    assert(fetched == 1);
                    poll_until_one(read_queue, fetched, poll_cnt);
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
            total_read_poll.fetch_add(poll_cnt, std::memory_order_relaxed);
            total_read_cnt.fetch_add(cnt, std::memory_order_relaxed);
        });
    }
    start_time = std::chrono::system_clock::now();
    bar.Wait();
    for (auto& thread: threads){
        thread.join();
    }
    end_time = std::chrono::system_clock::now();
    elapsed_time = end_time - start_time;
    tput = count / elapsed_time.count(); 
    std::cout<<"tput r: "<<tput<<" mops"<<std::endl;
    if (latency_test){
        // print_latency(latency_sample, count, kSampleGap, kFrequency);
        print_latency(latency_sample, count, kSampleGap, 1);
    }
    std::cout<<"read poll cnt: "<<total_read_poll<<std::endl;
    std::cout<<"read cnt: "<<total_read_cnt<<std::endl;
    /* Finish read back */

    #ifndef THREAD_PINNING
    j = 0;
    CPU_ZERO(&cpu_set[0]);
    for(j = 0; j < n_threads; j++){
      int cpuid = getCPUid(j,0);
      CPU_SET(cpuid, &cpu_set[0]);
    }
    #endif
    getCPUid(0, 1);

    /* Third mixed read and writes */
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
            uint64_t dummy_cnt = 0;
            uint64_t fetched = 0;
            uint64_t pushed = 0;
            uint64_t thread_start = tid * per_thread_size;
            ddc::remote_queue &read_queue = read_queues[tid];
            ddc::remote_queue &write_queue = write_queues[tid];
            bar.Wait();
            for (uint32_t iter = 0; iter < n_iter; iter ++){
                for (uint64_t i = thread_start; i < thread_start + per_thread_size; i++){
                    /* Another page to evict so no overlapping dma */
                    uint64_t j = (i - thread_start + (per_thread_size >> 1)) % per_thread_size + thread_start;
                    #ifndef NONOSV
                    mmu::phys paddr = mmu::virt_to_phys((void*)PagesPtr[i]);
                    #endif
                    uint64_t offset = i << 12;

                    #ifndef NONOSV
                    mmu::phys paddr_push = mmu::virt_to_phys((void*)PagesPtr[j]);
                    #endif
                    uint64_t offset_push = j << 12;

                    /* Issue fetch request */
                    assert(fetched < kFetchBatchSize);
                    #ifndef NONOSV
                    read_queue.fetch_vec(0, paddr, PagesPtr[i], offset, kDefaultVec);
                    #else
                    read_queue.fetch_vec(0, i, PagesPtr[i], offset, kDefaultVec);
                    #endif
                    fetched ++;
                    assert(fetched == 1);

                    /* Issue push request */
                    while (pushed >= kPushBatchSize){
                        poll_until_one(write_queue, pushed, dummy_cnt);
                    }
                    assert(pushed < kPushBatchSize);
                    #ifndef NONOSV
                    write_queue.push_vec(0, paddr_push, PagesPtr[j], offset_push, kDefaultVec);
                    #else
                    write_queue.push_vec(0, j, PagesPtr[j], offset_push, kDefaultVec);
                    #endif

                    pushed ++;

                    poll_until_one(read_queue, fetched, dummy_cnt);
                }
            }
        });
    }
    start_time = std::chrono::system_clock::now();
    bar.Wait();
    for (auto& thread: threads){
        thread.join();
    }
    end_time = std::chrono::system_clock::now();
    elapsed_time = end_time - start_time;
    tput = count / elapsed_time.count(); 
    std::cout<<"tput rw: "<<tput<<" mops"<<std::endl;
    /* Finish mixed workload */

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