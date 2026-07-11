# Provenance

This library vendors a de-Abseiled replica of Google's `robotstxt` matcher and
reporting layer, pinned to a single validated upstream commit.

## Upstream source of record

- **Repository:** https://github.com/google/robotstxt
- **Pinned commit:** `22b355ff855419e6a3ff8ff09c0ad7fdb17116f9`
  (the 2026-04-01 WASM merge; the matcher/reporting state at that commit is
  vendored, `robots_wasm.cc` is excluded from the product surface).
- **Import date:** 2026-07-11.

## Reproducing the frozen input

The pristine upstream files are fetched into `.upstream-pinned/` (gitignored) as
the import + diff base. To reproduce:

```sh
SHA=22b355ff855419e6a3ff8ff09c0ad7fdb17116f9
for f in robots.cc robots.h robots_test.cc \
         reporting_robots.cc reporting_robots.h reporting_robots_test.cc LICENSE; do
  curl -sSL -o ".upstream-pinned/$f" \
    "https://raw.githubusercontent.com/google/robotstxt/$SHA/$f"
done
```

### Pristine file checksums (SHA-256, pinned commit)

These are the checksums of the pristine upstream files as fetched into
`.upstream-pinned/` by the loop above. After a re-fetch, verify against these
values before importing (see the sync procedure below).

| File                        | SHA-256 |
|-----------------------------|---------|
| `robots.cc`                 | `d05a7fa74c0e8b9fc440d15f4c3ae9a80873c0d950e569de2926303fd75ed473` |
| `robots.h`                  | `912f3a5c6821f8a9a97269f9598b7f29ac1edf1ee20e173f1dd2d9ca48fb0bd5` |
| `reporting_robots.cc`       | `36153bf94160e063e8832a068240181ddab286ad1f705ef4d045198ff00a321f` |
| `reporting_robots.h`        | `e000f8914e574e0273ef99e8de8dc50bb48b7922068bd788d11a43780502216d` |
| `robots_test.cc`            | `9868caef2ba44991331e90418f0e45a0c13893b0e4e11c8b0385166face9d670` |
| `reporting_robots_test.cc`  | `3843c4bf3ca390cc094d14212dd2dece5b99c74628aeb9a54f82fc45e490ebc8` |
| `LICENSE`                   | `c79a7fea0e3cac04cd43f20e7b648e5a0ff8fa5344e644b0ee09ca1162b62747` |

The repository `LICENSE` is a byte-for-byte copy of the upstream Apache-2.0
`LICENSE` (identical SHA-256 above).

## Vendored files and local changes

Populated per slice as files are imported and de-Abseiled (C1: matcher; C3:
reporting). A diffable change manifest and the repeatable sync procedure are
finalized in slice C6.

### C1 — matcher (`robots.cc`, `robots.h`)

Imported from the pinned upstream (`.upstream-pinned/robots.{cc,h}`, SHAs
recorded above) and de-Abseiled to the C++17 standard library. Namespace
(`googlebot`), class/function names, file organization, and the preserved public
surface (`RobotsMatcher`, `AllowedByRobots`, `OneAgentAllowedByRobots`,
`ParseRobotsTxt`, `GetPathParamsQuery`, `MaybeEscapePattern`, `matching_line()`)
are unchanged. All edits are behavior-neutral Abseil → stdlib substitutions:

| Upstream Abseil surface | Local replacement |
|---|---|
| `absl/strings/string_view.h`, `absl::string_view` | `<string_view>`, `std::string_view` |
| `absl::FixedArray<T>` (`<size_t>`, `<char>`) | `std::vector<T>` |
| `absl::StartsWith` | local `StartsWith` helper |
| `absl::StartsWithIgnoreCase` | local `StartsWithIgnoreCase` helper |
| `absl::EqualsIgnoreCase` | local `EqualsIgnoreCaseAscii` helper |
| `absl::StripAsciiWhitespace` | local `StripAsciiWhitespace` helper |
| `absl::ascii_isxdigit` / `islower` / `isalpha` | local `AsciiIsXDigit` / `AsciiIsLower` / `AsciiIsAlpha` |
| `absl::ascii_toupper` | local `AsciiToUpper` (with `AsciiToLower` for case compares) |
| `absl::ClippedSubstr(v, pos)` | `v.substr(pos)` (pos is always in range here) |
| `ABSL_ASSERT` | `assert` (`<cassert>`) |

The local ASCII helpers are internal-linkage, ASCII-only, and byte-accurate:
each predicate classifies a single byte so non-ASCII bytes behave exactly as the
Abseil `ascii_*` helpers did (all-false / unchanged). `absl::SimpleAtoi`
(`<charconv>` `std::from_chars` per PRD §6.1) is not referenced by the matcher
and appears only in the reporting layer imported in slice C3. No product code
includes or links Abseil.

