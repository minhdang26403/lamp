#ifndef LOCK_FREE_QUEUE_RECYCLE_H_
#define LOCK_FREE_QUEUE_RECYCLE_H_

#include <atomic>
#include <optional>

#include "util/atomic_stamped_ptr.h"
#include "util/common.h"

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
    // Destructor: Cleans up all nodes in the pool when the queue is destroyed.
    // Uses relaxed ordering since no concurrent access occurs at this point.
    ~NodePool() {
      auto [curr, _] = unused_nodes_.get(std::memory_order_relaxed);
      while (curr != nullptr) {
        auto next = curr->next_.get_ptr(std::memory_order_relaxed);
        delete curr;
        curr = next;
      }
    }

    // Allocates a node, either by recycling from the pool or creating a new one.
    // Relies on relaxed ordering, assuming outer queue operations (enqueue/dequeue)
    // provide necessary synchronization.
    auto allocate(std::optional<T> value) -> Node* {
      while (true) {
        // Read the current head of the unused_nodes_ stack with relaxed ordering.
        // This assumes acquire semantics in enqueue/dequeue ensure visibility.
        auto [head, stamp] = unused_nodes_.get(std::memory_order_relaxed);
        if (head == nullptr) {
          return new Node(std::move(value));
        }
        // Read next pointer with relaxed ordering; CAS will validate.
        Node* next = head->next_.get_ptr(std::memory_order_relaxed);
        // Attempt to pop the head node with relaxed CAS, relying on outer
        // acquire/release for visibility and ordering.
        if (unused_nodes_.compare_and_swap(head, next, stamp, stamp + 1,
                                           std::memory_order_relaxed)) {
          head->value_ = std::move(value);
          // Clear next pointer with relaxed ordering since this node is now
          // privately owned by the caller.
          head->next_.set(nullptr, 0, std::memory_order_relaxed);
          return head;
        }
      }
    }

    // Frees a node by pushing it onto the unused_nodes_ stack.
    // Uses relaxed ordering, assuming dequeue's release ensures visibility.
    auto free(Node* node) -> void {
      while (true) {
        // Read current head with relaxed ordering, relying on outer acquire.
        auto [head, stamp] = unused_nodes_.get(std::memory_order_relaxed);
        // Link node to current head with relaxed ordering; CAS handles atomicity.
        node->next_.set(head, 0, std::memory_order_relaxed);
        // Push node onto stack with relaxed CAS, depending on outer release
        // (e.g., in dequeue) to publish this change.
        if (unused_nodes_.compare_and_swap(head, node, stamp, stamp + 1,
                                           std::memory_order_relaxed)) {
          return;
        }
      }
    }

   private:
    // Lock-free stack of unused nodes, managed with atomic stamped pointer.
    AtomicStampedPtr<Node> unused_nodes_{};
  };

 public:
  LockFreeQueueRecycle() {
    auto node = node_pool_.allocate(std::nullopt);
    // Release fence ensures the node allocation is visible before head/tail setup.
    std::atomic_thread_fence(std::memory_order_release);
    head_.set(node, 0, std::memory_order_relaxed);
    tail_.set(node, 0, std::memory_order_relaxed);
  }

  // Enqueues a value into the lock-free queue using a two-step CAS algorithm.
  auto enqueue(T value) -> void {
    auto node = node_pool_.allocate(std::optional<T>{std::move(value)});
    while (true) {
      // Acquire tail to ensure we see the latest queue state.
      auto [last, last_stamp] = tail_.get(std::memory_order_acquire);
      // Acquire next pointer to synchronize with prior enqueues or dequeues.
      auto [next, next_stamp] = last->next_.get(std::memory_order_acquire);
      // Relaxed check for tail consistency, relying on prior acquire.
      if (last_stamp == tail_.get_stamp(std::memory_order_relaxed)) {
        if (next == nullptr) {
          // Attempt to link the new node with release semantics to publish it.
          if (last->next_.compare_and_swap(next, node, next_stamp, next_stamp + 1,
                                           std::memory_order_release,
                                           std::memory_order_relaxed)) {
            // Update tail with release to ensure visibility; failure is okay as
            // another thread may have done it.
            tail_.compare_and_swap(last, node, last_stamp, last_stamp + 1,
                                   std::memory_order_release,
                                   std::memory_order_relaxed);
            return;
          }
        } else {
          // Help advance tail if it's behind, using release for visibility.
          tail_.compare_and_swap(last, next, last_stamp, last_stamp + 1,
                                 std::memory_order_release,
                                 std::memory_order_relaxed);
        }
      }
    }
  }

  // Dequeues a value from the lock-free queue, throwing if empty.
  auto dequeue() -> T {
    while (true) {
      // Relaxed load of head, relying on subsequent acquire for synchronization.
      auto [first, first_stamp] = head_.get(std::memory_order_relaxed);
      // Acquire tail to see the latest queue state.
      auto [last, last_stamp] = tail_.get(std::memory_order_acquire);
      // Acquire next pointer to ensure visibility of the node's state.
      auto [next, next_stamp] = first->next_.get(std::memory_order_acquire);
      // Relaxed check for head consistency, relying on prior acquire operations.
      if (first_stamp == head_.get_stamp(std::memory_order_relaxed)) {
        if (first == last) {
          if (next == nullptr) {
            throw EmptyException("dequeue: Try to dequeue from an empty queue");
          }
          // Advance tail if stalled, using release for visibility.
          tail_.compare_and_swap(last, next, last_stamp, last_stamp + 1,
                                 std::memory_order_release,
                                 std::memory_order_relaxed);
        } else {
          // Attempt to advance head with relaxed CAS, assuming outer acquire
          // ensures prior visibility and release in free() handles publication.
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
  AtomicStampedPtr<Node> head_;  // Head of the queue, updated by dequeue.
  AtomicStampedPtr<Node> tail_;  // Tail of the queue, updated by enqueue.
  NodePool node_pool_;           // Embedded pool for node recycling.
};

#endif  // LOCK_FREE_QUEUE_RECYCLE_H_
