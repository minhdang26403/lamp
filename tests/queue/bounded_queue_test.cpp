#include "queue/bounded_queue.h"

#include <algorithm>
#include <mutex>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

using namespace std::chrono_literals;

// Test 1: Basic enqueue and dequeue with a single element
TEST(BoundedQueueTest, SingleEnqueueDequeue) {
  BoundedQueue<int> queue(1);
  queue.enqueue(42);
  int value = queue.dequeue();
  EXPECT_EQ(value, 42);
}

// Test 2: Single producer enqueues a sequence, single consumer dequeues, check
// FIFO order
TEST(BoundedQueueTest, SingleProducerSingleConsumerFIFO) {
  const size_t kCapacity = 10;
  const size_t kNumElements = 100;  // More than capacity to test blocking
  BoundedQueue<int> queue(kCapacity);
  std::vector<int> dequeued;
  dequeued.reserve(kNumElements);

  // Producer thread: Enqueue 0 to 99
  std::thread producer([&queue]() {
    for (size_t i = 0; i < kNumElements; ++i) {
      queue.enqueue(i);
    }
  });

  // Consumer thread: Dequeue 100 elements
  std::thread consumer([&queue, &dequeued]() {
    for (size_t i = 0; i < kNumElements; ++i) {
      int value = queue.dequeue();
      dequeued.push_back(value);
    }
  });

  producer.join();
  consumer.join();

  // Verify size and FIFO order
  ASSERT_EQ(dequeued.size(), kNumElements);
  for (size_t i = 0; i < kNumElements; ++i) {
    EXPECT_EQ(dequeued[i], static_cast<int>(i))
        << "FIFO order violated at index " << i;
  }
}

// Test 3: Multiple producers and consumers, verify all elements are processed
TEST(BoundedQueueTest, MultipleProducersMultipleConsumers) {
  const size_t kCapacity = 10;
  const size_t kNumProducers = 4;
  const size_t kNumConsumers = 4;
  const size_t kElementsPerProducer = 25;  // Total 100 elements
  BoundedQueue<int> queue(kCapacity);

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
  for (size_t p = 0; p < kNumProducers; ++p) {
    producers.emplace_back([&, p]() {
      for (size_t i = 0; i < kElementsPerProducer; ++i) {
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
  for (size_t c = 0; c < kNumConsumers; ++c) {
    consumers.emplace_back([&]() {
      for (size_t i = 0; i < kElementsPerProducer; ++i) {
        int value = queue.dequeue();
        {
          std::lock_guard<std::mutex> lock(deq_mutex);
          dequeued.push_back(value);
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

// Test 4: Enqueue blocks when queue is full until dequeue occurs
TEST(BoundedQueueTest, EnqueueBlocksWhenFull) {
  BoundedQueue<int> queue(1);  // Capacity of 1
  queue.enqueue(1);            // Fill the queue

  bool has_enqueued = false;
  std::thread producer([&queue, &has_enqueued]() {
    queue.enqueue(2);  // Should block until dequeue
    has_enqueued = true;
  });

  // Give producer time to attempt enqueue and block
  std::this_thread::sleep_for(100us);
  EXPECT_FALSE(has_enqueued) << "Producer enqueued before queue was emptied";

  // Dequeue to unblock producer
  int value1 = queue.dequeue();
  EXPECT_EQ(value1, 1);

  // Wait for producer to finish
  producer.join();
  EXPECT_TRUE(has_enqueued) << "Producer did not enqueue after dequeue";

  // Dequeue the second element
  int value2 = queue.dequeue();
  EXPECT_EQ(value2, 2);
}

// Test 5: Dequeue blocks when queue is empty until enqueue occurs
TEST(BoundedQueueTest, DequeueBlocksWhenEmpty) {
  BoundedQueue<int> queue(1);  // Capacity of 1, starts empty
  bool has_dequeued = false;
  int dequeued_value = -1;

  // Consumer thread: Attempt to dequeue from an empty queue
  std::thread consumer([&queue, &has_dequeued, &dequeued_value]() {
    dequeued_value = queue.dequeue();  // Should block until enqueue
    has_dequeued = true;
  });

  // Give consumer time to attempt dequeue and block
  std::this_thread::sleep_for(100us);
  EXPECT_FALSE(has_dequeued)
      << "Consumer dequeued from empty queue prematurely";

  // Enqueue an element to unblock consumer
  queue.enqueue(42);

  // Wait for consumer to finish
  consumer.join();
  EXPECT_TRUE(has_dequeued) << "Consumer did not dequeue after enqueue";
  EXPECT_EQ(dequeued_value, 42) << "Incorrect value dequeued";
}
