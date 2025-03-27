#ifndef LOCK_FREE_QUEUE_RECYCLE_H_
#define LOCK_FREE_QUEUE_RECYCLE_H_

#include <atomic>
#include <optional>

#include "util/common.h"

template<typename T>
class NodePool;

template<typename T>
class LockFreeQueueRecycle {
  struct Node {
    std::optional<T> value_{};
    std::atomic<Node*> next_{};

    Node() = default;

    Node(std::optional<T> value) : value_(std::move(value)) {}
  };

 public:
  LockFreeQueueRecycle() {
    auto node = node_pool_.allocate(std::nullopt);
    std::atomic_thread_fence(std::memory_order_release);
    head_.store(node, std::memory_order_relaxed);
    tail_.store(node, std::memory_order_relaxed);
  }

  auto enqueue(T value) -> void {
    auto node = node_pool_.allocate(std::optional<T>{std::move(value)});
    while (true) {
      Node* last = tail_.load(std::memory_order_acquire);
      Node* next = last->next_.load(std::memory_order_acquire);
      if (last == tail_.load(std::memory_order_acquire)) {
        if (next == nullptr) {
          if (last->next_.compare_exchange_strong(next, node,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)) {
            tail_.compare_exchange_strong(last, node, std::memory_order_release,
                                          std::memory_order_relaxed);
            return;
          }
        } else {
          tail_.compare_exchange_strong(last, node, std::memory_order_release,
                                        std::memory_order_relaxed);
        }
      }
    }
  }

  auto dequeue() -> T {
    while (true) {
      Node* first = head_.load(std::memory_order_acquire);
      Node* last = tail_.load(std::memory_order_acquire);
      Node* next = first->next_.load(std::memory_order_acquire);

      if (first == head_.load(std::memory_order_acquire)) {
        if (first == last) {
          if (next == nullptr) {
            throw EmptyException("dequeue: Try to dequeue from an empty queue");
          }
          tail_.compare_exchange_strong(last, next, std::memory_order_release,
                                        std::memory_order_relaxed);
        } else {
          T value = next->value_.value();
          if (head_.compare_exchange_strong(first, next,
                                            std::memory_order_release,
                                            std::memory_order_relaxed)) {
            node_pool_.free(first);
            return value;
          }
        }
      }
    }
  }

 private:
  template<typename U>
  friend class NodePool;

  std::atomic<Node*> head_;
  std::atomic<Node*> tail_;

  static thread_local NodePool<T> node_pool_;
};

template<typename T>
class NodePool {
  using Node = LockFreeQueueRecycle<T>::Node;

 public:
  ~NodePool() {
    Node* curr = unused_nodes_;
    while (curr != nullptr) {
      Node* next = curr->next_.load(std::memory_order_relaxed);
      delete curr;
      curr = next;
    }
  }

  auto allocate(std::optional<T> value) -> Node* {
    if (unused_nodes_ == nullptr) {
      return new Node(std::move(value));
    }
    Node* node = unused_nodes_;
    node->value_ = std::move(value);
    node->next_.store(nullptr, std::memory_order_relaxed);
    unused_nodes_ = unused_nodes_->next_.load(std::memory_order_relaxed);
    return node;
  }

  auto free(Node* node) -> void {
    node->next_.store(unused_nodes_, std::memory_order_relaxed);
    unused_nodes_ = node;
  }

 private:
  Node* unused_nodes_{nullptr};
};

template<typename T>
thread_local NodePool<T> LockFreeQueueRecycle<T>::node_pool_;

#endif  // LOCK_FREE_QUEUE_RECYCLE_H_
