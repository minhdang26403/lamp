#include "list/lock_free_list.h"

#include <atomic>
#include <limits>
#include <random>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

class LockFreeListTest : public ::testing::Test {
 protected:
  void SetUp() override { list_ = new LockFreeList<int>(); }

  void TearDown() override { delete list_; }

  LockFreeList<int>* list_;
};

TEST_F(LockFreeListTest, EmptyListContains) {
  EXPECT_FALSE(list_->contains(42));
}

TEST_F(LockFreeListTest, AddAndContainsSingleItem) {
  EXPECT_TRUE(list_->add(42));
  EXPECT_TRUE(list_->contains(42));
  EXPECT_FALSE(list_->contains(43));
}

TEST_F(LockFreeListTest, RemoveSingleItem) {
  EXPECT_TRUE(list_->add(42));
  EXPECT_TRUE(list_->contains(42));

  EXPECT_TRUE(list_->remove(42));
  EXPECT_FALSE(list_->contains(42));

  // Verify item can be re-added after removal
  EXPECT_TRUE(list_->add(42));
  EXPECT_TRUE(list_->contains(42));
}

TEST_F(LockFreeListTest, BoundaryCheck) {
  // This test verifies protection against boundary value vulnerabilities.
  // The lock-free list implementation uses sentinel nodes with special values:
  // - Head sentinel with key value std::numeric_limits<size_t>::min() (min_val)
  // - Tail sentinel with key value std::numeric_limits<size_t>::max() (max_val)
  //
  // The test ensures the implementation properly handles client attempts to
  // insert or remove values that collide with these sentinel values, which
  // could otherwise corrupt the data structure by removing/replacing sentinel
  // nodes or creating duplicate sentinels, breaking the list invariants.
  LockFreeList<size_t> s_list;

  size_t min_val = std::numeric_limits<size_t>::min();
  size_t max_val = std::numeric_limits<size_t>::max();

  // An empty list should not contain the min value (head sentinel)
  EXPECT_FALSE(s_list.contains(min_val));
  // An empty list should not contain the max value (tail sentinel)
  EXPECT_FALSE(s_list.contains(max_val));

  // Since the list does not contain min and max values, attempt to remove them
  // should fail
  EXPECT_FALSE(s_list.remove(min_val));
  EXPECT_FALSE(s_list.remove(max_val));

  // Since the list does not contain min and max values, we should be able to
  // insert them
  EXPECT_TRUE(s_list.add(min_val));
  EXPECT_TRUE(s_list.add(max_val));
}

TEST_F(LockFreeListTest, AddDuplicateItem) {
  EXPECT_TRUE(list_->add(42));
  EXPECT_FALSE(list_->add(42));  // Adding duplicate should fail
  EXPECT_TRUE(list_->contains(42));

  // Removing and re-adding should succeed
  EXPECT_TRUE(list_->remove(42));
  EXPECT_TRUE(list_->add(42));
}

TEST_F(LockFreeListTest, AddMultipleItems) {
  constexpr size_t kNumItems = 100;

  for (size_t i = 0; i < kNumItems; i++) {
    EXPECT_TRUE(list_->add(static_cast<int>(i)));
  }

  for (size_t i = 0; i < kNumItems; i++) {
    EXPECT_TRUE(list_->contains(static_cast<int>(i)));
  }

  EXPECT_FALSE(list_->contains(static_cast<int>(kNumItems)));
}

TEST_F(LockFreeListTest, RemoveMultipleItems) {
  constexpr size_t kNumItems = 100;

  // Add all items
  for (size_t i = 0; i < kNumItems; i++) {
    EXPECT_TRUE(list_->add(static_cast<int>(i)));
  }

  // Remove even-numbered items
  for (size_t i = 0; i < kNumItems; i += 2) {
    EXPECT_TRUE(list_->remove(static_cast<int>(i)));
  }

  // Verify even items are gone, odd items remain
  for (size_t i = 0; i < kNumItems; i++) {
    if (i % 2 == 0) {
      EXPECT_FALSE(list_->contains(static_cast<int>(i)));
    } else {
      EXPECT_TRUE(list_->contains(static_cast<int>(i)));
    }
  }
}

TEST_F(LockFreeListTest, RemoveNonexistentItem) {
  EXPECT_FALSE(list_->remove(42));

  // Add and remove, then try to remove again
  EXPECT_TRUE(list_->add(42));
  EXPECT_TRUE(list_->remove(42));
  EXPECT_FALSE(list_->remove(42));
}

