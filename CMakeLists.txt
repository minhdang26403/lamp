cmake_minimum_required(VERSION 3.28)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # For clang-tidy and clangd.
set(CMAKE_CXX_STANDARD 20) # Compile as C++20
set(CMAKE_CXX_STANDARD_REQUIRED ON) # Require C++20 support.

project(Lamp VERSION 2025.1
  DESCRIPTION "A concurrent data structures library"
  LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to `Debug` as none was specified.")
  set(CMAKE_BUILD_TYPE "Debug")
endif()

# Expected directory structure.
set(LAMP_BUILD_SUPPORT_DIR "${CMAKE_SOURCE_DIR}/build_support")
set(LAMP_CLANG_SEARCH_PATH "/usr/bin/" "/opt/homebrew/opt/llvm/bin/")

message(STATUS "You're using ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

# #####################################################################################################################
# COMPILER SETUP
# #####################################################################################################################

# Sanitizers: address, leak (included in address or use as stand-alone), memory, undefined, thread
if(NOT DEFINED SANITIZER)
  set(SANITIZER address)
endif()

message("Build mode: ${CMAKE_BUILD_TYPE}")
message("${SANITIZER} sanitizer will be enabled in debug mode.")

# Compiler flags.
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra -Werror -Wstrict-aliasing=2 -Wno-unqualified-std-cast-call -Wno-sign-conversion")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb -fsanitize=${SANITIZER} -fno-omit-frame-pointer -fno-optimize-sibling-calls")

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mtune=native -march=native")

message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CMAKE_CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE}")

# Ensure consistent visibility settings across the project
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)

# Output directory
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# #####################################################################################################################
# DEPENDENCIES
# #####################################################################################################################

# cpplint
find_program(CPPLINT_BIN
  NAMES cpplint cpplint.py
  HINTS ${LAMP_BUILD_SUPPORT_DIR}
)

message(${LAMP_BUILD_SUPPORT_DIR})

if("${CPPLINT_BIN}" STREQUAL "CPPLINT_BIN-NOTFOUND")
  message(WARNING "lamp/main couldn't find cpplint.")
else()
  message(STATUS "lamp/main found cpplint at ${CPPLINT_BIN}")
endif()

# FetchContent module to download and build Google Benchmark and GoogleTest
include(FetchContent)

# GoogleTest
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.16.0 # Use the main branch or specify a specific commit/tag
)

# Google Benchmark
FetchContent_Declare(
  googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.9.1 # Use the main branch or specify a specific commit/tag
)

FetchContent_MakeAvailable(googletest googlebenchmark)

include(GoogleTest)

include_directories(${PROJECT_SOURCE_DIR}/lamp)

add_subdirectory(benchmarks)
add_subdirectory(tests)

# #####################################################################################################################
# MAKE TARGETS
# #####################################################################################################################

# #########################################
# "make check-lint"
# #########################################
file(GLOB_RECURSE LAMP_LINT_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/lamp/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
)

# Balancing act: cpplint.py takes a non-trivial time to launch,
# so process 12 files per invocation, while still ensuring parallelism
add_custom_target(check-lint echo '${LAMP_LINT_FILES}' | xargs -n12 -P8
  ${CPPLINT_BIN}
  --verbose=2 --quiet
  --linelength=120
  --filter=-legal/copyright,-build/header_guard,-build/include_subdir,-build/c++11,-build/namespaces,-runtime/references # https://github.com/cpplint/cpplint/issues/148
)
