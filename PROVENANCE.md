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

| File                        | SHA-256 |
|-----------------------------|---------|
| `robots.cc`                 | `d05a7fa74c0e8b9fc440d15f4c3ae9a80873c0d950e569de2926303fd75ed473` |
| `robots.h`                  | `912f3a5c6821f8a9a97269f9598b7f29ac1edf1ee20e173f1dd2d9ca48fb0bd5` |
| `reporting_robots.cc`       | `36153bf94160e063e8832a068240181ddab286ad1f705ef4d045198ff00a321f` |
| `reporting_robots.h`        | `e000f8914e574e0273ef99e8de8dc50bb48b7922068bd788d11a43780502216d` |

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
