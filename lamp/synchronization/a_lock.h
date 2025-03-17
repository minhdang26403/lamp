#ifndef ALOCK_H_
#define ALOCK_H_

#include <atomic>
#include <vector>

#include "synchronization/lock.h"

struct Flag {
  std::atomic<bool> flag_{false};
  // Force each flag to be on a new cache line
  char padding[127];
};

/**
 * @brief A simple array-based queue lock.
 */
class ALock : public Lock {
 public:
  ALock(uint64_t capacity) : flags_(capacity), kSize(capacity) {
    flags_[0].flag_.store(true, std::memory_order_relaxed);
  }

  auto lock() -> void override {
    uint64_t slot = tail_.fetch_add(1, std::memory_order_relaxed) % kSize;
    my_slot_index = slot;
    while (!flags_[slot].flag_.load(std::memory_order_acquire)) {}
  }

  auto unlock() -> void override {
    uint64_t slot = my_slot_index;
    flags_[slot].flag_.store(false, std::memory_order_relaxed);
    flags_[(slot + 1) % kSize].flag_.store(true, std::memory_order_release);
  }

  static thread_local uint64_t my_slot_index;

 private:
  std::vector<Flag> flags_;
  std::atomic<uint64_t> tail_{0};
  const uint64_t kSize;
};

#endif  // ALOCK_H_
