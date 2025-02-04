#ifndef TAS_LOCK_H_
#define TAS_LOCK_H_

#include <atomic>

#include "lock.h"

/**
 * @brief Test-and-set lock
 *
 */
class TASLock : public Lock {
 public:
  TASLock() {}

  auto lock() -> void override {
    while (state.test_and_set(std::memory_order_acquire)) {}
  }

  auto unlock() -> void override { state.clear(std::memory_order_release); }

 private:
  std::atomic_flag state{false};
};

#endif  // TAS_LOCK_H_
