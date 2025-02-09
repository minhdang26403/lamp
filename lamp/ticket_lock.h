#ifndef TICKET_LOCK_H_
#define TICKET_LOCK_H_

#include <atomic>
#include <thread>

#include "lock.h"

class TicketLock : public Lock {
 public:
  auto lock() -> void override {
    // Take a ticket - atomic increment guarantees unique, monotonically
    // increasing numbers
    uint64_t my_ticket = next_ticket_.fetch_add(1, std::memory_order_relaxed);

    // Wait until it's our turn
    while (now_serving_.load(std::memory_order_acquire) != my_ticket) {
      std::this_thread::yield();
    }
  }

  auto unlock() -> void override {
    // Move to next ticket
    now_serving_.fetch_add(1, std::memory_order_release);
  }

 private:
  std::atomic<uint64_t> next_ticket_{0};
  std::atomic<uint64_t> now_serving_{0};
};

#endif  // TICKET_LOCK_H_