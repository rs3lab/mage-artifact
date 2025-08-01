#include <slab.hpp>
#include <sched.h>

namespace far_memory {

Slab::Slab(uint8_t *base, uint64_t len) : base_(base), len_(len), cur_(base) {}

Slab::~Slab() {}

void Slab::replenish(uint32_t slab_idx, int cpu_id) {
  mutex_.lock();
  auto guard = helpers::finally([&]() { mutex_.unlock(); });

  auto slab_size = get_slab_size(slab_idx);
  for (uint32_t i = 0;
       i < kReplenishChunkSize && cur_ + slab_size <= base_.get() + len_;
       i++, cur_ += slab_size) {
    slabs_[cpu_id][slab_idx].push_back(cur_);
  }
}

uint8_t *Slab::allocate(uint32_t size) {
  int cpu_id = sched_getcpu();
  slabs_mutex_[cpu_id].lock();

  auto guard = helpers::finally([&]() { slabs_mutex_[cpu_id].unlock(); });

  auto slab_idx = get_slab_idx(size);
  auto &slab = slabs_[cpu_id][slab_idx];
  if (unlikely(slab.empty())) {
    replenish(slab_idx, cpu_id);
    if (unlikely(slab.empty())) {
      return nullptr;
    }
  }
  auto ret = slab.back();
  slab.pop_back();
  return ret;
}

void Slab::free(uint8_t *ptr, uint32_t size) {
  int cpu_id = sched_getcpu();
  slabs_mutex_[cpu_id].lock();
  auto guard = helpers::finally([&]() { slabs_mutex_[cpu_id].unlock(); });

  auto slab_idx = get_slab_idx(size);
  slabs_[cpu_id][slab_idx].push_back(ptr);
}

} // namespace far_memory
