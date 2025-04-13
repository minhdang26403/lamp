#ifndef COMMON_H_
#define COMMON_H_

#include <format>
#include <stdexcept>
#include <string>

class EmptyException : public std::runtime_error {
 public:
  explicit EmptyException(const std::string& what_arg)
      : std::runtime_error(std::format("EmptyException: {}", what_arg)) {}
};

class TimeoutException : public std::runtime_error {
 public:
  TimeoutException(const std::string& what_arg)
      : std::runtime_error(std::format("TimeoutException: {}", what_arg)) {}
};

#endif  // COMMON_H_
