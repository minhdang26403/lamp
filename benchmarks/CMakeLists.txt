list(APPEND BENCHMARKS
  list_benchmark
  lock_benchmark
  read_write_lock_benchmark
)

foreach(BENCHMARK IN LISTS BENCHMARKS)
  add_executable(${BENCHMARK} ${BENCHMARK}.cpp)
  target_link_libraries(${BENCHMARK} benchmark::benchmark)
endforeach()
