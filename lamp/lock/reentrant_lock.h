#ifndef REENTRANT_LOCK_H_
#define REENTRANT_LOCK_H_

#include "lock/condition_variable.h"
#include "lock/ttas_lock.h"

#include <stdexcept>
#include <thread>

class ReentrantLock {
 public:
  auto lock() -> void {
    auto me = std::this_thread::get_id();
    mutex_.lock();
    if (owner_ == me) {
      hold_count_++;
      mutex_.unlock();
      return;
    }
    while (hold_count_ != 0) {
      cv_.wait(mutex_);
    }
    owner_ = me;
    hold_count_ = 1;
    mutex_.unlock();
  }

  auto unlock() -> void {
    bool to_notify = false;
    mutex_.lock();
    if (hold_count_ == 0 || owner_ != std::this_thread::get_id()) {
      mutex_.unlock();
      throw std::runtime_error("The caller does not hold the lock");
    }
    hold_count_--;
    if (hold_count_ == 0) {
      to_notify = true;
    }
    mutex_.unlock();
    if (to_notify) {
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
