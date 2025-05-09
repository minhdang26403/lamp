#include "list/coarse_list.h"

#include <random>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

class CoarseListTest : public ::testing::Test {
 protected:
  void SetUp() override { list_ = new CoarseList<int>(); }

  void TearDown() override { delete list_; }

  CoarseList<int>* list_;
};

TEST_F(CoarseListTest, EmptyListContainsReturnsFalse) {
  EXPECT_FALSE(list_->contains(1));
}

TEST_F(CoarseListTest, AddSuccess) {
  EXPECT_TRUE(list_->add(1));
  EXPECT_TRUE(list_->contains(1));
}

TEST_F(CoarseListTest, AddDuplicate) {
  EXPECT_TRUE(list_->add(1));
  EXPECT_FALSE(list_->add(1));
}

TEST_F(CoarseListTest, RemoveSuccess) {
  EXPECT_TRUE(list_->add(1));
  EXPECT_TRUE(list_->remove(1));
  EXPECT_FALSE(list_->contains(1));
}

TEST_F(CoarseListTest, RemoveNonExistent) {
  EXPECT_FALSE(list_->remove(1));
}

TEST_F(CoarseListTest, BoundaryCheck) {
  // This test verifies protection against boundary value vulnerabilities.
  // The lock-free list implementation uses sentinel nodes with special values:
  // - Head sentinel with key value std::numeric_limits<size_t>::min() (min_val)
  // - Tail sentinel with key value std::numeric_limits<size_t>::max() (max_val)
  //
  // The test ensures the implementation properly handles client attempts to
  // insert or remove values that collide with these sentinel values, which
  // could otherwise corrupt the data structure by removing/replacing sentinel
  // nodes or creating duplicate sentinels, breaking the list invariants.
  CoarseList<size_t> s_list;

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

TEST_F(CoarseListTest, AddMultipleItems) {
  for (size_t i = 1; i <= 5; i++) {
    EXPECT_TRUE(list_->add(i));
  }

  for (size_t i = 1; i <= 5; i++) {
    EXPECT_TRUE(list_->contains(i));
  }
}

TEST_F(CoarseListTest, RemoveMiddleItem) {
  for (size_t i = 1; i <= 5; i++) {
    EXPECT_TRUE(list_->add(i));
  }

  EXPECT_TRUE(list_->remove(3));
  EXPECT_FALSE(list_->contains(3));

  // Check other items still exist
  EXPECT_TRUE(list_->contains(1));
  EXPECT_TRUE(list_->contains(2));
  EXPECT_TRUE(list_->contains(4));
  EXPECT_TRUE(list_->contains(5));
}

// Test with custom type and hash function
struct TestItem {
  int id;
  std::string name;

  bool operator==(const TestItem& other) const {
    return id == other.id && name == other.name;
  }
};

struct TestItemHasher {
  size_t operator()(const TestItem& item) const {
    return std::hash<int>()(item.id);
  }
};

TEST(CoarseListCustomTypeTest, BasicOperations) {
  CoarseList<TestItem, TestItemHasher> list;

  TestItem item1{1, "Item1"};
  TestItem item2{2, "Item2"};

  EXPECT_TRUE(list.add(item1));
  EXPECT_TRUE(list.add(item2));
  EXPECT_TRUE(list.contains(item1));
  EXPECT_TRUE(list.contains(item2));

  EXPECT_TRUE(list.remove(item1));
  EXPECT_FALSE(list.contains(item1));
  EXPECT_TRUE(list.contains(item2));
}

// Concurrency tests
TEST_F(CoarseListTest, ConcurrentAddDifferentItems) {
  constexpr size_t kNumThreads = 4;
  constexpr size_t kItemsPerThread = 250;

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);

  for (size_t t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([this, t]() {
      for (size_t i = 0; i < kItemsPerThread; i++) {
        int value = static_cast<int>(t * kItemsPerThread + i);
        list_->add(value);
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // Verify all items were added
  size_t expected_count = kNumThreads * kItemsPerThread;
  size_t actual_count = 0;

  for (size_t t = 0; t < kNumThreads; ++t) {
    for (size_t i = 0; i < kItemsPerThread; ++i) {
      int value = static_cast<int>(t * kItemsPerThread + i);
      if (list_->contains(value)) {
        actual_count++;
      }
    }
  }

  EXPECT_EQ(expected_count, actual_count);
}

TEST_F(CoarseListTest, ConcurrentAddRemove) {
  constexpr size_t kNumItems = 100;
  constexpr size_t kNumThreads = 4;
  constexpr size_t kOperationsPerThread = 1000;

  // Track operations to verify consistency
  std::atomic<size_t> successful_adds{0};
  std::atomic<size_t> successful_removes{0};

  // First add some initial items
  for (size_t i = 0; i < kNumItems / 2; i++) {
    EXPECT_TRUE(list_->add(static_cast<int>(i)));
  }

  // Run multiple threads doing both adds and removes
  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);

  for (size_t t = 0; t < kNumThreads; t++) {
    threads.emplace_back([this, &successful_adds, &successful_removes]() {
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> val_dist(0,
                                               static_cast<int>(kNumItems - 1));
      std::uniform_int_distribution<> op_dist(0, 1);  // 0=add, 1=remove

      for (size_t i = 0; i < kOperationsPerThread; i++) {
        int value = val_dist(gen);
        bool is_add = op_dist(gen) == 0;

        if (is_add) {
          if (list_->add(value)) {
            successful_adds++;
          }
        } else {
          if (list_->remove(value)) {
            successful_removes++;
          }
        }
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // Count items in list at the end
  size_t items_in_list = 0;
  for (size_t i = 0; i < kNumItems; i++) {
    if (list_->contains(static_cast<int>(i))) {
      items_in_list++;
    }
  }

  // The number of items should equal: initial items + successful adds -
  // successful removes
  EXPECT_EQ(items_in_list,
            (kNumItems / 2) + successful_adds - successful_removes);
}

TEST_F(CoarseListTest, StressTest) {
  constexpr size_t kNumThreads = 8;
  constexpr size_t kOperationsPerThread = 1000;
  constexpr int kMaxValue = static_cast<int>(kOperationsPerThread * 2);

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  std::atomic<uint32_t> completed_operations{0};

  for (size_t t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([this, &completed_operations]() {
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> dist_op(
          0, 2);  // 0=add, 1=remove, 2=contains
      std::uniform_int_distribution<> dist_val(0, kMaxValue);

      for (size_t i = 0; i < kOperationsPerThread; i++) {
        int operation = dist_op(gen);
        int value = dist_val(gen);

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

  // Wait for all threads to complete
  for (auto& thread : threads) {
    thread.join();
  }

  // All operations should have completed
  EXPECT_EQ(completed_operations, kNumThreads * kOperationsPerThread);
}
