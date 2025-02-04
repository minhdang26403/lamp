enable_testing()

list(APPEND TESTS
  filter_lock_test
  peterson_lock_test
  tas_lock_test
  ticket_lock_test
)

foreach(TEST IN LISTS TESTS)
  add_executable(${TEST} ${TEST}.cpp)
  target_link_libraries(${TEST} GTest::gtest_main)
  gtest_discover_tests(${TEST})
endforeach()
