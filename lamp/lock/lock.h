#ifndef LOCK_H_
#define LOCK_H_

#include <stddef.h>

class Lock {
 public:
  ~Lock() = default;  // Ensure proper cleanup for derived classes
  virtual auto lock() -> void = 0;
  virtual auto unlock() -> void = 0;
};

#endif  // LOCK_H_
