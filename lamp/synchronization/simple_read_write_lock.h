#ifndef SIMPLE_READ_WRITE_LOCK_H_
#define SIMPLE_READ_WRITE_LOCK_H_

#include "synchronization/condition_variable.h"
#include "synchronization/scoped_lock.h"
#include "synchronization/ttas_lock.h"

class SimpleReadWriteLock {
 public:
  auto read_lock() -> void {
    ScopedLock<TTASLock> lk(mutex_);
    while (writer_entered_) {
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
    while (num_readers_ > 0 || writer_entered_) {
      cv_.wait(mutex_);
    }
    writer_entered_ = true;
  }

  auto write_unlock() -> void {
    ScopedLock<TTASLock> lk(mutex_);
    writer_entered_ = false;
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
