#ifndef LOCK_FREE_STACK_H_
#define LOCK_FREE_STACK_H_

#include <atomic>
#include <chrono>
#include "util/backoff.h"
#include "util/common.h"

template<typename T, typename Duration = std::chrono::microseconds>
class LockFreeStack {
  struct Node {
    T value_;
    Node* next_{nullptr};
    Node* next_deleted_{nullptr};

    Node(T value) : value_(std::move(value)) {}
  };

 public:
  LockFreeStack() = default;

  LockFreeStack(int64_t min_delay, int64_t max_delay)
      : kMinDelay(min_delay), kMaxDelay(max_delay) {}

  ~LockFreeStack() {
    // Clean up in two phases to avoid memory leaks
    // Phase 1: Clean garbage list (nodes already removed from main queue)
    Node* curr = garbage_list_.load(std::memory_order_relaxed);
    while (curr != nullptr) {
      Node* next = curr->next_deleted_;
      delete curr;
      curr = next;
    }

    // Phase 2: Clean remaining nodes in queue including sentinel
    curr = top_.load(std::memory_order_relaxed);
    while (curr != nullptr) {
      Node* next = curr->next_;
      delete curr;
      curr = next;
    }
  }

  auto push(T value) -> void {
    Backoff<Duration> backoff{kMinDelay, kMaxDelay};
    auto node = new Node(std::move(value));
    while (true) {
      if (try_push(node)) {
        return;
      }
      backoff.backoff();
    }
  }

  auto pop() -> T {
    Backoff<Duration> backoff{kMinDelay, kMaxDelay};
    while (true) {
      Node* return_node = try_pop();
      if (return_node != nullptr) {
        T value = std::move(return_node->value_);

        // Chain the node into garbage list to clean up
        return_node->next_deleted_ =
            garbage_list_.load(std::memory_order_relaxed);
        while (!garbage_list_.compare_exchange_weak(
            return_node->next_deleted_, return_node,
            std::memory_order_relaxed)) {}

        return value;
      }
      backoff.backoff();
    }
  }

 private:
  auto try_push(Node* node) -> bool {
    Node* old_top = top_.load(std::memory_order_acquire);
    node->next_ = old_top;
    return top_.compare_exchange_strong(
        old_top, node, std::memory_order_release, std::memory_order_relaxed);
  }

  auto try_pop() -> Node* {
    Node* old_top = top_.load(std::memory_order_acquire);
    if (old_top == nullptr) {
      throw EmptyException("Try to pop from an empty stack");
    }
    Node* new_top = old_top->next_;
    if (top_.compare_exchange_strong(old_top, new_top,
                                     std::memory_order_acquire,
                                     std::memory_order_relaxed)) {
      return old_top;
    }
    return nullptr;
  }

  std::atomic<Node*> top_{nullptr};
  std::atomic<Node*> garbage_list_;

  // Default backoff duration ranges from 5ms - 25ms
  const int64_t kMinDelay{5};
  const int64_t kMaxDelay{25};
};

#endif  // LOCK_FREE_STACK_H_
