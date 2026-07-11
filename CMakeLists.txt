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
# Slice C3 adds the de-Abseiled reporting layer (reporting_robots.cc).
add_library(robots STATIC robots.cc reporting_robots.cc)
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

  # Slice C3: the mechanically adapted upstream reporting suite
  # (reporting_robots_test.cc). Same test-only, never-downloaded GTest wiring.
  add_executable(reporting_robots_test reporting_robots_test.cc)
  target_link_libraries(reporting_robots_test
                        PRIVATE robots GTest::gtest GTest::gtest_main)

  # Slice C4: the Abseil-replacement regression suite and the callback-contract
  # test. Same test-only, never-downloaded GTest wiring; no Abseil.
  add_executable(abseil_replacement_test abseil_replacement_test.cc)
  target_link_libraries(abseil_replacement_test
                        PRIVATE robots GTest::gtest GTest::gtest_main)

  add_executable(callback_contract_test callback_contract_test.cc)
  target_link_libraries(callback_contract_test
                        PRIVATE robots GTest::gtest GTest::gtest_main)

  include(GoogleTest)
  gtest_discover_tests(robots_test)
  gtest_discover_tests(reporting_robots_test)
  gtest_discover_tests(abseil_replacement_test)
  gtest_discover_tests(callback_contract_test)
endif()

# ---------------------------------------------------------------------------
# Slice C5: maintainer-only differential harness (default OFF).
#
# This block is the ONLY place that references Abseil or the pinned pristine
# upstream sources. It is unreachable from the default build and from the CI
# build (-DROBOTS_BUILD_TESTS=ON): neither configures find_package(absl) nor
# touches .upstream-pinned/. Nothing here downloads anything at configure or
# build time -- the pristine sources are read from a LOCAL directory.
#
# The pristine (Abseil-using) upstream engine and the de-Abseiled replica engine
# share the `googlebot` namespace and the `RobotsMatcher` type, so they cannot
# be linked into one executable. We build two runner executables from a single
# shared source (diff_harness/diff_runner.cc): the replica runner links the
# product `robots` library; the pristine runner compiles the pinned upstream
# sources and links Abseil, with the pinned directory placed ahead on the
# include path so `#include "robots.h"` resolves to the pristine headers. A
# driver executable runs both and diffs their canonical per-case output.
# ---------------------------------------------------------------------------
option(ROBOTS_BUILD_DIFFERENTIAL
       "Build the maintainer-only differential harness (needs Abseil + pinned upstream sources)."
       OFF)

if(ROBOTS_BUILD_DIFFERENTIAL)
  # Local path to the pristine upstream sources. Never fetched; overridable so a
  # maintainer can point at an alternate local checkout of the pinned SHA.
  set(ROBOTS_UPSTREAM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.upstream-pinned"
      CACHE PATH "Local directory containing the pristine upstream robots.{cc,h} and reporting_robots.{cc,h} at the pinned SHA.")

  foreach(_f robots.cc robots.h reporting_robots.cc reporting_robots.h)
    if(NOT EXISTS "${ROBOTS_UPSTREAM_DIR}/${_f}")
      message(FATAL_ERROR
        "ROBOTS_BUILD_DIFFERENTIAL is ON but the pristine upstream source "
        "'${_f}' was not found under ROBOTS_UPSTREAM_DIR='${ROBOTS_UPSTREAM_DIR}'. "
        "These are frozen local inputs (see PROVENANCE.md); provide them or set "
        "-DROBOTS_UPSTREAM_DIR=<local path>. Nothing is downloaded.")
    endif()
  endforeach()

  # Abseil is a MAINTAINER-only dependency, referenced only here.
  find_package(absl CONFIG REQUIRED)

  set(_diff_dir "${CMAKE_CURRENT_SOURCE_DIR}/diff_harness")

  # Pristine upstream engine, compiled from the local pinned sources with Abseil.
  add_library(robots_pristine STATIC
              "${ROBOTS_UPSTREAM_DIR}/robots.cc"
              "${ROBOTS_UPSTREAM_DIR}/reporting_robots.cc")
  target_include_directories(robots_pristine PUBLIC "${ROBOTS_UPSTREAM_DIR}")
  target_link_libraries(robots_pristine PUBLIC
                        absl::strings absl::string_view absl::btree
                        absl::fixed_array absl::core_headers)

  # Replica runner: links the de-Abseiled product `robots` library.
  add_executable(diff_runner_replica "${_diff_dir}/diff_runner.cc")
  target_include_directories(diff_runner_replica PRIVATE "${_diff_dir}")
  target_link_libraries(diff_runner_replica PRIVATE robots)

  # Pristine runner: same source, but resolves robots.h/reporting_robots.h to
  # the pinned upstream headers and links the Abseil-using pristine engine.
  add_executable(diff_runner_pristine "${_diff_dir}/diff_runner.cc")
  target_include_directories(diff_runner_pristine PRIVATE
                             "${_diff_dir}" "${ROBOTS_UPSTREAM_DIR}")
  target_link_libraries(diff_runner_pristine PRIVATE robots_pristine)

  # Driver: regenerates the shared case stream, runs both runners, diffs output.
  add_executable(differential_harness "${_diff_dir}/differential_harness.cc")
  target_include_directories(differential_harness PRIVATE "${_diff_dir}")
  target_compile_definitions(differential_harness PRIVATE
    "REPLICA_RUNNER=\"$<TARGET_FILE:diff_runner_replica>\""
    "PRISTINE_RUNNER=\"$<TARGET_FILE:diff_runner_pristine>\"")
  add_dependencies(differential_harness diff_runner_replica diff_runner_pristine)
endif()
