#include <atomic>
#include <barrier>
#include <thread>
#include <vector>

#include "benchmark/benchmark.h"
#include "synchronization/fifo_read_write_lock.h"
#include "synchronization/simple_read_write_lock.h"

// Shared test parameters
constexpr uint32_t kSharedValue = 42;
constexpr uint32_t kWriteValue = 100;

// Common benchmark function that can be used with different lock
// implementations
template<typename LockType>
static void BM_ReadHeavyWorkload(benchmark::State& state) {
  LockType lock;
  uint32_t shared_data = kSharedValue;
  std::atomic<uint32_t> reads_completed = 0;
  std::atomic<uint32_t> writes_completed = 0;

  // Read-heavy ratio: 95% reads, 5% writes
  constexpr double read_ratio = 0.95;

  const size_t kNumThreads = state.range(0);
  const size_t kOperationsPerThread = state.range(1);

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);

  for (auto _ : state) {
    state.PauseTiming();
    reads_completed = 0;
    writes_completed = 0;
    threads.clear();

    // Create a barrier to ensure all threads start at the same time
    std::barrier sync_point(kNumThreads + 1);

    for (size_t i = 0; i < kNumThreads; i++) {
      threads.emplace_back([&]() {
        // Wait for all threads to be ready
        sync_point.arrive_and_wait();

        for (size_t op = 0; op < kOperationsPerThread; op++) {
          // Determine if this operation should be a read or write
          bool do_read = (static_cast<double>(rand()) / RAND_MAX) < read_ratio;

          if (do_read) {
            // Read operation
            lock.read_lock();
            // Simulate reading by just accessing the value
            benchmark::DoNotOptimize(shared_data);
            lock.read_unlock();
            reads_completed++;
          } else {
            // Write operation
            lock.write_lock();
            // Simulate writing
            shared_data = kWriteValue;
            benchmark::DoNotOptimize(shared_data);
            shared_data = kSharedValue;
            lock.write_unlock();
            writes_completed++;
          }
        }
      });
    }

    // Start timing
    sync_point.arrive_and_wait();
    state.ResumeTiming();

    // Wait for all threads to complete
    for (auto& t : threads) {
      t.join();
    }

    state.PauseTiming();
    // Verify the final value to ensure writes were properly synchronized
    assert(shared_data == kSharedValue);
    state.ResumeTiming();
  }

  // Report custom counters
  state.counters["reads"] = benchmark::Counter(reads_completed.load(),
                                               benchmark::Counter::kAvgThreads);
  state.counters["writes"] = benchmark::Counter(
      writes_completed.load(), benchmark::Counter::kAvgThreads);
  state.counters["read_ratio"] =
      benchmark::Counter(static_cast<double>(reads_completed.load()) /
                             (reads_completed.load() + writes_completed.load()),
                         benchmark::Counter::kAvgThreads);
}

