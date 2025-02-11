#ifndef CLH_LOCK_H_
#define CLH_LOCK_H_

#include <atomic>

#include "lock.h"

class CLHLock : public Lock {
 private:
  struct QNode {
    // Record the status of a thread:
    // - If true, the corresponding thread has either acquired the lock or is
    // waiting for the lock.
    // - If false, the thread has released the lock.
    std::atomic<bool> locked_{false};
  };

 public:
  CLHLock() {
    auto qnode = new QNode();
    tail_.store(qnode, std::memory_order_relaxed);
  }

  ~CLHLock() { delete tail_.load(std::memory_order_relaxed); }

  auto lock() -> void override {
    // Load the pointer to our node from the thread-local variable.
    QNode* qnode = my_node_;
    // Indicate our intention to acquire the lock.
    qnode->locked_.store(true, std::memory_order_release);

    // Get the node that represents the state of the predecessor thread.
    QNode* pred = tail_.exchange(qnode, std::memory_order_relaxed);

    // Save the pointer to the predecessor node.
    my_pred_ = pred;

    // Check if the predecessor thread has acquired the lock or is waiting for
    // the lock.
    while (pred->locked_.load(std::memory_order_acquire)) {}
  }

  auto unlock() -> void override {
    my_node_->locked_.store(false, std::memory_order_release);
    // We can reuse the predecessor node as our own node.
    my_node_ = my_pred_;
  }

 private:
  std::atomic<QNode*> tail_;
  static thread_local QNode* my_pred_;
  static thread_local QNode* my_node_;
};

#endif  // CLH_LOCK_H_
