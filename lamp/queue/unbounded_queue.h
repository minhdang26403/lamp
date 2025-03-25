#ifndef UNBOUNDED_QUEUE_H_
#define UNBOUNDED_QUEUE_H_

#include <format>
#include <optional>

#include "synchronization/scoped_lock.h"
#include "synchronization/ttas_lock.h"

class EmptyException : public std::runtime_error {
 public:
  EmptyException(const std::string& what_arg)
      : std::runtime_error(std::format("EmptyException: {}", what_arg)) {}
};

template<typename T>
class UnboundedQueue {
  struct Node {
    std::optional<T> value_{};
    Node* next_{nullptr};

    Node() = default;

    Node(const T& value) : value_(value) {}
  };

 public:
  UnboundedQueue() {
    head_ = new Node();
    tail_ = head_;
  }

  ~UnboundedQueue() {
    Node* curr = head_;
    while (curr != nullptr) {
      Node* next = curr->next_;
      delete curr;
      curr = next;
    }
  }

  auto enqueue(const T& value) -> void {
    ScopedLock<TTASLock> scoped_lock{enq_mutex_};
    auto node = new Node(value);
    tail_->next_ = node;
    tail_ = node;
  }

  auto dequeue() -> T {
    ScopedLock<TTASLock> scoped_lock{deq_mutex_};
    if (head_->next_ == nullptr) {
      throw EmptyException("dequeue: Try to dequeue from an empty queue");
    }

    T value = head_->next_->value_.value();
    Node* old_head = head_;
    head_ = head_->next_;
    delete old_head;

    return value;
  }

 private:
  Node* head_;
  Node* tail_;
  TTASLock enq_mutex_;
  TTASLock deq_mutex_;
};

#endif  // UNBOUNDED_QUEUE_H_
