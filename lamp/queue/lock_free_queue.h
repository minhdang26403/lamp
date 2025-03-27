#ifndef LOCK_FREE_QUEUE_H_
#define LOCK_FREE_QUEUE_H_

#include <atomic>
#include <optional>

#include "util/common.h"

template<typename T>
class LockFreeQueue {
  struct Node {
    std::optional<T> value_{};          // Optional to distinguish dummy nodes
    std::atomic<Node*> next_{nullptr};  // Atomic for thread-safe linking
    Node* next_deleted_{nullptr};       // Non-atomic link for garbage list

    Node() = default;

    Node(const T& value) : value_(value) {}
  };

 public:
  LockFreeQueue() {
    // Create sentinel node that always remains in the queue
    auto node = new Node();
    std::atomic_thread_fence(std::memory_order_release);
    head_.store(node, std::memory_order_relaxed);
    tail_.store(node, std::memory_order_relaxed);
  }

  ~LockFreeQueue() {
    // Clean up in two phases to avoid memory leaks
    // Phase 1: Clean garbage list (nodes already removed from main queue)
    Node* curr = garbage_list_.load(std::memory_order_relaxed);
    while (curr != nullptr) {
      Node* next = curr->next_deleted_;
      delete curr;
      curr = next;
    }

    // Phase 2: Clean remaining nodes in queue including sentinel
    curr = head_.load(std::memory_order_relaxed);
    while (curr != nullptr) {
      Node* next = curr->next_.load(std::memory_order_relaxed);
      delete curr;
      curr = next;
    }
  }

  auto enqueue(const T& value) -> void {
    auto node = new Node(value);
    while (true) {
      // Load current tail and its next pointer
      Node* last = tail_.load(std::memory_order_acquire);
      Node* next = last->next_.load(std::memory_order_acquire);

      // Double-check tail hasn't changed. Note that this check may seem
      // vulnerable to ABA, but our deferred deletion scheme (nodes only
      // freed in destructor) prevents ABA problems
      if (last == tail_.load(std::memory_order_relaxed)) {
        if (next == nullptr) {
          // Attempt to link new node at the end
          // If successful, try to update tail (might fail if others help)
          if (last->next_.compare_exchange_strong(next, node,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)) {
            // Tail update is best-effort; other threads might do it first
            tail_.compare_exchange_strong(last, node, std::memory_order_release,
                                          std::memory_order_relaxed);
            return;
          }
        } else {
          // Tail is lagging behind; help advance it
          // This helps maintain queue consistency across threads
          tail_.compare_exchange_strong(last, next, std::memory_order_release,
                                        std::memory_order_relaxed);
        }
      }
      // Loop continues if CAS fails or tail changed
    }
  }

  auto dequeue() -> T {
    while (true) {
      Node* first = head_.load(std::memory_order_relaxed);
      Node* last = tail_.load(std::memory_order_acquire);
      Node* next = first->next_.load(std::memory_order_acquire);

      // Verify head hasn't changed. Note that this check may seem
      // vulnerable to ABA, but our deferred deletion scheme (nodes only
      // freed in destructor) prevents ABA problems
      if (first == head_.load(std::memory_order_relaxed)) {
        if (first == last) {
          if (next == nullptr) {
            // Queue is empty (head=tail and no next node)
            throw EmptyException("dequeue: Try to dequeue from an empty queue");
          }
          // Special case: last node being enqueued, help finish it
          tail_.compare_exchange_strong(last, next, std::memory_order_release,
                                        std::memory_order_relaxed);
        } else {
          // Normal case: remove head node
          if (head_.compare_exchange_strong(first, next,
                                            std::memory_order_relaxed)) {
            // Successfully dequeued, add old head to garbage list
            T value = next->value_.value();
            add_to_garbage(first);
            return value;
          }
        }
      }
      // Loop continues if CAS fails or head changed
    }
  }

 private:
  // Add node to garbage list for deferred deletion
  auto add_to_garbage(Node* node) -> void {
    // Simple lock-free linked list insertion
    node->next_deleted_ = garbage_list_.load(std::memory_order_relaxed);
    while (!garbage_list_.compare_exchange_weak(node->next_deleted_, node,
                                                std::memory_order_relaxed)) {
      // Retry if CAS fails due to concurrent modifications
    }
  }

  std::atomic<Node*> head_;  // Points to sentinel or first real node
  std::atomic<Node*> tail_;  // Points to last node (might lag)
  std::atomic<Node*> garbage_list_{nullptr};  // Deferred deletion list
};

#endif  // LOCK_FREE_QUEUE_H_
