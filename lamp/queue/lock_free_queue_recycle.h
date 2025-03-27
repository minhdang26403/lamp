#ifndef LOCK_FREE_QUEUE_RECYCLE_H_
#define LOCK_FREE_QUEUE_RECYCLE_H_

#include <atomic>
#include <optional>

#include "util/atomic_stamped_ptr.h"
#include "util/common.h"

template<typename T>
class NodePool;

template<typename T>
class LockFreeQueueRecycle {
  struct Node {
    std::optional<T> value_{};
    AtomicStampedPtr<Node> next_{};

    Node() = default;

    Node(std::optional<T> value) : value_(std::move(value)) {}
  };

  class NodePool {
   public:
    ~NodePool() {
      auto [curr, _] = unused_nodes_.get(std::memory_order_relaxed);
      while (curr != nullptr) {
        auto next = curr->next_.get_ptr(std::memory_order_relaxed);
        delete curr;
        curr = next;
      }
    }

    auto allocate(std::optional<T> value) -> Node* {
      while (true) {
        auto [head, stamp] = unused_nodes_.get(std::memory_order_relaxed);
        if (head == nullptr) {
          return new Node(std::move(value));
        }
        Node* next = head->next_.get_ptr(std::memory_order_relaxed);
        if (unused_nodes_.compare_and_swap(head, next, stamp, stamp + 1,
                                           std::memory_order_relaxed)) {
          head->value_ = std::move(value);
          head->next_.set(nullptr, 0, std::memory_order_relaxed);
          return head;
        }
      }
    }

    auto free(Node* node) -> void {
      while (true) {
        auto [head, stamp] = unused_nodes_.get(std::memory_order_relaxed);
        node->next_.set(head, 0, std::memory_order_relaxed);
        if (unused_nodes_.compare_and_swap(head, node, stamp, stamp + 1,
                                           std::memory_order_relaxed)) {
          return;
        }
      }
    }

   private:
    AtomicStampedPtr<Node> unused_nodes_{};
  };

 public:
  LockFreeQueueRecycle() {
    auto node = node_pool_.allocate(std::nullopt);
    std::atomic_thread_fence(std::memory_order_release);
    head_.set(node, 0, std::memory_order_relaxed);
    tail_.set(node, 0, std::memory_order_relaxed);
  }

  auto enqueue(T value) -> void {
    auto node = node_pool_.allocate(std::optional<T>{std::move(value)});
    while (true) {
      auto [last, last_stamp] = tail_.get(std::memory_order_acquire);
      auto [next, next_stamp] = last->next_.get(std::memory_order_acquire);
      if (last_stamp == tail_.get_stamp(std::memory_order_relaxed)) {
        if (next == nullptr) {
          if (last->next_.compare_and_swap(
                  next, node, next_stamp, next_stamp + 1,
                  std::memory_order_release, std::memory_order_relaxed)) {
            tail_.compare_and_swap(last, node, last_stamp, last_stamp + 1,
                                   std::memory_order_release,
                                   std::memory_order_relaxed);
            return;
          }
        } else {
          tail_.compare_and_swap(last, next, last_stamp, last_stamp + 1,
                                 std::memory_order_release,
                                 std::memory_order_relaxed);
        }
      }
    }
  }

  auto dequeue() -> T {
    while (true) {
      auto [first, first_stamp] = head_.get(std::memory_order_relaxed);
      auto [last, last_stamp] = tail_.get(std::memory_order_acquire);
      auto [next, next_stamp] = first->next_.get(std::memory_order_acquire);

      if (first_stamp == head_.get_stamp(std::memory_order_relaxed)) {
        if (first == last) {
          if (next == nullptr) {
            throw EmptyException("dequeue: Try to dequeue from an empty queue");
          }
          tail_.compare_and_swap(last, next, last_stamp, last_stamp + 1,
                                 std::memory_order_release,
                                 std::memory_order_relaxed);
        } else {
          if (head_.compare_and_swap(first, next, first_stamp, first_stamp + 1,
                                     std::memory_order_relaxed)) {
            T value = next->value_.value();
            node_pool_.free(first);
            return value;
          }
        }
      }
    }
  }

 private:
  AtomicStampedPtr<Node> head_;
  AtomicStampedPtr<Node> tail_;

  NodePool node_pool_;
};

#endif  // LOCK_FREE_QUEUE_RECYCLE_H_
