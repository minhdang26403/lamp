#include "synchronization/reentrant_lock.h"

#include <atomic>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

using namespace std::chrono_literals;

/**
 * @brief This test verifies basic locking and unlocking functionality
 * of the ReentrantLock class.
 */
TEST(ReentrantLockTest, BasicLockUnlock) {
  ReentrantLock lock;

  // Simple lock and unlock
  lock.lock();
  lock.unlock();

  // Second lock and unlock should work too
  lock.lock();
  lock.unlock();
}

/**
 * @brief This test ensures that the lock is reentrant for the same thread.
 */
TEST(ReentrantLockTest, Reentrancy) {
  ReentrantLock lock;

  // First acquisition
  lock.lock();

  // Second acquisition (reentrant)
  lock.lock();

  // Unlock in reverse order
  lock.unlock();
  lock.unlock();
}

/**
 * @brief This test verifies that unlocking without owning the lock throws an
 * exception.
 */
TEST(ReentrantLockTest, UnlockWithoutOwning) {
  ReentrantLock lock;

  // Unlock without locking should throw
  EXPECT_THROW(lock.unlock(), std::runtime_error);

  // Lock and then unlock multiple times should throw
  lock.lock();
  lock.unlock();
  EXPECT_THROW(lock.unlock(), std::runtime_error);
}

/**
 * @brief This test ensures that multiple threads can acquire the lock
 * when it's released.
 */
TEST(ReentrantLockTest, MultipleThreadsAcquire) {
  constexpr size_t kNumThreads = 8;
  constexpr size_t kNumIterations = 1000;

  ReentrantLock lock;
  uint32_t counter = 0;

  auto critical_section = [&]() {
    for (size_t i = 0; i < kNumIterations; i++) {
      lock.lock();
      uint32_t prev = counter;
      counter++;
      std::this_thread::yield();
      EXPECT_EQ(counter, prev + 1) << "Race condition detected!";
      lock.unlock();
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (size_t i = 0; i < kNumThreads; i++) {
    threads.emplace_back(critical_section);
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(counter, kNumThreads * kNumIterations)
      << "Final counter value incorrect!";
}

/**
 * @brief This test verifies that the lock properly handles reentrancy
 * across multiple threads.
 */
TEST(ReentrantLockTest, ReentrancyAcrossThreads) {
  ReentrantLock lock;
  std::atomic<bool> thread_started{false};
  std::atomic<bool> lock_acquired{false};
  std::atomic<bool> test_complete{false};

  // Thread 1 acquires the lock and holds it
  std::thread t1([&]() {
    lock.lock();
    thread_started = true;

    // Wait until the main thread confirms it observed the lock being held
    while (!lock_acquired.load()) {
      std::this_thread::sleep_for(10us);
    }

    // Release the lock
    lock.unlock();

    // Wait for the test to complete
    while (!test_complete.load()) {
      std::this_thread::sleep_for(10us);
    }
  });

  // Wait for thread 1 to acquire the lock
  while (!thread_started.load()) {
    std::this_thread::sleep_for(10us);
  }

  // Try to acquire the lock - should block
  std::atomic<bool> thread2_blocked{false};
  std::thread t2([&]() {
    thread2_blocked = true;
    lock.lock();
    lock.unlock();
    thread2_blocked = false;
  });

  // Give thread 2 time to try acquiring the lock
  std::this_thread::sleep_for(100us);

  // Thread 2 should be blocked
  EXPECT_TRUE(thread2_blocked.load());

  // Signal thread 1 to release the lock
  lock_acquired = true;

  // Wait for thread 2 to acquire and release the lock
  std::this_thread::sleep_for(1ms);

  // Thread 2 should have completed
  EXPECT_FALSE(thread2_blocked.load());

  // Clean up
  test_complete = true;
  t1.join();
  t2.join();
}

/**
 * @brief This test verifies that reentrant locking works as expected
 * with recursive functions.
 */
TEST(ReentrantLockTest, RecursiveLocking) {
  ReentrantLock lock;
  uint32_t count = 0;

  // Define a recursive function that uses the lock
  std::function<void(int)> recursive_function = [&](int depth) {
    // Base case
    if (depth <= 0) {
      return;
    }

    lock.lock();
    count++;

    // Recursive call - reentrancy allows this to work
    recursive_function(depth - 1);

    lock.unlock();
  };

  // Call with depth 5 (should acquire lock 5 times)
  recursive_function(5);

  // Count should be 5
  EXPECT_EQ(count, 5);
}

/**
 * @brief This test verifies correct behavior when one thread attempts
 * to unlock a lock owned by another thread.
 */
TEST(ReentrantLockTest, UnlockFromDifferentThread) {
  ReentrantLock lock;
  std::atomic<bool> lock_acquired{false};
  std::atomic<bool> unlock_attempted{false};
  std::atomic<bool> exception_thrown{false};

  // Thread 1 acquires the lock
  std::thread t1([&]() {
    lock.lock();
    lock_acquired = true;

    // Wait until the second thread attempts to unlock
    while (!unlock_attempted.load()) {
      std::this_thread::sleep_for(100us);
    }

    // Now we can release the lock
    lock.unlock();
  });

  // Wait for thread 1 to acquire the lock
  while (!lock_acquired.load()) {
    std::this_thread::sleep_for(100us);
  }

  // Thread 2 tries to unlock the lock owned by thread 1
  std::thread t2([&]() {
    try {
      lock.unlock();
    } catch (const std::runtime_error&) {
      exception_thrown = true;
    }
    unlock_attempted = true;
  });

  t2.join();
  t1.join();

  // Exception should have been thrown
  EXPECT_TRUE(exception_thrown);
}

/**
 * @brief This test verifies that ConditionVariable's notify_all works correctly
 * by waking up all waiting threads when the lock is released.
 */
TEST(ReentrantLockTest, NotifyAllWakesUpWaitingThreads) {
  constexpr size_t kNumThreads = 5;

  ReentrantLock lock;
  std::atomic<size_t> waiting_thread_count{0};
  std::atomic<int> woken_thread_count{0};
  std::atomic<bool> main_thread_has_lock{false};

  // Lock from the main thread
  lock.lock();
  main_thread_has_lock = true;

  // Create threads that will wait for the lock
  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);

  for (size_t i = 0; i < kNumThreads; i++) {
    threads.emplace_back([&]() {
      // Wait until main thread has the lock
      while (!main_thread_has_lock.load()) {
        std::this_thread::sleep_for(10us);
      }

      // Try to acquire the lock - this will block
      waiting_thread_count++;
      lock.lock();

      // When we get here, we've been woken up
      woken_thread_count++;

      lock.unlock();
    });
  }

  // Wait until all threads are waiting
  while (waiting_thread_count.load() < kNumThreads) {
    std::this_thread::sleep_for(10us);
  }

  // Give threads time to ensure they're all blocked on the lock
  std::this_thread::sleep_for(500us);

  // No threads should have been woken yet
  EXPECT_EQ(woken_thread_count.load(), 0);

  // Release the lock - should wake up all waiting threads
  lock.unlock();

  // Give threads time to wake up
  std::this_thread::sleep_for(500us);

  // All threads should have been woken
  EXPECT_EQ(woken_thread_count.load(), kNumThreads);

  // Clean up
  for (auto& t : threads) {
    t.join();
  }
}
