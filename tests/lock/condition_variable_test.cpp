#include "lock/condition_variable.h"

#include <chrono>
#include <thread>

#include "gtest/gtest.h"
#include "lock/ttas_lock.h"

using namespace std::chrono_literals;

class ConditionVariableTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Reset shared variables
    shared_counter_ = 0;
    ready_flag_ = false;
  }

  // Shared variables for tests
  uint32_t shared_counter_{0};
  bool ready_flag_{false};
  TTASLock mutex_;
  ConditionVariable cv_;
};

TEST_F(ConditionVariableTest, BasicWaitAndNotifyOne) {
  std::thread consumer([this]() {
    // Wait for producer to set the flag
    mutex_.lock();
    while (!ready_flag_) {
      cv_.wait(mutex_);
    }
    shared_counter_++;
    mutex_.unlock();
  });

  std::thread producer([this]() {
    std::this_thread::sleep_for(100us);
    mutex_.lock();
    ready_flag_ = true;
    mutex_.unlock();
    cv_.notify_one();
  });

  producer.join();
  consumer.join();

  EXPECT_EQ(shared_counter_, 1) << "Consumer should increment counter once";
}

// Test notify_all with multiple threads waiting
TEST_F(ConditionVariableTest, NotifyAll) {
  constexpr uint32_t kNumThreads = 10;
  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);

  for (uint32_t i = 0; i < kNumThreads; i++) {
    threads.emplace_back([this]() {
      mutex_.lock();
      while (!ready_flag_) {
        cv_.wait(mutex_);
      }
      shared_counter_++;
      mutex_.unlock();
    });
  }

  // Give threads time to start and wait
  std::this_thread::sleep_for(1ms);

  mutex_.lock();
  ready_flag_ = true;
  mutex_.unlock();
  cv_.notify_all();

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(shared_counter_, kNumThreads)
      << "All threads should increment counter";
}

// Test predicate-based wait
TEST_F(ConditionVariableTest, PredicateWait) {
  std::thread consumer([this]() {
    mutex_.lock();
    cv_.wait(mutex_, [this]() { return ready_flag_; });
    shared_counter_++;
    mutex_.unlock();
  });

  std::thread producer([this]() {
    std::this_thread::sleep_for(100us);
    mutex_.lock();
    ready_flag_ = true;
    mutex_.unlock();
    cv_.notify_one();
  });

  producer.join();
  consumer.join();

  EXPECT_EQ(shared_counter_, 1) << "Consumer should increment counter once";
}

// Test wait_for with timeout that expires
TEST_F(ConditionVariableTest, WaitForTimeout) {
  auto start = std::chrono::steady_clock::now();

  mutex_.lock();
  auto status = cv_.wait_for(mutex_, 5ms);
  mutex_.unlock();

  auto elapsed = std::chrono::steady_clock::now() - start;

  EXPECT_EQ(status, CVStatus::kTimeout) << "wait_for should timeout";
  EXPECT_GE(elapsed, 5ms)
      << "wait_for should wait at least the specified duration";
  EXPECT_LT(elapsed, 6ms) << "wait_for shouldn't wait too long";
}

// Test wait_for with notification before timeout
TEST_F(ConditionVariableTest, WaitForNotified) {
  std::atomic<bool> thread_started{false};

  std::thread notifier([this, &thread_started]() {
    // Wait for the test thread to start waiting
    while (!thread_started.load()) {
      std::this_thread::yield();
    }

    std::this_thread::sleep_for(1ms);
    cv_.notify_one();
  });

  thread_started.store(true);

  mutex_.lock();
  auto status = cv_.wait_for(mutex_, 5ms);
  mutex_.unlock();

  notifier.join();

  EXPECT_EQ(status, CVStatus::kNoTimeout)
      << "wait_for should not timeout when notified";
}

// Test wait_until with timeout
TEST_F(ConditionVariableTest, WaitUntilTimeout) {
  auto deadline = std::chrono::steady_clock::now() + 1ms;

  mutex_.lock();
  auto status = cv_.wait_until(mutex_, deadline);
  mutex_.unlock();

  EXPECT_EQ(status, CVStatus::kTimeout) << "wait_until should timeout";
  EXPECT_GE(std::chrono::steady_clock::now(), deadline)
      << "wait_until should wait until the deadline";
}

// Test predicate-based wait_for
TEST_F(ConditionVariableTest, PredicateWaitFor) {
  std::thread producer([this]() {
    std::this_thread::sleep_for(1ms);
    mutex_.lock();
    ready_flag_ = true;
    mutex_.unlock();
    cv_.notify_one();
  });

  mutex_.lock();
  bool result = cv_.wait_for(mutex_, 5ms, [this]() { return ready_flag_; });
  mutex_.unlock();

  producer.join();

  EXPECT_TRUE(result)
      << "Predicate wait_for should return true when condition is met";
}

// Test predicate-based wait_for with timeout
TEST_F(ConditionVariableTest, PredicateWaitForTimeout) {
  mutex_.lock();
  bool result = cv_.wait_for(mutex_, 1ms, [this]() { return ready_flag_; });
  mutex_.unlock();

  EXPECT_FALSE(result) << "Predicate wait_for should return false on timeout";
  EXPECT_FALSE(ready_flag_) << "ready_flag should still be false";
}

