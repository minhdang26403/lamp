#include "peterson_lock.h"

#include <gtest/gtest.h>
#include <atomic>
#include <thread>

/**
 * @brief This test ensures that two threads cannot enter the critical section
 * simultaneously.
 */
TEST(PetersonLockTest, MutualExclusion) {
  PetersonLock lock;
  constexpr int num_iterations = 1000;
  std::atomic<int> counter = 0;

  auto critical_section = [&](int id) {
    for (int i = 0; i < num_iterations; ++i) {
      lock.Acquire(id);
      int expected = counter.load();
      std::this_thread::yield();  // Yield to encourage race conditions
      counter.store(expected + 1);
      EXPECT_EQ(counter.load(), expected + 1);
      lock.Release(id);
    }
  };

  std::thread t1(critical_section, 0);
  std::thread t2(critical_section, 1);

  t1.join();
  t2.join();
}

/**
 * @brief This test ensures the lock can be acquired and released repeatedly
 * without issues.
 */
TEST(PetersonLockTest, StressTest) {
  PetersonLock lock;
  constexpr int num_iterations = 1000000;
  std::atomic<int> counter = 0;

  auto worker = [&](int id) {
    for (int i = 0; i < num_iterations; i++) {
      lock.Acquire(id);
      counter.fetch_add(1, std::memory_order_relaxed);
      counter.fetch_sub(1, std::memory_order_relaxed);
      lock.Release(id);
    }
  };

  std::thread t1(worker, 0);
  std::thread t2(worker, 1);

  t1.join();
  t2.join();

  EXPECT_EQ(counter.load(), 0);
}

/**
 * @brief This test ensures no deadlocks occur.
 */
TEST(PetersonLockTest, NoDeadLock) {
  PetersonLock lock;
  std::atomic<bool> done = false;

  auto worker = [&](int id) {
    lock.Acquire(id);
    done.store(true, std::memory_order_relaxed);
    lock.Release(id);
  };

  std::thread t1(worker, 0);
  std::thread t2(worker, 1);

  t1.join();
  t2.join();

  EXPECT_TRUE(done.load());
}
