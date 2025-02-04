#include "ttas_lock.h"

#include <atomic>
#include <thread>

#include <gtest/gtest.h>

/**
 * @brief This test ensures that at most one thread is in the critical section
 * at any time.
 */
TEST(TTASLockTest, MutualExclusion) {
  constexpr uint32_t num_threads = 8;
  constexpr uint32_t num_iterations = 10000;

  TTASLock lock;
  uint32_t counter = 0;

  auto critical_section = [&]() {
    for (uint32_t i = 0; i < num_iterations; i++) {
      lock.lock();
      uint32_t prev = counter;
      counter++;
      std::this_thread::yield();
      EXPECT_EQ(counter, prev + 1) << "Race condition detected!";
      lock.unlock();
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(num_threads);
  for (uint32_t i = 0; i < num_threads; i++) {
    threads.emplace_back(critical_section);
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(counter, num_threads * num_iterations)
      << "Final counter value incorrect!";
}

/**
 * @brief This test checks correctness under high contention.
 */
TEST(TTASLockTest, StressTest) {
  constexpr uint32_t num_threads = 8;
  constexpr uint32_t num_iterations = 125000;

  TTASLock lock;
  std::atomic<uint32_t> counter = 0;

  auto worker = [&]() {
    for (uint32_t i = 0; i < num_iterations; i++) {
      lock.lock();
      counter.fetch_add(1, std::memory_order_relaxed);
      counter.fetch_sub(1, std::memory_order_relaxed);
      lock.unlock();
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(num_threads);
  for (uint32_t i = 0; i < num_threads; i++) {
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
TEST(TTASLockTest, NoDeadLock) {
  constexpr uint32_t num_threads = 8;

  TTASLock lock;
  bool done = false;

  auto worker = [&]() {
    lock.lock();
    done = true;
    lock.unlock();
  };

  std::vector<std::thread> threads;
  threads.reserve(num_threads);
  for (uint32_t i = 0; i < num_threads; i++) {
    threads.emplace_back(worker);
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_TRUE(done);
}
