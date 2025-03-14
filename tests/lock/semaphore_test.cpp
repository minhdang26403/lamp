#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "gtest/gtest.h"
#include "lock/semaphore.h"

using namespace std::chrono_literals;

// Test basic acquire and release functionality
TEST(SemaphoreTest, BasicAcquireRelease) {
  Semaphore sem{1};
  sem.acquire();
  EXPECT_EQ(sem.get_value(), 0);
  sem.release();
  EXPECT_EQ(sem.get_value(), 1);
}

// Test try_acquire functionality
TEST(SemaphoreTest, TryAcquire) {
  Semaphore sem{2};
  EXPECT_TRUE(sem.try_acquire());
  EXPECT_EQ(sem.get_value(), 1);
  EXPECT_TRUE(sem.try_acquire());
  EXPECT_EQ(sem.get_value(), 0);
  EXPECT_FALSE(sem.try_acquire());
  EXPECT_EQ(sem.get_value(), 0);
}

// Test try_acquire_for with timeout
TEST(SemaphoreTest, TryAcquireWithTimeout) {
  Semaphore sem{0};
  auto start = std::chrono::steady_clock::now();
  EXPECT_FALSE(sem.try_acquire_for(100us));
  auto end = std::chrono::steady_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  EXPECT_GE(duration.count(), 95);  // Allow for slight timing variations
}

// Test try_acquire_for with release during wait
TEST(SemaphoreTest, TryAcquireForSuccess) {
  Semaphore sem{0};
  std::atomic<bool> acquired(false);

  std::thread t([&]() {
    EXPECT_TRUE(sem.try_acquire_for(50us));
    acquired = true;
  });

  std::this_thread::sleep_for(10us);
  EXPECT_FALSE(acquired);
  sem.release();
  t.join();
  EXPECT_TRUE(acquired);
}

// Test try_acquire(count) functionality
TEST(SemaphoreTest, TryAcquireMultiple) {
  Semaphore sem{10};
  EXPECT_TRUE(sem.try_acquire(5));
  EXPECT_EQ(sem.get_value(), 5);
  EXPECT_TRUE(sem.try_acquire(5));
  EXPECT_EQ(sem.get_value(), 0);
  EXPECT_FALSE(sem.try_acquire(1));
  EXPECT_EQ(sem.get_value(), 0);
}

// Test that acquire blocks when the semaphore value is 0
TEST(SemaphoreTest, AcquireBlocksWhenZero) {
  Semaphore sem{0};
  std::atomic<bool> thread_blocked(false);
  std::atomic<bool> thread_completed(false);

  std::thread t([&]() {
    thread_blocked = true;
    sem.acquire();  // This should block
    thread_blocked = false;
    thread_completed = true;
  });

  // Wait for the thread to start and block
  while (!thread_blocked) {
    std::this_thread::yield();
  }

  // Give the thread some time to potentially complete (which it shouldn't)
  std::this_thread::sleep_for(100us);

  // The thread should still be blocked
  EXPECT_TRUE(thread_blocked);
  EXPECT_FALSE(thread_completed);

  // Release the semaphore
  sem.release();

  // Wait for the thread to complete
  t.join();

  // The thread should have completed
  EXPECT_TRUE(thread_completed);
  EXPECT_FALSE(thread_blocked);
}

// Test that multiple threads can acquire and release the semaphore
TEST(SemaphoreTest, MultipleThreads) {
  constexpr size_t kNumThreads = 10;
  constexpr int kInitValue = 3;

  Semaphore sem{kInitValue};
  std::atomic<uint32_t> completed_count{0};

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (size_t i = 0; i < kNumThreads; i++) {
    threads.emplace_back([&]() {
      sem.acquire();

      // Simulate some work
      std::this_thread::sleep_for(10us);

      sem.release();
      completed_count++;
    });
  }

  // Wait for all threads to complete
  for (auto& t : threads) {
    t.join();
  }

  // All threads should have completed their work
  EXPECT_EQ(completed_count, kNumThreads);
  EXPECT_EQ(sem.get_value(), kInitValue);
}

// Test that multiple threads can work with try_acquire
TEST(SemaphoreTest, MultipleThreadsTryAcquire) {
  constexpr int kNumThreads = 20;
  constexpr int kInitValue = 5;

  Semaphore sem{kInitValue};
  std::atomic<int> success_count{0};
  std::atomic<int> failure_count{0};

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([&]() {
      if (sem.try_acquire()) {
        // Simulate some work
        std::this_thread::sleep_for(10us);
        sem.release();
        success_count++;
      } else {
        failure_count++;
      }
    });
  }

  // Wait for all threads to complete
  for (auto& t : threads) {
    t.join();
  }

  // The sum of successes and failures should equal the number of threads
  EXPECT_EQ(success_count + failure_count, kNumThreads);
  EXPECT_EQ(sem.get_value(), kInitValue);
}

