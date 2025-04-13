#include "stack/elimination_backoff_stack.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <random>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

class EliminationBackoffStackTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a stack with reasonable elimination array capacity
    stack_ = std::make_unique<EliminationBackoffStack<int>>(16);
  }

  void TearDown() override { stack_.reset(); }

  std::unique_ptr<EliminationBackoffStack<int>> stack_;
};

TEST_F(EliminationBackoffStackTest, BasicPushPop) {
  stack_->push(10);
  EXPECT_EQ(stack_->pop(), 10);
}

TEST_F(EliminationBackoffStackTest, PushPopMultipleValues) {
  const int kNumOperations = 100;

  // Push values in reverse order
  for (int i = kNumOperations - 1; i >= 0; i--) {
    stack_->push(i);
  }

  // Verify LIFO order
  for (int i = 0; i < kNumOperations; i++) {
    EXPECT_EQ(stack_->pop(), i);
  }
}

TEST_F(EliminationBackoffStackTest, PopEmptyStack) {
  EXPECT_THROW(stack_->pop(), EmptyException);
}

TEST_F(EliminationBackoffStackTest, PushPopLargeValue) {
  struct LargeValue {
    int data[1000];

    LargeValue() { std::fill_n(data, 1000, 0); }

    explicit LargeValue(int value) { std::fill_n(data, 1000, value); }

    bool operator==(const LargeValue& other) const {
      return data[0] == other.data[0] && data[999] == other.data[999];
    }
  };

  auto large_stack = std::make_unique<EliminationBackoffStack<LargeValue>>(16);

  large_stack->push(LargeValue(42));
  large_stack->push(LargeValue(43));

  EXPECT_EQ(large_stack->pop(), LargeValue(43));
  EXPECT_EQ(large_stack->pop(), LargeValue(42));
}

