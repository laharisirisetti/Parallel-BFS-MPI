cmake_minimum_required(VERSION 3.16)

project(ParallelBFS
    VERSION 1.0.0
    DESCRIPTION "Distributed-memory Breadth-First Search with MPI"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Build configuration
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to an optimized build when the user does not specify one.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

# Emit all executables into build/bin.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

find_package(MPI REQUIRED COMPONENTS CXX)

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------

# Sequential baseline (no MPI dependency).
add_executable(sequential_bfs src/sequential_bfs.cpp)

# Distributed BFS (links against MPI).
add_executable(parallel_bfs src/parallel_bfs.cpp)
target_link_libraries(parallel_bfs PRIVATE MPI::MPI_CXX)
