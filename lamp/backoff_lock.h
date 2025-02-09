#ifndef BACKOFF_LOCK_H_
#define BACKOFF_LOCK_H_

#include <atomic>
#include <chrono>

#include "backoff.h"
#include "lock.h"

/**
 * @brief A test-and-test-and-set lock with an exponential backoff mechanism.
 */
class BackoffLock : public Lock {
 public:
  auto lock() -> void override {
    Backoff backoff{MIN_DELAY, MAX_DELAY};
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
  /* Minumum and maximum delay duration for backoff (in microseconds) */
  static constexpr int64_t MIN_DELAY = 1;
  static constexpr int64_t MAX_DELAY = 100;
};

#endif  // BACKOFF_LOCK_H_