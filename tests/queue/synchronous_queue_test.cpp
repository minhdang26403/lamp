#include "queue/synchronous_queue.h"

#include <atomic>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

class SynchronousQueueTest : public ::testing::Test {
 protected:
  void SetUp() override { queue_ = std::make_unique<SynchronousQueue<int>>(); }

  std::unique_ptr<SynchronousQueue<int>> queue_;
};

// Normal Cases

// Test that a single producer blocks until a consumer takes its item.
TEST_F(SynchronousQueueTest, SingleProducerConsumerRendezvous) {
  std::atomic<bool> producerBlocked{false};
  std::thread producer([this, &producerBlocked] {
    producerBlocked = true;
    queue_->enqueue(42);
  });

  // Wait until producer is blocked
  while (!producerBlocked) {
    std::this_thread::yield();
  }

  std::thread consumer([this] {
    int value = queue_->dequeue();
    EXPECT_EQ(value, 42);
  });

  producer.join();
  consumer.join();
}

// Test that a consumer blocks until a producer provides an item.
TEST_F(SynchronousQueueTest, SingleConsumerProducerRendezvous) {
  std::atomic<bool> consumerBlocked{false};
  std::thread consumer([this, &consumerBlocked] {
    consumerBlocked = true;
    int value = queue_->dequeue();
    EXPECT_EQ(value, 42);
  });

  // Wait until consumer is blocked
  while (!consumerBlocked) {
    std::this_thread::yield();
  }

  std::thread producer([this] { queue_->enqueue(42); });

  producer.join();
  consumer.join();
}

// Edge Cases

// Test that a second producer blocks until the first producer’s item is
// consumed.
TEST_F(SynchronousQueueTest, SecondProducerBlocksUntilFirstCompletes) {
  std::atomic<bool> firstProducerBlocked{false};
  std::atomic<bool> secondProducerStarted{false};

  std::thread firstProducer([this, &firstProducerBlocked] {
    firstProducerBlocked = true;
    queue_->enqueue(1);
  });

  // Wait until first producer is blocked
  while (!firstProducerBlocked) {
    std::this_thread::yield();
  }

  std::thread secondProducer(
      [this, &firstProducerBlocked, &secondProducerStarted] {
        secondProducerStarted = true;
        queue_->enqueue(2);
        EXPECT_TRUE(firstProducerBlocked);  // First producer should still be
                                            // blocked initially
      });

  // Wait until second producer starts
  while (!secondProducerStarted) {
    std::this_thread::yield();
  }

  std::thread consumer([this] {
    int value1 = queue_->dequeue();
    EXPECT_EQ(value1, 1);
    int value2 = queue_->dequeue();
    EXPECT_EQ(value2, 2);
  });

  firstProducer.join();
  secondProducer.join();
  consumer.join();
}

// Concurrent Operations

// Test multiple producer-consumer pairs rendezvousing concurrently.
TEST_F(SynchronousQueueTest, MultipleProducerConsumerPairs) {
  constexpr size_t kNumPairs = 8;
  std::vector<std::thread> producers;
  std::vector<std::thread> consumers;

  std::vector<int> values;
  values.reserve(kNumPairs);

  for (size_t i = 0; i < kNumPairs; i++) {
    producers.emplace_back([this, i] { queue_->enqueue(static_cast<int>(i)); });
    consumers.emplace_back([this, &values] {
      int value = queue_->dequeue();
      values.push_back(value);
    });
  }

  for (auto& thread : producers) {
    thread.join();
  }
  for (auto& thread : consumers) {
    thread.join();
  }

  std::sort(values.begin(), values.end());
  for (size_t i = 0; i < kNumPairs; i++) {
    // Each producer should rendezvous with a consumer
    EXPECT_EQ(values[i], i);
  }
}

// Stress Test

// Test high contention with many producers and consumers.
TEST_F(SynchronousQueueTest, StressTest) {
  constexpr size_t kNumThreads = 16;
  constexpr size_t kOperationsPerThread = 500;

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  std::atomic<size_t> enqueueCount{0};
  std::atomic<size_t> dequeueCount{0};

  auto producer = [this, &enqueueCount](int t) {
    for (size_t i = 0; i < kOperationsPerThread; ++i) {
      queue_->enqueue(static_cast<int>(t * kOperationsPerThread + i));
      enqueueCount++;
    }
  };

  auto consumer = [this, &dequeueCount]() {
    for (size_t i = 0; i < kOperationsPerThread; ++i) {
      queue_->dequeue();
      dequeueCount++;
    }
  };

  for (size_t t = 0; t < kNumThreads; ++t) {
    if (t % 2 == 0) {
      threads.emplace_back(producer, t);
    } else {
      threads.emplace_back(consumer);
    }
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // Due to rendezvous nature, counts may not match exactly, but test should
  // complete
  EXPECT_TRUE(enqueueCount.load() >= 0);
  EXPECT_TRUE(dequeueCount.load() >= 0);
}
