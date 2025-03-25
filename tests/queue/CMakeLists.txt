list(APPEND QUEUE_TESTS
  bounded_queue_test
)

foreach(QUEUE_TEST IN LISTS QUEUE_TESTS)
  add_executable(${QUEUE_TEST} ${QUEUE_TEST}.cpp)
  target_link_libraries(${QUEUE_TEST} GTest::gtest_main)
  gtest_discover_tests(${QUEUE_TEST})
endforeach()
