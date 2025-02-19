enable_testing()

list(APPEND TESTS
  a_lock_test
  backoff_lock_test
  clh_lock_test
  composite_lock_test
  filter_lock_test
  mcs_lock_test
  peterson_lock_test
  stamped_reference_test
  tas_lock_test
  ticket_lock_test
  timeout_lock_test
  ttas_lock_test
)

foreach(TEST IN LISTS TESTS)
  add_executable(${TEST} ${TEST}.cpp)
  target_link_libraries(${TEST} GTest::gtest_main)
  gtest_discover_tests(${TEST})
endforeach()
