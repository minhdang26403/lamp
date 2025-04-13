#ifndef ATOMIC_STAMPED_REFERENCE_H_
#define ATOMIC_STAMPED_REFERENCE_H_

#include <atomic>

template<typename T>
class AtomicStampedReference {
 public:
  AtomicStampedReference(const T& ref, uint64_t stamp)
      : atomic_stamped_ref_(StampedRef(&ref, stamp)) {}

  auto compare_and_swap(
      const T& expected_ref, const T& desired_ref, uint64_t expected_stamp,
      uint64_t desired_stamp,
      std::memory_order order = std::memory_order_seq_cst) noexcept -> bool {

    StampedRef expected{&expected_ref, expected_stamp};
    StampedRef desired{&desired_ref, desired_stamp};
    return atomic_stamped_ref_.compare_exchange_strong(expected, desired,
                                                       order);
  }

  auto compare_and_swap(const T& expected_ref, const T& desired_ref,
                        uint64_t expected_stamp, uint64_t desired_stamp,
                        std::memory_order success,
                        std::memory_order failure) noexcept -> bool {

    StampedRef expected{&expected_ref, expected_stamp};
    StampedRef desired{&desired_ref, desired_stamp};
    return atomic_stamped_ref_.compare_exchange_strong(expected, desired,
                                                       success, failure);
  }

  auto get(std::memory_order order = std::memory_order_seq_cst) const noexcept
      -> std::pair<const T&, uint64_t> {
    StampedRef stamped_ref = atomic_stamped_ref_.load(order);
    return {*stamped_ref.ref_ptr_, stamped_ref.stamp_};
  }

  auto get_ref(std::memory_order order =
                   std::memory_order_seq_cst) const noexcept -> const T& {
    return *atomic_stamped_ref_.load(order).ref_ptr_;
  }

  auto get_stamp(std::memory_order order =
                     std::memory_order_seq_cst) const noexcept -> uint64_t {
    return *atomic_stamped_ref_.load(order).stamp_;
  }

  auto set(const T& ref, uint64_t stamp,
           std::memory_order order = std::memory_order_seq_cst) noexcept
      -> void {
    StampedRef stamped_ref{&ref, stamp};
    atomic_stamped_ref_.store(stamped_ref, order);
  }

 private:
  struct StampedRef {
    const T* ref_ptr_{nullptr};
    uint64_t stamp_{};

    StampedRef() = default;

    StampedRef(const T* ref_ptr, uint64_t stamp) noexcept
        : ref_ptr_(ref_ptr), stamp_(stamp) {}
  };

  std::atomic<StampedRef> atomic_stamped_ref_;
};

#endif  // ATOMIC_STAMPED_REFERENCE_H_
