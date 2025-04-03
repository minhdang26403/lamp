add_subdirectory(list)
add_subdirectory(synchronization)

foreach(SOURCE_FILE IN LISTS BENCHMARKS)
  get_filename_component(BENCHMARK ${SOURCE_FILE} NAME_WE)  # Extracts the filename without extension
  add_executable(${BENCHMARK} ${SOURCE_FILE})
  target_link_libraries(${BENCHMARK} benchmark::benchmark)
endforeach()