Supporting build files added in this slice (not upstream copies):

- `CMakeLists.txt` — minimal offline C++17 build: `robots` static library +
  `robots_smoke` example binary, with an OFF-by-default `ROBOTS_BUILD_TESTS`
  toggle (tests land in C2).
- `robots_smoke.cc` — tiny offline tracer-bullet binary printing `disallow` for
  `http://example.com/private/x` and `allow` for `http://example.com/public/x`
  against `user-agent: *\ndisallow: /private`.

### C2 — matcher test adaptations (`robots_test.cc`)

Imported from the pinned upstream (`.upstream-pinned/robots_test.cc`) and
adapted with **only mechanical, behavior-neutral** changes. No assertion,
fixture, `TEST` name, URL, user-agent string, or expected value was changed; the
21 upstream cases (including `ID_Encoding`) run byte-for-byte identical inputs
and expectations. All edits mirror C1's Abseil → C++17 stdlib substitution set:

| Upstream Abseil / build surface | Local replacement |
|---|---|
| `#include "absl/strings/string_view.h"` | removed; added `<string_view>` |
| `#include "absl/strings/str_cat.h"` | removed (no replacement include needed) |
| `absl::string_view` (locals, `IsUserAgentAllowed` param, `RobotsStatsReporter` `Handle*` overrides, `IsValidUserAgentToObey(absl::string_view())`) | `std::string_view` |
| `absl::StrAppend(&s, a, b, …)` | `s.append(a).append(b)…` (identical bytes) |
| `absl::StrCat(a, b, …)` | `std::string(a).append(b)…` (identical bytes) |

The `RobotsStatsReporter` `Handle*` methods **must** take `std::string_view` to
still override the C1-de-Abseiled `googlebot::RobotsParseHandler` virtuals; this
is a required mechanical consequence of C1, not an input/expectation change. The
`StrAppend`/`StrCat` rewrites build the exact same string bytes that Abseil
produced, so every matched URL and robots.txt body is unchanged. `robots.h` is
included unchanged; no test-only plumbing change to the library was needed.

Supporting build change (not an upstream copy):

- `CMakeLists.txt` — under the existing OFF-by-default `ROBOTS_BUILD_TESTS`
  toggle, wires GoogleTest as a **test-only, never-downloaded** dependency via
  `find_package(GTest REQUIRED)` (system install; `GTest::gtest` /
  `GTest::gtest_main`), builds the `robots_test` executable, and registers its
  cases with CTest via `gtest_discover_tests`. No FetchContent, no network. The
  production `robots` library and `robots_smoke` binary are unchanged and link
  no Abseil and no test dependency.

### C3 — reporting layer + test adaptations (`reporting_robots.{cc,h}`, `reporting_robots_test.cc`)

Imported from the pinned upstream (`.upstream-pinned/reporting_robots.{cc,h,test.cc}`,
SHAs recorded above) and de-Abseiled to the C++17 standard library, mirroring
C1's substitution style. Namespace (`googlebot`), class/function names, file
organization, and the preserved correlation surface (`RobotsParsingReporter`,
`RobotsParsedLine`, the `RobotsTagName` enum, and all `Handle*` /
`ReportLineMetadata` / accessor signatures) are unchanged. All library edits are
behavior-neutral Abseil → stdlib substitutions:

| Upstream Abseil surface | Local replacement |
|---|---|
| `absl/container/btree_map.h`, `absl::btree_map<int, RobotsParsedLine>` | `<map>`, `std::map<int, RobotsParsedLine>` |
| `absl/strings/string_view.h`, `absl::string_view` (all `Handle*` params) | `<string_view>`, `std::string_view` |
| `absl/strings/ascii.h`, `absl::AsciiStrToLower(action)` | local internal-linkage `AsciiStrToLower` helper (ASCII-only, byte-accurate: only `A`–`Z` shifted, non-ASCII bytes unchanged) |

**`btree_map` → `std::map` (sorted-by-line output preserved).** The results
container is keyed by the `int` line number. `absl::btree_map` and `std::map`
both keep keys in ascending order, so iterating in `parse_results()` yields the
identical observable sorted-by-line sequence. This is verified by the adapted
`LinesNumbersAreCountedCorrectly` test, which indexes `parse_results()[line_num
- 1]` for a densely numbered file (lines 1–16) and matches each entry to its
line — only an ascending, contiguous ordering satisfies those assertions, and it
does (test green). No expected value or assertion was touched to accommodate the
swap. `absl::SimpleAtoi` (PRD §6.1 → `std::from_chars`) is not referenced by the
reporting layer. No product code includes or links Abseil.

