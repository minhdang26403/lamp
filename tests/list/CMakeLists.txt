list(APPEND LIST_TESTS
  coarse_list_test
  fine_list_test
  lazy_list_test
  lock_free_list_test
  optimistic_list_test
)

foreach(LIST_TEST IN LISTS LIST_TESTS)
  add_executable(${LIST_TEST} ${LIST_TEST}.cpp)
  target_link_libraries(${LIST_TEST} GTest::gtest_main)
  gtest_discover_tests(${LIST_TEST})
endforeach()
