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
