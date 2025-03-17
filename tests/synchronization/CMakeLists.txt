list(APPEND LIST_TESTS
  coarse_list_test
)

foreach(LIST_TEST IN LISTS LIST_TESTS)
  add_executable(${LIST_TEST} ${LIST_TEST}.cpp)
  target_link_libraries(${LIST_TEST} GTest::gtest_main)
  gtest_discover_tests(${LIST_TEST})
endforeach()
