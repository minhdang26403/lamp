#ifndef TAS_LOCK_H_
#define TAS_LOCK_H_

#include <atomic>

#include "lock.h"

/**
 * @brief Test-and-test-and-set lock improves the performance of test-and-set
 * lock by only setting the flag when the thread notices the flag to be false.
 * This lock reduces load on memory bus and avoid cache ping pong.
 *
 */
class TTASLock : public Lock {
 public:
  TTASLock() {}

  auto lock() -> void override {
    while (true) {
      while (state.test(std::memory_order_relaxed)) {}
      if (!state.test_and_set(std::memory_order_acquire)) {
        return;
      }
    }
  }

  auto unlock() -> void override { state.clear(std::memory_order_release); }

 private:
  std::atomic_flag state{false};
};

#endif  // TAS_LOCK_H_
