#include "helpers.hpp"
#include "snappy.h"
#include "zipf.hpp"
#include "local_concurrent_hopscotch.hpp"
#include "numa-config.h"


#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <random>
#include <vector>
#include <thread>

using namespace far_memory;

#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

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

namespace far_memory {
class FarMemTest {
public:
  constexpr static uint32_t kNumMutatorThreads = 48;
  constexpr static uint32_t kThreadSleepTime = 0;
private:
  // Hashtable.
  constexpr static uint32_t kKeyLen = 12;
  constexpr static uint32_t kValueLen = 4;
  constexpr static uint32_t kLocalHashTableNumEntriesShift = 28;
  constexpr static uint32_t kNumKVPairs = 1 << 27;
  constexpr static uint64_t kHTDataSize = 2ULL * (kNumKVPairs) * (3 + kKeyLen + kValueLen); // KVDataHeader = 3

  // Array.
  constexpr static uint32_t kNumArrayEntries = 2 << 20; // 2 M entries.
  constexpr static uint32_t kArrayEntrySize = 8192;     // 8 K

  // Runtime.
  constexpr static double kZipfParamS = 0.85;
  constexpr static uint32_t kNumKeysPerRequest = 32;
  constexpr static uint32_t kNumReqs = kNumKVPairs / kNumKeysPerRequest;
  constexpr static uint32_t kLog10NumKeysPerRequest =
      helpers::static_log(10, kNumKeysPerRequest);
  constexpr static uint32_t kReqLen = kKeyLen - kLog10NumKeysPerRequest;
  constexpr static uint32_t kReqSeqLen = kNumReqs;

  // Output.
  constexpr static uint32_t kPrintPerIters = 8192;
  constexpr static uint32_t kMaxPrintIntervalUs = 1000 * 1000; // 1 second(s).
  constexpr static uint32_t kPrintTimes = 10;
  constexpr static uint32_t kLatsWinSize = 1 << 12;

  struct Req {
    char data[kReqLen];
  };

  struct Key {
    char data[kKeyLen];
  };

  union Value {
    uint32_t num;
    char data[kValueLen];
  };

  struct ArrayEntry {
    uint8_t data[kArrayEntrySize];
  };

  struct alignas(64) Cnt {
    uint64_t c;
  };

  bool run_flag = 1;
  cpu_set_t cpu_set[500]; // Up to 500 threads;
  std::unique_ptr<std::mt19937> generators[helpers::kNumCPUs];
  alignas(helpers::kPageSize) Req all_gen_reqs[kNumReqs];
  uint32_t all_zipf_req_indices[helpers::kNumCPUs][kReqSeqLen];

  Cnt req_cnts[kNumMutatorThreads];
  uint32_t lats[helpers::kNumCPUs][kLatsWinSize];
  Cnt lats_idx[helpers::kNumCPUs];
  Cnt per_core_req_idx[helpers::kNumCPUs];

  std::atomic_flag flag;
  uint64_t print_times = 0;
  uint64_t prev_sum_reqs = 0;
  std::chrono::time_point<std::chrono::system_clock> prev_us;
  std::vector<double> mops_records;
  
  std::string compress_result[kNumMutatorThreads];

  inline void append_uint32_to_char_array(uint32_t n, uint32_t suffix_len,
                                          char *array) {
    uint32_t len = 0;
    while (n) {
      auto digit = n % 10;
      array[len++] = digit + '0';
      n = n / 10;
    }
    while (len < suffix_len) {
      array[len++] = '0';
    }
    std::reverse(array, array + suffix_len);
  }

  inline void random_string(char *data, uint32_t len, uint32_t tid) {
    BUG_ON(len <= 0);
    BUG_ON(tid >= helpers::kNumCPUs);
    auto &generator = *generators[tid];
    std::uniform_int_distribution<int> distribution('a', 'z' + 1);
    for (uint32_t i = 0; i < len; i++) {
      data[i] = char(distribution(generator));
    }
  }

  inline void random_req(char *data, uint32_t tid) {
    auto tid_len = helpers::static_log(10, kNumMutatorThreads);
    random_string(data, kReqLen - tid_len, tid);
    append_uint32_to_char_array(tid, tid_len, data + kReqLen - tid_len);
  }

