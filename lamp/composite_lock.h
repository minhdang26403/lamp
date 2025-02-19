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
    std::atomic<State> state_{
        FREE};  // Synchronize updates on QNode through this field.
    QNode* pred_{nullptr};
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
    my_node_->state_.store(RELEASED);
    my_node_ = nullptr;
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
      if (node->state_.compare_exchange_strong(state, WAITING)) {
        return node;
      }

      uint64_t stamp;
      QNode* cur_tail = tail_.get(stamp);
      // We may clean up a node if its state is ABORTED or RELEASED.
      if (state == ABORTED || state == RELEASED) {
        // Only clean up the last queue node to avoid synchronization
        // conflicts with other threads.
        if (node == cur_tail) {
          QNode* my_pred = nullptr;
          if (state == ABORTED) {
            // If the node's state is ABORTED, it may have a predecessor. If its
            // state is RELEASED, this node is the only node in the queue since
            // all nodes before it must be FREE (so that this node can go from
            // WAITING to RELEASED), so let `my_pred` be nullptr in that case.
            my_pred = node->pred_;
          }
          if (tail_.compare_and_set(cur_tail, my_pred, stamp, stamp + 1)) {
            node->state_.store(WAITING);
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
        node->state_.store(FREE);
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

    State pred_state = pred->state_.load();
    while (pred_state != RELEASED) {
      if (pred_state == ABORTED) {
        // The predecessor aborted, so wait on predecessor's predecessor
        // instead.
        QNode* temp = pred;
        // IMPORTANT: We must read the `pred_` field before publishing our state
        // as FREE to other threads.
        pred = pred->pred_;
        temp->state_.store(FREE);
      }

      if (timeout(start, timeout_duration)) {
        // Set the predecessor pointer so that later nodes can skip waiting on
        // this node.
        node->pred_ = pred;
        node->state_.store(ABORTED);
        throw TimeoutException(
            "Thread times out while waiting for predecessor to release the "
            "lock");
      }
      pred_state = pred->state_.load();
    }

    pred->state_.store(FREE);
    my_node_ = node;
    return;
  }

  const size_t kSize;
  const int64_t kMinDelay;
  const int64_t kMaxDelay;

  StampedReference<QNode> tail_;
  std::vector<QNode> waiting_;
  static thread_local QNode* my_node_;
};

#endif  // COMPOSITE_LOCK_H_
