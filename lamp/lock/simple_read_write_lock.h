#ifndef SIMPLE_READ_WRITE_LOCK_H_
#define SIMPLE_READ_WRITE_LOCK_H_

#include "lock/condition_variable.h"
#include "lock/ttas_lock.h"

class SimpleReadWriteLock {
 public:
  auto read_lock() -> void {
    mutex_.lock();
    while (writer_entered_) {
      cv_.wait(mutex_);
    }
    num_readers_++;
    mutex_.unlock();
  }

  auto read_unlock() -> void {
    bool to_notify = false;
    mutex_.lock();
    num_readers_--;
    if (num_readers_ == 0) {
      to_notify = true;
    }
    mutex_.unlock();
    // unlock the mutex before waking waiting threads to reduce lock contention
    if (to_notify) {
      cv_.notify_all();
    }
  }

  auto write_lock() -> void {
    mutex_.lock();
    while (num_readers_ > 0 || writer_entered_) {
      cv_.wait(mutex_);
    }
    writer_entered_ = true;
    mutex_.unlock();
  }

  auto write_unlock() -> void {
    mutex_.lock();
    writer_entered_ = false;
    mutex_.unlock();
    cv_.notify_all();
  }

 private:
  uint64_t num_readers_{0};     // number of readers that have acquired the lock
  bool writer_entered_{false};  // true if there is a writer that has acquired
                                // the lock and entered the critical section
  TTASLock mutex_;
  ConditionVariable cv_;
};

#endif  // SIMPLE_READ_WRITE_LOCK_H_
