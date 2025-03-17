#ifndef OPTIMISTIC_LIST_H_
#define OPTIMISTIC_LIST_H_

#include <atomic>
#include <limits>
#include <optional>

#include "synchronization/ttas_lock.h"

template<typename T, typename Hash = std::hash<T>>
class OptimisticList {
  struct Node {
    size_t key_{};
    std::optional<T> item_;
    Node* next_{nullptr};
    TTASLock mutex_;

    Node(size_t key) : key_(key) {}

    Node(size_t key, const T& item) : key_(key), item_(item) {}

    auto lock() -> void { mutex_.lock(); }

    auto unlock() -> void { mutex_.unlock(); }
  };

 public:
  OptimisticList() {
    // Initialize with sentinel nodes for min and max values
    // This eliminates edge cases when searching the list
    size_t min_key = std::numeric_limits<size_t>::min();
    size_t max_key = std::numeric_limits<size_t>::max();
    head_ = new Node(min_key);
    head_->next_ = new Node(max_key);
    garbage_list_.store(new Node(max_key), std::memory_order_relaxed);
  }

  OptimisticList(const OptimisticList<T, Hash>&) = delete;
  auto operator=(const OptimisticList<T, Hash>&)
      -> OptimisticList<T, Hash>& = delete;

  ~OptimisticList() {
    // Note that the destructor is not thread-safe (i.e., it does not acquire a
    // mutex), so the caller should guarantee that no threads can access the
    // data structure at the time the destructor is invoked.

    // First, free all nodes in the garbage_list_ (logically deleted nodes)
    // This ensures no memory is leaked from removed nodes
    Node* node = garbage_list_.load(std::memory_order_relaxed);
    while (node != nullptr) {
      Node* next = node->next_;
      delete node;
      node = next;
    }

    // Then free all nodes still in the main list
    node = head_;
    while (node != nullptr) {
      Node* next = node->next_;
      delete node;
      node = next;
    }
  }

  auto add(const T& item) -> bool {
    size_t key = get_hash_value(item);
    Node* pred;
    bool key_exists = search(key, pred);
    Node* curr = pred->next_;

    if (!key_exists) {
      Node* node = new Node(key, item);
      node->next_ = curr;

      // Without this fence, the processor or compiler might reorder operations
      // so that pred->next_ = node executes BEFORE node->next_ = curr is
      // complete. This would make our new node visible to other threads while
      // its next pointer is still null, breaking the list integrity.
      //
      // The release fence ensures that all memory operations before this point
      // (including node->next_ = curr) are completed before any memory
      // operations after this point (specifically pred->next_ = node) become
      // visible to other threads.
      std::atomic_thread_fence(std::memory_order_release);

      // This is the linearization point - the moment when the node becomes
      // visible to other threads. After the fence, we can safely make our node
      // visible.
      pred->next_ = node;
    }

    curr->unlock();
    pred->unlock();

    return !key_exists;
  }

  auto remove(const T& item) -> bool {
    size_t key = get_hash_value(item);
    Node* pred;
    bool key_exists = search(key, pred);
    Node* curr = pred->next_;

    if (key_exists) {
      // This is the linearization point - the moment when the node is logically
      // removed
      pred->next_ = curr->next_;

      // Without proper memory ordering, the processor might reorder operations,
      // causing curr to be added to the garbage list before it is fully removed
      // from the main list, leading to a potential race condition.
      //
      // Using memory_order_release in compare_exchange_weak ensures that all
      // prior operations (including pred->next_ = curr->next_) are visible to
      // other threads before the node is added to the garbage list.
      //
      // The garbage list uses a lock-free approach where the next_ pointer of
      // the removed node is reused. The CAS loop ensures that concurrent
      // modifications to garbage_list_ remain thread-safe.
      curr->next_ = garbage_list_.load(std::memory_order_relaxed);
      while (!garbage_list_.compare_exchange_weak(curr->next_, curr,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)) {
        // If CAS fails, curr->next_ is updated with the latest value of
        // garbage_list_, and we retry with the updated value.
      }
    }

    // Important: unlock the nodes after all operations are complete
    curr->unlock();
    pred->unlock();

    return key_exists;
  }

  auto contains(const T& item) -> bool {
    size_t key = get_hash_value(item);
    Node* pred;
    bool key_exists = search(key, pred);

    pred->next_->unlock();
    pred->unlock();

    return key_exists;
  }

 private:
  /**
   * Searches for a node with the given key and returns whether it exists.
   * Uses an optimistic approach with hand-over-hand locking for traversal.
   *
   * @param key The key to search for
   * @param pred Reference to store the predecessor node (will be locked if
   * found)
   * @return true if the key exists, false otherwise
   *
   * Note: This method leaves both pred and pred->next_ locked when it returns.
   * Callers are responsible for unlocking these nodes.
   */
  auto search(size_t key, Node*& pred) -> bool {
    while (true) {
      pred = head_;
      Node* curr = pred->next_;

      // Traverse the list without locking until we find the right position
      while (curr->key_ < key) {
        pred = curr;
        curr = curr->next_;
      }

      // Lock the nodes in order to prevent race conditions
      pred->lock();
      curr->lock();

      // Validate that the list hasn't changed during our traversal
      // This is what makes the algorithm "optimistic" - we assume no changes
      // and verify after locking
      if (validate(pred, curr)) {
        return curr->key_ == key;
      }

      // If validation fails, unlock and try again
      // This handles cases where the list was modified during our traversal
      pred->unlock();
      curr->unlock();
    }
  }

  /**
   * Validates that pred and curr are still connected in the list.
   * This is necessary because nodes may have been removed between our traversal
   * and acquiring the locks.
   *
   * @param pred The predecessor node
   * @param curr The current node
   * @return true if pred still points to curr, false otherwise
   */
  auto validate(Node* pred, Node* curr) -> bool {
    Node* node = head_;
    while (node->key_ <= pred->key_) {
      if (node == pred) {
        return pred->next_ == curr;
      }
      node = node->next_;
    }
    return false;
  }

  auto get_hash_value(const T& item) const noexcept -> size_t {
    size_t hash = hash_fn_(item);

    // Ensure the hash is never 0 or size_t::max() by wrapping around
    return (hash % (std::numeric_limits<size_t>::max() - 1)) + 1;
  }

  Node* head_;      // Pointer to the first (sentinel) node
  Hash hash_fn_{};  // Hash function for items
  std::atomic<Node*>
      garbage_list_{};  // Lock-free list of logically deleted nodes
};

#endif  // OPTIMISTIC_LIST_H_
