#ifndef FINE_LIST_H_
#define FINE_LIST_H_

#include <limits>
#include <optional>

#include "synchronization/ttas_lock.h"

/**
 * @brief A thread-safe linked list implementation using fine-grained locking.
 *
 * This class implements a sorted linked list with per-node locking,
 * allowing multiple threads to operate on different parts of the list
 * concurrently. Each node has its own lock, enabling higher concurrency
 * compared to a coarse-grained locking approach.
 *
 * @tparam T The type of elements stored in the list
 * @tparam Hash A hash functor type used to compute hash values for elements
 */
template<typename T, typename Hash = std::hash<T>>
class FineList {
  /**
   * @brief Node structure with its own lock for fine-grained synchronization
   */
  struct Node {
    size_t key_{};           // Hash key for the item
    std::optional<T> item_;  // The actual data stored (optional because
                             // sentinel nodes don't store data)
    Node* next_{nullptr};    // Pointer to next node in list
    TTASLock mutex_;  // Per-node lock for fine-grained concurrency control

    Node(size_t key) : key_(key) {}

    Node(size_t key, const T& item) : key_(key), item_(item) {}

    auto lock() -> void { mutex_.lock(); }

    auto unlock() -> void { mutex_.unlock(); }
  };

 public:
  /**
   * @brief Constructor initializes a list with sentinel head and tail nodes.
   *
   * Creates a list with two sentinel nodes:
   * - Head node with minimum possible key value
   * - Tail node with maximum possible key value
   * This simplifies boundary condition handling in the algorithms.
   */
  FineList() {
    head_ = new Node(std::numeric_limits<size_t>::min());
    tail_ = new Node(std::numeric_limits<size_t>::max());
    head_->next_ = tail_;
  }

  /**
   * @brief Copy constructor deleted to prevent copying
   */
  FineList(const FineList<T, Hash>&) = delete;

  /**
   * @brief Assignment operator deleted to prevent copying
   */
  auto operator=(const FineList<T, Hash>&) -> FineList<T, Hash>& = delete;

  /**
   * @brief Destructor that safely deletes all nodes with proper locking
   *
   * Traverses the list, locking each node before deletion to ensure
   * thread safety during cleanup. This prevents potential race conditions
   * if threads are still accessing the list during destruction.
   */
  ~FineList() {
    // Note that the destructor is not thread-safe (i.e., it does not acquire a
    // mutex), so the caller should guarantee that no threads can access the
    // data structure at the time the destructor is invoked.
    Node* node = head_;
    while (node != nullptr) {
      Node* next = node->next_;
      delete node;
      node = next;
    }
  }

  /**
   * @brief Adds an item to the list if it doesn't already exist
   *
   * This method acquires locks in ascending key order (hand-over-hand locking)
   * through the search method, then releases them after the operation.
   * The search method will lock both the predecessor and current node when it
   * returns.
   *
   * @param item The item to add
   * @return true if the item was added, false if it already exists
   */
  auto add(const T& item) -> bool {
    size_t key = get_hash_value(item);
    Node* pred;
    bool key_exists =
        search(key, pred);  // After this call, pred and pred->next_ are locked
    Node* curr = pred->next_;

    if (!key_exists) {
      Node* node = new Node(key, item);
      node->next_ = curr;
      pred->next_ = node;
    }

    // Unlock nodes before returning
    curr->unlock();
    pred->unlock();

    return !key_exists;
  }

  /**
   * @brief Removes an item from the list if it exists
   *
   * This method acquires locks in ascending key order (hand-over-hand locking)
   * through the search method, then releases them after the operation.
   * The search method will lock both the predecessor and current node when it
   * returns.
   *
   * @param item The item to remove
   * @return true if the item was removed, false if it wasn't found
   */
  auto remove(const T& item) -> bool {
    size_t key = get_hash_value(item);
    Node* pred;
    bool key_exists =
        search(key, pred);  // After this call, pred and pred->next_ are locked
    Node* curr = pred->next_;

    if (key_exists) {
      pred->next_ = curr->next_;
      curr->unlock();
      delete curr;
    } else {
      curr->unlock();
    }

    // Unlock predecessor node before returning
    pred->unlock();

    return key_exists;
  }

  /**
   * @brief Checks if an item exists in the list
   *
   * This method acquires locks in ascending key order (hand-over-hand locking)
   * through the search method, then releases them after checking.
   * The search method will lock both the predecessor and current node when it
   * returns.
   *
   * @param item The item to check for
   * @return true if the item exists, false otherwise
   */
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
   * @brief Searches for a key in the list using hand-over-hand locking
   *
   * This method implements the hand-over-hand (or lock coupling) technique:
   * 1. Locks a node
   * 2. Locks its successor
   * 3. If we need to continue searching, unlocks the first node and advances
   *
   * IMPORTANT: When this method returns, both pred and pred->next_ nodes are
   * locked. The caller is responsible for unlocking these nodes when done.
   *
   * @param key The key to search for
   * @param[out] pred Reference to a pointer that will point to the predecessor
   * node
   * @return true if the key was found, false otherwise
   */
  auto search(size_t key, Node*& pred) -> bool {
    head_->lock();
    pred = head_;
    Node* curr = pred->next_;
    curr->lock();

    while (curr->key_ < key) {
      pred->unlock();
      pred = curr;
      curr = curr->next_;
      curr->lock();
    }

    return curr != tail_ && curr->key_ == key;
  }

  /**
   * @brief Computes the hash value for an item
   *
   * @param item The item to hash
   * @return size_t Hash value for the item
   */
  auto get_hash_value(const T& item) const noexcept -> size_t {
    return hash_fn_(item);
  }

  Node* head_{nullptr};  // Pointer to the sentinel head node
  Node* tail_{nullptr};  // Pointer to the tail head node
  Hash hash_fn_{};       // Hash function object
};

#endif  // FINE_LIST_H_
