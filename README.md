# robotstxt-cpp

A standard-library-only C++ replica of Google's open-source
[`robotstxt`](https://github.com/google/robotstxt) parser and matcher, pinned to
a validated upstream commit. It preserves the upstream API and algorithm
structure wherever practical, replacing Abseil with C++17 standard-library
equivalents while keeping byte-level behavior identical.

> **Not affiliated with or endorsed by Google.** This is an independent replica
> of Google's Apache-2.0 licensed `robotstxt` project.

## Status

Under construction. See `PROVENANCE.md` for the pinned upstream commit and the
list of vendored files.

## Building

Requires CMake ≥ 3.16 and a C++17 compiler (GCC, Clang, Apple Clang, or MSVC).
The production library target depends only on the C++ standard library; no
Abseil, no Bazel, and no network access during configure or build.

```sh
cmake -S . -B build
cmake --build build
```

## License

Apache-2.0. Google's original copyright and Apache-2.0 headers are preserved on
vendored files; see `LICENSE` and `NOTICE`.