template<typename LockType>
static void BM_WriteHeavyWorkload(benchmark::State& state) {
  LockType lock;
  uint32_t shared_data = kSharedValue;
  std::atomic<uint32_t> reads_completed = 0;
  std::atomic<uint32_t> writes_completed = 0;

  // Write-heavy ratio: 60% writes, 40% reads
  constexpr double write_ratio = 0.6;

  const size_t kNumThreads = state.range(0);
  const size_t kOperationsPerThread = state.range(1);

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);

  for (auto _ : state) {
    state.PauseTiming();
    reads_completed = 0;
    writes_completed = 0;
    threads.clear();

    // Create a barrier to ensure all threads start at the same time
    std::barrier sync_point(kNumThreads + 1);

    for (size_t i = 0; i < kNumThreads; i++) {
      threads.emplace_back([&]() {
        // Wait for all threads to be ready
        sync_point.arrive_and_wait();

        for (size_t op = 0; op < kOperationsPerThread; op++) {
          // Determine if this operation should be a write or read
          bool do_write =
              (static_cast<double>(rand()) / RAND_MAX) < write_ratio;

          if (do_write) {
            // Write operation
            lock.write_lock();
            // Simulate writing
            shared_data = kWriteValue;
            benchmark::DoNotOptimize(shared_data);
            shared_data = kSharedValue;
            lock.write_unlock();
            writes_completed++;
          } else {
            // Read operation
            lock.read_lock();
            // Simulate reading by just accessing the value
            benchmark::DoNotOptimize(shared_data);
            lock.read_unlock();
            reads_completed++;
          }
        }
      });
    }

    // Start timing
    sync_point.arrive_and_wait();
    state.ResumeTiming();

    // Wait for all threads to complete
    for (auto& t : threads) {
      t.join();
    }

    state.PauseTiming();
    // Verify the final value to ensure writes were properly synchronized
    assert(shared_data == kSharedValue);
    state.ResumeTiming();
  }

  // Report custom counters
  state.counters["reads"] = benchmark::Counter(reads_completed.load(),
                                               benchmark::Counter::kAvgThreads);
  state.counters["writes"] = benchmark::Counter(
      writes_completed.load(), benchmark::Counter::kAvgThreads);
  state.counters["write_ratio"] =
      benchmark::Counter(static_cast<double>(writes_completed.load()) /
                             (reads_completed.load() + writes_completed.load()),
                         benchmark::Counter::kAvgThreads);
}

template<typename LockType>
static void BM_BalancedWorkload(benchmark::State& state) {
  LockType lock;
  uint32_t shared_data = kSharedValue;
  std::atomic<uint32_t> reads_completed = 0;
  std::atomic<uint32_t> writes_completed = 0;

  // Balanced ratio: 50% reads, 50% writes
  constexpr double read_ratio = 0.5;

  const size_t kNumThreads = state.range(0);
  const size_t kOperationsPerThread = state.range(1);

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);

  for (auto _ : state) {
    state.PauseTiming();
    reads_completed = 0;
    writes_completed = 0;
    threads.clear();

    // Create a barrier to ensure all threads start at the same time
    std::barrier sync_point(kNumThreads + 1);

    for (size_t i = 0; i < kNumThreads; i++) {
      threads.emplace_back([&]() {
        // Wait for all threads to be ready
        sync_point.arrive_and_wait();

        for (size_t op = 0; op < kOperationsPerThread; op++) {
          // Determine if this operation should be a read or write
          bool do_read = (static_cast<double>(rand()) / RAND_MAX) < read_ratio;

          if (do_read) {
            // Read operation
            lock.read_lock();
            // Simulate reading by just accessing the value
            benchmark::DoNotOptimize(shared_data);
            lock.read_unlock();
            reads_completed++;
          } else {
            // Write operation
            lock.write_lock();
            // Simulate writing
            shared_data = kWriteValue;
            benchmark::DoNotOptimize(shared_data);
            shared_data = kSharedValue;
            lock.write_unlock();
            writes_completed++;
          }
        }
      });
    }

    // Start timing
    sync_point.arrive_and_wait();
    state.ResumeTiming();

    // Wait for all threads to complete
    for (auto& t : threads) {
      t.join();
    }

    state.PauseTiming();
    // Verify the final value to ensure writes were properly synchronized
    assert(shared_data == kSharedValue);
    state.ResumeTiming();
  }

  // Report custom counters
  state.counters["reads"] = benchmark::Counter(reads_completed.load(),
                                               benchmark::Counter::kAvgThreads);
  state.counters["writes"] = benchmark::Counter(
      writes_completed.load(), benchmark::Counter::kAvgThreads);
  state.counters["read_ratio"] =
      benchmark::Counter(static_cast<double>(reads_completed.load()) /
                             (reads_completed.load() + writes_completed.load()),
                         benchmark::Counter::kAvgThreads);
}

