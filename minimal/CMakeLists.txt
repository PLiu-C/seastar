cmake_minimum_required(VERSION 3.16)

project(SeastarMinimal
    VERSION 1.0
    LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags (relaxed warnings for minimal framework)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O2 -Wno-unused-parameter -Wno-unused-variable -Wno-reorder")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Find required packages
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files
set(SEASTAR_MINIMAL_SOURCES
    src/util/assert.cc
    src/util/later.cc
    src/core/memory.cc
    src/core/scheduling.cc
    src/core/smp.cc
    src/core/reactor.cc
    src/core/future.cc
    src/core/timer.cc
    src/net/socket_defs.cc
    src/net/inet_address.cc
)

# Create the minimal seastar library
add_library(seastar_minimal STATIC ${SEASTAR_MINIMAL_SOURCES})

# Link libraries
target_link_libraries(seastar_minimal 
    PRIVATE 
        Threads::Threads
)

# Compiler-specific options
target_compile_options(seastar_minimal PRIVATE
    -fcoroutines     # For C++20 coroutines
    -fno-omit-frame-pointer
    -march=native    # Optimize for current CPU
)

# System libraries needed
target_link_libraries(seastar_minimal PRIVATE
    rt       # POSIX real-time extensions
    pthread  # POSIX threads
)

# Install targets
install(TARGETS seastar_minimal
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hh"
)

# Test program
add_executable(minimal_test test/minimal_test.cc)
target_link_libraries(minimal_test PRIVATE seastar_minimal)

# Enable testing
enable_testing()
add_test(NAME MinimalTest COMMAND minimal_test) 