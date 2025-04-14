#ifndef HAZARD_PTR_
#define HAZARD_PTR_

#include <atomic>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <vector>

template<typename T>
void do_delete(void* ptr) {
  delete static_cast<T*>(ptr);
}

struct data_to_reclaim {
  void* data_;
  std::function<void(void*)> deleter_;

  template<typename T>
  data_to_reclaim(T* ptr) : data_(ptr), deleter_(&do_delete<T>) {}

  ~data_to_reclaim() { deleter_(data_); }
};

class HazardPtr {
  struct ThreadContext {
    std::vector<data_to_reclaim> pending_reclaims_;
    std::vector<std::atomic<void*>> reservations_;
    ThreadContext* next_{};

    ThreadContext(size_t num, HazardPtr* hazard_ptr) : reservations_(num) {
      for (auto& reservation : reservations_) {
        reservation.store(nullptr, std::memory_order_relaxed);
      }

      // Add this thread context to the list of thread contexts managed by the
      // hazard pointer object
      next_ = hazard_ptr->head_.load(std::memory_order_acquire);
      while (!hazard_ptr->head_.compare_exchange_weak(
          next_, this, std::memory_order_release, std::memory_order_acquire)) {}
    }
  };

 public:
  auto register_thread(size_t num) -> void {
    self_ = new ThreadContext(num, this);
  }

  auto unregister_thread() -> void {
    // no-op
  }

  auto op_begin() -> void {
    // no-op
  }

  template<typename T>
  auto sched_for_reclaim(T* ptr) -> void {
    self_->pending_reclaims_.emplace_back(ptr);
  }

  auto try_reserve(void* ptr) -> bool {
    for (auto& reservation : self_->reservations_) {
      if (reservation.load(std::memory_order_relaxed) == nullptr) {
        reservation.store(ptr, std::memory_order_release);
        return true;
      }
    }
    throw std::runtime_error{"Can't reserve a spot for the pointer"};
  }

  auto unreserve(void* ptr) -> void {
    for (auto& reservation : self_->reservations_) {
      if (reservation.load(std::memory_order_relaxed) == ptr) {
        reservation.store(nullptr, std::memory_order_release);
      }
    }
  }

  auto op_end() -> void {
    for (auto& reservation : self_->reservations_) {
      reservation.store(nullptr, std::memory_order_release);
    }

    auto it = self_->pending_reclaims_.begin();
    while (it != self_->pending_reclaims_.end()) {
      if (is_unreserved(it->data_)) {
        it = self_->pending_reclaims_.erase(it);
      } else {
        ++it;
      }
    }
  }

 private:
  auto is_unreserved(void* ptr) -> bool {
    ThreadContext* curr = head_.load(std::memory_order_acquire);
    while (curr != nullptr) {
      for (const auto& reservation : curr->reservations_) {
        if (reservation.load(std::memory_order_acquire) == ptr) {
          return false;
        }
      }
      curr = curr->next_;
    }

    return true;
  }

  static thread_local ThreadContext* self_;
  std::atomic<ThreadContext*> head_{nullptr};
};

// Define the thread_local static member
thread_local HazardPtr::ThreadContext* HazardPtr::self_ = nullptr;

#endif  // HAZARD_PTR_
