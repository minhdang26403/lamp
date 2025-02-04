#ifndef FILTER_LOCK_H_
#define FILTER_LOCK_H_

#include <atomic>
#include <vector>

class FilterLock {
 public:
  FilterLock(int num_threads) : n(num_threads), level(n), victim(n) {
    for (int i = 0; i < n; i++) {
      level[i] = 0;
    }
  }

  auto lock(int me) -> void {
    for (int i = 1; i < n; i++) {
      level[me] = i;
      victim[i] = me;
      // spin while conflict exists
      bool conflict;
      do {
        conflict = false;
        for (int k = 0; k < n; k++) {
          if (k != me && level[k] >= i && victim[i] == me) {
            conflict = true;
            break;
          }
        }
      } while (conflict);
    }
  }

  auto unlock(int me) -> void { level[me] = 0; }

 private:
  int n;
  std::vector<std::atomic<int>> level;
  std::vector<std::atomic<int>> victim;
};

#endif  // FILTER_LOCK_H_
