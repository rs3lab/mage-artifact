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

constexpr static uint64_t kTotalSize = 20UL * 1024 * 1024 * 1024; /* 20GB total size */
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
static uint8_t *PagesPtrAsync[kTotalPages];

/* Because in our code, the page fault and reclamation are using different queueus */
/* We also use two different vectors here */
std::array<ddc::remote_queue, kMaxCPUs> write_queues;
std::array<ddc::remote_queue, kMaxCPUs> read_queues;
std::array<ddc::remote_queue, kMaxCPUs> async_write_queues;

std::atomic<uint64_t> total_write_poll;
std::atomic<uint64_t> total_read_poll;
std::atomic<uint64_t> total_write_cnt;
std::atomic<uint64_t> total_read_cnt;
std::atomic<uint64_t> total_write_async_cnt;
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

size_t poll_once_pipelined(ddc::remote_queue &rq, size_t &pushed, uint64_t &cnt, uintptr_t token){
    if (!pushed) return 0;
    uintptr_t tokens[pushed];
    int polled = rq.poll(tokens, pushed);
    cnt++;
    pushed -= polled;
    assert(pushed >= 0);
    assert(tokens[0] == token);
    //std::cout<<tokens[0]<<" "<<token<<std::endl;
    return polled;
}

void poll_until_one_pipelined(ddc::remote_queue &rq, size_t &pushed, uint64_t &cnt, uintptr_t token){
    size_t polled = poll_once_pipelined(rq, pushed, cnt, token);
    while ((pushed != 0 ) && (!polled)) {
        __asm volatile ("pause" ::: );
        polled = poll_once_pipelined(rq, pushed, cnt, token);
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
/* Then read from the remote side while write to the same region at the same time*/
int test(uint32_t n_threads, 
    uint32_t n_async_threads, 
    uint32_t async_io_depth, 
    uint32_t n_iter, 
    bool latency_test,
    bool chained_write){
    #ifdef NONOSV
    memory::non_osv_init(kIBDeviceName, kIBPort, kRDMASrvIP, kRDMASrvPort, kIBGidIdx, kTotalSize * 2);
    ddc::remote_init();
    #endif
    long kPageSize = sysconf(_SC_PAGE_SIZE);
    uint64_t per_thread_size = kTotalPages / n_threads;
    uint64_t per_async_thread_size = n_async_threads ?  kTotalPages / n_async_threads : 0;
    uint64_t latency_sample_size = 
        (per_thread_size * n_threads * n_iter + kSampleGap) / kSampleGap * sizeof(uint64_t);
    uint64_t *latency_sample;
    //uint64_t *latency_sample_verify;
    if (latency_test){
        latency_sample = reinterpret_cast<uint64_t*>(memalign(64, latency_sample_size));
        //latency_sample_verify = reinterpret_cast<uint64_t*>(memalign(64, latency_sample_size));
    }
    std::vector<std::thread> threads;
    std::vector<std::thread> async_reclaim_threads;
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
    for (uint32_t tid = 0; tid < n_async_threads; tid++){
        if (chained_write)
            async_write_queues[tid].add_push(ddc::current_max_evict);
        else
            async_write_queues[tid].add_push(async_io_depth);
        async_write_queues[tid].setup();
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
                PagesPtrAsync[i] = (uint8_t *)memory::alloc_page();
                for (uint64_t j = 0; j < kPageSize; j += sizeof(char)){
                    *(PagesPtr[i] + j) = (i + j) % 256;
                    *(PagesPtrAsync[i] + j) = (i + j) % 256;
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
    uint64_t async_count = per_async_thread_size * n_async_threads * n_iter;
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

    if (chained_write){
        std::cout<<"Chained Write uses batch size: "<<ddc::current_max_evict<<std::endl;
    }
    /* Second read back and async write concurrently*/
    threads.clear();
    volatile bool g_stop = false;
    /* Push all read threads */
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
            while (!g_stop){
                for (uint32_t iter = 0; iter < n_iter && !g_stop; iter ++){
                    for (uint64_t i = thread_start; i < thread_start + per_thread_size && !g_stop; i++){
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
            }
            total_read_poll.fetch_add(poll_cnt, std::memory_order_relaxed);
            total_read_cnt.fetch_add(cnt, std::memory_order_relaxed);
        });
    }
    
    Barrier async_bar(n_async_threads + 1);
    /* Push all async reclaim threads */
    for (uint32_t tid = 0; tid < n_async_threads; tid++){
        #ifdef THREAD_PINNING
        // Should be after 48
        int cpuid = getCPUid(tid + n_threads,0);
        CPU_ZERO(&cpu_set[tid + n_threads]);
        CPU_SET(cpuid, &cpu_set[tid + n_threads]);
        #endif
        async_reclaim_threads.emplace_back([&, tid](){
            #ifdef THREAD_PINNING
            sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set[tid + n_threads]);
            #else
            sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set[0]);
            #endif
            uint64_t while_cnt = 0;
            uint64_t poll_cnt = 0;
            uint64_t cnt = 0;
            uint64_t pushed = 0;
            uint64_t thread_start = tid * per_async_thread_size;
            std::array<uintptr_t, ddc::pipeline_batch_size> token_array;
            std::array<void*, ddc::pipeline_batch_size> virt_array;
            std::array<uintptr_t, ddc::pipeline_batch_size> offset_array;
            ddc::remote_queue &write_queue = async_write_queues[tid];
            async_bar.Wait();
            while (!g_stop){
                for (uint32_t iter = 0; iter < n_iter && !g_stop; iter ++){
                    if (!chained_write){
                        for (uint64_t i = thread_start; i < thread_start + per_async_thread_size && !g_stop; i++){
                            #ifndef NONOSV
                            mmu::phys paddr = mmu::virt_to_phys((void*)PagesPtrAsync[i]);
                            #endif
                            uint64_t offset = (i << 12) + kTotalSize;
                            while_cnt += (pushed >= async_io_depth);
                            while (pushed >= async_io_depth){
                                poll_until_one(write_queue, pushed, poll_cnt);
                            }
                            assert(pushed < async_io_depth);
                            #ifndef NONOSV
                            write_queue.push_vec(0, paddr, PagesPtrAsync[i], offset, kDefaultVec);
                            #else
                            write_queue.push_vec(0, i + kTotalPages, PagesPtrAsync[i], offset, kDefaultVec);
                            #endif

                            pushed ++;
                            cnt ++;
                        }
                        while_cnt += (pushed >= async_io_depth);
                        while (pushed >= async_io_depth){
                            poll_until_one(write_queue, pushed, poll_cnt);
                        }
                    } else {
                        for (uint64_t i = thread_start; 
                            i + ddc::current_max_evict < thread_start + per_async_thread_size && !g_stop; 
                            i += ddc::current_max_evict){
                            // std::cout<<i<<" "<<
                            // token_array[0]<<" "<<
                            // token_array[1]<<" "<<
                            // token_array[ddc::current_max_evict - 1]<<std::endl;
                            poll_until_one_pipelined(write_queue, pushed, poll_cnt, token_array[ddc::current_max_evict - 1]);
                            for (uint64_t j = 0; j < ddc::current_max_evict; j++){
                            
                                #ifndef NONOSV
                                token_array[j] = mmu::virt_to_phys((void*)PagesPtrAsync[i + j]);
                                virt_array[j] = PagesPtrAsync[i + j];
                                offset_array[j] = ((i + j) << 12) + kTotalSize;
                                #else
                                token_array[j] = i + j + kTotalPages;
                                virt_array[j] = PagesPtrAsync[i + j];
                                offset_array[j] = ((i + j) << 12) + kTotalSize;
                                #endif
                            }
                            assert(pushed == 0);
                            write_queue.push_multiple(0, ddc::current_max_evict, 
                                token_array, virt_array, offset_array, 4096UL);
                            pushed ++;
                            cnt += ddc::current_max_evict;
                        }
                        poll_until_one_pipelined(write_queue, pushed, poll_cnt, token_array[ddc::current_max_evict - 1]);
                    }
                }
            }
            total_write_async_cnt += cnt;
        });
    }

    Barrier main_bar(2);

    /* Start another async thread to monitor the async relcaim threads */
    auto monitor_trd = std::thread([&](){
        auto local_start_time = std::chrono::system_clock::now();
        async_bar.Wait();
        main_bar.Wait();
        for (auto& thread: async_reclaim_threads){
            thread.join();
        }
        auto local_end_time = std::chrono::system_clock::now();
        std::chrono::duration<double, std::micro> local_elapsed_time = local_end_time - local_start_time;
        double local_tput = total_write_async_cnt * 1.0 / local_elapsed_time.count();
        std::cout<<"tput async w: "<<local_tput<<" mops"<<std::endl;
        std::cout<<"async write cnt: "<<async_count<<std::endl;
        std::cout<<"async write time: "<<local_elapsed_time.count() << std::endl;
    });

    start_time = std::chrono::system_clock::now();
    bar.Wait();
    main_bar.Wait();
    sleep(5);
    g_stop = true;
    for (auto& thread: threads){
        thread.join();
    }
    end_time = std::chrono::system_clock::now();
    elapsed_time = end_time - start_time;
    tput = total_read_cnt / elapsed_time.count(); 
    std::cout<<"tput r: "<<tput<<" mops"<<std::endl;
    monitor_trd.join();
    if (latency_test){
        // print_latency(latency_sample, count, kSampleGap, kFrequency);
        print_latency(latency_sample, count, kSampleGap, 1);
    }
    std::cout<<"read poll cnt: "<<total_read_poll<<std::endl;
    std::cout<<"read cnt: "<<total_read_cnt<<std::endl;
    /* Finish read back */
    return 0;
}

// This benchmark simulates a case where t threads are faulting 
// while w threads are performing RDMA writes with an IO depth 
// of p
int main(int argc, char **argv){
    uint32_t n_threads = 1;
    uint32_t n_async_threads = 1;
    uint32_t async_io_depth = 32;
    uint32_t n_iter = 1;
    bool latency_test = false;
    bool chained_write = false;
    int c;
    while ((c = getopt(argc, argv, "cdt:i:w:p:")) != -1){
        switch (c)
        {
        case 't':
            n_threads = strtoul(optarg, NULL, 10);
            break;
        case 'w':
            n_async_threads = strtoul(optarg, NULL, 10);
            break;
        case 'i':
            n_iter = strtoul(optarg, NULL, 10);
            break;
        case 'p':
            async_io_depth = strtoul(optarg, NULL, 10);
            break;
        case 'd':
            latency_test = true;
            break;
        case 'c':
            chained_write = true;
            break;
        default:
            std::cerr<<"Unknown option "<<c<<std::endl;
            exit(-1);
        } 
    }
    test(n_threads, n_async_threads, async_io_depth, n_iter, 
        latency_test, chained_write);
    return 0;
}