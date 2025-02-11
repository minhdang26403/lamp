#ifndef MCS_LOCK_H_
#define MCS_LOCK_H_

#include <atomic>

#include "lock.h"

class MCSLock : public Lock {
 public:
  struct QNode {
    std::atomic<bool> locked{false};
    std::atomic<QNode*> next{nullptr};
  };

  auto lock() -> void override {
    QNode* qnode = &my_node_;
    QNode* pred = tail_.exchange(qnode, std::memory_order_acq_rel);
    if (pred != nullptr) {
      qnode->locked.store(true, std::memory_order_relaxed);
      // Use `memory_order_release` to ensure that when the next thread sees a
      // fully initialized `qnode`.
      pred->next.store(qnode, std::memory_order_release);
      // wait until predecessor gives up the lock
      while (qnode->locked.load(std::memory_order_acquire)) {}
    }
  }

  auto unlock() -> void override {
    QNode* qnode = &my_node_;
    QNode* succ = qnode->next.load();
    if (succ == nullptr) {
      auto expected = qnode;
      // need to have a separate variable `expected` so that failed
      // `compare_exchange_strong` does not modify the `qnode` variable.
      if (tail_.compare_exchange_strong(expected, nullptr,
                                        std::memory_order_acq_rel,
                                        std::memory_order_relaxed)) {
        return;
      }
      // wait until successor fills in its next field
      while (succ == nullptr) {
        succ = qnode->next.load(std::memory_order_acquire);
      }
    }
    succ->locked.store(false, std::memory_order_release);
    qnode->next.store(nullptr, std::memory_order_relaxed);
  }

 private:
  std::atomic<QNode*> tail_{nullptr};
  static thread_local QNode my_node_;
};

#endif  // MCS_LOCK_H_