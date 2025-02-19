#ifndef FILTER_LOCK_H_
#define FILTER_LOCK_H_

#include <atomic>
#include <vector>

class FilterLock {
 public:
  FilterLock(uint32_t n) : kNumThreads(n), level_(n), victim_(n) {}

  auto lock(uint32_t me) -> void {
    for (uint32_t i = 1; i < kNumThreads; i++) {
      // For some reasons, got race condition bugs when trying to use weaker
      // memory ordering, so stick with sequential consistency here.
      level_[me] = i;
      victim_[i] = me;
      // spin while conflict exists
      bool conflict;
      do {
        conflict = false;
        for (uint32_t k = 0; k < kNumThreads; k++) {
          if (k != me && level_[k] >= i && victim_[i] == me) {
            conflict = true;
            break;
          }
        }
      } while (conflict);
    }
  }

  auto unlock(uint32_t me) -> void { level_[me] = 0; }

 private:
  const uint32_t kNumThreads{};
  std::vector<std::atomic<uint32_t>> level_;
  std::vector<std::atomic<uint32_t>> victim_;
};

#endif  // FILTER_LOCK_H_
