#include <atomic>
#include <random>
#include <thread>
#include <vector>

#include "benchmark/benchmark.h"
#include "list/coarse_list.h"
#include "list/fine_list.h"
#include "list/lazy_list.h"
#include "list/lock_free_list.h"
#include "list/optimistic_list.h"

// Constants for benchmark configuration
constexpr int kSmallSize = 100;
constexpr int kMediumSize = 1000;
constexpr int kLargeSize = 10000;
constexpr int kOperationsPerThread = 100000;
constexpr int kMaxThreads = 8;

// Helper function to initialize a list with random values
template<typename ListType>
void InitializeList(ListType& list, int size) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> distrib(1, size * 10);

  for (int i = 0; i < size; ++i) {
    list.add(distrib(gen));
  }
}

// Benchmark for read-heavy workload (80% contains, 15% add, 5% remove)
template<typename ListType>
static void BM_ReadHeavyWorkload(benchmark::State& state) {
  const int thread_count = state.range(0);
  const int list_size = state.range(1);

  ListType list;
  InitializeList(list, list_size);

  std::atomic<int> counter(0);

  for (auto _ : state) {
    state.PauseTiming();
    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    counter.store(0);
    state.ResumeTiming();

    for (int t = 0; t < thread_count; ++t) {
      threads.emplace_back([&list, &counter, list_size]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> val_dist(1, list_size * 10);
        std::uniform_int_distribution<> op_dist(1, 100);

        for (int i = 0; i < kOperationsPerThread; ++i) {
          int op = op_dist(gen);
          int val = val_dist(gen);

          if (op <= 80) {
            // 80% contains operations
            list.contains(val);
          } else if (op <= 95) {
            // 15% add operations
            list.add(val);
          } else {
            // 5% remove operations
            list.remove(val);
          }
          counter.fetch_add(1, std::memory_order_relaxed);
        }
      });
    }

    for (auto& t : threads) {
      t.join();
    }
  }

  state.counters["ops"] =
      benchmark::Counter(counter.load(), benchmark::Counter::kIsRate);
}

// Benchmark for write-heavy workload (20% contains, 40% add, 40% remove)
template<typename ListType>
static void BM_WriteHeavyWorkload(benchmark::State& state) {
  const int thread_count = state.range(0);
  const int list_size = state.range(1);

  ListType list;
  InitializeList(list, list_size);

  std::atomic<int> counter(0);

  for (auto _ : state) {
    state.PauseTiming();
    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    counter.store(0);
    state.ResumeTiming();

    for (int t = 0; t < thread_count; ++t) {
      threads.emplace_back([&list, &counter, list_size]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> val_dist(1, list_size * 10);
        std::uniform_int_distribution<> op_dist(1, 100);

        for (int i = 0; i < kOperationsPerThread; ++i) {
          int op = op_dist(gen);
          int val = val_dist(gen);

          if (op <= 20) {
            // 20% contains operations
            list.contains(val);
          } else if (op <= 60) {
            // 40% add operations
            list.add(val);
          } else {
            // 40% remove operations
            list.remove(val);
          }
          counter.fetch_add(1);
        }
      });
    }

    for (auto& t : threads) {
      t.join();
    }
  }

  state.counters["ops"] =
      benchmark::Counter(counter.load(), benchmark::Counter::kIsRate);
}

// Benchmark for balanced workload (33% contains, 33% add, 33% remove)
template<typename ListType>
static void BM_BalancedWorkload(benchmark::State& state) {
  const int thread_count = state.range(0);
  const int list_size = state.range(1);

  ListType list;
  InitializeList(list, list_size);

  std::atomic<int> counter(0);

  for (auto _ : state) {
    state.PauseTiming();
    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    counter.store(0);
    state.ResumeTiming();

    for (int t = 0; t < thread_count; ++t) {
      threads.emplace_back([&list, &counter, list_size]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> val_dist(1, list_size * 10);
        std::uniform_int_distribution<> op_dist(1, 100);

        for (int i = 0; i < kOperationsPerThread; ++i) {
          int op = op_dist(gen);
          int val = val_dist(gen);

          if (op <= 33) {
            // 33% contains operations
            list.contains(val);
          } else if (op <= 66) {
            // 33% add operations
            list.add(val);
          } else {
            // 33% remove operations
            list.remove(val);
          }
          counter.fetch_add(1);
        }
      });
    }

    for (auto& t : threads) {
      t.join();
    }
  }

  state.counters["ops"] =
      benchmark::Counter(counter.load(), benchmark::Counter::kIsRate);
}

// Benchmark for single operations with different list sizes
template<typename ListType, typename OperationType>
static void BM_SingleOperation(benchmark::State& state, OperationType operation,
                               ListType*) {
  const int list_size = state.range(0);

  ListType list;
  InitializeList(list, list_size);

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> val_dist(1, list_size * 10);

  for (auto _ : state) {
    int val = val_dist(gen);
    operation(list, val);
  }
}

