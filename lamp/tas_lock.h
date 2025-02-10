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
  auto lock() -> void override {
    while (state_.test_and_set(std::memory_order_acquire)) {}
  }

  auto unlock() -> void override { state_.clear(std::memory_order_release); }

 private:
  std::atomic_flag state_{false};
};

#endif  // TAS_LOCK_H_
