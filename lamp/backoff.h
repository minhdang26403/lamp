#ifndef BACKOFF_H_
#define BACKOFF_H_

#include <chrono>
#include <random>
#include <thread>

static auto get_random_int64(int64_t upper_limit) -> int64_t {
  static std::random_device rd;
  static std::mt19937_64 gen(rd());

  std::uniform_int_distribution<std::int64_t> dist(0, upper_limit);
  return dist(gen);
}

class Backoff {
 public:
  Backoff(int64_t min_delay, int64_t max_delay)
      : min_delay_(min_delay), max_delay_(max_delay), limit_(min_delay_) {}

  auto backoff() -> void {
    int64_t delay = get_random_int64(limit_);
    limit_ = std::min(max_delay_, limit_ * 2);
    std::this_thread::sleep_for(std::chrono::microseconds(delay));
  }

 private:
  const int64_t min_delay_;
  const int64_t max_delay_;
  int64_t limit_;
};

#endif  // BACKOFF_H_