The reporting-layer test (`reporting_robots_test.cc`) was adapted with **only
mechanical, behavior-neutral** changes. No assertion, fixture, `TEST` name,
robots.txt input, or expected value was changed; both upstream cases run
byte-for-byte identical inputs and expectations. Mechanical categories:

| Upstream Abseil / build surface | Local replacement |
|---|---|
| `#include "absl/strings/string_view.h"` | removed; added `<string_view>` |
| `#include "absl/strings/str_cat.h"` / `str_split.h` | removed (replaced inline, below) |
| `absl::string_view` (locals, `expectLineToParseTo` param) | `std::string_view` |
| `absl::StrCat(a, b, …)` (debug-only `LineMetadataToString` / `RobotsParsedLineToString`, used only in `operator<<` failure messages) | `std::string(...) + ...` with `std::to_string` for the `int`/`bool` fields — identical bytes (`bool` → `"0"`/`"1"`, `int` → decimal) |
| `absl::StrAppend(&s, a, …)` | `s.append(a)…` (identical bytes) |
| `absl::StrSplit(text, '\n')` → `std::vector<absl::string_view>` | local `SplitLines` harness helper → `std::vector<std::string_view>` (trailing-delimiter empty piece preserved; used only for failure-message context) |

The `Handle*` overrides in the reporting surface **must** take `std::string_view`
to keep overriding the C1-de-Abseiled `RobotsParseHandler` virtuals; this is a
required mechanical consequence of C1, not a signature change of intent.

