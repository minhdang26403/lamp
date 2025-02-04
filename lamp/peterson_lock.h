#ifndef PETERSON_LOCK_H_
#define PETERSON_LOCK_H_

#include <array>
#include <atomic>

class PetersonLock {
 public:
  PetersonLock() {}

  auto lock(uint32_t id) -> void {
    uint32_t j = 1 - id;
    flag[id] = true;                    // I'm interested.
    victim = id;                        // you go first.
    while (flag[j] && victim == id) {}  // wait
  }

  auto unlock(uint32_t id) -> void {
    flag[id] = false;  // I'm not interested.
  }

 private:
  std::array<std::atomic<bool>, 2> flag{};
  std::atomic<uint32_t> victim{};
};

#endif  // PETERSON_LOCK_H_
