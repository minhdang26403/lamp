#include "stamped_reference.h"
#include <gtest/gtest.h>
#include <atomic>
#include <thread>

class StampedReferenceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    int* value = new int(0);
    uint64_t stamp = 0;
    ref = std::make_unique<StampedReference<int>>(value, stamp);
  }

  void TearDown() override {
    // We need to explicitly free the resource managed by StampedReference
    // since it's a non-owning reference.
    delete ref->get_reference();
  }

  std::unique_ptr<StampedReference<int>> ref;
};

// Basic functionality test
TEST_F(StampedReferenceTest, BasicOperations) {
  uint64_t initial_stamp;
  int* initial_ptr = ref->get(initial_stamp);

  int* new_ptr = new int(0);
  EXPECT_TRUE(ref->compare_and_set(initial_ptr, new_ptr, initial_stamp,
                                   initial_stamp + 1));

  uint64_t final_stamp;
  int* final_ptr = ref->get(final_stamp);

  EXPECT_EQ(final_ptr, new_ptr);
  EXPECT_EQ(final_stamp, initial_stamp + 1);

  delete initial_ptr;
}

// Test concurrent updates
TEST_F(StampedReferenceTest, ConcurrentUpdates) {
  constexpr uint32_t kNumThreads = 8;
  constexpr uint32_t kNumIterations = 125000;
  std::atomic<uint32_t> successful_updates{0};

  auto worker = [&](int thread_id) {
    for (uint32_t i = 0; i < kNumIterations; i++) {
      uint64_t stamp;
      int* current_value = ref->get(stamp);
      int* new_value = new int(thread_id);

      if (ref->compare_and_set(current_value, new_value, stamp, stamp + 1)) {
        successful_updates.fetch_add(1, std::memory_order_relaxed);
        delete current_value;  // Clean up old value
      } else {
        delete new_value;  // Failed update, clean up new value
      }
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (uint32_t i = 0; i < kNumThreads; i++) {
    threads.emplace_back(worker, i);
  }

  for (auto& thread : threads) {
    thread.join();
  }

  uint64_t final_stamp;
  ref->get(final_stamp);
  EXPECT_EQ(successful_updates.load(std::memory_order_relaxed), final_stamp);
}

// Test ABA Prevention
TEST_F(StampedReferenceTest, ABAProtection) {
  int *A, *B, *C;
  uint64_t stamp;

  // Get initial pointer A and stamp
  A = ref->get(stamp);

  // Create new pointers for swap
  B = new int(0);
  C = new int(0);

  // Thread 1 reads A and stamp
  int* observed_ptr = A;
  uint64_t observed_stamp = stamp;

  // Thread 2 does A -> B -> A
  EXPECT_TRUE(ref->compare_and_set(A, B, stamp, stamp + 1));
  EXPECT_TRUE(ref->compare_and_set(B, A, stamp + 1, stamp + 2));

  // Thread 1 attempts its original operation (should fail due to stamp
  // mismatch)
  EXPECT_FALSE(ref->compare_and_set(observed_ptr, C, observed_stamp,
                                    observed_stamp + 1));

  // Clean up
  delete B;
  delete C;
}
