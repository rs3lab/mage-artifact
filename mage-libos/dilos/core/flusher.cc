/*
 * Copyright (C) 2014 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <chrono>

#include <osv/migration-lock.hh>
#include <osv/sched.hh>
#include <osv/interrupt.hh>
#include <osv/debug.hh>
#include <osv/clock.hh>
#include <osv/trace.hh>
#include <osv/percpu.hh>
#include <osv/flusher.hh>
#include <osv/mmu-defs.hh>



static std::atomic<unsigned int> _active_cpus {0};
static std::atomic<bool> _started;
static unsigned int _n_cpus;
static sched::thread_handle _controller;
static mutex _control_lock;

class cpu_flusher : public sched::timer_base::client {
private:
    sched::timer_base _timer;
    const std::chrono::nanoseconds flush_interval = std::chrono::nanoseconds(20 * 1000);
    bool _active;

    void rearm()
    {
        _timer.set(flush_interval);
    }

public:
    unsigned id;
    cpu_flusher()
        : _timer(*this)
        , _active(false)
    {
    }

    void timer_fired()
    {
        mmu::flush_tlb_check_and_flush(id);
        //debug_early_u64("", id);
        rearm();
    }

    void start()
    {
        assert(!_active);
        _active = true;
        rearm();
    }

    void stop()
    {
        assert(_active);
        _active = false;
        _timer.cancel();
    }

    bool is_active()
    {
        return _active;
    }
};

static dynamic_percpu<cpu_flusher> _flusher;

template <typename T>
static bool fetch_and_inc_if_less(std::atomic<T>& var, T& previous, T max_value)
{
    do {
        previous = var;
        if (previous >= max_value) {
            return false;
        }
    } while (!var.compare_exchange_strong(previous, previous + 1));

    return true;
}

static void start_on_current()
{
    unsigned int prev_active;
    if (!fetch_and_inc_if_less(_active_cpus, prev_active, _n_cpus)) {
        // Rare race: this CPU was brought up after sampling was initiated.
        return;
    }
    _flusher->id =  sched::cpu::current() -> id;

    _flusher->start();

    if (prev_active + 1 == _n_cpus) {
        _started = true;
        _controller.wake();
    }
}

static void stop_on_current()
{
    if (!_flusher->is_active()) {
        return;
    }

    _flusher->stop();
    
    if (--_active_cpus == 0){
        _controller.wake();
    }
}

static inter_processor_interrupt start_flusher_ipi { IPI_FLUSHER_START, [] { start_on_current(); }};
static inter_processor_interrupt stop_flusher_ipi { IPI_FLUSHER_STOP, [] { stop_on_current(); }};

template<typename Duration>
static long to_nanoseconds(Duration duration)
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

void start_flusher() throw()
{
    SCOPE_LOCK(_control_lock);

    if (_started) {
        stop_flusher();
        assert(!_started);
    }

    debug("Starting flusher");

    _controller.reset(*sched::thread::current());

    assert(_active_cpus == 0);


    _n_cpus = sched::cpus.size();
    std::atomic_thread_fence(std::memory_order_release);

    WITH_LOCK(migration_lock) {
        start_on_current();
        start_flusher_ipi.send_allbutself();
    }

    sched::thread::wait_until([] { return _started.load(); });
    _controller.clear();

    debug("Sampler started.\n");
}

void stop_flusher() throw()
{
    SCOPE_LOCK(_control_lock);

    if (!_started) {
        return;
    }

    debug("Stopping sampler\n");
    _controller.reset(*sched::thread::current());

    WITH_LOCK(migration_lock) {
        stop_flusher_ipi.send_allbutself();
        stop_on_current();
    }

    sched::thread::wait_until([] { return _active_cpus == 0; });
    _controller.clear();

    _started = false;
    debug("Sampler stopped.\n");
}
