# Source files
set(SOURCES
    main.cpp
    server/server.cpp
    server/connection.cpp
    server/io_uring_handler.cpp
    protocol/resp_parser.cpp
    protocol/resp_serializer.cpp
    protocol/commands.cpp
    storage/swiss_table_store.cpp
    storage/string_value.cpp
    storage/command_executor.cpp
    persistence/rdb_writer.cpp
    persistence/rdb_reader.cpp
    utils/logger.cpp
    utils/config.cpp
)

# Header files
set(HEADERS
    server/server.h
    server/connection.h
    server/io_uring_handler.h
    protocol/resp_parser.h
    protocol/resp_serializer.h
    protocol/commands.h
    storage/swiss_table_store.h
    storage/string_value.h
    storage/command_executor.h
    persistence/rdb_writer.h
    persistence/rdb_reader.h
    utils/logger.h
    utils/config.h
)

# Create executable
add_executable(quantdb ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(quantdb
    ${LIBURING_LIBRARIES}
    absl::flat_hash_map
    absl::hash
    absl::strings
    pthread
)

# Include directories
target_include_directories(quantdb PRIVATE ${LIBURING_INCLUDE_DIRS})
target_compile_options(quantdb PRIVATE ${LIBURING_CFLAGS_OTHER})

# Install target
install(TARGETS quantdb DESTINATION bin)