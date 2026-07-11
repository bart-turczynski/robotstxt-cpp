#!/usr/bin/env python3
"""Derive the differential harness's "upstream corpus" from the pinned upstream
test files.

This reads the pristine (Abseil-using) upstream test sources under
``.upstream-pinned/`` and emits ``corpus_data.h`` -- a committed C++ header
holding a deterministic, order-stable list of (robots_body, user_agent, url)
triples plus a set of parse-only bodies for the reporting layer.

The harness only ever feeds *identical input* to both engines, so extraction
fidelity does not affect the correctness of the differential comparison: even a
mis-resolved body still exercises both engines with the exact same bytes. The
goal here is breadth of realistic upstream inputs, not reproducing the tests'
expected values.

Resolution strategy (best-effort, deterministic):
  * Track ``absl::string_view`` / ``std::string`` / ``const char[]`` definitions
    whose right-hand side is one or more adjacent C string literals.
  * Replay definitions and ``IsUserAgentAllowed(a, b, c)`` call sites in source
    order; resolve each argument to the most recent literal definition of that
    identifier (or an inline literal). ``absl::string_view()`` resolves to "".
  * Skip any call whose arguments use runtime construction (``absl::StrCat``,
    function calls, etc.) -- those are not needed for coverage.
  * Collect every ``ParseRobotsTxt(NAME, ...)`` body as a reporting-only case.

Run from the repo root:  python3 diff_harness/extract_corpus.py
"""
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
UP = REPO / ".upstream-pinned"

# A single C string literal: contents with escapes preserved.
LIT = re.compile(r'"((?:[^"\\]|\\.)*)"')
# A variable definition ending in ';' whose RHS we will inspect for literals.
DEF = re.compile(
    r'(?:static\s+)?const\s+'
    r'(?:absl::string_view|std::string|char)\s+'
    r'(\w+)\s*(?:\[\s*\])?\s*=\s*(.*?);',
    re.DOTALL,
)
CALL_OPEN = re.compile(r'IsUserAgentAllowed\s*\(')
PARSE = re.compile(r'ParseRobotsTxt\s*\(\s*(\w+)\s*,', re.DOTALL)


def balanced_args(src, open_paren_idx):
    """Given the index of the '(' that opens a call, return (argstr, end_idx)
    where argstr is the text between the matched parens (respecting nested
    parens and string literals)."""
    depth, i, in_str = 0, open_paren_idx, False
    start = open_paren_idx + 1
    while i < len(src):
        c = src[i]
        if in_str:
            if c == "\\":
                i += 2
                continue
            if c == '"':
                in_str = False
        else:
            if c == '"':
                in_str = True
            elif c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
                if depth == 0:
                    return src[start:i], i
        i += 1
    return src[start:], len(src)


def literals_only(text):
    """If ``text`` is nothing but adjacent string literals (and whitespace),
    return the concatenated literal content; otherwise None."""
    stripped = LIT.sub("", text)
    if stripped.strip():
        return None
    return "".join(m.group(1) for m in LIT.finditer(text))


def split_top_level(argstr):
    """Split a call argument list on top-level commas."""
    args, depth, cur, i = [], 0, [], 0
    in_str = False
    while i < len(argstr):
        c = argstr[i]
        if in_str:
            cur.append(c)
            if c == "\\":
                cur.append(argstr[i + 1])
                i += 2
                continue
            if c == '"':
                in_str = False
        else:
            if c == '"':
                in_str = True
                cur.append(c)
            elif c in "([{":
                depth += 1
                cur.append(c)
            elif c in ")]}":
                depth -= 1
                cur.append(c)
            elif c == "," and depth == 0:
                args.append("".join(cur))
                cur = []
            else:
                cur.append(c)
        i += 1
    if cur:
        args.append("".join(cur))
    return [a.strip() for a in args]


def resolve(arg, defs):
    if arg == "absl::string_view()":
        return ""
    lit = literals_only(arg)
    if lit is not None:
        return lit
    if re.fullmatch(r"\w+", arg) and arg in defs:
        return defs[arg]
    return None


