# robotstxt-cpp

A standard-library-only C++17 replica of Google's `robotstxt` parser and matcher, pinned to a
validated upstream commit (`PROVENANCE.md`). robotstxtr depends on it.

Verify with `cmake -S . -B build-verify -DROBOTS_BUILD_TESTS=ON`, `cmake --build build-verify`
and `ctest --test-dir build-verify`. There is no pre-push hook or CI, so run it by hand.

GitLab is the source of truth. GitHub carries a read-only push mirror with Actions off: add no
`.github/workflows/`, and never push there. Setup: seor `design/github-mirror.md`.