TEST_F(EliminationBackoffStackTest, ConcurrentPushOnly) {
  constexpr size_t kNumThreads = 4;
  constexpr size_t kPushesPerThread = 1000;
  std::vector<std::thread> threads;
  std::atomic<size_t> total_pushes{0};

  for (size_t t = 0; t < kNumThreads; t++) {
    threads.emplace_back([this, t, &total_pushes]() {
      for (size_t i = 0; i < kPushesPerThread; i++) {
        int value = static_cast<int>((t * kPushesPerThread) + i);
        stack_->push(value);
        total_pushes++;
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(total_pushes, kNumThreads * kPushesPerThread);

  // Verify we can pop all values
  size_t pop_count = 0;
  try {
    while (true) {
      stack_->pop();
      pop_count++;
    }
  } catch (const EmptyException&) {
    // Expected when stack is empty
  }

  EXPECT_EQ(pop_count, kNumThreads * kPushesPerThread);
}

TEST_F(EliminationBackoffStackTest, ConcurrentPopOnly) {
  constexpr size_t kNumElements = 10000;
  constexpr size_t kNumThreads = 4;
  std::vector<std::thread> threads;
  std::atomic<size_t> total_pops{0};

  // Pre-populate the stack
  for (size_t i = 0; i < kNumElements; i++) {
    stack_->push(static_cast<int>(i));
  }

  for (size_t t = 0; t < kNumThreads; t++) {
    threads.emplace_back([this, &total_pops]() {
      try {
        while (true) {
          stack_->pop();
          total_pops++;
        }
      } catch (const EmptyException&) {
        // Expected when stack is empty
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(total_pops, kNumElements);

  // Verify stack is empty
  EXPECT_THROW(stack_->pop(), EmptyException);
}

TEST_F(EliminationBackoffStackTest, ConcurrentPushPop) {
  constexpr size_t kNumThreads = 8;
  constexpr size_t kOperationsPerThread = 10000;
  std::vector<std::thread> threads;
  std::atomic<size_t> push_count{0};
  std::atomic<size_t> pop_count{0};

  for (size_t t = 0; t < kNumThreads; t++) {
    threads.emplace_back([this, t, &push_count, &pop_count]() {
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> op_dist(0, 1);  // 0=push, 1=pop

      for (size_t i = 0; i < kOperationsPerThread; i++) {
        bool do_push = op_dist(gen) == 0;

        if (do_push) {
          int value = static_cast<int>((t * kOperationsPerThread) + i);
          stack_->push(value);
          push_count++;
        } else {
          try {
            stack_->pop();
            pop_count++;
          } catch (const EmptyException&) {
            // Stack was empty, try again with push
            i--;
          }
        }
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  size_t remaining = 0;
  try {
    while (true) {
      stack_->pop();
      remaining++;
    }
  } catch (const EmptyException&) {
    // Expected when stack is empty
  }

  EXPECT_EQ(push_count, pop_count + remaining);
}

TEST_F(EliminationBackoffStackTest, EliminationMechanismStress) {
  // This test creates many threads that do rapid pairs of push-pop operations
  // to encourage elimination to happen frequently
  constexpr size_t kNumThreads = 16;
  constexpr size_t kPairsPerThread = 5000;

  std::vector<std::thread> threads;
  std::atomic<size_t> operations_completed{0};

  for (size_t t = 0; t < kNumThreads; t++) {
    threads.emplace_back([this, &operations_completed]() {
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> val_dist(0, 1000);

      for (size_t i = 0; i < kPairsPerThread; i++) {
        int val = val_dist(gen);

        if (i % 2 == 0) {
          // Even iterations: push
          stack_->push(val);
        } else {
          // Odd iterations: pop
          try {
            stack_->pop();
          } catch (const EmptyException&) {
            // If stack is empty, do a push instead
            stack_->push(val);
          }
        }
        operations_completed++;
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(operations_completed, kNumThreads * kPairsPerThread);
}

TEST_F(EliminationBackoffStackTest, MemoryReclaimTest) {
  // Test that nodes are properly reclaimed by pushing and popping many elements
  constexpr size_t kNumIterations = 5;
  constexpr size_t kElementsPerIteration = 10000;

  for (size_t iter = 0; iter < kNumIterations; iter++) {
    // Push many elements
    for (size_t i = 0; i < kElementsPerIteration; i++) {
      stack_->push(static_cast<int>(i));
    }

    // Pop all elements
    for (size_t i = 0; i < kElementsPerIteration; i++) {
      EXPECT_NO_THROW(stack_->pop());
    }

    // Stack should be empty now
    EXPECT_THROW(stack_->pop(), EmptyException);
  }
}

TEST_F(EliminationBackoffStackTest, MixedWorkloadWithTimeouts) {
  constexpr size_t kNumThreads = 8;
  constexpr size_t kOperationsPerThread = 2000;

  std::vector<std::thread> threads;
  std::atomic<size_t> timeout_count{0};
  std::atomic<size_t> completed_operations{0};

  for (size_t t = 0; t < kNumThreads; t++) {
    threads.emplace_back([this, &timeout_count, &completed_operations]() {
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> op_dist(0, 1);  // 0=push, 1=pop
      std::uniform_int_distribution<> val_dist(0, 1000);

      for (size_t i = 0; i < kOperationsPerThread; i++) {
        bool do_push = op_dist(gen) == 0;
        int value = val_dist(gen);

        try {
          if (do_push) {
            stack_->push(value);
          } else {
            try {
              stack_->pop();
            } catch (const EmptyException&) {
              // If stack is empty, do a push instead
              stack_->push(value);
            }
          }
        } catch (const TimeoutException&) {
          timeout_count++;
          // Operation failed due to timeout, but we still count it
        }

        completed_operations++;
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(completed_operations, kNumThreads * kOperationsPerThread);

  // We don't assert on timeout_count as it's implementation-dependent,
  // but we log it for informational purposes
  std::cout << "Total timeout count: " << timeout_count.load() << std::endl;
}

TEST_F(EliminationBackoffStackTest, DifferentCapacities) {
  // Test with different elimination array capacities
  std::vector<size_t> capacities = {1, 4, 16, 64, 256};

  for (size_t capacity : capacities) {
    auto local_stack = std::make_unique<EliminationBackoffStack<int>>(capacity);

    // Basic functionality test
    for (int i = 0; i < 100; i++) {
      local_stack->push(i);
    }

    for (int i = 0; i < 100; i++) {
      EXPECT_EQ(local_stack->pop(), 99 - i);
    }

    EXPECT_THROW(local_stack->pop(), EmptyException);
  }
}

// Performance comparison test between different stack sizes
TEST(EliminationBackoffStackPerformanceTest, CapacityComparison) {
  constexpr size_t kNumThreads = 8;
  constexpr size_t kOperationsPerThread = 5000;
  std::vector<size_t> capacities = {1, 16, 64, 256};

  for (size_t capacity : capacities) {
    auto stack = std::make_unique<EliminationBackoffStack<int>>(capacity);
    std::vector<std::thread> threads;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (size_t t = 0; t < kNumThreads; t++) {
      threads.emplace_back([&stack]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> op_dist(0, 1);  // 0=push, 1=pop
        std::uniform_int_distribution<> val_dist(0, 1000);

        for (size_t i = 0; i < kOperationsPerThread; i++) {
          try {
            if (op_dist(gen) == 0) {
              stack->push(val_dist(gen));
            } else {
              try {
                stack->pop();
              } catch (const EmptyException&) {
                // If stack is empty, do a push instead
                stack->push(val_dist(gen));
              }
            }
          } catch (const TimeoutException&) {
            // Ignore timeouts
          }
        }
      });
    }

    for (auto& thread : threads) {
      thread.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    std::cout << "Capacity " << capacity << " took " << duration.count()
              << " ms for " << kNumThreads * kOperationsPerThread
              << " operations" << std::endl;
  }
}