// Test multiple waits and notifications in sequence
TEST_F(ConditionVariableTest, SequentialWaitsAndNotifications) {
  constexpr uint32_t kNumIterations = 5;

  std::thread consumer([this]() {
    for (uint32_t i = 0; i < kNumIterations; i++) {
      mutex_.lock();
      while (shared_counter_ != i * 2) {
        cv_.wait(mutex_);
      }
      shared_counter_++;
      mutex_.unlock();
      cv_.notify_one();
    }
  });

  std::thread producer([this]() {
    for (uint32_t i = 0; i < kNumIterations; i++) {
      mutex_.lock();
      while (shared_counter_ != i * 2 + 1) {
        cv_.wait(mutex_);
      }
      shared_counter_++;
      mutex_.unlock();
      cv_.notify_one();
    }
  });

  consumer.join();
  producer.join();

  EXPECT_EQ(shared_counter_, kNumIterations * 2)
      << "Counter should be incremented by both threads";
}

// Test for spurious wakeups handling
TEST_F(ConditionVariableTest, SpuriousWakeups) {
  uint32_t wakeup_counter = 0;

  std::thread consumer([this, &wakeup_counter]() {
    mutex_.lock();
    while (!ready_flag_) {
      wakeup_counter++;
      cv_.wait(mutex_);
    }
    mutex_.unlock();
  });

  // Send multiple notifications without setting the flag
  for (uint32_t i = 0; i < 5; i++) {
    std::this_thread::sleep_for(100us);
    cv_.notify_one();
  }

  // Finally set the flag and notify
  std::this_thread::sleep_for(100us);
  mutex_.lock();
  ready_flag_ = true;
  mutex_.unlock();
  cv_.notify_one();

  consumer.join();

  EXPECT_GE(wakeup_counter, 1) << "Consumer should handle wakeups correctly";
}

// Stress test with many threads
TEST_F(ConditionVariableTest, StressTest) {
  constexpr uint32_t kNumProducers = 20;
  constexpr uint32_t kNumConsumers = 20;
  constexpr uint32_t kItemsPerProducer = 100;

  std::atomic<uint32_t> produced{0};
  std::atomic<uint32_t> consumed{0};
  std::vector<uint32_t> queue;

  std::vector<std::thread> producers;
  std::vector<std::thread> consumers;

  // Create consumer threads
  for (uint32_t i = 0; i < kNumConsumers; i++) {
    consumers.emplace_back([this, &queue, &consumed]() {
      while (consumed < kItemsPerProducer * kNumProducers) {
        mutex_.lock();

        // Wait until there's an item in the queue or we've consumed everything
        cv_.wait(mutex_, [&queue, &consumed]() {
          return !queue.empty() ||
                 consumed >= kItemsPerProducer * kNumProducers;
        });

        // Check if there's still work to do
        if (consumed >= kItemsPerProducer * kNumProducers) {
          mutex_.unlock();
          break;
        }

        // Consume an item
        if (!queue.empty()) {
          queue.pop_back();
          consumed++;
        }

        mutex_.unlock();
        cv_.notify_all();  // Notify producers that there's space
      }
    });
  }

  // Create producer threads
  for (uint32_t i = 0; i < kNumProducers; i++) {
    producers.emplace_back([this, i, &queue, &produced]() {
      for (uint32_t j = 0; j < kItemsPerProducer; j++) {
        mutex_.lock();

        // Add item to queue
        queue.push_back(i * 1000 + j);
        produced++;

        mutex_.unlock();
        cv_.notify_all();  // Notify consumers that there's an item

        // Randomized small delay
        std::this_thread::sleep_for(std::chrono::microseconds(rand() % 10));
      }
    });
  }

  // Join all threads
  for (auto& p : producers) {
    p.join();
  }

  // Notify any remaining consumers
  cv_.notify_all();

  for (auto& c : consumers) {
    c.join();
  }

  EXPECT_EQ(produced.load(), kItemsPerProducer * kNumProducers)
      << "All items should be produced";
  EXPECT_EQ(consumed.load(), kItemsPerProducer * kNumProducers)
      << "All items should be consumed";
  EXPECT_TRUE(queue.empty()) << "Queue should be empty at the end";
}

// Test proper cleanup/destruction
TEST_F(ConditionVariableTest, DestructionTest) {
  {
    ConditionVariable local_cv;
    TTASLock local_mutex;

    std::thread waiter([&local_cv, &local_mutex]() {
      local_mutex.lock();
      // Start waiting but will be interrupted when cv is destroyed
      local_cv.wait_for(local_mutex, 5ms);
      local_mutex.unlock();
    });

    // Give the thread time to start waiting
    std::this_thread::sleep_for(1ms);

    // Let the thread finish before destroying
    local_cv.notify_all();
    waiter.join();

    // Now local_cv will be destroyed at the end of this scope
  }

  // If we get here without deadlock or crash, the test passes
  SUCCEED() << "ConditionVariable destruction should not cause issues";
}