template<typename LockType>
static void BM_HighContention(benchmark::State& state) {
  LockType lock;
  uint32_t shared_data = kSharedValue;
  std::atomic<uint32_t> reads_completed = 0;
  std::atomic<uint32_t> writes_completed = 0;

  const size_t kNumThreads =
      state.range(0);  // This should be more than CPU cores
  const size_t kOperationsPerThread = state.range(1);  // Many small operations

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);

  // More threads than CPU cores, short operations
  for (auto _ : state) {
    state.PauseTiming();
    reads_completed = 0;
    writes_completed = 0;
    threads.clear();

    // Create a barrier to ensure all threads start at the same time
    std::barrier sync_point(kNumThreads + 1);

    for (size_t i = 0; i < kNumThreads; i++) {
      threads.emplace_back([&]() {
        // Wait for all threads to be ready
        sync_point.arrive_and_wait();

        for (size_t op = 0; op < kOperationsPerThread; op++) {
          // Alternate between read and write based on thread ID
          // This ensures high contention as threads will be competing for the
          // same resource
          if (op % 2 == 0) {
            // Read operation
            lock.read_lock();
            // Very short read operation
            benchmark::DoNotOptimize(shared_data);
            lock.read_unlock();
            reads_completed++;
          } else {
            // Write operation
            lock.write_lock();
            // Very short write operation
            shared_data = kWriteValue;
            benchmark::DoNotOptimize(shared_data);
            shared_data = kSharedValue;
            lock.write_unlock();
            writes_completed++;
          }
        }
      });
    }

    // Start timing
    sync_point.arrive_and_wait();
    state.ResumeTiming();

    // Wait for all threads to complete
    for (auto& t : threads) {
      t.join();
    }

    state.PauseTiming();
    // Verify the final value to ensure writes were properly synchronized
    assert(shared_data == kSharedValue);
    state.ResumeTiming();
  }

  // Report custom counters
  state.counters["reads"] = benchmark::Counter(reads_completed.load(),
                                               benchmark::Counter::kAvgThreads);
  state.counters["writes"] = benchmark::Counter(
      writes_completed.load(), benchmark::Counter::kAvgThreads);
}

template<typename LockType>
static void BM_LowContention(benchmark::State& state) {
  LockType lock;
  uint32_t shared_data = kSharedValue;
  std::atomic<uint32_t> reads_completed = 0;
  std::atomic<uint32_t> writes_completed = 0;

  const size_t kNumThreads =
      state.range(0);  // This should be fewer than CPU cores
  const size_t kOperationsPerThread =
      state.range(1);  // Fewer but longer operations

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);

  // Fewer threads than CPU cores, longer operations
  for (auto _ : state) {
    state.PauseTiming();
    reads_completed = 0;
    writes_completed = 0;
    threads.clear();

    // Create a barrier to ensure all threads start at the same time
    std::barrier sync_point(kNumThreads + 1);

    for (size_t i = 0; i < kNumThreads; i++) {
      threads.emplace_back([&, i]() {
        // Wait for all threads to be ready
        sync_point.arrive_and_wait();

        for (size_t op = 0; op < kOperationsPerThread; op++) {
          // Each thread tends to do mostly reads or mostly writes
          // This reduces contention as threads are less likely to compete
          bool do_read = (i % 3 != 0);  // 2/3 of threads do mostly reads

          if (do_read) {
            // Read operation
            lock.read_lock();
            // Simulate a longer read operation
            for (size_t j = 0; j < 100; j++) {
              benchmark::DoNotOptimize(shared_data);
            }
            lock.read_unlock();
            reads_completed++;
          } else {
            // Write operation
            lock.write_lock();
            // Simulate a longer write operation
            for (size_t j = 0; j < 50; j++) {
              shared_data = kWriteValue + j;
              benchmark::DoNotOptimize(shared_data);
            }
            shared_data = kSharedValue;
            lock.write_unlock();
            writes_completed++;
          }
        }
      });
    }

    // Start timing
    sync_point.arrive_and_wait();
    state.ResumeTiming();

    // Wait for all threads to complete
    for (auto& t : threads) {
      t.join();
    }

    state.PauseTiming();
    // Verify the final value to ensure writes were properly synchronized
    assert(shared_data == kSharedValue);
    state.ResumeTiming();
  }

  // Report custom counters
  state.counters["reads"] = benchmark::Counter(reads_completed.load(),
                                               benchmark::Counter::kAvgThreads);
  state.counters["writes"] = benchmark::Counter(
      writes_completed.load(), benchmark::Counter::Counter::kAvgThreads);
  state.counters["read_ratio"] =
      benchmark::Counter(static_cast<double>(reads_completed.load()) /
                             (reads_completed.load() + writes_completed.load()),
                         benchmark::Counter::kAvgThreads);
}

