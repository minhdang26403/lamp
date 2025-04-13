#ifndef ELIMINATION_BACKOFF_STACK_H_
#define ELIMINATION_BACKOFF_STACK_H_

#include <atomic>
#include <cassert>
#include <chrono>

#include "util/atomic_stamped_ptr.h"
#include "util/backoff.h"
#include "util/common.h"

template<typename T>
class LockFreeExchanger {
  enum State : uint64_t { EMPTY, WAITING, BUSY };

 public:
  LockFreeExchanger() : slot_(nullptr, EMPTY) {}

  template<typename Rep, typename Period>
  auto exchange(T* my_item,
                const std::chrono::duration<Rep, Period>& timeout_duration)
      -> T* {

    auto time_bound = get_current_time() + timeout_duration;
    while (true) {
      if (get_current_time() > time_bound) {
        throw TimeoutException("Thread waits too long to exchange value");
      }

      auto [your_item, stamp] = slot_.get(std::memory_order_acquire);
      switch (stamp) {
        case EMPTY:
          if (slot_.compare_and_swap(your_item, my_item, EMPTY, WAITING,
                                     std::memory_order_release,
                                     std::memory_order_relaxed)) {
            while (get_current_time() < time_bound) {
              auto [your_item, stamp] = slot_.get(std::memory_order_acquire);
              if (stamp == BUSY) {
                // Consume the item and reset the state to EMPTY. Resetting to
                // EMPTY can be done using a simple write because the waiting
                // thread is the only one that can change the state from BUSY to
                // EMPTY
                slot_.set(nullptr, EMPTY, std::memory_order_release);
                return your_item;
              }
            }
            if (slot_.compare_and_swap(my_item, nullptr, WAITING, EMPTY,
                                       std::memory_order_release,
                                       std::memory_order_relaxed)) {
              throw TimeoutException("Thread waits too long to exchange value");
            } else {
              auto your_item = slot_.get_ptr(std::memory_order_acquire);
              slot_.set(nullptr, EMPTY, std::memory_order_release);
              return your_item;
            }
          }
          break;
        case WAITING:
          if (slot_.compare_and_swap(your_item, my_item, WAITING, BUSY,
                                     std::memory_order_release,
                                     std::memory_order_relaxed)) {
            return your_item;
          }
          break;
        case BUSY:
          break;
        default:
          // Impossible
          break;
      }
    }
  }

 private:
  static auto get_current_time() { return std::chrono::steady_clock::now(); }

  AtomicStampedPtr<T> slot_;
};

template<typename T, typename Duration = std::chrono::microseconds>
class EliminationArray {
 public:
  EliminationArray(size_t capacity) : exchanger_(capacity) {}

  auto visit(T* value, int lower_limit, int upper_limit) -> T* {
    int slot = get_random_int<int>(lower_limit, upper_limit);
    return exchanger_[slot].exchange(value, duration_);
  }

  auto size() const noexcept -> size_t { return exchanger_.size(); }

 private:
  std::vector<LockFreeExchanger<T>> exchanger_;
  static constexpr Duration duration_{std::chrono::microseconds(50)};
};

template<typename T>
class EliminationBackoffStack {
  struct Node {
    T value_;
    Node* next_{nullptr};
    Node* next_deleted_{nullptr};

    Node(T value) : value_(std::move(value)) {}
  };

 public:
  EliminationBackoffStack(size_t capacity) : elimination_array_(capacity) {
    assert(capacity >= 1);
  }

  ~EliminationBackoffStack() {
    // Clean up in two phases to avoid memory leaks
    // Phase 1: Clean garbage list (nodes already removed from main queue)
    Node* curr = garbage_list_.load(std::memory_order_relaxed);
    while (curr != nullptr) {
      Node* next = curr->next_deleted_;
      delete curr;
      curr = next;
    }

    // Phase 2: Clean remaining nodes in queue including sentinel
    curr = top_.load(std::memory_order_relaxed);
    while (curr != nullptr) {
      Node* next = curr->next_;
      delete curr;
      curr = next;
    }
  }

  auto push(T value) -> void {
    auto node = new Node(std::move(value));
    while (true) {
      if (try_push(node)) {
        return;
      }

      try {
        const int max_index = elimination_array_.size() - 1;
        int lower_limit = get_random_int(0, max_index);
        int upper_limit = get_random_int(lower_limit, max_index);
        Node* other_node =
            elimination_array_.visit(node, lower_limit, upper_limit);
        if (other_node == nullptr) {
          return;  // exchanged with pop
        }
      } catch (const TimeoutException&) {}
    }
  }

  auto pop() -> T {
    while (true) {
      Node* return_node = try_pop();
      if (return_node != nullptr) {
        T value = std::move(return_node->value_);
        clean_up(return_node);
        return value;
      }

      try {
        const int max_index = elimination_array_.size() - 1;
        int lower_limit = get_random_int(0, max_index);
        int upper_limit = get_random_int(lower_limit, max_index);
        Node* other_node =
            elimination_array_.visit(nullptr, lower_limit, upper_limit);
        if (other_node != nullptr) {
          T value = std::move(other_node->value_);
          clean_up(other_node);
          return value;
        }
      } catch (const TimeoutException&) {}
    }
  }

 private:
  auto try_push(Node* node) -> bool {
    Node* old_top = top_.load(std::memory_order_acquire);
    node->next_ = old_top;
    return top_.compare_exchange_strong(
        old_top, node, std::memory_order_release, std::memory_order_relaxed);
  }

  auto try_pop() -> Node* {
    Node* old_top = top_.load(std::memory_order_acquire);
    if (old_top == nullptr) {
      throw EmptyException("Try to pop from an empty stack");
    }
    Node* new_top = old_top->next_;
    if (top_.compare_exchange_strong(old_top, new_top,
                                     std::memory_order_acquire,
                                     std::memory_order_relaxed)) {
      return old_top;
    }
    return nullptr;
  }

  auto clean_up(Node* node) noexcept -> void {
    // Chain the node into garbage list to clean up
    node->next_deleted_ = garbage_list_.load(std::memory_order_relaxed);
    while (!garbage_list_.compare_exchange_weak(node->next_deleted_, node,
                                                std::memory_order_relaxed)) {}
  }

  EliminationArray<Node> elimination_array_;
  std::atomic<Node*> top_{nullptr};
  std::atomic<Node*> garbage_list_{nullptr};
};

#endif  // ELIMINATION_BACKOFF_STACK_H_
