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
    std::vector<data_to_reclaim*> pending_reclaims_;
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
  /**
   * @brief Called once, before any call to op_begin()
   * @param num the maximum number of locations the caller can reserve
   */
  auto register_thread(size_t num) -> void {
    self_ = new ThreadContext(num, this);
  }

  /**
   * @brief Called once, after the last call to op_end()
   */
  auto unregister_thread() -> void {
    // no-op
  }

  /**
   * @brief Indicate the beginning of a concurrent operation
   */
  auto op_begin() -> void {
    // no-op
  }

  /**
   * @brief Try to reclaim a pointer
   * @tparam T the type of the data pointed by the pointer; need to know the
   * data type for deallocation
   * @param ptr the pointer to reclaim
   */
  template<typename T>
  auto sched_for_reclaim(T* ptr) -> void {
    self_->pending_reclaims_.push_back(new data_to_reclaim(ptr));
  }

  /**
   * @brief Try to protect a pointer from reclamation
   * @param ptr the pointer to protect
   * @return return true if we can reserve a spot to protect the pointer;
   * otherwise, throw an exception
   */
  auto try_reserve(void* ptr) -> bool {
    for (auto& reservation : self_->reservations_) {
      if (reservation.load(std::memory_order_relaxed) == nullptr) {
        reservation.store(ptr, std::memory_order_release);
        return true;
      }
    }
    throw std::runtime_error{"Can't reserve a spot for the pointer"};
  }

  /**
   * @brief Stop protecting a pointer
   * @param ptr the pointer to stop protecting
   */
  auto unreserve(void* ptr) -> void {
    for (auto& reservation : self_->reservations_) {
      if (reservation.load(std::memory_order_relaxed) == ptr) {
        reservation.store(nullptr, std::memory_order_release);
      }
    }
  }

  /**
   * @brief Indicate the end of a concurrent operation
   */
  auto op_end() -> void {
    for (auto& reservation : self_->reservations_) {
      reservation.store(nullptr, std::memory_order_release);
    }

    auto it = self_->pending_reclaims_.begin();
    while (it != self_->pending_reclaims_.end()) {
      data_to_reclaim* reclaim_obj = *it;
      if (is_unreserved(reclaim_obj->data_)) {
        it = self_->pending_reclaims_.erase(it);
        delete reclaim_obj;
      } else {
        ++it;
      }
    }
  }

 private:
  /**
   * @brief Checks whether the pointer is currently reserved by any threads
   * @param ptr the pointer to check
   * @return true if the pointer is not reserved by any threads; otherwise,
   * return false
   */
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
