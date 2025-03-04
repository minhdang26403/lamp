#ifndef PETERSON_LOCK_H_
#define PETERSON_LOCK_H_

#include <array>
#include <atomic>

class PetersonLock {
 public:
  auto lock(uint32_t id) -> void {
    uint32_t j = 1 - id;
    flag_[id] = true;                    // I'm interested.
    victim_ = id;                        // you go first.
    while (flag_[j] && victim_ == id) {}  // wait
  }

  auto unlock(uint32_t id) -> void {
    flag_[id] = false;  // I'm not interested.
  }

 private:
  std::array<std::atomic<bool>, 2> flag_{};
  std::atomic<uint32_t> victim_{};
};

#endif  // PETERSON_LOCK_H_
