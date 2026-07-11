# Minimal, offline CMake build for the de-Abseiled Google robots.txt matcher.
#
# Slice C1: builds a C++17 static library (no Abseil, standard library only)
# plus a tiny smoke binary. No FetchContent, no downloads, no network access.
#
# The ROBOTS_BUILD_TESTS toggle is defined here but wires up nothing yet; the
# upstream test suite lands in slice C2.

cmake_minimum_required(VERSION 3.16)

project(robotstxt_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ROBOTS_BUILD_TESTS "Build the robots.txt test suite (added in slice C2)." OFF)

# Production static library. Links only the C++ standard library.
add_library(robots STATIC robots.cc)
target_include_directories(robots PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Tiny smoke/example binary that prints a real allow/disallow decision.
add_executable(robots_smoke robots_smoke.cc)
target_link_libraries(robots_smoke PRIVATE robots)

if(ROBOTS_BUILD_TESTS)
  # Test targets are introduced in slice C2 (upstream matcher suite). The
  # toggle exists now so the build interface is stable; it intentionally wires
  # up nothing yet.
  message(STATUS "ROBOTS_BUILD_TESTS=ON, but no tests exist until slice C2.")
endif()
