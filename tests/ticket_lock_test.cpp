#include "ticket_lock.h"

#include <gtest/gtest.h>

/**
 * @brief This test ensures that at most one thread is in the critical section
 * at any time.
 */
TEST(TicketLockTest, MutualExclusion) {
  constexpr uint32_t kNumThreads = 8;
  constexpr uint32_t kNumIterations = 1000;

  TicketLock lock;
  uint32_t counter = 0;

  auto critical_section = [&]() {
    for (uint32_t i = 0; i < kNumIterations; i++) {
      lock.lock();
      uint32_t expected = counter++;
      std::this_thread::yield();  // Encourage race conditions
      EXPECT_EQ(counter, expected + 1);
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

  EXPECT_EQ(counter, kNumIterations * kNumThreads);
}

/**
 * @brief This test checks correctness under high contention.
 */
TEST(TicketLockTest, StressTest) {
  constexpr uint32_t kNumThreads = 8;
  constexpr uint32_t kNumIterations = 125000;

  TicketLock lock;
  std::atomic<uint32_t> counter = 0;

  auto worker = [&]() {
    for (uint32_t i = 0; i < kNumIterations; i++) {
      lock.lock();
      counter.fetch_add(1, std::memory_order_relaxed);
      counter.fetch_sub(1, std::memory_order_relaxed);
      lock.unlock();
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

  EXPECT_EQ(counter.load(), 0);
}

/**
 * @brief This test ensures all threads make progress and don't get stuck
 * indefinitely.
 */
TEST(FilterLockTest, NoDeadLock) {
  constexpr uint32_t kNumThreads = 8;

  TicketLock lock;
  bool done = false;

  auto worker = [&]() {
    lock.lock();
    done = true;
    lock.unlock();
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