// Test that try_acquire_for works correctly with multiple threads
TEST(SemaphoreTest, MultithreadedTryAcquireFor) {
  constexpr size_t kNumThreads = 10;
  Semaphore sem{0};
  std::atomic<uint32_t> success_count{0};
  std::atomic<uint32_t> timeout_count{0};

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (size_t i = 0; i < kNumThreads; i++) {
    threads.emplace_back([&]() {
      if (sem.try_acquire_for(100us)) {
        success_count++;
      } else {
        timeout_count++;
      }
    });

    // Stagger thread starts
    std::this_thread::sleep_for(10us);
  }

  // Release just one permit
  sem.release();

  // Wait for all threads to complete
  for (auto& t : threads) {
    t.join();
  }

  // Only one thread should have succeeded
  EXPECT_EQ(success_count, 1);
  EXPECT_EQ(timeout_count, kNumThreads - 1);
  EXPECT_EQ(sem.get_value(), 0);
}

// Test that try_acquire(count) works correctly with multiple threads
TEST(SemaphoreTest, MultithreadedTryAcquireCount) {
  constexpr size_t kNumThreads = 10;
  constexpr int kInitValue = 20;
  constexpr int acquire_count = 5;

  Semaphore sem{kInitValue};
  std::atomic<int> success_count{0};
  std::atomic<int> failure_count{0};

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (size_t i = 0; i < kNumThreads; i++) {
    threads.emplace_back([&]() {
      if (sem.try_acquire(acquire_count)) {
        // Simulate some work
        std::this_thread::sleep_for(10us);
        sem.release(acquire_count);
        success_count++;
      } else {
        failure_count++;
      }
    });
  }

  // Wait for all threads to complete
  for (auto& t : threads) {
    t.join();
  }

  // The sum of successes and failures should equal the number of threads
  EXPECT_EQ(success_count + failure_count, kNumThreads);
  EXPECT_EQ(sem.get_value(), kInitValue);
}

// Test edge cases with zero and negative counts
TEST(SemaphoreTest, EdgeCases) {
  Semaphore sem{5};

  // Release with zero count
  sem.release(0);
  EXPECT_EQ(sem.get_value(), 5);

  // Try acquire with zero count
  EXPECT_TRUE(sem.try_acquire(0));
  EXPECT_EQ(sem.get_value(), 5);

  // Try acquire with negative count (should be treated as zero)
  EXPECT_TRUE(sem.try_acquire(-1));
  EXPECT_EQ(sem.get_value(), 5);

  // Release with negative count (should be ignored)
  sem.release(-1);
  EXPECT_EQ(sem.get_value(), 5);
}

// Test that the semaphore count doesn't go below zero
TEST(SemaphoreTest, NoNegativeCount) {
  Semaphore sem{0};

  // Try to acquire when count is already zero
  EXPECT_FALSE(sem.try_acquire());
  EXPECT_EQ(sem.get_value(), 0);

  // Release then acquire
  sem.release();
  EXPECT_EQ(sem.get_value(), 1);
  sem.acquire();
  EXPECT_EQ(sem.get_value(), 0);
}

// Test that multiple releases wake up multiple blocked threads
TEST(SemaphoreTest, MultipleReleasesWakeMultipleThreads) {
  constexpr size_t kNumThreads = 5;
  Semaphore sem{0};
  std::atomic<uint32_t> woken_count{0};

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (size_t i = 0; i < kNumThreads; i++) {
    threads.emplace_back([&]() {
      sem.acquire();  // This should block
      woken_count++;
    });
  }

  // Give threads time to start and block
  std::this_thread::sleep_for(100us);

  // No threads should be woken yet
  EXPECT_EQ(woken_count, 0);

  // Release enough for all threads
  sem.release(kNumThreads);

  // Wait for all threads to complete
  for (auto& t : threads) {
    t.join();
  }

  // All threads should have been woken
  EXPECT_EQ(woken_count, kNumThreads);
  EXPECT_EQ(sem.get_value(), 0);
}

// Stress test with many threads and operations
TEST(SemaphoreTest, StressTest) {
  constexpr size_t kNumThreads = 64;
  constexpr size_t kOperationsPerThread = 100;
  constexpr int kInitValue = 10;

  Semaphore sem{kInitValue};
  std::atomic<uint32_t> completed_operations{0};

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (size_t i = 0; i < kNumThreads; i++) {
    threads.emplace_back([&, i]() {
      for (size_t j = 0; j < kOperationsPerThread; j++) {
        if ((i + j) % 4 == 0) {
          sem.acquire();
          sem.release();
        } else if ((i + j) % 4 == 1) {
          if (sem.try_acquire()) {
            sem.release();  
          }
        } else if ((i + j) % 4 == 2) {
          if (sem.try_acquire_for(100us)) {
            sem.release();
          }
        } else {
          if (sem.try_acquire(2)) {
            sem.release(2);
          }
        }
        completed_operations++;
      }
    });
  }

  // Wait for all threads to complete
  for (auto& t : threads) {
    t.join();
  }

  // All operations should have completed
  EXPECT_EQ(completed_operations, kNumThreads * kOperationsPerThread);
  // The final value should be the same as the initial value
  EXPECT_EQ(sem.get_value(), kInitValue);
}
