
#include <ddc/prefetch.h>

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <osv/trace.hh>

// #define NO_USE_HIT

TRACEPOINT(trace_ddc_prefetcher_readahead_event, "faultaddr=%p hits=%ld, prev=%p, addr=%p, f=%ld",
           uintptr_t, size_t, uintptr_t, uintptr_t, size_t);

namespace ddc {

static constexpr int max_pages = (1 << 3);

/*
!NOTE:
Unlike Linux, our implementation does not have a SWAP CACHE, so we use 
the accessed bit to check if a prefetched page is a hit, which can be a 
bit inaccurate.
Also, if it is local at the time of prefetch, we do not walk the PT, so 
we cannot access the accessed bit, which is treated as a hit.
*/

int handler_readahead(const ddc_event_t *event) {
    if (event->type != ddc_event_t::DDC_EVENT_PREFETCH_START) return 0;

    static __thread uintptr_t prev_fault_addr = 0;
    static __thread unsigned long prev_win = 0;

    uintptr_t fault_addr = event->fault_addr;

    unsigned long offset = fault_addr >> 12;
    unsigned long prev_offset = prev_fault_addr >> 12;

    bool increasing = offset > prev_offset ? true : false;

#ifdef NO_USE_HIT
    unsigned long offset_change =
        offset > prev_offset ? offset - prev_offset : prev_offset - offset;
    unsigned int hits = 0;
    assert(offset_change > 0);

    if (offset_change == prev_win) {
        hits = prev_win;
    }
#else
    unsigned int hits = event->start.hits;
#endif

    /*
     * This heuristic has been found to work well on both sequential and
     * random loads, swapping to hard disk or to SSD: please don't ask
     * what the "+ 2" means, it just happens to work well, that's all.
     */
    unsigned int pages = hits + 2;
    unsigned int last_ra = 0;

    if (pages == 2) {
        /*
         * We can have no readahead hits to judge by: but must not get
         * stuck here forever, so check for an adjacent offset instead
         * (and don't even bother to check whether swap type is same).
         */
        if (offset != prev_offset + 1 && offset != prev_offset - 1) pages = 1;

    } else {
        unsigned int roundup = 4;
        while (roundup < pages) roundup <<= 1;
        pages = roundup;
    }

    if (pages > max_pages) pages = max_pages;

    /* Don't shrink readahead too fast */
    last_ra = prev_win / 2;
    if (pages < last_ra) pages = last_ra;

    assert(pages > 0);
    unsigned int i = 1;

    trace_ddc_prefetcher_readahead_event(
        event->fault_addr, event->start.hits, prev_fault_addr, fault_addr, pages);
    for (i = 1; i < pages; ++i) {
        uintptr_t prefetch_addr =
            increasing ? ((offset + i) << 12) : ((offset - i) << 12);

        ddc_prefetch_t command;
        command.type = ddc_prefetch_t::DDC_PREFETCH_PAGE;
        command.addr = prefetch_addr;

        ddc_prefetch_result_t ret = ddc_prefetch(event, &command);

        if (ret == DDC_RESULT_ERR_QP_FULL) {
            break;
        }

        if (ret != DDC_RESULT_OK_ISSUED && ret != DDC_RESULT_OK_LOCAL &&
            ret != DDC_RESULT_OK_PROCESSING && ret != DDC_RESULT_ERR_NOT_DDC) {
            abort();
        }
    }

    prev_fault_addr = fault_addr;
    prev_win = i;

    return 0;
}
}  // namespace ddc
