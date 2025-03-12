#include <mutex>
#include <thread>
#include <vector>

#include "benchmark/benchmark.h"
#include "lock/backoff_lock.h"
#include "lock/clh_lock.h"
#include "lock/mcs_lock.h"
#include "lock/tas_lock.h"
#include "lock/ticket_lock.h"
#include "lock/ttas_lock.h"

thread_local MCSLock::QNode MCSLock::my_node_;
thread_local CLHLock::QNode* CLHLock::my_pred_ = nullptr;
thread_local CLHLock::QNode* CLHLock::my_node_ = new QNode();

// A simple counter protected by the lock being benchmarked
class ProtectedCounter {
 public:
  template<typename LockType>
  auto increment(LockType& lock) noexcept -> void {
    lock.lock();
    counter_++;
    lock.unlock();
  }

  auto get() const noexcept -> uint64_t { return counter_; }

  auto reset() noexcept -> void { counter_ = 0; }

 private:
  uint64_t counter_{0};
};

// Templated Lock Benchmark
template<typename LockType>
static void BM_Lock(benchmark::State& state) {
  // Number of threads to use
  const uint32_t kNumThreads = state.range(0);
  constexpr uint32_t kNumIterations = 10000;

  // Counter and lock shared between threads
  ProtectedCounter counter;
  LockType lock;

  for (auto _ : state) {
    // Reset counter for each iteration
    counter.reset();

    // Barrier to synchronize thread start
    std::atomic<bool> start{false};
    std::atomic<uint32_t> threads_ready{0};

    std::vector<std::thread> threads;
    threads.reserve(kNumThreads);

    // Create threads
    for (uint32_t i = 0; i < kNumThreads; i++) {
      threads.emplace_back([&counter, &lock, &start, &threads_ready]() {
        // Signal this thread is ready
        threads_ready.fetch_add(1);

        // Wait for all threads to be ready
        while (!start.load(std::memory_order_acquire)) {
          std::this_thread::yield();
        }

        // Perform increments in the critical section
        for (uint32_t j = 0; j < kNumIterations; j++) {
          counter.increment(lock);
        }
      });
    }

    // Wait for all threads to be ready
    while (threads_ready.load() < kNumThreads) {
      std::this_thread::yield();
    }

    // Start timing
    auto start_time = std::chrono::high_resolution_clock::now();

    // Signal threads to start
    start.store(true, std::memory_order_release);

    // Wait for all threads to complete
    for (auto& t : threads) {
      t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end_time - start_time);

    // Verify correctness
    if (counter.get() != kNumThreads * kNumIterations) {
      state.SkipWithError("Race condition detected!");
    }

    // Report metrics (in milliseconds)
    state.SetIterationTime(elapsed.count() / 1e6);
    state.counters["ops_per_second"] = benchmark::Counter(
        kNumThreads * kNumIterations, benchmark::Counter::kIsRate);
  }
}

// Register benchmarks. Test with 1, 2, 4, 8, 16, 32 threads
BENCHMARK(BM_Lock<BackoffLock<>>)
    ->RangeMultiplier(2)
    ->Range(2, 32)
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_Lock<CLHLock>)
    ->RangeMultiplier(2)
    ->Range(2, 32)
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_Lock<MCSLock>)
    ->RangeMultiplier(2)
    ->Range(2, 32)
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_Lock<TASLock>)
    ->RangeMultiplier(2)
    ->Range(2, 32)
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_Lock<TicketLock>)
    ->RangeMultiplier(2)
    ->Range(2, 32)
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_Lock<TTASLock>)
    ->RangeMultiplier(2)
    ->Range(2, 32)
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_Lock<std::mutex>)
    ->RangeMultiplier(2)
    ->Range(1, 32)
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond);

// Add more benchmarks for your other lock implementations here

// Main function to run the benchmarks
BENCHMARK_MAIN();
