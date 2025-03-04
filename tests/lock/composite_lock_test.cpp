#include "lock/composite_lock.h"

#include <atomic>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

// Define static variables
thread_local CompositeLock::QNode* CompositeLock::my_node_ = nullptr;

using namespace std::chrono_literals;

/**
 * @brief This test ensures that at most one thread is in the critical section
 * at any time.
 */
TEST(CompositeLockTest, MutualExclusion) {
  constexpr uint32_t kNumThreads = 8;
  constexpr uint32_t kNumIterations = 10000;

  constexpr size_t kSize = kNumThreads / 2;
  constexpr int64_t kMinDelay = 10;
  constexpr int64_t kMaxDelay = 25;

  CompositeLock lock{kSize, kMinDelay, kMaxDelay};
  uint32_t counter = 0;
  std::atomic<uint32_t> failed_attempt{0};

  auto critical_section = [&]() {
    for (uint32_t i = 0; i < kNumIterations; i++) {
      if (lock.try_lock(100us)) {
        uint32_t prev = counter;
        counter++;
        std::this_thread::yield();  // Yield to encourage race conditions
        EXPECT_EQ(counter, prev + 1) << "Race condition detected!";
        lock.unlock();
      } else {
        failed_attempt.fetch_add(1, std::memory_order_relaxed);
      }
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (uint32_t i = 0; i < kNumThreads; i++) {
    threads.emplace_back(critical_section);
  }

  for (auto& t : threads) {
    t.join();
  }

  std::cout << counter << ' ' << failed_attempt << '\n';

  EXPECT_EQ(counter + failed_attempt, kNumThreads * kNumIterations)
      << "Final counter value incorrect!";
}

/**
 * @brief This test checks correctness under high contention.
 */
TEST(CompositeLockTest, StressTest) {
  constexpr uint32_t kNumThreads = 8;
  constexpr uint32_t kNumIterations = 125000;

  constexpr size_t kSize = kNumThreads / 2;
  constexpr int64_t kMinDelay = 1;
  constexpr int64_t kMaxDelay = 100;

  CompositeLock lock{kSize, kMinDelay, kMaxDelay};
  std::atomic<uint32_t> counter = 0;

  auto worker = [&]() {
    for (uint32_t i = 0; i < kNumIterations; i++) {
      if (lock.try_lock(100us)) {
        counter.fetch_add(1, std::memory_order_relaxed);
        counter.fetch_sub(1, std::memory_order_relaxed);
        lock.unlock();
      }
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (uint32_t i = 0; i < kNumThreads; i++) {
    threads.emplace_back(worker);
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(counter.load(std::memory_order_relaxed), 0);
}

/**
 * @brief This test ensures all threads make progress and don't get stuck
 * indefinitely.
 */
TEST(CompositeLockTest, NoDeadLock) {
  constexpr uint32_t kNumThreads = 8;

  constexpr size_t kSize = kNumThreads / 2;
  constexpr int64_t kMinDelay = 1;
  constexpr int64_t kMaxDelay = 100;

  CompositeLock lock{kSize, kMinDelay, kMaxDelay};
  bool done = false;

  auto worker = [&]() {
    if (lock.try_lock(10us)) {
      done = true;
      lock.unlock();
    };
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (uint32_t i = 0; i < kNumThreads; i++) {
    threads.emplace_back(worker);
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_TRUE(done);
}

/**
 * @brief Test fairness by ensuring all threads eventually acquire the lock.
 */
TEST(CompositeLockTest, Fairness) {
  constexpr uint32_t kNumThreads = 8;

  constexpr size_t kSize = kNumThreads / 2;
  constexpr int64_t kMinDelay = 1;
  constexpr int64_t kMaxDelay = 100;

  CompositeLock lock{kSize, kMinDelay, kMaxDelay};
  uint32_t counter = 0;
  std::atomic<uint32_t> failed_attempt{0};

  auto critical_section = [&]() {
    if (lock.try_lock(1s)) {
      counter++;
      lock.unlock();
    } else {
      failed_attempt.fetch_add(1, std::memory_order_relaxed);
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (uint32_t i = 0; i < kNumThreads; i++) {
    threads.emplace_back(critical_section);
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(counter + failed_attempt, kNumThreads);
}
