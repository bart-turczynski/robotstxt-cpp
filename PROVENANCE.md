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
