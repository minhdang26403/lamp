#ifndef COMPOSITE_LOCK_H_
#define COMPOSITE_LOCK_H_

#include <atomic>
#include <chrono>
#include <vector>

#include "util/atomic_stamped_ptr.h"
#include "util/backoff.h"
#include "util/common.h"

template<typename Duration>
using TimePoint = std::chrono::time_point<std::chrono::steady_clock, Duration>;

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
    size_t index = get_random_int<size_t>(0, kSize - 1);
    QNode* node = &waiting_[index];
    Backoff<Duration> backoff{kMinDelay, kMaxDelay};

    while (true) {
      State state = FREE;
      // Try to acquire the node.
      if (node->state_.compare_exchange_strong(state, WAITING,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire)) {
        return node;
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
      std::tie(cur_tail, stamp) = tail_.get(std::memory_order_acquire);
      if (timeout(start, timeout_duration)) {
        node->state_.store(FREE, std::memory_order_release);
        throw TimeoutException(
            "Thread times out while trying to splice the acquired node into "
            "the waiting queue");
      }
    } while (!tail_.compare_and_swap(cur_tail, node, stamp, stamp + 1,
                                     std::memory_order_release,
                                     std::memory_order_relaxed));
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

    State pred_state = pred->state_.load(std::memory_order_acquire);
    while (pred_state != RELEASED) {
      if (pred_state == ABORTED) {
        QNode* next_pred = pred->pred_.load(std::memory_order_relaxed);
        pred->state_.store(FREE, std::memory_order_release);
        pred = next_pred;
      }

      if (timeout(start, timeout_duration)) {
        node->pred_.store(pred, std::memory_order_relaxed);
        node->state_.store(ABORTED, std::memory_order_release);
        throw TimeoutException("Thread timed out waiting for predecessor");
      }

      pred_state = pred->state_.load(std::memory_order_acquire);
    }

    pred->state_.store(FREE, std::memory_order_release);
    my_node_ = node;
    return;
  }

  const size_t kSize;
  const int64_t kMinDelay;
  const int64_t kMaxDelay;

  AtomicStampedPtr<QNode> tail_;
  std::vector<QNode> waiting_;
  static thread_local QNode* my_node_;
};

#endif  // COMPOSITE_LOCK_H_
