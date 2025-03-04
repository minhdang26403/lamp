#ifndef TIMEOUT_LOCK_H_
#define TIMEOUT_LOCK_H_

#include <atomic>
#include <chrono>

/**
 * @brief A queue lock based on the CLHLock class that supports wait-free
 * timeout even for threads in the middle of the list of nodes waiting for the
 * lock.
 */
class TOLock {
 private:
  struct QNode {
    // Pointer to the predecessor node in the queue:
    // - null: waiting for the lock.
    // - &AVAILABLE: release the lock.
    // - non-null: abandoned.
    std::atomic<QNode*> pred_{nullptr};
  };

 public:
  template<typename Rep, typename Period>
  auto try_lock(const std::chrono::duration<Rep, Period>& timeout_duration)
      -> bool {

    auto start = std::chrono::steady_clock::now();
    QNode* qnode = new QNode();
    // Save a reference to the node we created so that we can use it in
    // `unlock()`.
    my_node_ = qnode;
    QNode* my_pred = tail_.exchange(qnode, std::memory_order_acq_rel);

    if (my_pred == nullptr ||
        my_pred->pred_.load(std::memory_order_acquire) == &AVAILABLE) {
      // We are the only thread in the queue, or the predecessor thread has
      // already released the lock.
      return true;
    }

    // Spin while waiting for the lock, but stop if the timeout is reached.
    while (std::chrono::steady_clock::now() - start < timeout_duration) {
      QNode* pred_pred = my_pred->pred_.load(std::memory_order_acquire);
      if (pred_pred == &AVAILABLE) {
        // The predecessor thread has released the lock, so we can proceed.
        return true;
      } else if (pred_pred != nullptr) {
        // The predecessor thread has aborted, so we update our predecessor to
        // the predecessor's predecessor, effectively skipping the abandoned
        // node.
        my_pred = pred_pred;
      }
    }

    QNode* expected = qnode;
    // We stop trying to acquire the lock. If we are the last thread in the
    // queue, remove ourselves from the queue by setting the tail to our
    // predecessor (which could be null if no one else is waiting). Otherwise,
    // we must stay in the queue but in an abandoned state, allowing subsequent
    // threads to skip over us.
    if (!tail_.compare_exchange_strong(expected, my_pred,
                                       std::memory_order_acquire,
                                       std::memory_order_relaxed)) {
      qnode->pred_.store(
          my_pred, std::memory_order_relaxed);  // Mark ourselves as abandoned
    }

    return false;
  }

  auto unlock() -> void {
    QNode* qnode = my_node_;
    QNode* expected = qnode;
    // If this thread has no successor, set the tail to null. Otherwise, set its
    // predecessor to AVAILABLE to signal successor thread that we have released
    // the lock.
    if (!tail_.compare_exchange_strong(expected, nullptr,
                                       std::memory_order_release,
                                       std::memory_order_relaxed)) {
      qnode->pred_.store(const_cast<QNode*>(&AVAILABLE),
                        std::memory_order_release);
    }
  }

 private:
  std::atomic<QNode*> tail_{nullptr};
  static thread_local QNode* my_node_;
  static const QNode AVAILABLE;
};

#endif  // TIMEOUT_LOCK_H_