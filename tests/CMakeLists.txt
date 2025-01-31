enable_testing()

list(APPEND TESTS
  counter
)

foreach(test IN LISTS tests)
  add_executable(${test} ${test}.cpp)
  target_link_libraries(${test} GTest::gtest_main)
  gtest_discover_tests(${test})
endforeach()