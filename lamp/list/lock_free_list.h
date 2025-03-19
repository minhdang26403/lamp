#ifndef LOCK_FREE_LIST_H_
#define LOCK_FREE_LIST_H_

#include <atomic>
#include <limits>
#include <optional>

#include "util/atomic_markable_ptr.h"

/**
 * LockFreeList - A concurrent linked list that supports lock-free add, remove,
 * and contains operations
 *
 * This implementation follows the Harris-Michael algorithm for lock-free linked
 * lists, using atomic marked pointers to perform logical deletion before
 * physical removal. The list maintains sorted nodes based on hash values of
 * items, with sentinel nodes at min and max values to simplify boundary
 * conditions.
 */
template<typename T, typename Hash = std::hash<T>>
class LockFreeList {
  /**
   * Node structure for the linked list
   *
   * Each node contains:
   * - A key (hash of the stored item)
   * - The optional item value
   * - An atomic markable pointer to the next node (mark bit indicates logical
   * deletion)
   * - A pointer to the next logically deleted node (for garbage collection)
   */
  struct Node {
    size_t key_{};
    std::optional<T> item_{};
    std::unique_ptr<AtomicMarkablePtr<Node>>
        next_;  // AtomicMarkPtr does not have copy constructor, so use a
                // unique_ptr to work around that
    Node* next_deleted_{nullptr};

    Node(size_t key)
        : key_(key),
          next_(std::make_unique<AtomicMarkablePtr<Node>>(nullptr, false)) {}

    Node(size_t key, const T& item)
        : key_(key),
          item_(item),
          next_(std::make_unique<AtomicMarkablePtr<Node>>(nullptr, false)) {}
  };

 public:
  /**
   * Constructor - Creates a lock-free list with sentinel nodes
   *
   * Initializes the list with two sentinel nodes:
   * - A head node with minimum possible key value
   * - A tail node with maximum possible key value
   * This simplifies boundary conditions in other operations.
   */
  LockFreeList() {
    size_t min_key = std::numeric_limits<size_t>::min();
    size_t max_key = std::numeric_limits<size_t>::max();
    head_ = new Node(min_key);
    tail_ = new Node(max_key);
    head_->next_ = std::make_unique<AtomicMarkablePtr<Node>>(tail_, false);
  }

  // Prevent copying to avoid complex ownership issues
  LockFreeList(const LockFreeList<T, Hash>&) = delete;
  auto operator=(const LockFreeList<T, Hash>&)
      -> LockFreeList<T, Hash>& = delete;

  /**
   * Destructor - Cleans up all nodes including those in the garbage list
   */
  ~LockFreeList() {
    // First, free all nodes in the garbage_list_ (logically deleted nodes)
    // This ensures no memory is leaked from removed nodes
    Node* curr = garbage_list_.load(std::memory_order_acquire);
    while (curr != nullptr) {
      Node* next = curr->next_deleted_;
      delete curr;
      curr = next;
    }

    // Then free all nodes still in the main list
    curr = head_;
    while (curr != nullptr) {
      Node* next = curr->next_->get_ptr(std::memory_order_acquire);
      delete curr;
      curr = next;
    }
  }

  /**
   * Adds an item to the list if it's not already present
   *
   * Thread-safe and lock-free. Uses optimistic traversal with retry on failure.
   *
   * @param item The item to add
   * @return true if the item was added, false if it already exists
   */
  auto add(const T& item) -> bool {
    size_t key = get_hash_value(item);
    while (true) {
      // Find insertion point - returns a pair of nodes (pred, curr) where
      // pred->key < key <= curr->key and neither is logically deleted
      auto [pred, curr] = find(head_, key);

      // If key already exists, return false
      if (curr != tail_ && curr->key_ == key) {
        return false;
      }

      // Create new node with pointers to the current node
      auto node = new Node(key, item);
      node->next_ = std::make_unique<AtomicMarkablePtr<Node>>(curr, false);

      // Try to insert new node between pred and curr
      // This will fail if another thread modified this region of the list
      if (pred->next_->compare_and_swap(curr, node, false, false,
                                        std::memory_order_release,
                                        std::memory_order_relaxed)) {
        // Succeed only if pred is unmarked and still points to curr
        return true;
      }

      // CAS failed - retry from beginning
      // Note: We must delete the created node to avoid memory leak
      delete node;
    }
  }

