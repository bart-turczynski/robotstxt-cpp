# Maintainer-only differential harness (Slice C5)

This directory contains a **maintainer-only** differential test harness that
proves the de-Abseiled replica engine (`robots.{cc,h}` / `reporting_robots.{cc,h}`
at the repo root) is behaviorally identical to the **pristine upstream** engine
(the original Abseil-using Google `robotstxt` sources at the pinned SHA).

It is **excluded from the default build, the CI build, and every install /
offline path**. It downloads nothing. It builds only when you explicitly enable
the `ROBOTS_BUILD_DIFFERENTIAL` CMake option, and it is the only part of the
build that references Abseil or the pinned upstream sources.

## What it does

1. Builds **both** engines at the same pinned SHA from **local** sources:
   - the replica from the repo-root product files (the `robots` library);
   - the pristine engine from `../.upstream-pinned/` (frozen local inputs; see
     `../PROVENANCE.md`), linked against Abseil.
2. Runs a fixed **upstream corpus** (cases derived from the pinned upstream test
   files) **plus at least 10,000 deterministically generated cases**.
3. For every case, diffs **both** the matcher decision (allow/disallow, matching
   line, specific-agent flag) **and** the reporting-layer output
   (`RobotsParsingReporter`: `last_line_seen`, `valid_directives`,
   `unused_directives`, and every `RobotsParsedLine`'s tag / typo / metadata).
4. Exits `0` only when there are **zero** matcher and **zero** reporting
   differences; otherwise it prints the offending cases and exits non-zero.

The seed and corpus-generation version are recorded in
[`corpus.lock`](corpus.lock) and echoed at the top of every run.

## Provisioning needs

- A C++17 toolchain, CMake >= 3.16 (same as the rest of the project).
- **Abseil** installed and discoverable by `find_package(absl CONFIG)`. On this
  machine it lives at `/opt/homebrew/opt/abseil`; point CMake at another prefix
  with `-Dabsl_DIR=<...>` / `-DCMAKE_PREFIX_PATH=<...>` if needed.
- The **pinned pristine upstream sources** present locally. By default the
  harness reads them from `../.upstream-pinned/` (a frozen, gitignored input,
  reproducible via `../PROVENANCE.md` — never re-fetched automatically). Point
  at an alternate local checkout of the pinned SHA with
  `-DROBOTS_UPSTREAM_DIR=<path>`.

Nothing above is needed for, or reachable from, the default or CI builds.

## How to run

```sh
# From the repo root. Use a build dir OUTSIDE the default/CI ones.
cmake -S . -B build-diff -DROBOTS_BUILD_DIFFERENTIAL=ON
cmake --build build-diff

# Run the harness (uses the recorded seed and count from corpus.lock/the header).
./build-diff/differential_harness
```

Optional flags:

- `--seed <n>` override the PRNG seed (decimal or `0x` hex).
- `--count <n>` override the number of generated cases (default 10000).
- `--max-report <n>` cap how many differing cases are printed (default 20).

A clean run ends with:

```
result: matcher diffs = 0, reporting diffs = 0
PASS: zero differences across 10138 cases.
```

## How it is wired (symbol-collision note)

The pristine and replica engines share the `googlebot` namespace and the
`RobotsMatcher` type name (same filenames, too), so they **cannot be linked into
one executable**. The harness therefore builds two **separate** runner
executables from one shared source, `diff_runner.cc`:

- `diff_runner_replica` links the product `robots` library (no Abseil);
- `diff_runner_pristine` compiles the pinned upstream sources with the pinned
  directory ahead on the include path (so `#include "robots.h"` resolves to the
  pristine headers) and links Abseil.

The only public-API difference between the two engines is `absl::string_view`
vs `std::string_view` on the body parameter; a `std::string` argument converts
to either, so the runner source is engine-agnostic.

`differential_harness` (the driver) regenerates the identical case stream,
launches both runners as subprocesses (their absolute paths are injected at
compile time via the `REPLICA_RUNNER` / `PRISTINE_RUNNER` defines), and diffs
their canonical per-case output line by line. When a line differs it splits it
into the matcher half and the reporting half to attribute the difference.

## Files

| File | Purpose |
| --- | --- |
| `case_generator.h` | Engine-independent deterministic case generator + recorded seed/version constants. |
| `extract_corpus.py` | Derives `corpus_data.h` from `../.upstream-pinned/` test sources. |
| `corpus_data.h` | Generated committed upstream corpus (do not hand-edit; regenerate with the script). |
| `diff_runner.cc` | Shared runner, compiled once per engine; emits canonical per-case output. |
| `differential_harness.cc` | Driver: runs both runners and diffs their output. |
| `corpus.lock` | Committed record of the seed, corpus version, and counts. |

## Regenerating the upstream corpus

The corpus is derived from the pinned upstream test files. If the pinned SHA
changes (a new validation event, not an automatic update), regenerate it:

```sh
python3 diff_harness/extract_corpus.py   # reads ../.upstream-pinned, writes corpus_data.h
```

Then bump `corpus_version` in `case_generator.h` and `corpus.lock`.
