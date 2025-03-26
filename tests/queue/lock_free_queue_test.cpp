#include "queue/lock_free_queue.h"

#include <random>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

class LockFreeQueueTest : public ::testing::Test {
 protected:
  void SetUp() override { queue_ = std::make_unique<LockFreeQueue<int>>(); }

  std::unique_ptr<LockFreeQueue<int>> queue_;
};

// Basic functionality tests
TEST_F(LockFreeQueueTest, EnqueueDequeueSingleItem) {
  queue_->enqueue(42);
  EXPECT_EQ(queue_->dequeue(), 42);
}

TEST_F(LockFreeQueueTest, DequeueEmptyThrows) {
  EXPECT_THROW(queue_->dequeue(), EmptyException);
}

TEST_F(LockFreeQueueTest, MultipleItems) {
  queue_->enqueue(1);
  queue_->enqueue(2);
  queue_->enqueue(3);

  EXPECT_EQ(queue_->dequeue(), 1);
  EXPECT_EQ(queue_->dequeue(), 2);
  EXPECT_EQ(queue_->dequeue(), 3);
}

// Edge cases
TEST_F(LockFreeQueueTest, EnqueueAfterDequeueEmpty) {
  queue_->enqueue(1);
  EXPECT_EQ(queue_->dequeue(), 1);
  EXPECT_THROW(queue_->dequeue(), EmptyException);
  queue_->enqueue(2);
  EXPECT_EQ(queue_->dequeue(), 2);
}

// Concurrent operations test
TEST_F(LockFreeQueueTest, ConcurrentEnqueueDequeue) {
  constexpr size_t kNumThreads = 8;
  constexpr size_t kItemsPerThread = 1000;

  std::vector<std::thread> producers;
  std::vector<std::thread> consumers;
  std::atomic<size_t> enqueue_count{0};
  std::atomic<size_t> dequeue_count{0};

  producers.reserve(kNumThreads);
  consumers.reserve(kNumThreads);

  // Producer threads
  for (size_t t = 0; t < kNumThreads; t++) {
    producers.emplace_back([this, &enqueue_count, t]() {
      std::mt19937 gen(static_cast<unsigned int>(t + 100));
      std::uniform_int_distribution<> dist(0, 999);

      for (size_t i = 0; i < kItemsPerThread; i++) {
        queue_->enqueue(dist(gen));
        enqueue_count++;
      }
    });
  }

  // Consumer threads
  for (size_t t = 0; t < kNumThreads; t++) {
    consumers.emplace_back([this, &dequeue_count, t]() {
      std::mt19937 gen(static_cast<unsigned int>(t + 200));
      std::bernoulli_distribution should_dequeue(0.8);

      for (size_t i = 0; i < kItemsPerThread; i++) {
        if (should_dequeue(gen)) {
          try {
            queue_->dequeue();
            dequeue_count++;
          } catch (const EmptyException&) {
            // Expected when queue is empty
          }
        }
      }
    });
  }

  // Join all threads
  for (auto& thread : producers) {
    thread.join();
  }
  for (auto& thread : consumers) {
    thread.join();
  }

  // Verify all remaining items can be dequeued
  size_t remaining_count = 0;
  while (true) {
    try {
      queue_->dequeue();
      remaining_count++;
    } catch (const EmptyException&) {
      break;
    }
  }

  // Total enqueued should equal total dequeued plus remaining
  EXPECT_EQ(enqueue_count.load(), dequeue_count.load() + remaining_count);
}

// Stress test with mixed operations
TEST_F(LockFreeQueueTest, StressTest) {
  constexpr size_t kNumThreads = 16;
  constexpr size_t kOperationsPerThread = 5000;

  std::vector<std::thread> threads;
  std::atomic<size_t> enqueue_count{0};
  std::atomic<size_t> dequeue_count{0};

  threads.reserve(kNumThreads);

  // Pre-populate with some items
  for (int i = 0; i < 100; i++) {
    queue_->enqueue(i);
  }

  for (size_t t = 0; t < kNumThreads; t++) {
    threads.emplace_back([this, &enqueue_count, &dequeue_count, t]() {
      std::mt19937 gen(static_cast<unsigned int>(t + 100));
      std::uniform_int_distribution<> op_dist(0, 1);  // 0=enqueue, 1=dequeue
      std::uniform_int_distribution<> val_dist(0, 999);

      for (size_t i = 0; i < kOperationsPerThread; i++) {
        int operation = op_dist(gen);

        switch (operation) {
          case 0: {  // Enqueue
            queue_->enqueue(val_dist(gen));
            enqueue_count++;
            break;
          }
          case 1: {  // Dequeue
            try {
              queue_->dequeue();
              dequeue_count++;
            } catch (const EmptyException&) {
              // Expected when queue is empty
            }
            break;
          }
        }
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // Drain remaining items
  size_t remaining_count = 0;
  while (true) {
    try {
      queue_->dequeue();
      remaining_count++;
    } catch (const EmptyException&) {
      break;
    }
  }

  // Verify counts match
  size_t initial_items = 100;
  size_t total_enqueued = initial_items + enqueue_count.load();
  size_t total_dequeued = dequeue_count.load() + remaining_count;
  EXPECT_EQ(total_enqueued, total_dequeued);
}

// Test rapid enqueue/dequeue from single thread
TEST_F(LockFreeQueueTest, RapidSingleThreadOperations) {
  constexpr size_t kOperations = 10000;

  for (size_t i = 0; i < kOperations; i++) {
    queue_->enqueue(static_cast<int>(i));
  }

  for (size_t i = 0; i < kOperations; i++) {
    EXPECT_EQ(queue_->dequeue(), static_cast<int>(i));
  }

  EXPECT_THROW(queue_->dequeue(), EmptyException);
}
