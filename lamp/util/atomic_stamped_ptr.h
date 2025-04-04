#ifndef ATOMIC_STAMPED_PTR_H_
#define ATOMIC_STAMPED_PTR_H_

#include <atomic>

template<typename T>
class AtomicStampedPtr {
 public:
  AtomicStampedPtr() : atomic_stamped_ptr_(StampedPtr{}) {}

  AtomicStampedPtr(T* ptr, uint64_t stamp)
      : atomic_stamped_ptr_(StampedPtr(ptr, stamp)) {}

  auto compare_and_swap(
      T* expected_ptr, T* desired_ptr, uint64_t expected_stamp,
      uint64_t desired_stamp,
      std::memory_order order = std::memory_order_seq_cst) noexcept -> bool {
    StampedPtr expected{expected_ptr, expected_stamp};
    StampedPtr desired{desired_ptr, desired_stamp};
    return atomic_stamped_ptr_.compare_exchange_strong(expected, desired,
                                                       order);
  }

  auto compare_and_swap(T* expected_ptr, T* desired_ptr,
                        uint64_t expected_stamp, uint64_t desired_stamp,
                        std::memory_order success,
                        std::memory_order failure) noexcept -> bool {
    StampedPtr expected{expected_ptr, expected_stamp};
    StampedPtr desired{desired_ptr, desired_stamp};
    return atomic_stamped_ptr_.compare_exchange_strong(expected, desired,
                                                       success, failure);
  }

  auto get(std::memory_order order = std::memory_order_seq_cst) const noexcept
      -> std::pair<T*, uint64_t> {
    StampedPtr stamped_ptr = atomic_stamped_ptr_.load(order);
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

  auto set(T* ptr, uint64_t stamp,
           std::memory_order order = std::memory_order_seq_cst) noexcept
      -> void {
    StampedPtr stamped_ptr{ptr, stamp};
    atomic_stamped_ptr_.store(stamped_ptr, order);
  }

 private:
  struct StampedPtr {
    T* ptr_{nullptr};
    uint64_t stamp_{};

    StampedPtr() = default;

    StampedPtr(T* ptr, uint64_t stamp) noexcept : ptr_(ptr), stamp_(stamp) {}
  };

  std::atomic<StampedPtr> atomic_stamped_ptr_;
};

#endif  // ATOMIC_STAMPED_PTR_H_
