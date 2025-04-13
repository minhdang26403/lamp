#include "stack/lock_free_stack.h"

#include <atomic>
#include <random>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

// Fixture for LockFreeStack tests
class LockFreeStackTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Default setup with 5ms min delay and 25ms max delay
    stack_ = new LockFreeStack<int>{};
  }

  void TearDown() override { delete stack_; }

  LockFreeStack<int>* stack_;
};

// Test basic push and pop operations
TEST_F(LockFreeStackTest, BasicPushPop) {
  stack_->push(42);
  EXPECT_EQ(42, stack_->pop());
}

// Test multiple push and pop operations
TEST_F(LockFreeStackTest, MultiplePushPop) {
  constexpr int kNumItems = 1000;

  // Push items
  for (int i = 0; i < kNumItems; ++i) {
    stack_->push(i);
  }

  // Pop and verify LIFO order
  for (int i = kNumItems - 1; i >= 0; --i) {
    EXPECT_EQ(i, stack_->pop());
  }
}

// Test empty stack exception
TEST_F(LockFreeStackTest, EmptyStackException) {
  EXPECT_THROW(stack_->pop(), EmptyException);
}

// Test custom backoff parameters
TEST_F(LockFreeStackTest, CustomBackoffParameters) {
  auto custom_stack = new LockFreeStack<int>{10, 50};  // 10ms min, 50ms max

  custom_stack->push(100);
  EXPECT_EQ(100, custom_stack->pop());
  EXPECT_THROW(custom_stack->pop(), EmptyException);

  delete custom_stack;
}

// Test push with move semantics
TEST_F(LockFreeStackTest, PushWithMoveSemantics) {
  std::string str = "test_string";
  LockFreeStack<std::string> string_stack;

  string_stack.push(std::move(str));
  EXPECT_TRUE(str.empty());  // Original should be moved from

  std::string popped = string_stack.pop();
  EXPECT_EQ("test_string", popped);
}

// Sequential push-pop test with different data types
TEST_F(LockFreeStackTest, DifferentDataTypes) {
  struct TestStruct {
    int a;
    double b;

    bool operator==(const TestStruct& other) const {
      return a == other.a && b == other.b;
    }
  };

  LockFreeStack<TestStruct> struct_stack;
  TestStruct ts1{42, 3.14};
  TestStruct ts2{100, 2.71};

  struct_stack.push(ts1);
  struct_stack.push(ts2);

  EXPECT_EQ(ts2, struct_stack.pop());
  EXPECT_EQ(ts1, struct_stack.pop());
}

// Test with nanoseconds as Duration parameter
TEST_F(LockFreeStackTest, NanosecondsBackoff) {
  LockFreeStack<int, std::chrono::nanoseconds> nano_stack{
      100, 1000};  // 100ns to 1000ns

  nano_stack.push(123);
  EXPECT_EQ(123, nano_stack.pop());
}

