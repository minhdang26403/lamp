#ifndef BACKOFF_LOCK_H_
#define BACKOFF_LOCK_H_

#include <atomic>

#include "backoff.h"
#include "lock.h"

/**
 * @brief A test-and-test-and-set lock with an exponential backoff mechanism.
 */
template<typename Duration>
class BackoffLock : public Lock {
 public:
  BackoffLock(int64_t min_delay, int64_t max_delay)
      : kMinDelay(min_delay), kMaxDelay(max_delay) {}

  auto lock() -> void override {
    Backoff<Duration> backoff{kMinDelay, kMaxDelay};
    while (true) {
      while (state_.test(std::memory_order_relaxed)) {}
      if (!state_.test_and_set(std::memory_order_acquire)) {
        return;
      }
      backoff.backoff();
    }
  }

  auto unlock() -> void override { state_.clear(std::memory_order_release); }

 private:
  std::atomic_flag state_{false};
  const int64_t kMinDelay;
  const int64_t kMaxDelay;
};

#endif  // BACKOFF_LOCK_H_