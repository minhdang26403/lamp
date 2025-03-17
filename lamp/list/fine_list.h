#ifndef FINE_LIST_H_
#define FINE_LIST_H_

#include <optional>

#include "synchronization/ttas_lock.h"

template<typename T, typename Hash = std::hash<T>>
class FineList {
  struct Node {
    size_t key_;
    std::optional<T> item_;
    Node* next_;
    TTASLock mutex_;
  };
public:

  FineList() {
    
  }

private:
  Node* head_;
};

#endif // FINE_LIST_H_