// Reader starvation benchmark
template<typename LockType>
static void BM_ReaderStarvation(benchmark::State& state) {
  LockType lock;
  uint32_t shared_data = kSharedValue;
  std::atomic<uint32_t> reads_completed = 0;
  std::atomic<uint32_t> writes_completed = 0;

  const size_t kNumThreads = state.range(0);
  const size_t kOperationsPerThread = state.range(1);

  // Calculate number of reader and writer threads
  const size_t kNumWriterThreads =
      std::max<size_t>(1, kNumThreads / 4);  // 25% writers
  const size_t kNumReaderThreads =
      kNumThreads - kNumWriterThreads;  // 75% readers

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);

  for (auto _ : state) {
    state.PauseTiming();
    reads_completed = 0;
    writes_completed = 0;
    threads.clear();

    // Create a barrier to ensure all threads start at the same time
    std::barrier sync_point(kNumThreads + 1);

    // Create writer threads first (more likely to get priority with some
    // implementations)
    for (size_t i = 0; i < kNumWriterThreads; i++) {
      threads.emplace_back([&]() {
        sync_point.arrive_and_wait();

        for (size_t op = 0; op < kOperationsPerThread; op++) {
          lock.write_lock();
          // Simulate a longer write operation to increase chance of reader
          // starvation
          for (size_t j = 0; j < 20; j++) {
            shared_data = kWriteValue + j;
            benchmark::DoNotOptimize(shared_data);
          }
          shared_data = kSharedValue;
          lock.write_unlock();
          writes_completed++;

          // Small yield to encourage another writer to get the lock
          std::this_thread::yield();
        }
      });
    }

    // Create reader threads
    for (size_t i = 0; i < kNumReaderThreads; i++) {
      threads.emplace_back([&]() {
        sync_point.arrive_and_wait();

        for (size_t op = 0; op < kOperationsPerThread; op++) {
          lock.read_lock();
          // Short read operation
          benchmark::DoNotOptimize(shared_data);
          lock.read_unlock();
          reads_completed++;
        }
      });
    }

    // Start timing
    sync_point.arrive_and_wait();
    state.ResumeTiming();

    // Wait for all threads to complete
    for (auto& t : threads) {
      t.join();
    }

    state.PauseTiming();
    // Verify the final value to ensure writes were properly synchronized
    assert(shared_data == kSharedValue);
    state.ResumeTiming();
  }

  state.counters["reads_per_write"] =
      benchmark::Counter(static_cast<double>(reads_completed.load()) /
                         std::max<uint32_t>(1, writes_completed.load()));
}