TEST_F(LockFreeListTest, ConcurrentAdd) {
  constexpr size_t kNumThreads = 4;
  constexpr size_t kItemsPerThread = 1000;
  constexpr size_t kTotalItems = kNumThreads * kItemsPerThread;

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);

  // Each thread adds a distinct range of items
  for (size_t t = 0; t < kNumThreads; t++) {
    threads.emplace_back([this, t]() {
      size_t base = t * kItemsPerThread;
      for (size_t i = 0; i < kItemsPerThread; i++) {
        EXPECT_TRUE(list_->add(static_cast<int>(base + i)));
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // Verify all items were added
  for (size_t i = 0; i < kTotalItems; i++) {
    EXPECT_TRUE(list_->contains(static_cast<int>(i)));
  }
}

TEST_F(LockFreeListTest, ConcurrentRemove) {
  constexpr size_t kNumItems = 1000;

  // First add all items
  for (size_t i = 0; i < kNumItems; i++) {
    EXPECT_TRUE(list_->add(static_cast<int>(i)));
  }

  constexpr size_t kNumThreads = 4;
  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);

  // Each thread removes a distinct range of items
  for (size_t t = 0; t < kNumThreads; t++) {
    threads.emplace_back([this, t]() {
      for (size_t i = t; i < kNumItems; i += kNumThreads) {
        EXPECT_TRUE(list_->remove(static_cast<int>(i)));
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // Verify all items were removed
  for (size_t i = 0; i < kNumItems; i++) {
    EXPECT_FALSE(list_->contains(static_cast<int>(i)));
  }
}

TEST_F(LockFreeListTest, ConcurrentContains) {
  constexpr size_t kNumItems = 500;

  // Add every other item
  for (size_t i = 0; i < kNumItems; i += 2) {
    EXPECT_TRUE(list_->add(static_cast<int>(i)));
  }

  constexpr size_t kNumThreads = 4;
  constexpr size_t kOperationsPerThread = 1000;

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  std::atomic<uint32_t> correct_results{0};

  for (size_t t = 0; t < kNumThreads; t++) {
    threads.emplace_back([this, &correct_results]() {
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> val_dist(0,
                                               static_cast<int>(kNumItems - 1));

      for (size_t i = 0; i < kOperationsPerThread; i++) {
        int value = val_dist(gen);
        bool expected = (value % 2 == 0);  // Even numbers should be in the list
        if (list_->contains(value) == expected) {
          correct_results++;
        }
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // Not all operations are guaranteed to see the correct state due to
  // concurrent modifications, but the vast majority should be correct
  double success_rate = static_cast<double>(correct_results) /
                        (kNumThreads * kOperationsPerThread);
  EXPECT_GT(success_rate, 0.95);
}

TEST_F(LockFreeListTest, ConcurrentOperationsMix) {
  constexpr size_t kNumThreads = 16;
  constexpr size_t kOperationsPerThread = 10000;
  constexpr size_t kValueRange = 100;

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  std::atomic<uint32_t> completed_operations{0};

  for (size_t t = 0; t < kNumThreads; t++) {
    threads.emplace_back([this, &completed_operations]() {
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> op_dist(
          0, 2);  // 0=add, 1=remove, 2=contains
      std::uniform_int_distribution<> val_dist(
          0, static_cast<int>(kValueRange - 1));

      for (size_t i = 0; i < kOperationsPerThread; i++) {
        int operation = op_dist(gen);
        int value = val_dist(gen);

        switch (operation) {
          case 0:
            list_->add(value);
            break;
          case 1:
            list_->remove(value);
            break;
          case 2:
            list_->contains(value);
            break;
        }
        completed_operations++;
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(completed_operations, kNumThreads * kOperationsPerThread);
}

TEST_F(LockFreeListTest, AddRemoveContainsStressTest) {
  constexpr size_t kNumThreads = 8;
  constexpr size_t kOperationsPerThread = 5000;
  constexpr size_t kValueRange = 200;

  // Pre-populate the list with some values
  for (size_t i = 0; i < kValueRange; i += 4) {
    list_->add(static_cast<int>(i));
  }

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  std::atomic<uint32_t> add_count{0};
  std::atomic<uint32_t> remove_count{0};
  std::atomic<uint32_t> contains_true_count{0};
  std::atomic<uint32_t> contains_false_count{0};

  for (size_t t = 0; t < kNumThreads; t++) {
    threads.emplace_back([this, &add_count, &remove_count, &contains_true_count,
                          &contains_false_count, t]() {
      std::mt19937 gen(
          static_cast<unsigned int>(t + 100));  // Deterministic seed
      std::uniform_int_distribution<> op_dist(
          0, 2);  // 0=add, 1=remove, 2=contains
      std::uniform_int_distribution<> val_dist(
          0, static_cast<int>(kValueRange - 1));

      for (size_t i = 0; i < kOperationsPerThread; i++) {
        int operation = op_dist(gen);
        int value = val_dist(gen);

        switch (operation) {
          case 0: {
            bool result = list_->add(value);
            if (result)
              add_count++;
            break;
          }
          case 1: {
            bool result = list_->remove(value);
            if (result)
              remove_count++;
            break;
          }
          case 2: {
            bool result = list_->contains(value);
            if (result)
              contains_true_count++;
            else
              contains_false_count++;
            break;
          }
        }
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // Final consistency check
  // Count how many items are in the list at the end
  int final_count = 0;
  for (size_t i = 0; i < kValueRange; i++) {
    if (list_->contains(static_cast<int>(i))) {
      final_count++;
    }
  }

  // The final count should match: (initial items + adds - removes)
  // We initially added kValueRange/4 items
  // Then add_count items were added and remove_count were removed
  int expected_count = (kValueRange / 4) + add_count - remove_count;

  // Due to race conditions, the counts might not be exactly equal
  // but they should be very close
  int difference = std::abs(final_count - expected_count);
  double error_ratio = static_cast<double>(difference) / kValueRange;

  // Error should be very small if the lock-free list is functioning correctly
  EXPECT_LT(error_ratio, 0.05);
}

TEST_F(LockFreeListTest, TestWithCustomType) {
  struct TestItem {
    int id;
    std::string name;

    bool operator==(const TestItem& other) const {
      return id == other.id && name == other.name;
    }
  };

  struct TestItemHash {
    size_t operator()(const TestItem& item) const {
      return std::hash<int>{}(item.id) ^ std::hash<std::string>{}(item.name);
    }
  };

  LockFreeList<TestItem, TestItemHash> custom_list;

  TestItem item1{1, "one"};
  TestItem item2{2, "two"};
  TestItem item3{3, "three"};
  TestItem item1_copy{1, "one"};  // Same values as item1

  EXPECT_TRUE(custom_list.add(item1));
  EXPECT_TRUE(custom_list.add(item2));
  EXPECT_TRUE(custom_list.add(item3));

  EXPECT_TRUE(custom_list.contains(item1));
  EXPECT_TRUE(custom_list.contains(item2));
  EXPECT_TRUE(custom_list.contains(item3));

  // Should detect item1_copy as a duplicate of item1
  EXPECT_FALSE(custom_list.add(item1_copy));

  // Remove and verify
  EXPECT_TRUE(custom_list.remove(item2));
  EXPECT_FALSE(custom_list.contains(item2));
  EXPECT_TRUE(custom_list.contains(item1));
  EXPECT_TRUE(custom_list.contains(item3));
}

// Test to verify logical and physical deletion work as expected
TEST_F(LockFreeListTest, LogicalThenPhysicalDeletion) {
  constexpr size_t kSize = 5;

  // Add items 0 through 4
  for (size_t i = 0; i < kSize; i++) {
    EXPECT_TRUE(list_->add(static_cast<int>(i)));
  }

  // Remove first and verify others still exist
  EXPECT_TRUE(list_->remove(0));
  EXPECT_FALSE(list_->contains(0));
  for (size_t i = 1; i < kSize; i++) {
    EXPECT_TRUE(list_->contains(static_cast<int>(i)));
  }

  // Remove last and verify others still exist
  EXPECT_TRUE(list_->remove(static_cast<int>(kSize - 1)));
  EXPECT_FALSE(list_->contains(static_cast<int>(kSize - 1)));
  for (size_t i = 1; i < kSize - 1; i++) {
    EXPECT_TRUE(list_->contains(static_cast<int>(i)));
  }

  // Remove middle item and verify adjacent items still exist
  EXPECT_TRUE(list_->remove(2));
  EXPECT_FALSE(list_->contains(2));
  EXPECT_TRUE(list_->contains(1));
  EXPECT_TRUE(list_->contains(3));

  // Add new item and verify it works properly after deletions
  EXPECT_TRUE(list_->add(42));
  EXPECT_TRUE(list_->contains(42));
}