**License-header addition (C6, comment-only).** The upstream reporting sources
ship *without* a per-file copyright/license header — they are covered only by
the repository-level Apache-2.0 `LICENSE`. To ensure every vendored source file
carries the applicable notice, C6 prepends the standard Apache-2.0 header block
(with Google's copyright and a derivation/de-Abseil note pointing here) to
`reporting_robots.cc` and `reporting_robots.h`. This is a leading comment block
only; it adds no code and changes no behavior (verified by the unchanged 35/35
ctest run). The matcher files `robots.cc`/`robots.h` already carried the upstream
`Copyright 1999 Google LLC` Apache-2.0 header, which is preserved verbatim.

Supporting build change (not an upstream copy):

- `CMakeLists.txt` — adds `reporting_robots.cc` to the production `robots`
  static library, and (under the existing `ROBOTS_BUILD_TESTS` toggle) builds
  the `reporting_robots_test` executable against the same test-only,
  never-downloaded GTest and registers it with CTest via
  `gtest_discover_tests`. No FetchContent, no network.

### C4 — regression + callback-contract tests (`abseil_replacement_test.cc`, `callback_contract_test.cc`)

New, project-authored test files (not upstream copies). They add no product
code and change nothing in `robots.{cc,h}`, `reporting_robots.{cc,h}`, or the
adapted upstream suites; they only pin behavior through the preserved public
surface. Wired under the existing OFF-by-default `ROBOTS_BUILD_TESTS` toggle
against the same test-only, never-downloaded GTest (`find_package` +
`gtest_discover_tests`). No Abseil, no FetchContent, no network.

**`abseil_replacement_test.cc` — one dedicated failing-if-broken case per
replacement.** Each `TEST` asserts the specific observable behavior the
Abseil -> C++17 stdlib substitution must preserve:

| Abseil surface replaced | Local replacement | Pinning test(s) |
|---|---|---|
| `absl::ascii_is*` (non-ASCII bytes classify all-false / unchanged) | `AsciiIs*` helpers | `NonAsciiBytesAreEscapedByteForByte`, `NonAsciiBytesNeverClassifyAsHexDigit`, `NonAsciiUserAgentByteIsNotAlpha` |
| `absl::ascii_isxdigit` / `islower` / `ascii_toupper` (ASCII classification + case fold) | `AsciiIsXDigit` / `AsciiIsLower` / `AsciiToUpper` | `AsciiClassificationHexNormalization`, `AsciiClassificationUserAgentAlpha` |
| `absl::FixedArray<size_t>` / `<char>` | `std::vector<size_t>` (matcher `pos[]`), `std::vector<char>` (index.htm rewrite) | `FixedArrayWildcardAndEndAnchorMatching`, `FixedArrayIndexHtmlRewriteBuffer` |
| `absl::btree_map<int, RobotsParsedLine>` (sorted-by-line output) | `std::map` | `SortedReportingOutputByAscendingLine` |

The non-ASCII / classification cases go through the public `MaybeEscapePattern`
and `RobotsMatcher::IsValidUserAgentToObey`; the `FixedArray` cases through
`RobotsMatcher::OneAgentAllowedByRobots` (wildcard, `$` anchor, and the
`index.htm(l) -> /` rewrite); the sorted-output case through
`RobotsParsingReporter::parse_results()`.

*Integer conversion (`absl::SimpleAtoi` -> `std::from_chars`, PRD §6.1) has no
case because that Abseil surface is not present in the vendored source at the
pinned SHA* — neither `robots.cc` nor `reporting_robots.cc` parses integers
(line numbers are incremented `int`s; the reporting layer uses only the
`AsciiStrToLower` string helper). There is no product code path to pin with a
failing-if-broken test, so none is fabricated. This matches the C1/C3 notes
above that `SimpleAtoi` is unreferenced.

**`callback_contract_test.cc` — the callback-contract correlation test (PRD
§6.6).** A private, read-only `RobotsParseHandler` collector records
Allow/Disallow callback type + value keyed by one-based line; the matcher is run
to obtain `matching_line()`; the positive line is joined into the collector's
lookup and the joined entry's **type, value, and line** are asserted. Cases:
`DisallowWithComment` (comment removed), `DisallowWithSurroundingWhitespace`
(leading/trailing whitespace stripped), `DisallowWithPercentEscapedPath`
(percent-escape preserved verbatim; the directive is a prefix of the URL path so
the asserted value can only originate from the parse callback, not from the
URL), and `AllowWinsOverDisallowWithComment` (Allow type; winning line is the
Allow line). The asserted value is the value the parse callback emits — comment
and whitespace already handled by `ParseRobotsTxt()`. The percent-escape input
is chosen invariant under the parser's internal `MaybeEscapePattern`
canonicalization (already-uppercase, ASCII-only hex), so the asserted value is
unambiguously the pre-escape directive text. See `_worklog/C4.md`.

### C5 — differential harness (maintainer tooling, not vendored upstream)

`diff_harness/` and its build (`build-diff/`) are **project-authored maintainer
tooling**, not copied upstream source, so they are not part of the vendored
provenance surface and carry no upstream checksum. The harness builds pristine
upstream and this replica from a local source path and diffs matcher and
reporting output over the upstream corpus plus deterministic generated cases; it
never downloads source and is excluded from the offline product build (PRD §7
C5). It is noted here only because it consumes the same pinned
`.upstream-pinned/` frozen input as the reproduce step below.

## Complete vendored file inventory

The tracked source tree contains exactly these files. Only the **upstream-derived**
files below carry a pinned-state checksum (next section); the project-authored
files evolve independently of the upstream SHA.

| File | Origin | Header |
|---|---|---|
| `robots.cc` | upstream-derived (de-Abseiled) | preserved `Copyright 1999 Google LLC` Apache-2.0 |
| `robots.h` | upstream-derived (de-Abseiled) | preserved `Copyright 1999 Google LLC` Apache-2.0 |
| `reporting_robots.cc` | upstream-derived (de-Abseiled) | Apache-2.0 header added in C6 (upstream had none) |
| `reporting_robots.h` | upstream-derived (de-Abseiled) | Apache-2.0 header added in C6 (upstream had none) |
| `robots_test.cc` | upstream-derived (adapted) | preserved upstream Apache-2.0 |
| `reporting_robots_test.cc` | upstream-derived (adapted) | preserved upstream Apache-2.0 |
| `abseil_replacement_test.cc` | project-authored (C4) | — |
| `callback_contract_test.cc` | project-authored (C4) | — |
| `robots_smoke.cc` | project-authored (C1) | — |
| `CMakeLists.txt` | project-authored | — |
| `LICENSE` | upstream Apache-2.0 (verbatim) | — |
| `NOTICE` | project-authored | — |
| `PROVENANCE.md`, `README.md` | project-authored | — |
| `diff_harness/` | project-authored (C5) | — |

## Sync / re-sync procedure

A "sync" reproduces the vendored engine from the pinned upstream. The **online**
re-fetch step is needed only for a fresh sync of the frozen input; the
**reproduce / verify** step runs fully **offline** against the recorded
checksums below. The offline build, CI, and R-package install never re-fetch.

**1. Re-fetch the frozen input (online, maintainer-only, only if `.upstream-pinned/`
is absent).** Run the fetch loop under "Reproducing the frozen input" above at
the pinned SHA `22b355ff855419e6a3ff8ff09c0ad7fdb17116f9`. `.upstream-pinned/`
is gitignored and must never be committed.

**2. Verify the frozen input (offline).** Confirm the fetched pristine files
match the pinned commit before importing. The expected hashes are embedded
inline, so no separate manifest file is needed:

```sh
shasum -a 256 -c - <<'EOF'
d05a7fa74c0e8b9fc440d15f4c3ae9a80873c0d950e569de2926303fd75ed473  .upstream-pinned/robots.cc
912f3a5c6821f8a9a97269f9598b7f29ac1edf1ee20e173f1dd2d9ca48fb0bd5  .upstream-pinned/robots.h
36153bf94160e063e8832a068240181ddab286ad1f705ef4d045198ff00a321f  .upstream-pinned/reporting_robots.cc
e000f8914e574e0273ef99e8de8dc50bb48b7922068bd788d11a43780502216d  .upstream-pinned/reporting_robots.h
9868caef2ba44991331e90418f0e45a0c13893b0e4e11c8b0385166face9d670  .upstream-pinned/robots_test.cc
3843c4bf3ca390cc094d14212dd2dece5b99c74628aeb9a54f82fc45e490ebc8  .upstream-pinned/reporting_robots_test.cc
c79a7fea0e3cac04cd43f20e7b648e5a0ff8fa5344e644b0ee09ca1162b62747  .upstream-pinned/LICENSE
EOF
```

A mismatch means the upstream content at that path changed or the SHA moved —
treat it as a new validation event (PRD §6.1), not an automatic update.

**3. Re-apply local changes.** Copy each upstream file to its repo path and
re-apply exactly the behavior-neutral edits catalogued in "Vendored files and
local changes" above (Abseil -> C++17 stdlib substitutions per file; plus the
C6 comment-only Apache-2.0 header on the two reporting files). No algorithm,
namespace, signature, assertion, or expected-value change is introduced.

