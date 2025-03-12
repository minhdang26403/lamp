#ifndef BACKOFF_LOCK_H_
#define BACKOFF_LOCK_H_

#include <atomic>
#include <chrono>

#include "lock/lock.h"
#include "util/backoff.h"

/**
 * @brief A test-and-test-and-set lock with an exponential backoff mechanism.
 */
template<typename Duration = std::chrono::microseconds>
class BackoffLock : public Lock {
 public:
  BackoffLock() = default;

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
  // Default backoff duration ranges from 5ms - 25ms
  const int64_t kMinDelay{5};
  const int64_t kMaxDelay{25};
};

#endif  // BACKOFF_LOCK_H_