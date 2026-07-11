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
  # Slice C2: the mechanically adapted upstream matcher suite (robots_test.cc).
  #
  # GoogleTest is a TEST-ONLY dependency resolved from the system install via
  # find_package. It is NEVER downloaded: no FetchContent, no network access
  # during configure or build. The production `robots` library above links no
  # test dependency and no Abseil.
  enable_testing()
  find_package(GTest REQUIRED)

  add_executable(robots_test robots_test.cc)
  target_link_libraries(robots_test PRIVATE robots GTest::gtest GTest::gtest_main)

  include(GoogleTest)
  gtest_discover_tests(robots_test)
endif()
