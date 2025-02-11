#ifndef ALOCK_H_
#define ALOCK_H_

#include <atomic>
#include <vector>

#include "lock.h"

struct Flag {
  std::atomic<bool> flag{false};
  // Force each flag to be on a new cache line
  char padding[127];
};

/**
 * @brief A simple array-based queue lock.
 */
class ALock : public Lock {
 public:
  ALock(uint64_t capacity) : flag_(capacity), size_(capacity) {
    flag_[0].flag.store(true, std::memory_order_relaxed);
  }

  auto lock() -> void override {
    uint64_t slot = tail_.fetch_add(1, std::memory_order_relaxed) % size_;
    my_slot_index = slot;
    while (!flag_[slot].flag.load(std::memory_order_acquire)) {}
  }

  auto unlock() -> void override {
    uint64_t slot = my_slot_index;
    flag_[slot].flag.store(false, std::memory_order_relaxed);
    flag_[(slot + 1) % size_].flag.store(true, std::memory_order_release);
  }

  static thread_local uint64_t my_slot_index;

 private:
  std::vector<Flag> flag_;
  std::atomic<uint64_t> tail_{0};
  const uint64_t size_;
};

#endif  // ALOCK_H_
