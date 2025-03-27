list(APPEND UTIL_TESTS
  atomic_markable_ptr_test
  atomic_stamped_ptr_test
)

foreach(UTIL_TEST IN LISTS UTIL_TESTS)
  add_executable(${UTIL_TEST} ${UTIL_TEST}.cpp)
  target_link_libraries(${UTIL_TEST} GTest::gtest_main)
  gtest_discover_tests(${UTIL_TEST})
endforeach()
