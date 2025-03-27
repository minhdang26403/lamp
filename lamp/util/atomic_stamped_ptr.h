#ifndef ATOMIC_STAMPED_PTR_H_
#define ATOMIC_STAMPED_PTR_H_

#include <atomic>

template<typename T>
class AtomicStampedPtr {
 public:
  AtomicStampedPtr(T* ptr, uint64_t stamp)
      : atomic_stamped_ptr_(StampedPointer(ptr, stamp)) {}

  auto compare_and_swap(
      T* expected_ptr, T* desired_ptr, uint64_t expected_stamp,
      uint64_t desired_stamp,
      std::memory_order order = std::memory_order_seq_cst) noexcept -> bool {
    StampedPointer expected{expected_ptr, expected_stamp};
    StampedPointer desired{desired_ptr, desired_stamp};
    return atomic_stamped_ptr_.compare_exchange_strong(expected, desired,
                                                       order);
  }

  auto compare_and_swap(T* expected_ptr, T* desired_ptr,
                        uint64_t expected_stamp, uint64_t desired_stamp,
                        std::memory_order success,
                        std::memory_order failure) noexcept -> bool {
    StampedPointer expected{expected_ptr, expected_stamp};
    StampedPointer desired{desired_ptr, desired_stamp};
    return atomic_stamped_ptr_.compare_exchange_strong(expected, desired,
                                                       success, failure);
  }

  auto get(std::memory_order order = std::memory_order_seq_cst) const noexcept
      -> std::pair<T*, uint64_t> {
    StampedPointer stamped_ptr = atomic_stamped_ptr_.load(order);
    return {stamped_ptr.ptr_, stamped_ptr.stamp_};
  }

  auto get_ptr(std::memory_order order =
                   std::memory_order_seq_cst) const noexcept -> T* {
    return atomic_stamped_ptr_.load(order).ptr_;
  }

  auto get_stamp(std::memory_order order =
                     std::memory_order_seq_cst) const noexcept -> uint64_t {
    return atomic_stamped_ptr_.load(order).stamp_;
  }

 private:
  struct StampedPointer {
    T* ptr_;
    uint64_t stamp_;

    StampedPointer(T* ptr, uint64_t stamp) noexcept
        : ptr_(ptr), stamp_(stamp) {}
  };

  std::atomic<StampedPointer> atomic_stamped_ptr_;
};

#endif  // ATOMIC_STAMPED_PTR_H_
