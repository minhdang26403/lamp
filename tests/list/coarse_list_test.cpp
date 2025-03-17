#include "list/coarse_list.h"
#include <gtest/gtest.h>
#include <random>
#include <thread>
#include <vector>

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

  // First add items sequentially
  for (size_t i = 0; i < kNumItems; ++i) {
    EXPECT_TRUE(list_->add(static_cast<int>(i)));
  }

  // Now have two threads: one adding, one removing
  std::thread remove_thread([this]() {
    for (size_t i = 0; i < kNumItems; i += 2) {
      list_->remove(static_cast<int>(i));
    }
  });

  std::thread add_thread([this]() {
    for (size_t i = 0; i < kNumItems; i += 2) {
      list_->add(static_cast<int>(i));
    }
  });

  remove_thread.join();
  add_thread.join();

  // Check that all numbers are still in the list
  for (size_t i = 0; i < kNumItems; ++i) {
    EXPECT_TRUE(list_->contains(static_cast<int>(i)));
  }
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
