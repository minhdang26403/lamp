#ifndef PETERSON_LOCK_H_
#define PETERSON_LOCK_H_

#include "lock.h"

#include <array>
#include <atomic>

class PetersonLock {
 public:
  PetersonLock() {}

  auto lock(int id) -> void {
    int j = 1 - id;
    flag[id] = true;                    // I'm interested.
    victim = id;                        // you go first.
    while (flag[j] && victim == id) {}  // wait
  }

  auto release(int id) -> void {
    flag[id] = false;  // I'm not interested.
  }

 private:
  std::array<std::atomic<bool>, 2> flag;
  std::atomic<int> victim;
};

#endif  // PETERSON_LOCK_H_
