#ifndef CLH_LOCK_H_
#define CLH_LOCK_H_

#include <atomic>

#include "lock.h"

class CLHLock : public Lock {
 private:
  struct QNode {
    QNode(bool value) : locked_(value) {}

    // Record the status of a thread:
    // - If true, the corresponding thread has either acquired the lock or is
    // waiting for the lock.
    // - If false, the thread has released the lock.
    std::atomic<bool> locked_;
  };

 public:
  CLHLock() {
    auto qnode = new QNode(false);
    tail_.store(qnode, std::memory_order_relaxed);
  }

  virtual ~CLHLock() { delete tail_.load(std::memory_order_relaxed); }

  auto lock() -> void override {
    QNode* qnode = new QNode(true);
    // Get the node that represents the state of the predecessor thread.
    QNode* prev = tail_.exchange(qnode, std::memory_order_release);

    // Check if the predecessor thread has acquired the lock or is waiting for
    // the lock.
    while (prev->locked_.load(std::memory_order_acquire)) {}

    // This thread has acquired the lock and it is now safe to clean up the
    // memory of the previous node.
    delete prev;

    // Store my_node for unlock to use.
    my_node_ = qnode;
  }

  auto unlock() -> void override {
    my_node_->locked_.store(false, std::memory_order_release);
  }

 private:
  QNode* my_node_;
  std::atomic<QNode*> tail_;
};

#endif  // CLH_LOCK_H_
