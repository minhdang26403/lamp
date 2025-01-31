#ifndef LOCK_H_
#define LOCK_H_

#include <stddef.h>

class Lock {
 public:
  virtual auto Acquire() -> void = 0;
  virtual auto Release() -> void = 0;
};

#endif  // LOCK_H_