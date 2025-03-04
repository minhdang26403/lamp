#ifndef BACKOFF_H_
#define BACKOFF_H_

#include <concepts>
#include <random>
#include <thread>
#include <type_traits>

template<typename T>
concept IntType = std::integral<T> &&
                  (std::is_same_v<T, short> || std::is_same_v<T, int> ||
                   std::is_same_v<T, long> || std::is_same_v<T, long long> ||
                   std::is_same_v<T, unsigned short> ||
                   std::is_same_v<T, unsigned int> ||
                   std::is_same_v<T, unsigned long> ||
                   std::is_same_v<T, unsigned long long>);

template<typename T>
  requires IntType<T>
auto get_random_int(T lower_limit, T upper_limit) -> T {
  thread_local std::random_device rd;
  thread_local std::mt19937_64 gen(rd());  // Each thread gets its own generator

  std::uniform_int_distribution<T> dist(lower_limit, upper_limit);
  return dist(gen);
}

template<typename Duration>
class Backoff {
 public:
  Backoff(int64_t min_delay, int64_t max_delay)
      : kMinDelay(min_delay), kMaxDelay(max_delay), current_limit_(kMinDelay) {}

  auto backoff() -> void {
    int64_t delay = get_random_int<int64_t>(0, current_limit_);
    current_limit_ = std::min(kMaxDelay, current_limit_ * 2);
    std::this_thread::sleep_for(Duration(delay));
  }

 private:
  const int64_t kMinDelay;
  const int64_t kMaxDelay;
  int64_t current_limit_;
};

#endif  // BACKOFF_H_