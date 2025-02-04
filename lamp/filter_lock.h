#ifndef FILTER_LOCK_H_
#define FILTER_LOCK_H_

#include <atomic>
#include <vector>

class FilterLock {
 public:
  FilterLock(uint32_t n) : num_threads(n), level(n), victim(n) {
    for (uint32_t i = 0; i < num_threads; i++) {
      level[i] = 0;
    }
  }

  auto lock(uint32_t me) -> void {
    for (uint32_t i = 1; i < num_threads; i++) {
      level[me] = i;
      victim[i] = me;
      // spin while conflict exists
      bool conflict;
      do {
        conflict = false;
        for (uint32_t k = 0; k < num_threads; k++) {
          if (k != me && level[k] >= i && victim[i] == me) {
            conflict = true;
            break;
          }
        }
      } while (conflict);
    }
  }

  auto unlock(uint32_t me) -> void { level[me] = 0; }

 private:
  uint32_t num_threads{};
  std::vector<std::atomic<uint32_t>> level;
  std::vector<std::atomic<uint32_t>> victim;
};

#endif  // FILTER_LOCK_H_
