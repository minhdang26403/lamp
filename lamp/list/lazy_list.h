#ifndef LAZY_LIST_H_
#define LAZY_LIST_H_

#include <atomic>
#include <limits>
#include <optional>

#include "synchronization/ttas_lock.h"

/**
 * LazyList - A concurrent linked list implementation using lazy synchronization
 *
 * This implementation uses a combination of optimistic and two-phase locking
 * for thread safety. The key features are:
 * 1. Logical deletion (marking) before physical removal
 * 2. Lock coupling during traversal for thread safety
 * 3. Lock-free garbage management for deleted nodes
 *
 * The list maintains sentinel nodes with min and max keys to simplify boundary
 * conditions.
 */
template<typename T, typename Hash = std::hash<T>>
class LazyList {
  /**
   * Node structure for the linked list
   * Contains a key, optional item, next pointer, marked flag for logical
   * deletion, and a mutex for concurrency control
   */
  struct Node {
    size_t key_;             // Hash key for ordering in the list
    std::optional<T> item_;  // Optional value stored in the node
    Node* next_{nullptr};    // Pointer to the next node
    bool marked_{false};     // Logical deletion flag
    TTASLock mutex_;  // Test-and-test-and-set lock for concurrency control

    Node(size_t key) : key_(key) {}  // Constructor for sentinel nodes

    Node(size_t key, const T& item)
        : key_(key), item_(item) {}  // Constructor for data nodes

    auto lock() -> void { mutex_.lock(); }  // Acquire the node's lock

    auto unlock() -> void { mutex_.unlock(); }  // Release the node's lock
  };

 public:
  /**
   * Constructor - Creates an empty list with sentinel nodes
   *
   * Initializes the list with two sentinel nodes:
   * - head with minimum key value
   * - tail with maximum key value
   * Also initializes the garbage list with a sentinel node
   */
  LazyList() {
    size_t min_key = std::numeric_limits<size_t>::min();
    size_t max_key = std::numeric_limits<size_t>::max();
    head_ = new Node(min_key);         // Create head sentinel node
    head_->next_ = new Node(max_key);  // Create tail sentinel node
    garbage_list_.store(
        new Node(max_key),
        std::memory_order_relaxed);  // Initialize garbage list with sentinel
  }

  // Prevent copying to avoid complex ownership issues
  LazyList(const LazyList<T, Hash>&) = delete;
  auto operator=(const LazyList<T, Hash>&) -> LazyList<T, Hash>& = delete;

  /**
   * Destructor - Cleans up all nodes including those in the garbage list
   *
   * NOT thread-safe - caller must ensure no other threads are accessing the
   * list
   */
  ~LazyList() {
    // First, free all nodes in the garbage_list_ (logically deleted nodes)
    // This ensures no memory is leaked from removed nodes
    Node* curr = garbage_list_.load(std::memory_order_relaxed);
    while (curr != nullptr) {
      Node* next = curr->next_;
      delete curr;
      curr = next;
    }

    // Then free all nodes still in the main list
    curr = head_;
    while (curr != nullptr) {
      Node* next = curr->next_;
      delete curr;
      curr = next;
    }
  }

  /**
   * Adds an item to the list if its key doesn't already exist
   *
   * @param item The item to add
   * @return true if the item was added, false if the key already exists
   *
   * Thread safety: Uses the search method for lock acquisition before
   * modification
   */
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

    // Release locks held by search
    curr->unlock();
    pred->unlock();

    return !key_exists;
  }

  /**
   * Removes an item from the list if its key exists
   *
   * @param item The item to remove
   * @return true if the item was removed, false if not found
   *
   * Thread safety: Uses two-phase locking - mark for logical deletion first,
   * then physically remove from list. Moves deleted node to garbage list.
   */
  auto remove(const T& item) -> bool {
    size_t key = get_hash_value(item);
    Node* pred;
    bool key_exists = search(key, pred);  // Find position and lock nodes
    Node* curr = pred->next_;

    if (key_exists) {
      curr->marked_ = true;       // Logical deletion
      pred->next_ = curr->next_;  // Physical removal

      // Add node to garbage list using lock-free algorithm
      // This defers actual deletion to avoid ABA problems and memory issues
      curr->next_ = garbage_list_.load(std::memory_order_relaxed);
      // Loop until successful CAS operation to insert at head of garbage list
      while (!garbage_list_.compare_exchange_weak(curr->next_, curr,
                                                  std::memory_order_relaxed)) {}
    }

    // Release locks held by search
    curr->unlock();
    pred->unlock();

    return key_exists;
  }

  /**
   * Checks if an item exists in the list
   *
   * @param item The item to check for
   * @return true if the item exists, false otherwise
   *
   * Thread safety: Wait-free traversal - no locks are acquired
   * Note: May return false negatives due to concurrent modifications
   */
  auto contains(const T& item) -> bool {
    size_t key = get_hash_value(item);
    Node* curr = head_;
    // Traverse list until we find a key >= target
    while (curr->key_ < key) {
      curr = curr->next_;
    }

    // Check if key matches and node is not marked as deleted
    return curr->key_ == key && !curr->marked_;
  }

 private:
  /**
   * Locates the position for a key and locks the relevant nodes
   *
   * @param key The key to search for
   * @param pred Reference to store the predecessor node
   * @return true if key exists, false otherwise
   *
   * Thread safety: Uses optimistic validation with retries and hand-over-hand
   * locking Critical for both add and remove operations to maintain list
   * integrity
   */
  auto search(size_t key, Node*& pred) -> bool {
    while (true) {
      pred = head_;
      Node* curr = pred->next_;

      // Optimistic traversal without locks
      while (curr->key_ < key) {
        pred = curr;
        curr = curr->next_;
      }

      // Lock nodes in order of traversal to prevent deadlock
      pred->lock();
      curr->lock();

      // Validate that nodes are still valid and connected
      // If validation fails, release locks and retry
      if (validate(pred, curr)) {
        return curr->key_ == key;
      }

      pred->unlock();
      curr->unlock();
    }
  }

  /**
   * Validates that pred and curr nodes are still valid for modification
   *
   * @param pred The predecessor node
   * @param curr The current node
   * @return true if both nodes are valid and connected
   *
   * Checks three conditions:
   * 1. pred is not marked (not deleted)
   * 2. curr is not marked (not deleted)
   * 3. pred still points to curr (no concurrent modification)
   */
  auto validate(Node* pred, Node* curr) const noexcept -> bool {
    return !pred->marked_ && !curr->marked_ && pred->next_ == curr;
  }

  /**
   * Computes hash value for an item
   *
   * @param item The item to hash
   * @return A size_t hash value, offset by 1 to avoid potential hash of 0
   *
   * Adding 1 ensures keys are strictly positive, preserving sentinel node
   * ordering
   */
  auto get_hash_value(const T& item) const noexcept -> size_t {
    return hash_fn_(item) + 1;
  }

  Node* head_;      // Pointer to the head sentinel node
  Hash hash_fn_{};  // Hash function for generating keys
  std::atomic<Node*>
      garbage_list_;  // Lock-free list of logically deleted nodes
                      // Uses atomic operations for thread safety
};

#endif  // LAZY_LIST_H_
