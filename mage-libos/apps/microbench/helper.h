#pragma once
#include <mutex>
#include <condition_variable>
#include "numa-config.h"

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

int getCPUid(int index, bool reset);

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