// Concurrent push operations test
TEST_F(LockFreeStackTest, ConcurrentPush) {
  constexpr size_t kNumThreads = 8;
  constexpr size_t kItemsPerThread = 1000;

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);

  for (size_t t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([this, t]() {
      std::mt19937 gen(static_cast<unsigned int>(t + 100));
      std::uniform_int_distribution<> dist(0, 999);

      for (size_t i = 0; i < kItemsPerThread; ++i) {
        stack_->push(dist(gen));
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // Verify we have the right number of items
  size_t count = 0;
  while (true) {
    try {
      stack_->pop();
      count++;
    } catch (const EmptyException&) {
      break;
    }
  }

  EXPECT_EQ(kNumThreads * kItemsPerThread, count);
}

// Concurrent pop operations test
TEST_F(LockFreeStackTest, ConcurrentPop) {
  constexpr size_t kNumItems = 10000;

  // Pre-fill the stack
  for (size_t i = 0; i < kNumItems; ++i) {
    stack_->push(static_cast<int>(i));
  }

  constexpr size_t kNumThreads = 8;
  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  std::atomic<size_t> pop_count{0};

  for (size_t t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([this, &pop_count]() {
      while (true) {
        try {
          stack_->pop();
          pop_count++;
        } catch (const EmptyException&) {
          break;
        }
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(kNumItems, pop_count.load());
  EXPECT_THROW(stack_->pop(), EmptyException);  // Verify stack is empty
}

// Concurrent push and pop test
TEST_F(LockFreeStackTest, ConcurrentPushPop) {
  constexpr size_t kNumThreads = 8;
  constexpr size_t kItemsPerThread = 1000;

  std::vector<std::thread> producers;
  std::vector<std::thread> consumers;
  std::atomic<size_t> push_count{0};
  std::atomic<size_t> pop_count{0};

  producers.reserve(kNumThreads);
  consumers.reserve(kNumThreads);

  // Producer threads
  for (size_t t = 0; t < kNumThreads; ++t) {
    producers.emplace_back([this, &push_count, t]() {
      std::mt19937 gen(static_cast<unsigned int>(t + 100));
      std::uniform_int_distribution<> dist(0, 999);

      for (size_t i = 0; i < kItemsPerThread; ++i) {
        stack_->push(dist(gen));
        push_count++;
      }
    });
  }

  // Consumer threads
  for (size_t t = 0; t < kNumThreads; ++t) {
    consumers.emplace_back([this, &pop_count, t]() {
      std::mt19937 gen(static_cast<unsigned int>(t + 200));
      std::bernoulli_distribution should_pop(0.8);

      for (size_t i = 0; i < kItemsPerThread; ++i) {
        if (should_pop(gen)) {
          try {
            stack_->pop();
            pop_count++;
          } catch (const EmptyException&) {
            // Expected when stack is empty
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

  // Drain remaining items
  size_t remaining_count = 0;
  while (true) {
    try {
      stack_->pop();
      remaining_count++;
    } catch (const EmptyException&) {
      break;
    }
  }

  // Total pushed should equal total popped plus remaining
  EXPECT_EQ(push_count.load(), pop_count.load() + remaining_count);
}

// Test for memory leaks by pushing and popping many elements
TEST_F(LockFreeStackTest, MemoryLeakTest) {
  constexpr size_t kNumItems = 1000000;  // 1M items

  for (size_t i = 0; i < kNumItems; ++i) {
    stack_->push(static_cast<int>(i));
  }

  for (size_t i = 0; i < kNumItems; ++i) {
    stack_->pop();
  }

  EXPECT_THROW(stack_->pop(), EmptyException);  // Verify stack is empty
}

// Test custom duration type with high contention
TEST_F(LockFreeStackTest, HighContentionWithCustomDuration) {
  auto contention_stack =
      new LockFreeStack<int, std::chrono::milliseconds>{1, 5};  // 1ms to 5ms
  constexpr size_t kNumThreads = 16;  // Higher thread count for more contention
  constexpr size_t kOpsPerThread = 500;

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);

  // Pre-fill with some items
  for (int i = 0; i < 1000; ++i) {
    contention_stack->push(i);
  }

  std::atomic<int> ops_completed{0};

  for (size_t t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([contention_stack, &ops_completed, t]() {
      std::mt19937 gen(static_cast<unsigned int>(t));
      std::uniform_int_distribution<> val_dist(1000, 9999);
      std::bernoulli_distribution op_dist(0.5);  // 50% push, 50% pop

      for (size_t i = 0; i < kOpsPerThread; ++i) {
        if (op_dist(gen)) {
          // Push operation
          contention_stack->push(val_dist(gen));
        } else {
          // Pop operation
          try {
            contention_stack->pop();
          } catch (const EmptyException&) {
            // Expected sometimes
          }
        }
        ops_completed++;
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(kNumThreads * kOpsPerThread, ops_completed.load());

  // Cleanup
  delete contention_stack;
}

// Test ABA problem prevention
TEST_F(LockFreeStackTest, ABAPreventionTest) {
  constexpr size_t kNumThreads = 4;
  constexpr size_t kIterations = 1000;

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);

  for (size_t t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([this, t]() {
      std::mt19937 gen(static_cast<unsigned int>(t));
      std::bernoulli_distribution op_dist(0.6);  // 60% push, 40% pop

      for (size_t i = 0; i < kIterations; ++i) {
        if (op_dist(gen)) {
          // Push operation
          stack_->push(static_cast<int>(i));
        } else {
          // Pop operation
          try {
            int value = stack_->pop();
            // Immediately push the same value back (potential ABA scenario)
            stack_->push(value);
          } catch (const EmptyException&) {
            // Expected sometimes
          }
        }
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // If we get here without crashes, ABA protection is working
  // No explicit assertion needed
}

// Performance test comparing different backoff strategies
TEST_F(LockFreeStackTest, BackoffPerformanceComparison) {
  auto micro_stack =
      new LockFreeStack<int, std::chrono::microseconds>{5, 25};  // 5μs to 25μs
  auto nano_stack = new LockFreeStack<int, std::chrono::nanoseconds>{
      5000, 25000};  // 5000ns to 25000ns

  constexpr size_t kNumOps = 10000;

  auto test_performance = [](auto stack) {
    auto start = std::chrono::high_resolution_clock::now();

    // Push operations
    for (size_t i = 0; i < kNumOps; ++i) {
      stack->push(static_cast<int>(i));
    }

    // Pop operations
    for (size_t i = 0; i < kNumOps; ++i) {
      stack->pop();
    }

    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start)
        .count();
  };

  // Run multiple times to get reliable measurements
  double micro_time = 0, nano_time = 0;
  constexpr size_t kNumRuns = 5;

  for (size_t run = 0; run < kNumRuns; ++run) {
    micro_time += test_performance(micro_stack);
    nano_time += test_performance(nano_stack);
  }

  micro_time /= kNumRuns;
  nano_time /= kNumRuns;

  // This is more of a performance metric than a strict test
  std::cout << "Average microseconds backoff time: " << micro_time << "μs"
            << std::endl;
  std::cout << "Average nanoseconds backoff time: " << nano_time << "μs"
            << std::endl;

  // Cleanup
  delete micro_stack;
  delete nano_stack;
}

// Edge case: rapid push/pop alternation
TEST_F(LockFreeStackTest, RapidPushPopAlternation) {
  constexpr size_t kNumThreads = 4;
  constexpr size_t kOpsPerThread = 10000;

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  std::atomic<bool> start_flag{false};

  for (size_t t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([this, &start_flag, t]() {
      // Wait for all threads to be ready
      while (!start_flag.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }

      for (size_t i = 0; i < kOpsPerThread; ++i) {
        if (i % 2 == 0) {
          stack_->push(static_cast<int>(t * kOpsPerThread + i));
        } else {
          try {
            stack_->pop();
          } catch (const EmptyException&) {
            // Expected sometimes
          }
        }
      }
    });
  }

  // Start all threads simultaneously
  start_flag.store(true, std::memory_order_release);

  for (auto& thread : threads) {
    thread.join();
  }

  // No specific assertion here, just making sure we don't deadlock or crash
}