// Define operations as function objects
struct ContainsOp {
  template<typename ListType>
  void operator()(ListType& list, int val) const {
    list.contains(val);
  }
};

struct AddOp {
  template<typename ListType>
  void operator()(ListType& list, int val) const {
    list.add(val);
  }
};

struct RemoveOp {
  template<typename ListType>
  void operator()(ListType& list, int val) const {
    list.remove(val);
  }
};

// Register all benchmarks

// Read-heavy workload benchmarks
#define REGISTER_READ_HEAVY_BENCHMARK(ListType)                 \
  BENCHMARK(BM_ReadHeavyWorkload<ListType>)                     \
      ->ArgsProduct({benchmark::CreateRange(1, kMaxThreads, 2), \
                     {kSmallSize, kMediumSize, kLargeSize}})    \
      ->Unit(benchmark::kMillisecond)                           \
      ->UseRealTime();

REGISTER_READ_HEAVY_BENCHMARK(CoarseList<int>)
REGISTER_READ_HEAVY_BENCHMARK(FineList<int>)
REGISTER_READ_HEAVY_BENCHMARK(OptimisticList<int>)
REGISTER_READ_HEAVY_BENCHMARK(LazyList<int>)
REGISTER_READ_HEAVY_BENCHMARK(LockFreeList<int>)

// Write-heavy workload benchmarks
#define REGISTER_WRITE_HEAVY_BENCHMARK(ListType)                \
  BENCHMARK(BM_WriteHeavyWorkload<ListType>)                    \
      ->ArgsProduct({benchmark::CreateRange(1, kMaxThreads, 2), \
                     {kSmallSize, kMediumSize, kLargeSize}})    \
      ->Unit(benchmark::kMillisecond)                           \
      ->UseRealTime();

REGISTER_WRITE_HEAVY_BENCHMARK(CoarseList<int>)
REGISTER_WRITE_HEAVY_BENCHMARK(FineList<int>)
REGISTER_WRITE_HEAVY_BENCHMARK(OptimisticList<int>)
REGISTER_WRITE_HEAVY_BENCHMARK(LazyList<int>)
REGISTER_WRITE_HEAVY_BENCHMARK(LockFreeList<int>)

// Balanced workload benchmarks
#define REGISTER_BALANCED_BENCHMARK(ListType)                   \
  BENCHMARK(BM_BalancedWorkload<ListType>)                      \
      ->ArgsProduct({benchmark::CreateRange(1, kMaxThreads, 2), \
                     {kSmallSize, kMediumSize, kLargeSize}})    \
      ->Unit(benchmark::kMillisecond)                           \
      ->UseRealTime();

REGISTER_BALANCED_BENCHMARK(CoarseList<int>)
REGISTER_BALANCED_BENCHMARK(FineList<int>)
REGISTER_BALANCED_BENCHMARK(OptimisticList<int>)
REGISTER_BALANCED_BENCHMARK(LazyList<int>)
REGISTER_BALANCED_BENCHMARK(LockFreeList<int>)

// Register single operation benchmarks for different list types
#define REGISTER_SINGLE_OP_BENCHMARKS(ListType)                              \
  BENCHMARK_CAPTURE(BM_SingleOperation, ListType## > Contains, ContainsOp(), \
                    (ListType*)nullptr)                                      \
      ->Arg(kSmallSize)                                                      \
      ->Arg(kMediumSize)                                                     \
      ->Arg(kLargeSize)                                                      \
      ->Unit(benchmark::kMillisecond);                                       \
                                                                             \
  BENCHMARK_CAPTURE(BM_SingleOperation, ListType## > Add, AddOp(),           \
                    (ListType*)nullptr)                                      \
      ->Arg(kSmallSize)                                                      \
      ->Arg(kMediumSize)                                                     \
      ->Arg(kLargeSize)                                                      \
      ->Unit(benchmark::kMillisecond);                                       \
                                                                             \
  BENCHMARK_CAPTURE(BM_SingleOperation, ListType## > Remove, RemoveOp(),     \
                    (ListType*)nullptr)                                      \
      ->Arg(kSmallSize)                                                      \
      ->Arg(kMediumSize)                                                     \
      ->Arg(kLargeSize)                                                      \
      ->Unit(benchmark::kMillisecond);

REGISTER_SINGLE_OP_BENCHMARKS(CoarseList<int>)
REGISTER_SINGLE_OP_BENCHMARKS(FineList<int>)
REGISTER_SINGLE_OP_BENCHMARKS(OptimisticList<int>)
REGISTER_SINGLE_OP_BENCHMARKS(LazyList<int>)
REGISTER_SINGLE_OP_BENCHMARKS(LockFreeList<int>)

BENCHMARK_MAIN();
