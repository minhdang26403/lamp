#ifndef LAZY_LIST_H_
#define LAZY_LIST_H_

#include <atomic>
#include <limits>
#include <optional>

#include "synchronization/ttas_lock.h"

template<typename T, typename Hash = std::hash<T>>
class LazyList {
  struct Node {
    size_t key_;
    std::optional<T> item_;
    Node* next_{nullptr};
    bool marked_{false};
    TTASLock mutex_;

    Node(size_t key) : key_(key) {}

    Node(size_t key, const T& item) : key_(key), item_(item) {}

    auto lock() -> void { mutex_.lock(); }

    auto unlock() -> void { mutex_.unlock(); }
  };

 public:
  LazyList() {
    size_t min_key = std::numeric_limits<size_t>::min();
    size_t max_key = std::numeric_limits<size_t>::max();
    head_ = new Node(min_key);
    head_->next_ = new Node(max_key);
    garbage_list_.store(new Node(max_key), std::memory_order_relaxed);
  }

  LazyList(const LazyList<T, Hash>&) = delete;
  auto operator=(const LazyList<T, Hash>&) -> LazyList<T, Hash>& = delete;

  ~LazyList() {
    // Note that the destructor is not thread-safe (i.e., it does not acquire a
    // mutex), so the caller should guarantee that no threads can access the
    // data structure at the time the destructor is invoked.

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

  auto add(const T& item) -> bool {
    size_t key = get_hash_value(item);
    Node* pred;
    bool key_exists = search(key, pred);
    Node* curr = pred->next_;

    if (!key_exists) {
      Node* node = new Node(key, item);
      node->next_ = curr;
      std::atomic_thread_fence(std::memory_order_release);
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
      curr->marked_ = true;
      pred->next_ = curr->next_;

      curr->next_ = garbage_list_.load(std::memory_order_relaxed);
      while (!garbage_list_.compare_exchange_weak(curr->next_, curr,
                                                  std::memory_order_relaxed)) {}
    }

    curr->unlock();
    pred->unlock();

    return key_exists;
  }

  auto contains(const T& item) -> bool {
    size_t key = get_hash_value(item);
    Node* curr = head_;
    while (curr->key_ < key) {
      curr = curr->next_;
    }

    return curr->key_ == key && !curr->marked_;
  }

 private:
  auto search(size_t key, Node*& pred) -> bool {
    while (true) {
      pred = head_;
      Node* curr = pred->next_;

      while (curr->key_ < key) {
        pred = curr;
        curr = curr->next_;
      }

      pred->lock();
      curr->lock();

      if (validate(pred, curr)) {
        return curr->key_ == key;
      }

      pred->unlock();
      curr->unlock();
    }
  }

  auto validate(Node* pred, Node* curr) const noexcept -> bool {
    return !pred->marked_ && !curr->marked_ && pred->next_ == curr;
  }

  auto get_hash_value(const T& item) const noexcept -> size_t {
    return hash_fn_(item) + 1;
  }

  Node* head_;
  Hash hash_fn_{};
  std::atomic<Node*>
      garbage_list_{};  // Lock-free list of logically deleted nodes
};

#endif  // LAZY_LIST_H_
