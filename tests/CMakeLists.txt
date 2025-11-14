# Find Google Test
find_package(GTest REQUIRED)

# Unit tests
file(GLOB UNIT_TESTS "unit/*.cpp")
foreach(TEST_SOURCE ${UNIT_TESTS})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME}
        GTest::gtest
        GTest::gtest_main
        ../src/storage/swiss_table_store.cpp
        ../src/protocol/resp_parser.cpp
        ../src/protocol/resp_serializer.cpp
        ../src/storage/command_executor.cpp
        ../src/utils/logger.cpp
        ${LIBURING_LIBRARIES}
        absl::flat_hash_map
        absl::hash
        absl::strings
        pthread
    )
    target_include_directories(${TEST_NAME} PRIVATE ../src ${LIBURING_INCLUDE_DIRS})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()

# Benchmark tests
find_package(benchmark REQUIRED)
file(GLOB BENCHMARK_TESTS "benchmarks/*.cpp")
foreach(BENCH_SOURCE ${BENCHMARK_TESTS})
    get_filename_component(BENCH_NAME ${BENCH_SOURCE} NAME_WE)
    add_executable(${BENCH_NAME} ${BENCH_SOURCE})
    target_link_libraries(${BENCH_NAME}
        benchmark::benchmark
        benchmark::benchmark_main
        ../src/storage/swiss_table_store.cpp
        ../src/protocol/resp_parser.cpp
        ../src/protocol/resp_serializer.cpp
        ../src/storage/command_executor.cpp
        ../src/utils/logger.cpp
        ${LIBURING_LIBRARIES}
        absl::flat_hash_map
        absl::hash
        absl::strings
        pthread
    )
    target_include_directories(${BENCH_NAME} PRIVATE ../src ${LIBURING_INCLUDE_DIRS})
endforeach()