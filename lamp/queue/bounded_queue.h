#ifndef BOUNDED_QUEUE_H_
#define BOUNDED_QUEUE_H_

#include <atomic>
#include <optional>

#include "synchronization/condition_variable.h"
#include "synchronization/scoped_lock.h"
#include "synchronization/ttas_lock.h"

template<typename T>
class BoundedQueue {
  struct Node {
    std::optional<T> value_{};
    Node* next_{nullptr};

    Node() = default;

    Node(const T& value) : value_(value) {}
  };

 public:
  BoundedQueue(size_t capacity) : capacity_(capacity) {
    head_ = new Node();
    tail_ = head_;
  }

  auto enqueue(const T& value) -> void {
    bool must_wake_dequeuers = false;
    auto node = new Node(value);
    {
      ScopedLock<TTASLock> scoped_lock{enq_mutex_};

      while (size_.load(std::memory_order_relaxed) == capacity_) {
        not_full_condition_.wait(enq_mutex_);
      }

      tail_->next_ = node;
      tail_ = node;

      if (size_.fetch_add(1, std::memory_order_relaxed) == 0) {
        must_wake_dequeuers = true;
      }
    }

    if (must_wake_dequeuers) {
      // Important: the thread must acquire a `deq_mutex_` to avoid lost wake up
      // since if did not acquire the `deq_mutex_`, it may signal a dequeuer
      // after they see the queue is empty, but before they go to sleep.
      ScopedLock<TTASLock> scoped_lock{deq_mutex_};
      not_empty_condition_.notify_all();
    }
  }

  auto dequeue() -> T {
    bool must_wake_enqueuers = false;
    T value;
    {
      ScopedLock<TTASLock> scoped_lock{deq_mutex_};

      while (head_->next_ == nullptr) {
        not_empty_condition_.wait(deq_mutex_);
      }

      value = head_->next_->value_.value();
      head_ = head_->next_;

      if (size_.fetch_sub(1, std::memory_order_relaxed) == capacity_) {
        must_wake_enqueuers = true;
      }
    }

    if (must_wake_enqueuers) {
      // Important: the thread must acquire a `enq_mutex_` to avoid lost wakeup
      // since if did not acquire the `enq_mutex_`, it may signal an enqueuer
      // after they see the queue is full, but before they go to sleep.
      ScopedLock<TTASLock> scoped_lock{enq_mutex_};
      not_full_condition_.notify_all();
    }

    return value;
  }

 private:
  std::atomic<size_t> size_{};
  Node* head_;
  Node* tail_;
  size_t capacity_;

  TTASLock enq_mutex_;  // Mutex to prevent concurrent enqueuers
  ConditionVariable not_full_condition_;  // Used to notify enqueuers when the
                                          // queue is no longer full

  TTASLock deq_mutex_;  // Mutex to prevent concurrent dequeuers
  ConditionVariable not_empty_condition_;  // Used to notify dequeuers when the
                                           // queue is no longer empty
};

#endif  // BOUNDED_QUEUE_H_
