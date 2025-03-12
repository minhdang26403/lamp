#ifndef FIFO_READ_WRITE_LOCK_H_
#define FIFO_READ_WRITE_LOCK_H_

#include "lock/condition_variable.h"
#include "lock/ttas_lock.h"

class FIFOReadWriteLock {
 public:
  auto read_lock() -> void {
    mutex_.lock();
    while (has_writer_) {
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
    if (to_notify) {
      cv_.notify_all();
    }
  }

  auto write_lock() -> void {
    mutex_.lock();
    while (has_writer_) {
      cv_.wait(mutex_);
    }
    has_writer_ = true;
    while (num_readers_ > 0) {
      cv_.wait(mutex_);
    }
    mutex_.unlock();
  }

  auto write_unlock() -> void {
    mutex_.lock();
    has_writer_ = false;
    mutex_.unlock();
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
