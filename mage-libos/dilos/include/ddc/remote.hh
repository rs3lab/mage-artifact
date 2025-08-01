#pragma once
#include <stdint.h>
#include <stdlib.h>

#include <boost/circular_buffer.hpp>
#include <initializer_list>
#include <vector>
// Note: Remote moudle would include and implement this (not remote server)

extern std::string opt_rdma_mode;
namespace ddc {

constexpr size_t max_req_per_type = 128;
constexpr size_t max_req_init = max_req_per_type;
constexpr size_t max_req_page = max_req_per_type;
constexpr size_t max_req_subpage = max_req_per_type;
constexpr size_t max_syscall = max_req_per_type;
constexpr size_t max_evict = max_req_per_type;

// Current max_evict must be greater or equal to pipeline_batch_size
constexpr size_t current_max_evict = 256;
constexpr size_t pipeline_batch_size = current_max_evict;

void remote_init();
class remote_queue {
   public:
    remote_queue(std::initializer_list<int> fetch_sizes,
                 std::initializer_list<int> push_sizes)
        : cq(NULL), fetch_total(0), push_total(0) {
        for (auto &_size : fetch_sizes) {
            fetch_qp.push_back({NULL, 0, _size});
        }
        for (auto &_size : push_sizes) {
            push_qp.push_back({NULL, 0, _size});
        }
    }
    // This is only used for per cpu eviction queue!
    remote_queue(): cq(NULL), fetch_qp(0), push_qp(0), fetch_total(0), push_total(0){   
    }
    // This is only used for initializing per cpu eviction queue!
    void add_push(int push_size){
        push_qp.push_back({NULL, 0, push_size});
    }
    void add_fetch(int fetch_size){
        fetch_qp.push_back({NULL, 0, fetch_size});
    }

    void setup();
    ~remote_queue();
    bool fetch(int qp_id, uintptr_t token, void *paddr, uintptr_t offset,
               size_t size);
    bool push(int qp_id, uintptr_t token, void *paddr, uintptr_t offset,
              size_t size);

    // NOTE: Try to avoid any dynamic allocation
    template<std::size_t N>
    bool push_multiple(int qp_id, int n, const std::array<uintptr_t, N>& tokens,
        const std::array<void*, N> &paddrs, const std::array<uintptr_t, N>& offsets, 
        size_t size);

    // support only single frame
    bool fetch_vec(int qp_id, uintptr_t token, void *paddr, uintptr_t offset,
                   uintptr_t bitmask);
    bool push_vec(int qp_id, uintptr_t token, void *paddr, uintptr_t offset,
                  uintptr_t bitmask);

    int poll(uintptr_t tokens[], int len);

    void get_stat(size_t &fetch_total, size_t &push_total) {
        fetch_total = this->fetch_total;
        push_total = this->push_total;
    }

    void *cq;
    struct entry_t {
        uintptr_t token;
        uint64_t cycle;
    };

    struct qp_t {
        void *inner;
        int current;
        int max;
        // boost::circular_buffer<entry_t> cycles;
        // qp_t(void *inner, int current, int max)
        //     : inner(inner), current(current), max(max), cycles(max) {}
    };

    std::vector<qp_t> fetch_qp;
    std::vector<qp_t> push_qp;
    boost::circular_buffer<entry_t> complete;
    size_t fetch_total;
    size_t push_total;
};  // namespace ddc

size_t remote_reserve_size();
};  // namespace ddc
