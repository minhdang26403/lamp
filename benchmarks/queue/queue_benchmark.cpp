#include <benchmark/benchmark.h>
#include <atomic>
#include <thread>
#include <vector>

#include "queue/bounded_queue.h"
#include "queue/lock_free_queue.h"
#include "queue/lock_free_queue_recycle.h"
#include "queue/unbounded_queue.h"

// Test data type
struct TestData {
  int value;
  char padding[60];  // Pad to 64 bytes to avoid false sharing
};

static constexpr int kMinThreads = 2;
static constexpr int kMaxThreads = 16;
static constexpr int kMultiThreads = 2;
static constexpr int kMinOps = 1000;
static constexpr int kMaxOps = 1000000;
static constexpr int kMultiOps = 10;

// Single-threaded enqueue benchmark
template<typename QueueType>
static void BM_SingleThreadedEnqueue(benchmark::State& state) {
  const int kCount = state.range(0);

  for (auto _ : state) {
    state.PauseTiming();
    QueueType queue;
    std::vector<TestData> data(kCount);
    for (int i = 0; i < kCount; ++i) {
      data[i].value = i;
    }
    state.ResumeTiming();

    for (int i = 0; i < kCount; ++i) {
      queue.enqueue(data[i]);
    }
  }

  state.SetItemsProcessed(int64_t(state.iterations()) * kCount);
}

// Single-threaded dequeue benchmark
template<typename QueueType>
static void BM_SingleThreadedDequeue(benchmark::State& state) {
  const int kCount = state.range(0);

  for (auto _ : state) {
    state.PauseTiming();
    QueueType queue;
    std::vector<TestData> data(kCount);
    for (int i = 0; i < kCount; ++i) {
      data[i].value = i;
      queue.enqueue(data[i]);
    }
    state.ResumeTiming();

    for (int i = 0; i < kCount; ++i) {
      TestData item = queue.dequeue();
      benchmark::DoNotOptimize(item);
    }
  }

  state.SetItemsProcessed(int64_t(state.iterations()) * kCount);
}

// Multi-threaded enqueue benchmark
template<typename QueueType>
static void BM_MultiThreadedEnqueue(benchmark::State& state) {
  const int kCount = state.range(0);
  const int kNumThreads = state.range(1);

  for (auto _ : state) {
    state.PauseTiming();
    QueueType queue;
    state.ResumeTiming();

    std::vector<std::thread> threads;
    threads.reserve(kNumThreads);
    const int kItemsPerThread = kCount / kNumThreads;

    for (int t = 0; t < kNumThreads; ++t) {
      threads.emplace_back([&queue, t, kItemsPerThread]() {
        for (int i = 0; i < kItemsPerThread; ++i) {
          TestData data{t * kItemsPerThread + i, {0}};
          queue.enqueue(data);
        }
      });
    }

    for (auto& thread : threads) {
      thread.join();
    }
  }

  state.SetItemsProcessed(int64_t(state.iterations()) * kCount);
}

// Multi-threaded producer-consumer benchmark
template<typename QueueType>
static void BM_ProducerConsumer(benchmark::State& state) {
  const int kCount = state.range(0);
  const int kProducerThreads = state.range(1);
  const int kConsumerThreads = state.range(1);

  for (auto _ : state) {
    state.PauseTiming();
    QueueType queue;
    std::atomic<int> items_consumed(0);
    std::atomic<bool> producers_done(false);
    state.ResumeTiming();

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;
    const int kItemsPerProducer = kCount / kProducerThreads;

    producers.reserve(kProducerThreads);
    consumers.reserve(kConsumerThreads);

    // Start consumers
    for (int t = 0; t < kConsumerThreads; ++t) {
      consumers.emplace_back([&queue, &kItemsPerProducer, kCount]() {
        for (int i = 0; i < kItemsPerProducer; i++) {
          try {
            TestData item = queue.dequeue();
            benchmark::DoNotOptimize(item);
          } catch (...) {
            continue;
          }
        }
      });
    }

    // Start producers
    for (int t = 0; t < kProducerThreads; ++t) {
      producers.emplace_back([&queue, t, kItemsPerProducer]() {
        for (int i = 0; i < kItemsPerProducer; ++i) {
          TestData data{t * kItemsPerProducer + i, {0}};
          queue.enqueue(data);
        }
      });
    }

    // Join producers
    for (auto& thread : producers) {
      thread.join();
    }
    producers_done = true;

    // Join consumers
    for (auto& thread : consumers) {
      thread.join();
    }
  }

  state.SetItemsProcessed(int64_t(state.iterations()) * kCount);
}

