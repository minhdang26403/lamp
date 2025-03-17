list(APPEND SYNCHRONIZATION_TESTS
  a_lock_test
  backoff_lock_test
  clh_lock_test
  composite_lock_test
  condition_variable_test
  fifo_read_write_lock_test
  filter_lock_test
  mcs_lock_test
  peterson_lock_test
  reentrant_lock_test
  semaphore_test
  simple_read_write_lock_test
  tas_lock_test
  ticket_lock_test
  timeout_lock_test
  ttas_lock_test
)

foreach(SYNCHRONIZATION_TEST IN LISTS SYNCHRONIZATION_TESTS)
  add_executable(${SYNCHRONIZATION_TEST} ${SYNCHRONIZATION_TEST}.cpp)
  target_link_libraries(${SYNCHRONIZATION_TEST} GTest::gtest_main)
  gtest_discover_tests(${SYNCHRONIZATION_TEST})
endforeach()
