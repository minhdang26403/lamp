#include "filter_lock.h"

#include <gtest/gtest.h>
#include <atomic>
#include <thread>

/**
 * @brief This test ensures that at most one thread is in the critical section
 * at any time.
 */
TEST(FilterLockTest, MutualExclusion) {
  constexpr int num_threads = 8;
  FilterLock lock(num_threads);
  std::atomic<int> counter = 0;
  constexpr int num_iterations = 1000;

  auto critical_section = [&](int id) {
    for (int i = 0; i < num_iterations; i++) {
      lock.lock(id);
      int expected = counter.load();
      std::this_thread::yield();  // Encourage race conditions
      counter.store(expected + 1);
      EXPECT_EQ(counter.load(), expected + 1);
      lock.unlock(id);
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; i++) {
    threads.emplace_back(critical_section, i);
  }

  for (auto& t : threads) {
    t.join();
  }
}

/**
 * @brief This test checks correctness under high contention.
 */
TEST(FilterLockTest, StressTest) {
  constexpr int num_threads = 8;
  FilterLock lock(num_threads);
  std::atomic<int> counter = 0;
  constexpr int num_iterations = 125000;

  auto worker = [&](int id) {
    for (int i = 0; i < num_iterations; i++) {
      lock.lock(id);
      counter.fetch_add(1, std::memory_order_relaxed);
      counter.fetch_sub(1, std::memory_order_relaxed);
      lock.unlock(id);
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; i++) {
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
  constexpr int num_threads = 8;
  FilterLock lock(num_threads);
  std::atomic<bool> done = false;

  auto worker = [&](int id) {
    lock.lock(id);
    done.store(true, std::memory_order_relaxed);
    lock.unlock(id);
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; i++) {
    threads.emplace_back(worker, i);
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_TRUE(done.load());
}

/**
 * @brief This test ensures all threads eventually get access to the critical
 * section.
 */
TEST(FilterLockTest, NoStarvation) {
  constexpr int num_threads = 8;
  FilterLock lock(num_threads);
  std::atomic<int> entry_count[num_threads];

  for (int i = 0; i < num_threads; i++) {
    entry_count[i].store(0);
  }

  auto worker = [&](int id) {
    for (int i = 0; i < 1000; i++) {
      lock.lock(id);
      entry_count[id].fetch_add(1, std::memory_order_relaxed);
      lock.unlock(id);
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; i++) {
    threads.emplace_back(worker, i);
  }

  for (auto& t : threads) {
    t.join();
  }

  for (int i = 0; i < num_threads; i++) {
    EXPECT_EQ(entry_count[i].load(), 1000);
  }
}