// Specialized benchmarks for BoundedQueue which needs capacity
template<typename T>
class BoundedQueueWrapper {
  BoundedQueue<T> queue;

 public:
  BoundedQueueWrapper() : queue(1000000) {}  // Default capacity for benchmarks

  void enqueue(const T& value) { queue.enqueue(value); }

  T dequeue() { return queue.dequeue(); }
};

// Register benchmarks for all queue types
// Single-threaded enqueue
BENCHMARK_TEMPLATE(BM_SingleThreadedEnqueue, BoundedQueueWrapper<TestData>)
    ->RangeMultiplier(kMultiOps)
    ->Range(kMinOps, kMaxOps)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_SingleThreadedEnqueue, LockFreeQueueRecycle<TestData>)
    ->RangeMultiplier(kMultiOps)
    ->Range(kMinOps, kMaxOps)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_SingleThreadedEnqueue, LockFreeQueue<TestData>)
    ->RangeMultiplier(kMultiOps)
    ->Range(kMinOps, kMaxOps)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_SingleThreadedEnqueue, UnboundedQueue<TestData>)
    ->RangeMultiplier(kMultiOps)
    ->Range(kMinOps, kMaxOps)
    ->Unit(benchmark::kMillisecond);

// Single-threaded dequeue
BENCHMARK_TEMPLATE(BM_SingleThreadedDequeue, BoundedQueueWrapper<TestData>)
    ->RangeMultiplier(kMultiOps)
    ->Range(kMinOps, kMaxOps)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_SingleThreadedDequeue, LockFreeQueueRecycle<TestData>)
    ->RangeMultiplier(kMultiOps)
    ->Range(kMinOps, kMaxOps)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_SingleThreadedDequeue, LockFreeQueue<TestData>)
    ->RangeMultiplier(kMultiOps)
    ->Range(kMinOps, kMaxOps)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_SingleThreadedDequeue, UnboundedQueue<TestData>)
    ->RangeMultiplier(kMultiOps)
    ->Range(kMinOps, kMaxOps)
    ->Unit(benchmark::kMillisecond);

// Multi-threaded enqueue
BENCHMARK_TEMPLATE(BM_MultiThreadedEnqueue, BoundedQueueWrapper<TestData>)
    ->ArgsProduct({benchmark::CreateRange(kMinOps, kMaxOps, kMultiOps),
                   benchmark::CreateRange(kMinThreads, kMaxThreads,
                                          kMultiThreads)})
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_MultiThreadedEnqueue, LockFreeQueueRecycle<TestData>)
    ->ArgsProduct({benchmark::CreateRange(kMinOps, kMaxOps, kMultiOps),
                   benchmark::CreateRange(kMinThreads, kMaxThreads,
                                          kMultiThreads)})
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_MultiThreadedEnqueue, LockFreeQueue<TestData>)
    ->ArgsProduct({benchmark::CreateRange(kMinOps, kMaxOps, kMultiOps),
                   benchmark::CreateRange(kMinThreads, kMaxThreads,
                                          kMultiThreads)})
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_MultiThreadedEnqueue, UnboundedQueue<TestData>)
    ->ArgsProduct({benchmark::CreateRange(kMinOps, kMaxOps, kMultiOps),
                   benchmark::CreateRange(kMinThreads, kMaxThreads,
                                          kMultiThreads)})
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond);

// Producer-consumer benchmark
BENCHMARK_TEMPLATE(BM_ProducerConsumer, BoundedQueueWrapper<TestData>)
    ->ArgsProduct({benchmark::CreateRange(kMinOps, kMaxOps, kMultiOps),
                   benchmark::CreateRange(kMinThreads, kMaxThreads,
                                          kMultiThreads)})
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond);

BENCHMARK_TEMPLATE(BM_ProducerConsumer, LockFreeQueueRecycle<TestData>)
    ->ArgsProduct({benchmark::CreateRange(kMinOps, kMaxOps, kMultiOps),
                   benchmark::CreateRange(kMinThreads, kMaxThreads,
                                          kMultiThreads)})
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond);

BENCHMARK_TEMPLATE(BM_ProducerConsumer, LockFreeQueue<TestData>)
    ->ArgsProduct({benchmark::CreateRange(kMinOps, kMaxOps, kMultiOps),
                   benchmark::CreateRange(kMinThreads, kMaxThreads,
                                          kMultiThreads)})
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond);

BENCHMARK_TEMPLATE(BM_ProducerConsumer, UnboundedQueue<TestData>)
    ->ArgsProduct({benchmark::CreateRange(kMinOps, kMaxOps, kMultiOps),
                   benchmark::CreateRange(kMinThreads, kMaxThreads,
                                          kMultiThreads)})
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
