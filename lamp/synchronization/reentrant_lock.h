#ifndef REENTRANT_LOCK_H_
#define REENTRANT_LOCK_H_

#include "synchronization/condition_variable.h"
#include "synchronization/ttas_lock.h"
#include "synchronization/scoped_lock.h"

#include <stdexcept>
#include <thread>

class ReentrantLock {
 public:
  auto lock() -> void {
    auto me = std::this_thread::get_id();
    ScopedLock<TTASLock> lk(mutex_);
    if (owner_ == me) {
      hold_count_++;
      return;
    }
    while (hold_count_ != 0) {
      cv_.wait(mutex_);
    }
    owner_ = me;
    hold_count_ = 1;
  }

  auto unlock() -> void {
    ScopedLock<TTASLock> lk(mutex_);
    if (hold_count_ == 0 || owner_ != std::this_thread::get_id()) {
      throw std::runtime_error("The caller does not hold the lock");
    }
    hold_count_--;
    if (hold_count_ == 0) {
      cv_.notify_all();
    }
  }

 private:
  std::thread::id owner_{};
  uint64_t hold_count_{};

  TTASLock mutex_;
  ConditionVariable cv_;
};

#endif  // REENTRANT_LOCK_H_
