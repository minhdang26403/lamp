#include "queue/unbounded_queue.h"

#include <algorithm>
#include <mutex>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

class UnboundedQueueTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}
};

// Test 1: Basic enqueue and dequeue with a single element
TEST_F(UnboundedQueueTest, SingleEnqueueDequeue) {
  UnboundedQueue<int> queue;
  queue.enqueue(42);
  int value = queue.dequeue();
  EXPECT_EQ(value, 42);
}

// Test 2: Single producer enqueues a sequence, single consumer dequeues, check
// FIFO order
TEST_F(UnboundedQueueTest, SingleProducerSingleConsumerFIFO) {
  const size_t kNumElements = 100;
  UnboundedQueue<int> queue;
  std::vector<int> dequeued;
  dequeued.reserve(kNumElements);

  // Producer thread: Enqueue 0 to 99
  std::thread producer([&queue]() {
    for (size_t i = 0; i < kNumElements; i++) {
      queue.enqueue(i);
    }
  });

  // Consumer thread: Dequeue 100 elements
  std::thread consumer([&queue, &dequeued]() {
    for (size_t i = 0; i < kNumElements; i++) {
      try {
        int value = queue.dequeue();
        dequeued.push_back(value);
      } catch (const EmptyException&) {
        // Consumers consume items too fast, so the queue is empty. Try to
        // restart the operation.
        i--;
      }
    }
  });

  producer.join();
  consumer.join();

  // Verify size and FIFO order
  ASSERT_EQ(dequeued.size(), kNumElements);
  for (size_t i = 0; i < kNumElements; i++) {
    EXPECT_EQ(dequeued[i], static_cast<int>(i))
        << "FIFO order violated at index " << i;
  }
}

// Test 3: Multiple producers and consumers, verify all elements are processed
TEST_F(UnboundedQueueTest, MultipleProducersMultipleConsumers) {
  const size_t kNumProducers = 4;
  const size_t kNumConsumers = 4;
  const size_t kElementsPerProducer = 25;  // Total 100 elements
  UnboundedQueue<int> queue;

  std::vector<std::thread> producers;
  std::vector<std::thread> consumers;
  std::vector<int> enqueued;
  std::mutex enq_mutex;
  std::vector<int> dequeued;
  std::mutex deq_mutex;

  producers.reserve(kNumProducers);
  consumers.reserve(kNumConsumers);
  enqueued.reserve(kNumProducers * kElementsPerProducer);
  dequeued.reserve(enqueued.size());

  // Launch producers
  for (size_t p = 0; p < kNumProducers; p++) {
    producers.emplace_back([&, p]() {
      for (size_t i = 0; i < kElementsPerProducer; i++) {
        int value = static_cast<int>(p * kElementsPerProducer + i);
        queue.enqueue(value);
        {
          std::lock_guard<std::mutex> lock(enq_mutex);
          enqueued.push_back(value);
        }
      }
    });
  }

  // Launch consumers
  for (size_t c = 0; c < kNumConsumers; c++) {
    consumers.emplace_back([&]() {
      for (size_t i = 0; i < kElementsPerProducer; i++) {
        try {
          int value = queue.dequeue();
          {
            std::lock_guard<std::mutex> lock(deq_mutex);
            dequeued.push_back(value);
          }
        } catch (const EmptyException&) {
          i--;
        }
      }
    });
  }

  // Wait for all threads to finish
  for (auto& prod : producers) {
    prod.join();
  }
  for (auto& cons : consumers) {
    cons.join();
  }

  // Sort and compare enqueued and dequeued elements
  std::sort(enqueued.begin(), enqueued.end());
  std::sort(dequeued.begin(), dequeued.end());

  EXPECT_EQ(enqueued.size(), kNumProducers * kElementsPerProducer);
  EXPECT_EQ(dequeued.size(), kNumConsumers * kElementsPerProducer);
  EXPECT_EQ(enqueued, dequeued)
      << "Enqueued and dequeued elements do not match";
}

// Test 4: Dequeue from an empty queue throws EmptyException
TEST_F(UnboundedQueueTest, DequeueEmptyQueueThrows) {
  UnboundedQueue<int> queue;
  EXPECT_THROW({ queue.dequeue(); }, EmptyException);
}
