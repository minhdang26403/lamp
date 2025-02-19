#include "peterson_lock.h"

#include <gtest/gtest.h>
#include <atomic>
#include <thread>

/**
 * @brief This test ensures that two threads cannot enter the critical section
 * simultaneously.
 */
TEST(PetersonLockTest, MutualExclusion) {
  constexpr uint32_t kNumIterations = 1000;

  PetersonLock lock;
  uint32_t counter = 0;

  auto critical_section = [&](uint32_t id) {
    for (uint32_t i = 0; i < kNumIterations; ++i) {
      lock.lock(id);
      uint32_t expected = counter++;
      std::this_thread::yield();  // Yield to encourage race conditions
      EXPECT_EQ(counter, expected + 1);
      lock.unlock(id);
    }
  };

  std::thread t1(critical_section, 0);
  std::thread t2(critical_section, 1);

  t1.join();
  t2.join();

  EXPECT_EQ(counter, kNumIterations * 2);
}

/**
 * @brief This test ensures the lock can be lockd and released repeatedly
 * without issues.
 */
TEST(PetersonLockTest, StressTest) {
  constexpr uint32_t kNumIterations = 1000000;

  PetersonLock lock;
  std::atomic<uint32_t> counter = 0;

  auto worker = [&](uint32_t id) {
    for (uint32_t i = 0; i < kNumIterations; i++) {
      lock.lock(id);
      counter.fetch_add(1, std::memory_order_relaxed);
      counter.fetch_sub(1, std::memory_order_relaxed);
      lock.unlock(id);
    }
  };

  std::thread t1(worker, 0);
  std::thread t2(worker, 1);

  t1.join();
  t2.join();

  EXPECT_EQ(counter.load(std::memory_order_relaxed), 0);
}

/**
 * @brief This test ensures no deadlocks occur.
 */
TEST(PetersonLockTest, NoDeadLock) {
  PetersonLock lock;
  bool done = false;

  auto worker = [&](int id) {
    lock.lock(id);
    done = true;
    lock.unlock(id);
  };

  std::thread t1(worker, 0);
  std::thread t2(worker, 1);

  t1.join();
  t2.join();

  EXPECT_TRUE(done);
}