template<typename LockType>
static void BM_WriterStarvation(benchmark::State& state) {
  LockType lock;
  uint32_t shared_data = kSharedValue;
  std::atomic<uint32_t> reads_completed = 0;
  std::atomic<uint32_t> writes_completed = 0;
  std::atomic<long long> total_writer_wait_time = 0;  // in microseconds

  const size_t kNumThreads = state.range(0);           // Total threads
  const size_t kOperationsPerThread = state.range(1);  // Operations per thread

  // Split into 10% writers, 90% readers
  const size_t kNumWriterThreads = std::max<size_t>(1, kNumThreads / 10);
  const size_t kNumReaderThreads = kNumThreads - kNumWriterThreads;

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);

  for (auto _ : state) {
    // Pause timing during setup
    state.PauseTiming();
    reads_completed = 0;
    writes_completed = 0;
    total_writer_wait_time = 0;
    threads.clear();

    // Barrier to synchronize thread startup
    std::barrier sync_point(kNumThreads + 1);

    // Create reader threads
    for (size_t i = 0; i < kNumReaderThreads; i++) {
      threads.emplace_back([&]() {
        sync_point.arrive_and_wait();  // Wait for all threads to be ready

        for (size_t op = 0; op < kOperationsPerThread; op++) {
          lock.read_lock();
          // Simulate a medium-length read
          for (size_t j = 0; j < 10; j++) {
            benchmark::DoNotOptimize(shared_data);
          }
          lock.read_unlock();
          reads_completed++;

          // Occasionally yield to allow other readers in
          if (i % 3 == 0) {
            std::this_thread::yield();
          }
        }
      });
    }

    // Create writer threads
    for (size_t i = 0; i < kNumWriterThreads; i++) {
      threads.emplace_back([&]() {
        sync_point.arrive_and_wait();  // Wait for all threads to be ready

        for (size_t op = 0; op < kOperationsPerThread / 5;
             op++) {  // Fewer writes
          // Record time before attempting to acquire lock
          auto start_time = std::chrono::high_resolution_clock::now();
          lock.write_lock();
          // Record time after lock is acquired
          auto end_time = std::chrono::high_resolution_clock::now();

          // Calculate wait time in microseconds
          auto wait_time =
              std::chrono::duration_cast<std::chrono::microseconds>(end_time -
                                                                    start_time)
                  .count();
          total_writer_wait_time += wait_time;

          // Perform write operation
          shared_data = kWriteValue;
          benchmark::DoNotOptimize(shared_data);
          shared_data = kSharedValue;

          lock.write_unlock();
          writes_completed++;

          // Yield to give readers a chance
          std::this_thread::yield();
        }
      });
    }

    // Start timing and release the barrier
    sync_point.arrive_and_wait();
    state.ResumeTiming();

    // Wait for all threads to finish
    for (auto& t : threads) {
      t.join();
    }

    // Pause timing to verify and report
    state.PauseTiming();
    assert(shared_data == kSharedValue);  // Ensure writes were synchronized
    state.ResumeTiming();
  }

  // Report metrics
  int total_writes = writes_completed.load();
  long long total_wait = total_writer_wait_time.load();
  state.counters["avg_writer_wait"] = benchmark::Counter(
      static_cast<double>(total_wait) / std::max(1, total_writes),
      benchmark::Counter::kAvgThreads);  // Average wait time per write (us)
  state.counters["reads_per_write"] = benchmark::Counter(
      static_cast<double>(reads_completed.load()) / std::max(1, total_writes),
      benchmark::Counter::kAvgThreads);  // Reads per write ratio
}

// Register benchmarks for SimpleReadWriteLock
BENCHMARK_TEMPLATE(BM_ReadHeavyWorkload, SimpleReadWriteLock)
    ->Args({4, 10000})  // 4 threads, 10k ops per thread
    ->Args({8, 5000})   // 8 threads, 5k ops per thread
    ->Args({16, 2500})  // 16 threads, 2.5k ops per thread
    ->UseRealTime();

BENCHMARK_TEMPLATE(BM_WriteHeavyWorkload, SimpleReadWriteLock)
    ->Args({4, 5000})   // 4 threads, 5k ops per thread
    ->Args({8, 2500})   // 8 threads, 2.5k ops per thread
    ->Args({16, 1250})  // 16 threads, 1.25k ops per thread
    ->UseRealTime();

