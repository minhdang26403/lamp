#ifndef SYNCHRONOUS_QUEUE_H_
#define SYNCHRONOUS_QUEUE_H_

#include <optional>
#include "synchronization/condition_variable.h"
#include "synchronization/scoped_lock.h"
#include "synchronization/ttas_lock.h"

template<typename T>
class SynchronousQueue {
 public:
  auto enqueue(T value) -> void {
    ScopedLock<TTASLock> lock(mutex_);
    while (enqueuing_) {
      cv_.wait(mutex_);
    }
    enqueuing_ = true;
    item_ = value;
    cv_.notify_all();
    while (item_.has_value()) {
      cv_.wait(mutex_);
    }
    enqueuing_ = false;
    cv_.notify_all();
  }

  auto dequeue() -> T {
    ScopedLock<TTASLock> lock(mutex_);
    while (!item_.has_value()) {
      cv_.wait(mutex_);
    }
    T t = std::move(item_.value());
    item_ = std::nullopt;
    cv_.notify_all();
    return t;
  };

private:
  std::optional<T> item_;
  bool enqueuing_{false};
  TTASLock mutex_;
  ConditionVariable cv_;
};

#endif // SYNCHRONOUS_QUEUE_H_