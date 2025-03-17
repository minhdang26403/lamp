#ifndef COARSE_LIST_H_
#define COARSE_LIST_H_

#include <cstddef>
#include <limits>
#include <optional>

#include "synchronization/scoped_lock.h"
#include "synchronization/ttas_lock.h"

template<typename T, typename Hash = std::hash<T>>
class CoarseList {
  struct Node {
    size_t key_{};
    std::optional<T> item_;
    Node* next_{nullptr};

    Node(size_t key) : key_(key) {}

    Node(size_t key, const T& item) : key_(key), item_(item) {}
  };

 public:
  CoarseList() {
    head_ = new Node(std::numeric_limits<size_t>::min());
    head_->next_ = new Node(std::numeric_limits<size_t>::max());
  }

  CoarseList(const CoarseList<T, Hash>&) = delete;
  auto operator=(const CoarseList<T, Hash>&) -> CoarseList<T, Hash>& = delete;

  ~CoarseList() {
    ScopedLock<TTASLock> lock(mutex_);
    Node* pred = head_;
    while (pred != nullptr) {
      Node* curr = pred->next_;
      delete pred;
      pred = curr;
    }
  }

  auto add(const T& item) -> bool {
    size_t key = get_hash_value(item);
    ScopedLock<TTASLock> lk(mutex_);

    Node* pred;
    bool key_exists = search(key, pred);

    if (key_exists) {
      return false;
    }

    auto node = new Node(key, item);
    node->next_ = pred->next_;
    pred->next_ = node;
    return true;
  }

  auto remove(const T& item) -> bool {
    size_t key = get_hash_value(item);
    ScopedLock<TTASLock> lk(mutex_);

    Node* pred;
    bool key_exists = search(key, pred);

    if (!key_exists) {
      return false;
    }

    Node* node = pred->next_;
    pred->next_ = node->next_;
    delete node;
    return true;
  }

  auto contains(const T& item) -> bool {
    size_t key = get_hash_value(item);
    ScopedLock<TTASLock> lk(mutex_);
    Node* pred;
    return search(key, pred);
  }

 private:
  /**
   * @brief An internal method to search for a node with a given key. This
   * method assume the `mutex_` is already held.
   * @param key the key to search for
   * @param[out] pred a pointer to the predecessor the node that contains `key`.
   * @return true if the key exists; otherwise, false.
   */
  auto search(size_t key, Node*& pred) -> bool {
    pred = head_;
    Node* curr = pred->next_;

    while (curr->key_ < key) {
      pred = curr;
      curr = curr->next_;
    }

    return curr->key_ == key;
  }

  auto get_hash_value(const T& item) const noexcept -> size_t {
    return hash_fn_(item);
  }

  TTASLock mutex_;
  Node* head_{nullptr};
  Hash hash_fn_{};
};

#endif  // COARSE_LIST_H_