  inline uint32_t random_uint32(uint32_t tid) {
    BUG_ON(tid >= helpers::kNumCPUs);
    auto &generator = *generators[tid];
    std::uniform_int_distribution<uint32_t> distribution(
        0, std::numeric_limits<uint32_t>::max());
    return distribution(generator);
  }

  void prepare(LocalGenericConcurrentHopscotch *hopscotch) {
    for (uint32_t i = 0; i < helpers::kNumCPUs; i++) {
      std::random_device rd;
      generators[i].reset(new std::mt19937(rd()));
    }
    memset(lats_idx, 0, sizeof(lats_idx));
    std::vector<std::thread> threads;
    for (uint32_t tid = 0; tid < kNumMutatorThreads; tid++) {
      threads.emplace_back(std::thread([&, tid]() {
        auto num_reqs_per_thread = kNumReqs / kNumMutatorThreads;
        auto req_offset = tid * num_reqs_per_thread;
        auto *thread_gen_reqs = &all_gen_reqs[req_offset];
        for (uint32_t i = 0; i < num_reqs_per_thread; i++) {
          Req req;
          random_req(req.data, tid);
          Key key;
          memcpy(key.data, req.data, kReqLen);
          for (uint32_t j = 0; j < kNumKeysPerRequest; j++) {
            append_uint32_to_char_array(j, kLog10NumKeysPerRequest,
                                        key.data + kReqLen);
            Value value;
            value.num = (j ? 0 : req_offset + i);
            hopscotch->put(kKeyLen, (const uint8_t *)key.data, kValueLen,
                           (uint8_t *)value.data);
          }
          thread_gen_reqs[i] = req;
        }
      }));
    }
    for (auto &thread : threads) {
      thread.join();
    }
    zipf_table_distribution<> zipf(kNumReqs, kZipfParamS);
    auto &generator = generators[0];
    constexpr uint32_t kPerCoreWinInterval = kReqSeqLen / helpers::kNumCPUs;
    for (uint32_t i = 0; i < kReqSeqLen; i++) {
      auto rand_idx = zipf(*generator);
      for (uint32_t j = 0; j < helpers::kNumCPUs; j++) {
        all_zipf_req_indices[j][(i + (j * kPerCoreWinInterval)) % kReqSeqLen] =
            rand_idx;
      }
    }
  }

  void prepare(ArrayEntry *array) {
      for (uint32_t i = 0; i < kNumArrayEntries; i++) {
          memset(&array[i], i % 256, sizeof(array[i]));
      }
      for (uint32_t i = 0; i < kNumMutatorThreads; i++){
        compress_result[i].resize(snappy::MaxCompressedLength(kArrayEntrySize));
      }
  }

  void consume_array_entry(const ArrayEntry &entry, std::string &compressed) {

    snappy::Compress((const char *)&entry.data, sizeof(entry), &compressed);
    auto compressed_len = compressed.size();
    ACCESS_ONCE(compressed_len);
  }

  void print_perf() {
    if (!flag.test_and_set()) {
      auto us = std::chrono::system_clock::now();
      uint64_t sum_reqs = 0;
      for (uint32_t i = 0; i < kNumMutatorThreads; i++) {
        sum_reqs += ACCESS_ONCE(req_cnts[i].c);
      }
      std::chrono::duration<double, std::micro> elapsed_time = us - prev_us;
      if (elapsed_time.count() > kMaxPrintIntervalUs) {
        auto mops =
            ((double)(sum_reqs - prev_sum_reqs) / (elapsed_time.count())) * 1.098;
        mops_records.push_back(mops);
        us = std::chrono::system_clock::now();
        if (print_times++ >= kPrintTimes) {
          constexpr double kRatioChosenRecords = 0.1;
          uint32_t num_chosen_records =
              mops_records.size() * kRatioChosenRecords;
          mops_records.erase(mops_records.begin(),
                             mops_records.end() - num_chosen_records);
          std::cout << "mops = "
                    << accumulate(mops_records.begin(), mops_records.end(),
                                  0.0) /
                           mops_records.size()
                    << std::endl;
          std::vector<uint32_t> all_lats;
          for (uint32_t i = 0; i < helpers::kNumCPUs; i++) {
            auto num_lats = std::min((uint64_t)kLatsWinSize, lats_idx[i].c);
            all_lats.insert(all_lats.end(), &lats[i][0], &lats[i][num_lats]);
          }
          std::sort(all_lats.begin(), all_lats.end());
          std::cout << "99 tail lat (ns) = "
                    << all_lats[all_lats.size() * 99.0 / 100] << std::endl;
          run_flag = 0;
          return;
          exit(0);
        }
        prev_us = us;
        prev_sum_reqs = sum_reqs;
      }
      flag.clear();
    }
  }

