#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "timeout_lock.h"

// Define static variables
thread_local TOLock::QNode* TOLock::my_node_;
const TOLock::QNode TOLock::AVAILABLE;

using namespace std::chrono_literals;

/**
 * @brief This test ensures that at most one thread is in the critical section
 * at any time.
 */
TEST(TOLockTest, MutualExclusion) {
  constexpr uint32_t kNumThreads = 8;
  constexpr uint32_t kNumIterations = 10000;

  TOLock lock;
  uint32_t counter = 0;
  std::atomic<uint32_t> failed_attempt{0};

  auto critical_section = [&]() {
    for (uint32_t i = 0; i < kNumIterations; i++) {
      if (lock.try_lock(100us)) {
        uint32_t prev = counter;
        counter++;
        std::this_thread::yield();
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

  EXPECT_EQ(counter + failed_attempt, kNumThreads * kNumIterations)
      << "Final counter value incorrect!";
}

/**
 * @brief This test checks correctness under high contention.
 */
TEST(TOLockTest, StressTest) {
  constexpr uint32_t kNumThreads = 8;
  constexpr uint32_t kNumIterations = 125000;

  TOLock lock;
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
TEST(TOLockTest, NoDeadLock) {
  constexpr uint32_t kNumThreads = 8;

  TOLock lock;
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
TEST(TOLockTest, Fairness) {
  constexpr uint32_t kNumThreads = 8;

  TOLock lock;
  uint32_t counter = 0;

  auto critical_section = [&]() {
    if (lock.try_lock(1s)) {
      counter++;
      lock.unlock();
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

  EXPECT_EQ(counter, kNumThreads);
}
