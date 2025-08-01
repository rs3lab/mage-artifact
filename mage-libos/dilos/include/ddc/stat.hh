#pragma once
#include <cstddef>
#include <osv/debug.hh>
#include <osv/preempt-lock.hh>
#include <osv/sched.hh>

//#define __PER_CORE_PRINT_STAT

// Since we cannot get the perf for foreground threads only
// I use this temporary hack to get the stats for foreground 
// threads and background threads. BUT ONLY WHEN THE 
// FOREGROUND THREADS ARE PINNED!!
#define FOREGROUND_BACKGROUND_SEPARATION
#define MAX_RECLAIM_THREADS 2
namespace ddc {
constexpr size_t max_cpu = 56;

namespace stat {

constexpr size_t max_stats = 128;

class stat_t {
   public:
    virtual void print_stat() = 0;
};

class stat_list_t {
   public:
    void add(stat_t *stat) {
        debug_early_u64("new stat...: ", next);
        assert(next < max_stats);
        inner[next] = stat;
        ++next;
    }
    void print_stat() {
        debug_early("print stat...\n");
        for (size_t i = 0; i < next; ++i) {
            inner[i]->print_stat();
        }
    }

   private:
    std::array<stat_t *, max_stats> inner;
    size_t next = 0;
};

extern stat_list_t stat_list;

class counter_t : public stat_t {
   public:
    counter_t(const char *name) : name(name) {
        assert(sched::cpus.size() <= max_cpu);
        stat_list.add(this);
    }
    void inline count() {
        SCOPE_LOCK(preempt_lock);
        ++inner[sched::current_cpu->id];
    }

    virtual void print_stat() override {
        debug_early(name);
        debug_early(" (counter):\n");
        size_t sum = 0;
        for (size_t i = 0; i < sched::cpus.size(); ++i) {
            #ifdef __PER_CORE_PRINT_STAT
            debug_early_u64("  this core:  ", inner[i]);
            #endif
            sum += inner[i];
        }
        debug_early(name);
        debug_early_u64(" total: ", sum);
        debug_early("\n");
    }

   private:
    std::array<size_t, ddc::max_cpu> inner;
    const char *name;
};

class elapsed_t : public stat_t {
   public:
    elapsed_t(const char *name) : name(name) {
        assert(sched::cpus.size() <= ddc::max_cpu);
        stat_list.add(this);
    }
    void inline to(uint64_t token) {
        auto tock = processor::ticks();
        SCOPE_LOCK(preempt_lock);
        sum[sched::current_cpu->id] += (tock - token);
        ++count[sched::current_cpu->id];
    }

    virtual void print_stat() override {
        debug_early(name);
        debug_early(" (elapsed):\n");
        size_t sum_sum = 0;
        size_t sum_count = 0;
        for (size_t i = 0; i < sched::cpus.size(); ++i) {
            #ifdef __PER_CORE_PRINT_STAT
            if (count[i]){
                debug_early_u64("  this core avg:  ", sum[i] / count[i]);
                debug_early_u64("  this core total:  ", sum[i]);
                debug_early_u64("  this core count:  ", count[i]);
            }
            else
            {
                debug_early("  this core avg:  nil\n");
                debug_early("  this core total:  nil\n");
                debug_early("  this core count:  nil\n");
            }
            #endif
            sum_sum += sum[i];
            sum_count += count[i];
        }
        debug_early(name);
        if (sum_count) {
            debug_early_u64(" total avg:  ", sum_sum / sum_count);
            debug_early_u64(" total total:  ", sum_sum);
            debug_early_u64(" total count:  ", sum_count);
        }else{
            debug_early("  total avg:  nil\n");
            debug_early("  total total:  nil\n");
            debug_early("  total count:  nil\n");
        }
        #ifdef FOREGROUND_BACKGROUND_SEPARATION
        debug_early("\n");
        debug_early(name);
        debug_early(" Foreground");
        size_t sum_sum_background = 0;
        size_t sum_count_background = 0;
        for (size_t i = sched::cpus.size() - 1; i >= sched::cpus.size() - MAX_RECLAIM_THREADS; i-- ){
            sum_sum_background += sum[i];
            sum_count_background += count[i];
        }
        if (sum_count > sum_count_background) {
            debug_early_u64(" total avg:  ", (sum_sum - sum_sum_background) / (sum_count - sum_count_background));
            debug_early_u64(" total total:  ", sum_sum - sum_sum_background);
            debug_early_u64(" total count:  ", sum_count - sum_count_background);
        }else{
            debug_early("  total avg:  nil\n");
            debug_early("  total total:  nil\n");
            debug_early("  total count:  nil\n");
        }
        debug_early("\n");
        
        debug_early(name);
        debug_early(" Background");
        if (sum_count_background) {
            debug_early_u64(" total avg:  ", (sum_sum_background) / (sum_count_background));
            debug_early_u64(" total total:  ", sum_sum_background);
            debug_early_u64(" total count:  ", sum_count_background);
        }else{
            debug_early("  total avg:  nil\n");
            debug_early("  total total:  nil\n");
            debug_early("  total count:  nil\n");
        }

        #endif
        debug_early("\n");
    }

