#include "osv/mcs_lock.h"
#include "osv/spinlock.h"
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include "numa-config.h"

#define THREAD_PINNING
#define CRITICAL_SECTION 3000

spinlock_t lock;
uint64_t value;

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

int main(){
    std::vector<std::thread> threads;
    uint64_t n_threads = 56;
    Barrier bar(n_threads + 1);
    #ifndef THREAD_PINNING
    uint32_t j = 0;
    CPU_ZERO(&cpu_set[0]);
    for(j = 0; j < n_threads; j++){
      int cpuid = getCPUid(j,0);
      CPU_SET(cpuid, &cpu_set[0]);
    }
    getCPUid(0, 1);
    #endif
    for (uint64_t tid = 0; tid < n_threads; tid++){
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
                bar.Wait();
                for (uint64_t iter = 0; iter < 10000; iter ++){
                    lock.lock();
                    value ++;
                    uint64_t cs_length = CRITICAL_SECTION;
                    while(cs_length --)
                        asm volatile("rep; nop");
                    lock.unlock();
                }
            }
        );
    }
    bar.Wait();
    auto start_time = std::chrono::system_clock::now();
    for (auto& thread: threads){
        thread.join();
    }
    auto end_time = std::chrono::system_clock::now();
    std::chrono::duration<double, std::micro> elapsed_time = end_time - start_time;
    printf("%llu\n", value);
    std::cout<<"Duration: "<<elapsed_time.count()<<" us"<<std::endl;
}