  /**
   * Removes an item from the list
   *
   * Thread-safe and lock-free. Uses a two-phase removal approach:
   * 1. Logical removal: Mark the next pointer of the node (doesn't change
   * structure)
   * 2. Physical removal: Update predecessor to skip the removed node
   *
   * @param item The item to remove
   * @return true if the item was found and removed, false otherwise
   */
  auto remove(const T& item) -> bool {
    size_t key = get_hash_value(item);
    while (true) {
      // Find the node and its predecessor
      auto [pred, curr] = find(head_, key);

      // If key not found, return false
      if (curr == tail_ || curr->key_ != key) {
        return false;
      }

      Node* succ = curr->next_->get_ptr(std::memory_order_acquire);

      // Phase 1: Logical removal - mark the next pointer
      // This logically removes the node without changing list structure
      if (!curr->next_->compare_and_swap(succ, succ, false, true,
                                         std::memory_order_relaxed)) {
        // Someone else modified curr's next pointer or curr's mark bit - retry
        continue;
      }

      // Phase 2: Physical removal - try to update pred to skip curr
      // Even if this fails, the node is already logically removed
      // A future traversal through find() will clean it up eventually
      if (pred->next_->compare_and_swap(curr, succ, false, false,
                                        std::memory_order_release,
                                        std::memory_order_relaxed)) {
        // Successfully (physically) remove curr from the list, so we insert
        // curr into the garbage list to clean up its memory later
        add_to_garbage(curr);
      }

      // Return true because node was at least logically removed
      return true;
    }
  }

  /**
   * Checks if an item exists in the list
   *
   * Thread-safe and wait-free (bounded number of steps).
   * Only returns true for nodes that are not marked for deletion.
   *
   * @param item The item to check for
   * @return true if the item exists and is not logically deleted
   */
  auto contains(const T& item) -> bool {
    size_t key = get_hash_value(item);
    Node* curr = head_->next_->get_ptr(std::memory_order_acquire);

    // Traverse until we find a node with a key >= our target
    while (curr->key_ < key) {
      curr = curr->next_->get_ptr(std::memory_order_acquire);
    }

    // Check if we found the exact key and it's not marked for deletion
    return (curr != tail_ && curr->key_ == key &&
            !curr->next_->is_marked(std::memory_order_acquire));
  }

 private:
  /**
   * Traverses the list to find a key, cleaning up logically deleted nodes along
   * the way
   *
   * This is the core helper function used by add() and remove(). It provides
   * two critical services:
   * 1. Finds the position where a key belongs in the sorted list
   * 2. Physically removes any logically deleted nodes encountered during
   * traversal
   *
   * @param head The starting node for traversal
   * @param key The key to find
   * @return A pair of adjacent nodes (pred, curr) where pred->key < key <=
   * curr->key and neither node is logically deleted
   */
  auto find(Node* head, size_t key) -> std::pair<Node*, Node*> {
  retry:
    Node* pred = head;
    Node* curr = pred->next_->get_ptr(std::memory_order_relaxed);

    // Traverse the list indefinitely until we find the right spot
    while (true) {
      // Get the successor node and curr's marked status from curr's next ptr
      auto [succ, marked] = curr->next_->get(std::memory_order_acquire);

      // If curr is marked, it's logically removed; clean it up
      while (marked) {
        // Attempt to physically remove curr by updating pred->next to succ
        // - Expected: pred->next points to curr and pred is unmarked
        // - Desired: pred->next points to succ and pred remains unmarked
        if (pred->next_->compare_and_swap(curr, succ, false, false,
                                          std::memory_order_release,
                                          std::memory_order_relaxed)) {
          // Successfully (physically) remove curr from the list, so we insert
          // curr into the garbage list to clean up its memory later
          add_to_garbage(curr);
        } else {
          // CAS failed because:
          // 1. Another thread logically removed pred by setting marked bit in
          // pred->next
          // 2. Another thread physically removed curr by changing pred->next
          // Restart traversal from head to ensure we're working with a
          // consistent state
          goto retry;
        }

        // Successfully removed curr; advance to succ and check its next
        curr = succ;
        std::tie(succ, marked) = curr->next_->get(std::memory_order_acquire);
      }

      // At this point, curr is unmarked (not logically removed).
      // Check if we've reached or passed the target key
      if (curr->key_ >= key) {
        // Found the spot: pred->key_ < key <= curr->key_
        // Both pred and curr are unmarked, safe for add/remove
        return {pred, curr};
      }

      // Key not found yet; move forward in the list
      pred = curr;
      curr = succ;
    }
  }

  /**
   * Adds a node to the garbage list for deferred deletion
   *
   * Uses a lock-free approach to add to the garbage list, continuously
   * retrying if the head of the garbage list changes during the operation.
   *
   * @param node The node to add to the garbage list
   */
  void add_to_garbage(Node* node) {
    node->next_deleted_ = garbage_list_.load(std::memory_order_relaxed);
    while (!garbage_list_.compare_exchange_weak(node->next_deleted_, node,
                                                std::memory_order_relaxed)) {}
  }

  /**
   * Computes the hash value for an item
   *
   * @param item The item to hash
   * @return The hash value as a size_t
   */
  auto get_hash_value(const T& item) const noexcept -> size_t {
    return hash_fn_(item);
  }

  Node* head_{};    // Pointer to the head sentinel node
  Node* tail_{};    // Pointer to the tail sentinel node
  Hash hash_fn_{};  // Hash function to generate keys from items
  std::atomic<Node*> garbage_list_{
      nullptr};  // List of logically deleted nodes for deferred deletion
};

#endif  // LOCK_FREE_LIST_H_