def main():
    triples = []  # (body, ua, url)
    bodies = []   # reporting-only bodies
    seen_triples = set()
    seen_bodies = set()

    for name in ("robots_test.cc", "reporting_robots_test.cc"):
        src = (UP / name).read_text(encoding="utf-8", errors="replace")

        # Merge definitions and call/parse sites by source offset, replaying in
        # order so identifier resolution uses the most recent literal binding.
        events = []
        for m in DEF.finditer(src):
            events.append((m.start(), "def", m.group(1), m.group(2)))
        for m in CALL_OPEN.finditer(src):
            argstr, _ = balanced_args(src, m.end() - 1)
            events.append((m.start(), "call", argstr, None))
        for m in PARSE.finditer(src):
            events.append((m.start(), "parse", m.group(1), None))
        events.sort(key=lambda e: e[0])

        defs = {}
        for _, kind, a, b in events:
            if kind == "def":
                val = literals_only(b)
                if val is not None:
                    defs[a] = val
            elif kind == "call":
                args = split_top_level(a)
                if len(args) != 3:
                    continue
                r = [resolve(a, defs) for a in args]
                if any(x is None for x in r):
                    continue
                body, ua, url = r
                key = (body, ua, url)
                if key not in seen_triples:
                    seen_triples.add(key)
                    triples.append(key)
            elif kind == "parse":
                ident = a
                if ident in defs:
                    b = defs[ident]
                    if b not in seen_bodies:
                        seen_bodies.add(b)
                        bodies.append(b)

    # Every reporting body is also a good matcher case; the parse-only bodies
    # additionally get a couple of concrete UA/URL probes so the matcher diff
    # exercises them too.
    for b in bodies:
        for ua, url in (("FooBot", "http://foo.bar/x/y"), ("*", "http://foo.bar/")):
            key = (b, ua, url)
            if key not in seen_triples:
                seen_triples.add(key)
                triples.append(key)

    out = emit(triples, bodies)
    (Path(__file__).resolve().parent / "corpus_data.h").write_text(out, encoding="utf-8")
    sys.stderr.write(
        f"corpus_data.h: {len(triples)} matcher triples, "
        f"{len(bodies)} reporting bodies\n"
    )


def cpp_escape(s):
    out = []
    for ch in s:
        b = ord(ch)
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\t":
            out.append("\\t")
        elif ch == "\r":
            out.append("\\r")
        elif 32 <= b < 127:
            out.append(ch)
        else:
            # Emit raw UTF-8 bytes as hex escapes; break the run so following
            # hex/printable chars aren't absorbed into the escape.
            for byte in ch.encode("utf-8"):
                out.append(f'\\x{byte:02x}""')
    return "".join(out)


def emit(triples, bodies):
    lines = [
        "// GENERATED by diff_harness/extract_corpus.py -- do not edit by hand.",
        "// Upstream corpus for the maintainer-only differential harness, derived",
        "// from the pinned upstream test sources in .upstream-pinned/. See",
        "// extract_corpus.py for the derivation method.",
        "#ifndef ROBOTSTXT_DIFF_HARNESS_CORPUS_DATA_H_",
        "#define ROBOTSTXT_DIFF_HARNESS_CORPUS_DATA_H_",
        "",
        "#include <string>",
        "#include <vector>",
        "",
        "#include \"case_generator.h\"",
        "",
        "namespace robotstxt_diff {",
        "",
        "// Returns the upstream-derived corpus of differential cases.",
        "inline std::vector<DiffCase> UpstreamCorpus() {",
        "  std::vector<DiffCase> c;",
    ]
    for body, ua, url in triples:
        lines.append(
            f'  c.push_back(DiffCase{{"{cpp_escape(body)}", '
            f'"{cpp_escape(ua)}", "{cpp_escape(url)}"}});'
        )
    lines += [
        "  return c;",
        "}",
        "",
        "}  // namespace robotstxt_diff",
        "#endif  // ROBOTSTXT_DIFF_HARNESS_CORPUS_DATA_H_",
        "",
    ]
    return "\n".join(lines)


if __name__ == "__main__":
    main()
