#ifndef ATOMIC_MARKABLE_PTR_H_
#define ATOMIC_MARKABLE_PTR_H_

#include <atomic>
#include <cstdint>

/**
 * AtomicMarkablePtr - A specialized atomic pointer that stores both a pointer
 * and a mark bit
 *
 * This class leverages pointer alignment requirements to pack a boolean mark
 * bit into the least significant bit of the pointer, since T must be at least
 * 2-byte aligned. This allows atomic operations on both the pointer and its
 * mark in a single atomic operation.
 */
template<typename T>
class AtomicMarkablePtr {
 public:
  AtomicMarkablePtr(T* ptr, bool marked) {
    static_assert(alignof(T) >= 2,
                  "T must be at least 2-byte aligned for mark bit");
    ptr_and_mark_.store(pack(ptr, marked), std::memory_order_release);
  }

  /**
   * Atomically compares and sets both the pointer and mark bit
   *
   * This is the core operation that enables lock-free algorithms by allowing
   * atomic updates to both the pointer and its logical deletion status.
   */
  auto compare_and_swap(
      T* expected_ptr, T* desired_ptr, bool expected_mark, bool desired_mark,
      std::memory_order order = std::memory_order_seq_cst) noexcept -> bool {

    uintptr_t expected = pack(expected_ptr, expected_mark);
    uintptr_t desired = pack(desired_ptr, desired_mark);
    return ptr_and_mark_.compare_exchange_strong(expected, desired, order);
  }

  auto compare_and_swap(T* expected_ptr, T* desired_ptr, bool expected_mark,
                        bool desired_mark, std::memory_order success,
                        std::memory_order failure) noexcept -> bool {

    uintptr_t expected = pack(expected_ptr, expected_mark);
    uintptr_t desired = pack(desired_ptr, desired_mark);
    return ptr_and_mark_.compare_exchange_strong(expected, desired, success,
                                                 failure);
  }

  auto get(std::memory_order order = std::memory_order_seq_cst) const noexcept
      -> std::pair<T*, bool> {
    uintptr_t value = ptr_and_mark_.load(order);
    return {unpack_ptr(value), unpack_mark(value)};
  }

  auto get_ptr(std::memory_order order =
                   std::memory_order_seq_cst) const noexcept -> T* {
    return unpack_ptr(ptr_and_mark_.load(order));
  }

  auto is_marked(std::memory_order order =
                     std::memory_order_seq_cst) const noexcept -> bool {
    return unpack_mark(ptr_and_mark_.load(order));
  }

 private:
  static constexpr uintptr_t MARK_BIT = 1;

  // Helper to pack pointer and mark into a single uintptr_t
  static auto pack(T* ptr, bool marked) noexcept -> uintptr_t {
    return reinterpret_cast<uintptr_t>(ptr) | (marked ? MARK_BIT : 0);
  }

  // Helper to extract pointer from a packed value
  static auto unpack_ptr(uintptr_t value) noexcept -> T* {
    return reinterpret_cast<T*>(value & ~MARK_BIT);
  }

  // Helper to extract mark from a packed value
  static auto unpack_mark(uintptr_t value) noexcept -> bool {
    return (value & MARK_BIT) != 0;
  }

  std::atomic<uintptr_t> ptr_and_mark_;
};

#endif  // ATOMIC_MARKABLE_PTR_H_
