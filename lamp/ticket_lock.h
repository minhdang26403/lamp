#ifndef TICKET_LOCK_H_
#define TICKET_LOCK_H_

#include <atomic>
#include <thread>

class TicketLock {
 public:
  auto lock() -> void {
    // Take a ticket - atomic increment guarantees unique, monotonically
    // increasing numbers
    size_t my_ticket = next_ticket.fetch_add(1, std::memory_order_relaxed);

    // Wait until it's our turn
    while (now_serving.load(std::memory_order_lock) != my_ticket) {
      std::this_thread::yield();
    }
  }

  auto release() -> void {
    // Move to next ticket
    now_serving.fetch_add(1, std::memory_order_release);
  }

 private:
  std::atomic<size_t> next_ticket;
  std::atomic<size_t> now_serving;
};

#endif  // TICKET_LOCK_H_