  void bench(LocalGenericConcurrentHopscotch *hopscotch, ArrayEntry *array) {
    std::vector<std::thread> threads;
    prev_us = std::chrono::system_clock::now();
    for (uint32_t tid = 0; tid < kNumMutatorThreads; tid++) {
      int cpuid = getCPUid(tid,0);
      CPU_ZERO(&cpu_set[tid]);
      CPU_SET(cpuid, &cpu_set[tid]);
      threads.emplace_back(std::thread([&, tid]() {
        sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set[tid]);
        uint32_t cnt = 0;
        while (run_flag) {
          if (unlikely(cnt++ % kPrintPerIters == 0)) {
            print_perf();
          }
          auto req_idx =
              all_zipf_req_indices[tid][per_core_req_idx[tid].c];
          if (unlikely(++per_core_req_idx[tid].c == kReqSeqLen)) {
            per_core_req_idx[tid].c = 0;
          }
          auto &req = all_gen_reqs[req_idx];
          Key key;
          memcpy(key.data, req.data, kReqLen);
          std::this_thread::sleep_for(std::chrono::microseconds(kThreadSleepTime));
          std::chrono::_V2::system_clock::time_point start = std::chrono::system_clock::now();
          uint32_t array_index = 0;
          {
            for (uint32_t i = 0; i < kNumKeysPerRequest; i++) {
              append_uint32_to_char_array(i, kLog10NumKeysPerRequest,
                                          key.data + kReqLen);
              Value value;
              uint16_t value_len;
              hopscotch->get(kKeyLen, (const uint8_t *)key.data,
                             &value_len, (uint8_t *)value.data);
              array_index += value.num;
            }
          }
          {
            array_index %= kNumArrayEntries;
            const auto &array_entry = array[array_index];
            consume_array_entry(array_entry, compress_result[tid]);
          }
          std::chrono::_V2::system_clock::time_point end = std::chrono::system_clock::now();
          std::chrono::duration<uint32_t, std::nano> dur = end - start;
          lats[tid][(lats_idx[tid].c++) % kLatsWinSize] = dur.count();
          ACCESS_ONCE(req_cnts[tid].c)++;
        }
      }));
    }
    for (auto &thread : threads) {
      thread.join();
    }
  }

public:
  void inline do_work() {
    auto hopscotch = std::unique_ptr<LocalGenericConcurrentHopscotch>(
      new LocalGenericConcurrentHopscotch(kLocalHashTableNumEntriesShift, kHTDataSize)
    );
    std::cout << "Prepare HT..." << std::endl;
    auto prep_start =  std::chrono::system_clock::now();
    prepare(hopscotch.get());
    auto prep_end =  std::chrono::system_clock::now();
    std::chrono::duration<uint64_t, std::nano> dur = prep_end - prep_start;
    std::cout << "Prepare HT Time: " << dur.count() << std::endl;
    auto array_ptr = std::unique_ptr<ArrayEntry[]>(new ArrayEntry[kNumArrayEntries]);
    std::cout << "Prepare Array..." << std::endl;
    prep_start =  std::chrono::system_clock::now();
    prepare(array_ptr.get());
    prep_end =  std::chrono::system_clock::now();
    dur = prep_end - prep_start;
    std::cout << "Prepare Array Time: " << dur.count() << std::endl;
    std::cout << "Bench..." << std::endl;
    bench(hopscotch.get(), array_ptr.get());
  }
};
} // namespace far_memory

int argc;
FarMemTest test;

int main(int _argc, char *argv[]) {
  test.do_work();
  return 0;
}
