#include "ticket_lock.h"

#include <gtest/gtest.h>

/**
 * @brief This test ensures that at most one thread is in the critical section
 * at any time.
 */
TEST(TicketLockTest, MutualExclusion) {
  constexpr int num_threads = 8;
  TicketLock lock;
  std::atomic<int> counter = 0;
  constexpr int num_iterations = 1000;

  auto critical_section = [&]() {
    for (int i = 0; i < num_iterations; i++) {
      lock.lock();
      int expected = counter.load();
      std::this_thread::yield();  // Encourage race conditions
      counter.store(expected + 1);
      EXPECT_EQ(counter.load(), expected + 1);
      lock.unlock();
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; i++) {
    threads.emplace_back(critical_section);
  }

  for (auto& t : threads) {
    t.join();
  }
}

/**
 * @brief This test checks correctness under high contention.
 */
TEST(TicketLockTest, StressTest) {
  constexpr int num_threads = 8;
  TicketLock lock;
  std::atomic<int> counter = 0;
  constexpr int num_iterations = 125000;

  auto worker = [&]() {
    for (int i = 0; i < num_iterations; i++) {
      lock.lock();
      counter.fetch_add(1, std::memory_order_relaxed);
      counter.fetch_sub(1, std::memory_order_relaxed);
      lock.unlock();
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; i++) {
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
  constexpr int num_threads = 8;
  TicketLock lock;
  std::atomic<bool> done = false;

  auto worker = [&]() {
    lock.lock();
    done.store(true, std::memory_order_relaxed);
    lock.unlock();
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; i++) {
    threads.emplace_back(worker);
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_TRUE(done.load());
}