**4. Verify the vendored tree reproduces the pinned state (offline).** The
committed de-Abseiled files are the source of truth and are checksummed below,
so this check needs no network and no `.upstream-pinned/` cache — it runs from a
clean checkout, from the repo root, with the expected hashes embedded inline:

```sh
shasum -a 256 -c - <<'EOF'
e6d3b68701dd4c9062c7beecf973bab9834791ac1168e2907d5bc9dd9b7e32c3  robots.cc
a4c0528ea5d2f1e759f2d452e86b0e5f5c85e2617fd2f76f887798a15376cc38  robots.h
d45ce6c19e78df8c259e8dde9cf8fb7c2801cc6fdc63610c1885631122ecb186  reporting_robots.cc
854b19b1353cf92636bb9da62d80e4a4d8fa69182c42858efd6c3e6feb0302ca  reporting_robots.h
c5dcaf7261a7d8c40d440bacdd254ec449587afb6a0b5549fb69c81c65b275ef  robots_test.cc
c248202fef53b760af2ea303d32399b2901f00c0e270b88e369c621e07cb34de  reporting_robots_test.cc
EOF
```

**5. Re-validate.** Build and run the full gate; it must stay green:

```sh
cmake -S . -B build -DROBOTS_BUILD_TESTS=ON && cmake --build build
ctest --test-dir build --output-on-failure   # expect 35/35 passed
```

### Offline integrity checksums (vendored, upstream-derived)

SHA-256 of the committed de-Abseiled / adapted files at the pinned vendored
state. A clean checkout that matches these hashes is byte-for-byte the audited
vendored engine; verification is fully offline (no network, no
`.upstream-pinned/` needed). Recompute with `shasum -a 256 <file>`.

| File | SHA-256 (vendored, pinned state) |
|---|---|
| `robots.cc`                | `e6d3b68701dd4c9062c7beecf973bab9834791ac1168e2907d5bc9dd9b7e32c3` |
| `robots.h`                 | `a4c0528ea5d2f1e759f2d452e86b0e5f5c85e2617fd2f76f887798a15376cc38` |
| `reporting_robots.cc`      | `d45ce6c19e78df8c259e8dde9cf8fb7c2801cc6fdc63610c1885631122ecb186` |
| `reporting_robots.h`       | `854b19b1353cf92636bb9da62d80e4a4d8fa69182c42858efd6c3e6feb0302ca` |
| `robots_test.cc`           | `c5dcaf7261a7d8c40d440bacdd254ec449587afb6a0b5549fb69c81c65b275ef` |
| `reporting_robots_test.cc` | `c248202fef53b760af2ea303d32399b2901f00c0e270b88e369c621e07cb34de` |

The `shasum -a 256 -c` recipe in step 4 embeds these same hashes inline, so the
offline reproduce/verify check runs from a clean checkout without any separate
manifest file.
