#include "util/atomic_stamped_ptr.h"

#include <atomic>
#include <thread>

#include "gtest/gtest.h"

class AtomicStampedPtrTest : public ::testing::Test {
 protected:
  void SetUp() override {
    int* value = new int(0);
    uint64_t stamp = 0;
    atomic_stamped_ptr_ = std::make_unique<AtomicStampedPtr<int>>(value, stamp);
  }

  void TearDown() override {
    // We need to explicitly free the resource managed by AtomicStampedPtr
    // since it's a non-owning reference.
    delete atomic_stamped_ptr_->get_ptr(std::memory_order_relaxed);
  }

  std::unique_ptr<AtomicStampedPtr<int>> atomic_stamped_ptr_;
};

// Basic functionality test
TEST_F(AtomicStampedPtrTest, BasicOperations) {
  auto [initial_ptr, initial_stamp] =
      atomic_stamped_ptr_->get(std::memory_order_acquire);

  int* new_ptr = new int(0);
  EXPECT_TRUE(atomic_stamped_ptr_->compare_and_swap(
      initial_ptr, new_ptr, initial_stamp, initial_stamp + 1,
      std::memory_order_release, std::memory_order_relaxed));

  auto [final_ptr, final_stamp] =
      atomic_stamped_ptr_->get(std::memory_order_acquire);

  EXPECT_EQ(final_ptr, new_ptr);
  EXPECT_EQ(final_stamp, initial_stamp + 1);

  delete initial_ptr;
}

// Test concurrent updates
TEST_F(AtomicStampedPtrTest, ConcurrentUpdates) {
  constexpr uint32_t kNumThreads = 8;
  constexpr uint32_t kNumIterations = 125000;
  std::atomic<uint32_t> successful_updates{0};

  auto worker = [&](int thread_id) {
    for (uint32_t i = 0; i < kNumIterations; i++) {
      auto [current_value, stamp] =
          atomic_stamped_ptr_->get(std::memory_order_acquire);
      int* new_value = new int(thread_id);

      if (atomic_stamped_ptr_->compare_and_swap(
              current_value, new_value, stamp, stamp + 1,
              std::memory_order_release, std::memory_order_relaxed)) {
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

  uint64_t final_stamp = atomic_stamped_ptr_->get_stamp();
  EXPECT_EQ(successful_updates.load(std::memory_order_relaxed), final_stamp);
}

// Test ABA Prevention
TEST_F(AtomicStampedPtrTest, ABAProtection) {
  int *A, *B, *C;
  uint64_t stamp;

  // Get initial pointer A and stamp
  std::tie(A, stamp) = atomic_stamped_ptr_->get(std::memory_order_acquire);

  // Create new pointers for swap
  B = new int(0);
  C = new int(0);

  // Thread 1 reads A and stamp
  int* observed_ptr = A;
  uint64_t observed_stamp = stamp;

  // Thread 2 does A -> B -> A
  EXPECT_TRUE(atomic_stamped_ptr_->compare_and_swap(A, B, stamp, stamp + 1,
                                                    std::memory_order_release,
                                                    std::memory_order_relaxed));
  EXPECT_TRUE(atomic_stamped_ptr_->compare_and_swap(B, A, stamp + 1, stamp + 2,
                                                    std::memory_order_release,
                                                    std::memory_order_relaxed));

  // Thread 1 attempts its original operation (should fail due to stamp
  // mismatch)
  EXPECT_FALSE(atomic_stamped_ptr_->compare_and_swap(
      observed_ptr, C, observed_stamp, observed_stamp + 1,
      std::memory_order_release, std::memory_order_relaxed));

  // Clean up
  delete B;
  delete C;
}
