#ifndef SEMAPHORE_H_
#define SEMAPHORE_H_

#include <chrono>

#include "lock/condition_variable.h"
#include "lock/scoped_lock.h"
#include "lock/ttas_lock.h"

class Semaphore {
 public:
  Semaphore(int value) : value_(value) {}

  auto acquire() -> void {
    ScopedLock<TTASLock> lk(mutex_);
    while (value_ == 0) {
      cv_.wait(mutex_);
    }
    value_--;
  }

  auto release() -> void {
    ScopedLock<TTASLock> lk(mutex_);
    value_++;
    cv_.notify_all();
  }

  // Try to acquire without blocking, returns true if successful
  auto try_acquire() -> bool {
    ScopedLock<TTASLock> lk(mutex_);
    if (value_ > 0) {
      value_--;
      return true;
    }
    return false;
  }

  // Try to acquire with timeout, returns true if successful
  template<typename Rep, typename Period>
  auto try_acquire_for(const std::chrono::duration<Rep, Period>& timeout)
      -> bool {
    auto end_time = std::chrono::steady_clock::now() + timeout;
    ScopedLock<TTASLock> lk(mutex_);

    while (value_ == 0) {
      auto now = std::chrono::steady_clock::now();
      if (now >= end_time) {
        return false;
      }

      auto remaining_time = end_time - now;
      if (cv_.wait_for(mutex_, remaining_time) == CVStatus::kTimeout) {
        return false;  // Timeout
      }
    }

    value_--;
    return true;
  }

  // Get current value (for testing and debugging)
  auto get_value() const -> int {
    ScopedLock<TTASLock> lk(mutex_);
    return value_;
  }

  // Release multiple resources at once
  auto release(int count) -> void {
    if (count <= 0) {
      return;
    }

    ScopedLock<TTASLock> lk(mutex_);
    value_ += count;
    cv_.notify_all();
  }

  // Try to acquire multiple resources at once
  auto try_acquire(int count) -> bool {
    if (count <= 0)
      return true;

    ScopedLock<TTASLock> lk(mutex_);
    if (value_ >= count) {
      value_ -= count;
      return true;
    }
    return false;
  }

 private:
  int value_;
  mutable TTASLock mutex_;
  ConditionVariable cv_;
};

#endif  // SEMAPHORE_H_
