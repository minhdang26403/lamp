#ifndef COMPOSITE_LOCK_H_
#define COMPOSITE_LOCK_H_

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <vector>

#include "backoff.h"
#include "stamped_reference.h"

template<typename Duration>
using TimePoint = std::chrono::time_point<std::chrono::steady_clock, Duration>;

class TimeoutException : public std::runtime_error {
 public:
  TimeoutException(const std::string& what_arg)
      : std::runtime_error(std::format("TimeoutException: {}", what_arg)) {}
};

class CompositeLock {
  // - FREE: the node is available for threads to acquire.
  // - WAITING: a WAITING node is linked into the queue, and the owning thread
  // is either in the critical section or waiting to enter.
  // - RELEASED: a node becomes RELEASED when its owner leaves the critical
  // section and releases the lock.
  // - ABORTED: a node becomes ABORTED when a thread abandons its attempt to
  // acquire the lock after enqueueing its node into the queue.
  enum State { FREE, WAITING, RELEASED, ABORTED };

  struct QNode {
    std::atomic<State> state_{FREE};
    std::atomic<QNode*> pred_{nullptr};
  };

 public:
  CompositeLock(size_t size, int64_t min_delay, int64_t max_delay)
      : kSize(size),
        kMinDelay(min_delay),
        kMaxDelay(max_delay),
        tail_(nullptr, 0),
        waiting_(kSize) {}

  template<typename Duration>
  auto try_lock(const Duration& timeout_duration) -> bool {
    try {
      auto start = std::chrono::time_point_cast<Duration>(
          std::chrono::steady_clock::now());
      // Acquires a node in the waiting array.
      QNode* node = acquire_qnode(start, timeout_duration);
      // Enqueues that node in the queue.
      QNode* pred = splice_qnode(node, start, timeout_duration);
      // Waits until that node is at the head of the queue.
      wait_for_predecessor(pred, node, start, timeout_duration);
      return true;
    } catch (const TimeoutException&) {
      return false;
    }
  }

  auto unlock() -> void {
    if (my_node_ != nullptr) {
      my_node_->state_.store(RELEASED, std::memory_order_release);
      my_node_ = nullptr;
    }
  }

 private:
  template<typename Duration>
  static auto timeout(const TimePoint<Duration>& start,
                      const Duration& timeout_duration) -> bool {
    return std::chrono::steady_clock::now() - start > timeout_duration;
  }

  template<typename Duration>
  auto acquire_qnode(const TimePoint<Duration>& start,
                     const Duration& timeout_duration) -> QNode* {
    QNode* node = &waiting_[get_random_int<size_t>(0, kSize - 1)];
    Backoff<Duration> backoff{kMinDelay, kMaxDelay};

    while (true) {
      State state = FREE;
      // Try to acquire the node.
      if (node->state_.compare_exchange_strong(state, WAITING,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire)) {
        return node;
      }

      // We may clean up a node if its state is ABORTED or RELEASED.
      if (state == ABORTED || state == RELEASED) {
        uint64_t stamp;
        QNode* cur_tail = tail_.get(stamp);
        // Only clean up the last queue node to avoid synchronization
        // conflicts with other threads.
        if (node == cur_tail) {
          // If the node's state is ABORTED, it may have a predecessor. If its
          // state is RELEASED, this node is the only node in the queue since
          // all nodes before it must be FREE (so that this node can go from
          // WAITING to RELEASED), so let `my_pred` be nullptr in that case.
          QNode* my_pred = (state == ABORTED)
                               ? node->pred_.load(std::memory_order_relaxed)
                               : nullptr;
          if (tail_.compare_and_set(cur_tail, my_pred, stamp, stamp + 1)) {
            node->state_.store(WAITING, std::memory_order_release);
            return node;
          }
        }
      }

      // Backoff and retries if the allocated node is WAITING.
      backoff.backoff();
      if (timeout(start, timeout_duration)) {
        throw TimeoutException(
            "Thread times out while trying to acquire a node");
      }
    }
  }

  template<typename Duration>
  auto splice_qnode(QNode* node, const TimePoint<Duration>& start,
                    const Duration& timeout_duration) -> QNode* {
    QNode* cur_tail;
    uint64_t stamp;
    // Repeatedly trying to enqueue a node into the waiting queue.
    do {
      cur_tail = tail_.get(stamp);
      if (timeout(start, timeout_duration)) {
        node->state_.store(FREE, std::memory_order_release);
        throw TimeoutException(
            "Thread times out while trying to splice the acquired node into "
            "the waiting queue");
      }
    } while (!tail_.compare_and_set(cur_tail, node, stamp, stamp + 1));
    return cur_tail;
  }

  template<typename Duration>
  auto wait_for_predecessor(QNode* pred, QNode* node,
                            const TimePoint<Duration>& start,
                            const Duration& timeout_duration) -> void {
    if (pred == nullptr) {
      // The thread's node is first in the queue, so it can enter the critical
      // section. Saves the node in the thread-local `my_node_` variable for the
      // `unlock` method to use.
      my_node_ = node;
      return;
    }

    while (true) {
      State pred_state = pred->state_.load(std::memory_order_acquire);
      while (pred_state != RELEASED) {
        if (pred_state == ABORTED) {
          QNode* temp = pred;
          QNode* next_pred = temp->pred_.load(std::memory_order_relaxed);
          if (temp->state_.compare_exchange_strong(pred_state, FREE,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_relaxed)) {
            pred = next_pred;
          }
        }

        if (timeout(start, timeout_duration)) {
          node->pred_.store(pred, std::memory_order_relaxed);
          node->state_.store(ABORTED, std::memory_order_release);
          throw TimeoutException("Thread timed out waiting for predecessor");
        }
        pred_state = pred->state_.load(std::memory_order_acquire);
      }

      // Only one thread claims the lock by freeing pred
      State expected = RELEASED;
      if (pred->state_.compare_exchange_strong(expected, FREE,
                                               std::memory_order_acq_rel,
                                               std::memory_order_relaxed)) {
        // Verify node is at head
        uint64_t stamp;
        QNode* cur_tail = tail_.get(stamp);
        if (cur_tail == node) {
          my_node_ = node;
          return;
        }
        // If not at tail, retry with updated pred
        pred = cur_tail ? cur_tail->pred_.load(std::memory_order_relaxed)
                        : nullptr;
        if (pred == nullptr) {
          my_node_ = node;
          return;
        }
        continue;
      }

      // If CAS fails, another thread claimed it; update pred and retry
      if (timeout(start, timeout_duration)) {
        node->pred_.store(pred, std::memory_order_relaxed);
        node->state_.store(ABORTED, std::memory_order_release);
        throw TimeoutException("Predecessor released but not at head");
      }
      uint64_t stamp;
      QNode* new_tail = tail_.get(stamp);
      pred = (new_tail == node || new_tail == nullptr)
                 ? nullptr
                 : new_tail->pred_.load(std::memory_order_relaxed);
      if (pred == nullptr) {
        my_node_ = node;
        return;
      }
    }
  }

  const size_t kSize;
  const int64_t kMinDelay;
  const int64_t kMaxDelay;

  StampedReference<QNode> tail_;
  std::vector<QNode> waiting_;
  static thread_local QNode* my_node_;
};

#endif  // COMPOSITE_LOCK_H_
