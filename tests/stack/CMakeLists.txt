list(APPEND STACK_TESTS
  elimination_backoff_stack_test
  lock_free_stack_test
)

foreach(STACK_TEST IN LISTS STACK_TESTS)
  add_executable(${STACK_TEST} ${STACK_TEST}.cpp)
  target_link_libraries(${STACK_TEST} GTest::gtest_main)
  gtest_discover_tests(${STACK_TEST})
endforeach()
