#ifndef BACKOFF_H_
#define BACKOFF_H_

#include <chrono>
#include <random>
#include <thread>

static auto get_random_int64(int64_t upper_limit) -> int64_t {
  thread_local std::random_device rd;
  thread_local std::mt19937_64 gen(rd());  // Each thread gets its own generator

  std::uniform_int_distribution<std::int64_t> dist(0, upper_limit);
  return dist(gen);
}

template<typename Duration>
class Backoff {
 public:
  Backoff(int64_t min_delay, int64_t max_delay)
      : kMinDelay(min_delay), kMaxDelay(max_delay), current_limit_(kMinDelay) {}

  auto backoff() -> void {
    int64_t delay = get_random_int64(current_limit_);
    current_limit_ = std::min(kMaxDelay, current_limit_ * 2);
    std::this_thread::sleep_for(Duration(delay));
  }

 private:
  const int64_t kMinDelay;
  const int64_t kMaxDelay;
  int64_t current_limit_;
};

#endif  // BACKOFF_H_