#ifndef CONDITION_VARIABLE_H_
#define CONDITION_VARIABLE_H_

#include <atomic>
#include <chrono>
#include <list>
#include <thread>

#include "lock/ttas_lock.h"

enum class CVStatus {
  kNoTimeout,
  kTimeout,
};

class ConditionVariable {
 public:
  ConditionVariable() = default;

  // Delete copy constructor and assignment operator
  ConditionVariable(const ConditionVariable&) = delete;
  ConditionVariable& operator=(const ConditionVariable&) = delete;

  ~ConditionVariable() {
    // Clean up any remaining waiters (should be rare/none in proper usage)
    waiters_lock_.lock();
    for (const auto* signal : waiters_) {
      delete signal;
    }
    waiters_lock_.unlock();
  }

  // Notify one waiting thread
  auto notify_one() -> void {
    std::atomic<bool>* signal_to_set = nullptr;

    // Get one waiter to notify
    waiters_lock_.lock();
    if (!waiters_.empty()) {
      signal_to_set = waiters_.front();
      waiters_.erase(waiters_.begin());
    }
    waiters_lock_.unlock();

    // Signal the waiter
    if (signal_to_set) {
      signal_to_set->store(true, std::memory_order_release);
    }
  }

  // Notify all waiting threads
  auto notify_all() -> void {
    std::vector<std::atomic<bool>*> to_notify;

    // Get all waiters to notify
    waiters_lock_.lock();
    to_notify.reserve(waiters_.size());
    while (!waiters_.empty()) {
      to_notify.push_back(waiters_.front());
      waiters_.erase(waiters_.begin());
    }
    waiters_lock_.unlock();

    // Signal all waiters
    for (auto* signal : to_notify) {
      signal->store(true, std::memory_order_release);
    }
  }

  // Wait for a notification
  template<typename Lock>
  auto wait(Lock& lock) -> void {
    // Create a signal for this waiter
    auto signal = new std::atomic<bool>(false);

    // Register this waiter
    waiters_lock_.lock();
    waiters_.push_back(signal);
    waiters_lock_.unlock();

    // Release the external lock before waiting
    lock.unlock();

    // Wait for the signal to be set
    while (!signal->load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }

    // Clean up
    delete signal;

    // Reacquire the lock before returning
    lock.lock();
  }

  // Wait for a notification with predicate
  template<typename Lock, typename Predicate>
  auto wait(Lock& lock, Predicate pred) -> void {
    while (!pred()) {
      wait(lock);
    }
  }

  // Wait for a notification with timeout
  template<typename Lock, typename Clock, typename Duration>
  auto wait_until(Lock& lock,
                  const std::chrono::time_point<Clock, Duration>& abs_time)
      -> CVStatus {

    // Create a signal for this waiter
    auto signal = new std::atomic<bool>(false);

    // Register this waiter
    waiters_lock_.lock();
    waiters_.push_back(signal);
    waiters_lock_.unlock();

    // Release the external lock before waiting
    lock.unlock();

    // Wait for the signal to be set or timeout
    bool signaled = false;
    while (!signaled && Clock::now() < abs_time) {
      signaled = signal->load(std::memory_order_acquire);
      if (!signaled) {
        std::this_thread::yield();
      }
    }

    // If not signaled, we need to unregister ourselves
    if (!signaled) {
      waiters_lock_.lock();
      auto it = std::find(waiters_.begin(), waiters_.end(), signal);
      if (it != waiters_.end()) {
        waiters_.erase(it);
      }
      waiters_lock_.unlock();
    }

    // Clean up
    delete signal;

    // Reacquire the lock before returning
    lock.lock();

    return signaled ? CVStatus::kNoTimeout : CVStatus::kTimeout;
  }

  // Wait for a notification with timeout and predicate
  template<typename Lock, typename Clock, typename Duration, typename Predicate>
  auto wait_until(Lock& lock,
                  const std::chrono::time_point<Clock, Duration>& abs_time,
                  Predicate pred) -> bool {
    while (!pred()) {
      if (wait_until(lock, abs_time) == CVStatus::kTimeout) {
        return pred();
      }
    }

    return true;
  }

  // Wait for a notification with timeout
  template<typename Lock, typename Rep, typename Period>
  auto wait_for(Lock& lock, const std::chrono::duration<Rep, Period>& rel_time)
      -> CVStatus {
    return wait_until(lock, std::chrono::steady_clock::now() + rel_time);
  }

  // Wait for a notification with timeout and predicate
  template<typename Lock, typename Rep, typename Period, typename Predicate>
  auto wait_for(Lock& lock, const std::chrono::duration<Rep, Period>& rel_time,
                Predicate pred) -> bool {
    return wait_until(lock, std::chrono::steady_clock::now() + rel_time,
                      std::move(pred));
  }

 private:
  std::list<std::atomic<bool>*> waiters_;
  TTASLock waiters_lock_;
};

#endif  // CONDITION_VARIABLE_H_
