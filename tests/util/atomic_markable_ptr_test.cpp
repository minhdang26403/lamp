#include "util/atomic_markable_ptr.h"

#include "gtest/gtest.h"

TEST(AtomicMarkablePtrTest, AtomicMarkablePtrBasics) {
  struct TestNode {
    int value;

    TestNode(int v) : value(v) {}
  };

  TestNode* node1 = new TestNode(1);
  TestNode* node2 = new TestNode(2);

  // Test initialization and basic operations
  AtomicMarkablePtr<TestNode> ptr(node1, false);

  auto [ptr_val, marked] = ptr.get();
  EXPECT_EQ(ptr_val, node1);
  EXPECT_FALSE(marked);

  EXPECT_EQ(ptr.get_ptr(), node1);
  EXPECT_FALSE(ptr.is_marked());

  // Test CAS operation
  EXPECT_TRUE(ptr.compare_and_swap(node1, node2, false, false));
  EXPECT_EQ(ptr.get_ptr(), node2);
  EXPECT_FALSE(ptr.is_marked());

  // Test mark bit
  EXPECT_TRUE(ptr.compare_and_swap(node2, node2, false, true));
  EXPECT_EQ(ptr.get_ptr(), node2);
  EXPECT_TRUE(ptr.is_marked());

  // Test failed CAS with wrong expectations
  EXPECT_FALSE(ptr.compare_and_swap(node1, node1, true, false));  // Wrong ptr
  EXPECT_FALSE(ptr.compare_and_swap(node2, node1, false, true));  // Wrong mark

  // Cleanup
  delete node1;
  delete node2;
}