   private:
    std::array<size_t, ddc::max_cpu> sum;
    std::array<size_t, ddc::max_cpu> count;
    const char *name;
};

class avg_t : public stat_t {
   public:
    avg_t(const char *name) : name(name) {
        assert(sched::cpus.size() <= ddc::max_cpu);
        stat_list.add(this);
    }
    void inline add(uint64_t value) {
        SCOPE_LOCK(preempt_lock);
        sum[sched::current_cpu->id] += value;
        ++count[sched::current_cpu->id];
    }

    virtual void print_stat() override {
        debug_early(name);
        debug_early(" (avg):\n");
        size_t sum_sum = 0;
        size_t sum_count = 0;
        for (size_t i = 0; i < sched::cpus.size(); ++i) {
            #ifdef __PER_CORE_PRINT_STAT
            if (count[i]){
                debug_early_u64("  this core avg:  ", sum[i] / count[i]);
                debug_early_u64("  this core total:  ", sum[i]);
                debug_early_u64("  this core count:  ", count[i]);
            }
            else
            {
                debug_early("  this core avg:  nil\n");
                debug_early("  this core total:  nil\n");
                debug_early("  this core count:  nil\n");
            }
            #endif
            sum_sum += sum[i];
            sum_count += count[i];
        }
        debug_early(name);
        if (sum_count) {
            debug_early_u64(" total avg:  ", sum_sum / sum_count);
            debug_early_u64(" total total:  ", sum_sum);
            debug_early_u64(" total count:  ", sum_count);
        }else{
            debug_early("  total avg:  nil\n");
            debug_early("  total total:  nil\n");
            debug_early("  total count:  nil\n");
        }
        debug_early("\n");
        
    }

   private:
    std::array<size_t, ddc::max_cpu> sum;
    std::array<size_t, ddc::max_cpu> count;
    const char *name;
};

}  // namespace stat

}  // namespace ddc

#define NO_STAT

#ifndef NO_STAT
#define STAT_COUNTER(name)                                            \
    static ::ddc::stat::counter_t __attribute__((init_priority(300))) \
    __counter__##name(#name);
#define STAT_COUNTING(name) __counter__##name.count();

#define STAT_ELAPSED(name)                                            \
    static ::ddc::stat::elapsed_t __attribute__((init_priority(300))) \
    __elapsed__##name(#name);

// Change from processor::ticks() 
#define STAT_ELAPSED_FROM() processor::ticks()
#define STAT_ELAPSED_TO(name, token) __elapsed__##name.to(token);

#define STAT_ELAPSED_FROM_WRAP(name)                              \
    auto __tic__##name = processor::ticks();

#define STAT_ELAPSED_TO_WRAP(name) __elapsed__##name.to(__tic__##name);

#define STAT_AVG(name)                                            \
    static ::ddc::stat::avg_t __attribute__((init_priority(300))) \
    __avg__##name(#name);
#define STAT_AVG_ADD(name, value) __avg__##name.add(value);

#else

#define STAT_COUNTER(name)
#define STAT_COUNTING(name)
#define STAT_ELAPSED(name)
#define STAT_ELAPSED_FROM() 0
#define STAT_ELAPSED_TO(name, token)
#define STAT_ELAPSED_FROM_WRAP(name)
#define STAT_ELAPSED_TO_WRAP(name)
#define STAT_AVG(name)
#define STAT_AVG_ADD(name, value)

#endif