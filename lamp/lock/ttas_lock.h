#ifndef TTAS_LOCK_H_
#define TTAS_LOCK_H_

#include <atomic>

#include "lock/lock.h"

/**
 * @brief Test-and-test-and-set lock improves the performance of test-and-set
 * lock by only setting the flag when the thread notices the flag to be false.
 * This lock reduces load on memory bus and avoid cache ping pong.
 *
 */
class TTASLock : public Lock {
 public:
  auto lock() -> void override {
    while (true) {
      while (state_.test(std::memory_order_relaxed)) {}
      if (!state_.test_and_set(std::memory_order_acquire)) {
        return;
      }
    }
  }

  auto unlock() -> void override { state_.clear(std::memory_order_release); }

 private:
  std::atomic_flag state_{false};
};

#endif  // TTAS_LOCK_H_
