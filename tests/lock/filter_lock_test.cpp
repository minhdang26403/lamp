#include "lock/filter_lock.h"

#include <atomic>
#include <thread>

#include "gtest/gtest.h"

/**
 * @brief This test ensures that at most one thread is in the critical section
 * at any time.
 */
TEST(FilterLockTest, MutualExclusion) {
  constexpr uint32_t kNumThreads = 8;
  constexpr uint32_t kNumIterations = 1000;

  FilterLock lock(kNumThreads);
  uint32_t counter = 0;

  auto critical_section = [&](uint32_t id) {
    for (uint32_t i = 0; i < kNumIterations; i++) {
      lock.lock(id);
      uint32_t expected = counter++;
      std::this_thread::yield();  // Encourage race conditions
      EXPECT_EQ(counter, expected + 1);
      lock.unlock(id);
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (uint32_t i = 0; i < kNumThreads; i++) {
    threads.emplace_back(critical_section, i);
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(counter, kNumIterations * kNumThreads);
}

/**
 * @brief This test checks correctness under high contention.
 */
TEST(FilterLockTest, StressTest) {
  constexpr uint32_t kNumThreads = 8;
  constexpr uint32_t kNumIterations = 125000;

  FilterLock lock(kNumThreads);
  std::atomic<uint32_t> counter = 0;

  auto worker = [&](uint32_t id) {
    for (uint32_t i = 0; i < kNumIterations; i++) {
      lock.lock(id);
      counter.fetch_add(1, std::memory_order_relaxed);
      counter.fetch_sub(1, std::memory_order_relaxed);
      lock.unlock(id);
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (uint32_t i = 0; i < kNumThreads; i++) {
    threads.emplace_back(worker, i);
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(counter.load(), 0);
}

/**
 * @brief This test ensures all threads make progress and don't get stuck
 * indefinitely.
 */
TEST(FilterLockTest, NoDeadLock) {
  constexpr uint32_t kNumThreads = 8;

  FilterLock lock(kNumThreads);
  bool done = false;

  auto worker = [&](uint32_t id) {
    lock.lock(id);
    done = true;
    lock.unlock(id);
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (uint32_t i = 0; i < kNumThreads; i++) {
    threads.emplace_back(worker, i);
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_TRUE(done);
}

/**
 * @brief This test ensures all threads eventually get access to the critical
 * section.
 */
TEST(FilterLockTest, NoStarvation) {
  constexpr uint32_t kNumThreads = 8;
  constexpr uint32_t kNumIterations = 1000;

  FilterLock lock(kNumThreads);
  // We can use an array of non-atomic ints here, but we use
  // atomic ints to make each thread do more work in its critical section.
  std::atomic<uint32_t> entry_count[kNumThreads];

  for (uint32_t i = 0; i < kNumThreads; i++) {
    entry_count[i].store(0, std::memory_order_relaxed);
  }

  auto worker = [&](uint32_t id) {
    for (uint32_t i = 0; i < kNumIterations; i++) {
      lock.lock(id);
      entry_count[id].fetch_add(1, std::memory_order_relaxed);
      lock.unlock(id);
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (uint32_t i = 0; i < kNumThreads; i++) {
    threads.emplace_back(worker, i);
  }

  for (auto& t : threads) {
    t.join();
  }

  for (uint32_t i = 0; i < kNumThreads; i++) {
    EXPECT_EQ(entry_count[i].load(std::memory_order_relaxed), kNumIterations);
  }
}
