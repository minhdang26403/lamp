#ifndef FIFO_READ_WRITE_LOCK_H_
#define FIFO_READ_WRITE_LOCK_H_

#include "lock/condition_variable.h"
#include "lock/scoped_lock.h"
#include "lock/ttas_lock.h"

class FIFOReadWriteLock {
 public:
  auto read_lock() -> void {
    ScopedLock<TTASLock> lk(mutex_);
    while (has_writer_) {
      cv_.wait(mutex_);
    }
    num_readers_++;
  }

  auto read_unlock() -> void {
    ScopedLock<TTASLock> lk(mutex_);
    num_readers_--;
    if (num_readers_ == 0) {
      cv_.notify_all();
    }
  }

  auto write_lock() -> void {
    ScopedLock<TTASLock> lk(mutex_);
    while (has_writer_) {
      cv_.wait(mutex_);
    }
    has_writer_ = true;
    while (num_readers_ > 0) {
      cv_.wait(mutex_);
    }
  }

  auto write_unlock() -> void {
    ScopedLock<TTASLock> lk(mutex_);
    has_writer_ = false;
    cv_.notify_all();
  }

 private:
  uint64_t num_readers_{0};  // number of readers that have acquired the lock
  bool has_writer_{false};  // true if there is writer that tries to acquire the
                            // lock or has already acquired the lock
  TTASLock mutex_;
  ConditionVariable cv_;
};

#endif  // FIFO_READ_WRITE_LOCK_H_
