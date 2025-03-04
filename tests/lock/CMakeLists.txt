list(APPEND LOCK_TESTS
  a_lock_test
  backoff_lock_test
  clh_lock_test
  composite_lock_test
  filter_lock_test
  mcs_lock_test
  peterson_lock_test
  tas_lock_test
  ticket_lock_test
  timeout_lock_test
  ttas_lock_test
)

foreach(LOCK_TEST IN LISTS LOCK_TESTS)
  add_executable(${LOCK_TEST} ${LOCK_TEST}.cpp)
  target_link_libraries(${LOCK_TEST} GTest::gtest_main)
  gtest_discover_tests(${LOCK_TEST})
endforeach()
