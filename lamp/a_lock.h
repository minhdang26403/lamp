#ifndef ALOCK_H_
#define ALOCK_H_

#include <atomic>
#include <vector>

#include "lock.h"

/**
 * @brief A simple array-based queue lock.
 */
class ALock : public Lock {
 public:
  ALock(uint64_t capacity) : flag_(capacity), size_(capacity) {
    flag_[0] = true;
  }

  auto lock() -> void override {
    uint64_t slot = tail_.fetch_add(1) % size_;
    my_slot_index = slot;
    while (!flag_[slot]) {}
  }

  auto unlock() -> void override {
    uint64_t slot = my_slot_index;
    flag_[slot] = false;
    flag_[(slot + 1) % size_] = true;
  }

  static thread_local uint64_t my_slot_index;

 private:
  std::vector<bool> flag_;
  std::atomic<uint64_t> tail_{0};
  const uint64_t size_;
};

#endif  // ALOCK_H_
