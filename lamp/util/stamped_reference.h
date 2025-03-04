#ifndef STAMPED_REFERENCE_H_
#define STAMPED_REFERENCE_H_

#include <atomic>

template<typename T>
class StampedReference {
 public:
  StampedReference(T* initial_ptr, uint64_t stamp)
      : atomic_stamped_ptr_(StampedPointer(initial_ptr, stamp)) {}

  auto get(uint64_t& stamp) const noexcept -> T* {
    StampedPointer current =
        atomic_stamped_ptr_.load(std::memory_order_acquire);
    stamp = current.stamp_;
    return current.ptr_;
  }

  auto compare_and_set(T* expected_ptr, T* new_ptr, uint64_t expected_stamp,
                       uint64_t new_stamp) noexcept -> bool {
    StampedPointer expected{expected_ptr, expected_stamp};
    StampedPointer desired{new_ptr, new_stamp};

    return atomic_stamped_ptr_.compare_exchange_strong(
        expected, desired, std::memory_order_acq_rel,
        std::memory_order_relaxed);
  }

  auto set(T* new_ptr, uint64_t new_stamp) noexcept -> void {
    StampedPointer new_value{new_ptr, new_stamp};
    atomic_stamped_ptr_.store(new_value, std::memory_order_release);
  }

  auto get_reference() const noexcept -> T* {
    return atomic_stamped_ptr_.load(std::memory_order_acquire).ptr_;
  }

  auto get_stamp() const noexcept -> uint64_t {
    return atomic_stamped_ptr_.load(std::memory_order_acquire).stamp_;
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

#endif  // STAMPED_REFERENCE_H_