BENCHMARK_TEMPLATE(BM_BalancedWorkload, SimpleReadWriteLock)
    ->Args({4, 10000})  // 4 threads, 10k ops per thread
    ->Args({8, 5000})   // 8 threads, 5k ops per thread
    ->Args({16, 2500})  // 16 threads, 2.5k ops per thread
    ->UseRealTime();

BENCHMARK_TEMPLATE(BM_HighContention, SimpleReadWriteLock)
    ->Args({32, 1000})  // 32 threads, 1k ops per thread (high thread count)
    ->Args({64,
            500})  // 64 threads, 500 ops per thread (very high thread count)
    ->UseRealTime();

BENCHMARK_TEMPLATE(BM_LowContention, SimpleReadWriteLock)
    ->Args({2, 10000})  // 2 threads, 10k ops per thread (low thread count)
    ->Args({4, 5000})   // 4 threads, 5k ops per thread
    ->UseRealTime();

BENCHMARK_TEMPLATE(BM_ReaderStarvation, SimpleReadWriteLock)
    ->Args({16, 2000})  // 16 threads, 2k ops per thread
    ->Args({32, 1000})  // 32 threads, 1k ops per thread
    ->UseRealTime();

BENCHMARK_TEMPLATE(BM_WriterStarvation, SimpleReadWriteLock)
    ->Args({16, 2000})  // 16 threads, 2k ops per thread
    ->Args({32, 1000})  // 32 threads, 1k ops per thread
    ->UseRealTime();

// Register benchmarks for FIFOReadWriteLock
BENCHMARK_TEMPLATE(BM_ReadHeavyWorkload, FIFOReadWriteLock)
    ->Args({4, 10000})  // 4 threads, 10k ops per thread
    ->Args({8, 5000})   // 8 threads, 5k ops per thread
    ->Args({16, 2500})  // 16 threads, 2.5k ops per thread
    ->UseRealTime();

BENCHMARK_TEMPLATE(BM_WriteHeavyWorkload, FIFOReadWriteLock)
    ->Args({4, 5000})   // 4 threads, 5k ops per thread
    ->Args({8, 2500})   // 8 threads, 2.5k ops per thread
    ->Args({16, 1250})  // 16 threads, 1.25k ops per thread
    ->UseRealTime();

BENCHMARK_TEMPLATE(BM_BalancedWorkload, FIFOReadWriteLock)
    ->Args({4, 10000})  // 4 threads, 10k ops per thread
    ->Args({8, 5000})   // 8 threads, 5k ops per thread
    ->Args({16, 2500})  // 16 threads, 2.5k ops per thread
    ->UseRealTime();

BENCHMARK_TEMPLATE(BM_HighContention, FIFOReadWriteLock)
    ->Args({32, 1000})  // 32 threads, 1k ops per thread (high thread count)
    ->Args({64,
            500})  // 64 threads, 500 ops per thread (very high thread count)
    ->UseRealTime();

BENCHMARK_TEMPLATE(BM_LowContention, FIFOReadWriteLock)
    ->Args({2, 10000})  // 2 threads, 10k ops per thread (low thread count)
    ->Args({4, 5000})   // 4 threads, 5k ops per thread
    ->UseRealTime();

BENCHMARK_TEMPLATE(BM_ReaderStarvation, FIFOReadWriteLock)
    ->Args({16, 2000})  // 16 threads, 2k ops per thread
    ->Args({32, 1000})  // 32 threads, 1k ops per thread
    ->UseRealTime();

BENCHMARK_TEMPLATE(BM_WriterStarvation, FIFOReadWriteLock)
    ->Args({16, 2000})  // 16 threads, 2k ops per thread
    ->Args({32, 1000})  // 32 threads, 1k ops per thread
    ->UseRealTime();

BENCHMARK_MAIN();
