// Copyright 2026 The robotstxt-cpp authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// File: abseil_replacement_test.cc  (slice C4)
// -----------------------------------------------------------------------------
//
// Regression suite pinning each behavior-neutral Abseil -> C++17 stdlib
// replacement made in the de-Abseiled matcher/reporting layers (PRD v3 §6.1).
// Every case exercises the replacement through the preserved PUBLIC surface and
// asserts the SPECIFIC observable behavior the replacement must preserve, so a
// broken or reverted-to-buggy replacement makes a case fail (not a tautology).
//
// Replacements pinned here (see the TEST names):
//   * absl::ascii_is* / ascii_toupper -> local AsciiIs*/AsciiToUpper helpers
//     (ASCII-only, byte-accurate: non-ASCII bytes classify all-false / unchanged)
//       - NonAscii*  cases  (non-ASCII byte handling)
//       - AsciiClassification* cases (isalpha/islower/isxdigit/toupper)
//   * absl::FixedArray<T> -> std::vector<T>
//       - FixedArray* cases (matcher pos[] buffer and index.htm rewrite buffer)
//   * absl::btree_map<int, RobotsParsedLine> -> std::map (sorted-by-line output)
//       - SortedReporting* case
//
// Note on integer conversion (absl::SimpleAtoi -> std::from_chars, PRD §6.1):
// that Abseil surface is NOT present in the vendored source at the pinned SHA
// (neither robots.cc nor reporting_robots.cc parse integers; the reporting layer
// uses a string lowercase helper, and line numbers are incremented ints, never
// parsed). There is therefore no product code path to pin with a failing-if-
// broken test, and none is fabricated here. See PROVENANCE.md (C4 section).

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "gtest/gtest.h"
#include "reporting_robots.h"
#include "robots.h"

using ::googlebot::RobotsMatcher;
using ::googlebot::RobotsParsedLine;
using ::googlebot::RobotsParsingReporter;

// MaybeEscapePattern is available to the linker but intentionally kept out of
// the public header; forward-declare it the same way robots_test.cc does.
namespace googlebot {
bool MaybeEscapePattern(const char* src, char** dst);
}  // namespace googlebot

namespace {

// Runs MaybeEscapePattern and returns the canonicalized pattern as a string.
std::string Escape(const std::string& src) {
  char* escaped_value = nullptr;
  const bool is_escaped = googlebot::MaybeEscapePattern(src.c_str(),
                                                        &escaped_value);
  std::string out = escaped_value;
  if (is_escaped) delete[] escaped_value;
  return out;
}

// Runs one URL against one robots.txt body for a single agent and returns
// whether it is allowed. Exercises the full parse + longest-match path, which
// is where the FixedArray -> std::vector replacements live.
bool Allowed(const std::string& robots, const std::string& url) {
  RobotsMatcher matcher;
  return matcher.OneAgentAllowedByRobots(robots, "FooBot", url);
}

// ---------------------------------------------------------------------------
// absl::ascii_is* -> local helpers: non-ASCII bytes must classify all-false /
// pass through unchanged, exactly as Abseil's ascii_* helpers did.
// ---------------------------------------------------------------------------

// A high-bit byte (& 0x80) is %-escaped by MaybeEscapePattern. This is the
// documented `/SanJoséSellers ==> /Sanjos%C3%A9Sellers` behavior and pins that
// the ASCII helpers leave the surrounding ASCII bytes untouched while the raw
// non-ASCII octets are escaped byte-for-byte.
TEST(AbseilReplacement, NonAsciiBytesAreEscapedByteForByte) {
  // "é" == UTF-8 0xC3 0xA9.
  EXPECT_EQ(Escape("/Sanjos\xC3\xA9"), "/Sanjos%C3%A9");
  // Multiple non-ASCII bytes, all escaped, ASCII neighbours preserved.
  EXPECT_EQ(Escape("caf\xC3\xA9/x"), "caf%C3%A9/x");
}

// A '%' followed by two NON-ASCII bytes must NOT be treated as a %-escape
// sequence, because AsciiIsXDigit must return false for every non-ASCII byte
// (a naive <cctype> isxdigit() call on a negative char would be UB / could
// classify it true). Here the '%' therefore stays literal and each non-ASCII
// byte is escaped independently.
TEST(AbseilReplacement, NonAsciiBytesNeverClassifyAsHexDigit) {
  // 0x25 '%', 0xC3, 0xA9.  Expect the '%' passed through, both octets escaped.
  EXPECT_EQ(Escape("\x25\xC3\xA9"), "%%C3%A9");
}

// AsciiIsAlpha must classify non-ASCII bytes as false. ExtractUserAgent (via
// the public IsValidUserAgentToObey) stops at the first non-[a-zA-Z_-] byte, so
// a user agent containing a non-ASCII byte is not fully consumed and is invalid.
TEST(AbseilReplacement, NonAsciiUserAgentByteIsNotAlpha) {
  EXPECT_TRUE(RobotsMatcher::IsValidUserAgentToObey("FooBot"));
  // "Foöbot": ö == 0xC3 0xB6. Extraction stops at 0xC3 -> not the whole string.
  // (Split literal so the following 'b' is not folded into the hex escape.)
  EXPECT_FALSE(RobotsMatcher::IsValidUserAgentToObey("Fo\xC3\xB6" "bot"));
}

// ---------------------------------------------------------------------------
// absl::ascii_isxdigit / islower / ascii_toupper -> local helpers: ASCII
// character classification and case folding must behave exactly as Abseil's.
// ---------------------------------------------------------------------------

// MaybeEscapePattern normalizes an existing %-escape by upper-casing its two
// hex digits (%2f -> %2F). This pins AsciiIsXDigit (recognizes the sequence),
// AsciiIsLower (detects the lowercase digit), and AsciiToUpper (folds it).
TEST(AbseilReplacement, AsciiClassificationHexNormalization) {
  EXPECT_EQ(Escape("/x%2f"), "/x%2F");   // lowercase hex folded up
  EXPECT_EQ(Escape("/x%2F"), "/x%2F");   // already upper: unchanged
  EXPECT_EQ(Escape("/x%Ab"), "/x%AB");   // mixed: only the lower digit folds
  EXPECT_EQ(Escape("/x%gg"), "/x%gg");   // 'g' not xdigit: not an escape, kept
}

// IsValidUserAgentToObey accepts exactly [a-zA-Z_-]. A digit is not alpha, so
// extraction stops and the agent is invalid; letters (either case), '-' and '_'
// are accepted. This pins AsciiIsAlpha's ASCII letter classification.
TEST(AbseilReplacement, AsciiClassificationUserAgentAlpha) {
  EXPECT_TRUE(RobotsMatcher::IsValidUserAgentToObey("FooBot"));      // mixed case
  EXPECT_TRUE(RobotsMatcher::IsValidUserAgentToObey("Foo-Bar_baz"));  // - and _
  EXPECT_FALSE(RobotsMatcher::IsValidUserAgentToObey("FooBot1"));     // digit
}

// ---------------------------------------------------------------------------
// absl::FixedArray<T> -> std::vector<T>: the matcher's pos[] scratch buffer
// (std::vector<size_t> pos(pathlen + 1)) drives longest-match wildcard / '$'
// matching, and the index.htm rewrite uses std::vector<char>. Broken sizing or
// indexing here changes match decisions.
// ---------------------------------------------------------------------------

// The '*' branch of the matcher refills pos[] with (pathlen - pos[0] + 1)
// entries; the '$' branch reads pos[numpos - 1]. These cases push the buffer
// across several positions and the end anchor.
TEST(AbseilReplacement, FixedArrayWildcardAndEndAnchorMatching) {
  const std::string robots = "user-agent: *\ndisallow: /a*b\n";
  EXPECT_FALSE(Allowed(robots, "http://x/axxxb"));  // '*' spans many positions
  EXPECT_FALSE(Allowed(robots, "http://x/ab"));      // '*' matches empty run
  EXPECT_TRUE(Allowed(robots, "http://x/axxx"));     // no trailing 'b': no match

  const std::string anchored = "user-agent: *\ndisallow: /a$\n";
  EXPECT_FALSE(Allowed(anchored, "http://x/a"));   // '$' matches end of path
  EXPECT_TRUE(Allowed(anchored, "http://x/ab"));   // '$' rejects longer path
}

// The Google-specific 'index.htm(l) -> /' rewrite builds its replacement
// pattern in a std::vector<char> (was absl::FixedArray<char>). With the rewrite
// working, an `allow: /index.html` re-allows exactly "/" over a blanket
// disallow, but nothing deeper.
TEST(AbseilReplacement, FixedArrayIndexHtmlRewriteBuffer) {
  const std::string robots =
      "user-agent: *\ndisallow: /\nallow: /index.html\n";
  EXPECT_TRUE(Allowed(robots, "http://x/"));       // rewritten "/$" re-allows "/"
  EXPECT_FALSE(Allowed(robots, "http://x/page"));  // deeper path stays disallowed
}

// ---------------------------------------------------------------------------
// absl::btree_map<int, RobotsParsedLine> -> std::map: parse_results() must emit
// lines in ascending one-based line order. Both containers key on the int line
// number and iterate ascending; this pins that observable sorted-by-line output.
// ---------------------------------------------------------------------------
TEST(AbseilReplacement, SortedReportingOutputByAscendingLine) {
  RobotsParsingReporter report;
  static const char kBody[] =
      "user-agent: *\n"   // 1
      "disallow: /a\n"    // 2
      "# a comment\n"     // 3
      "allow: /b\n"       // 4
      "disallow: /c";     // 5 (no trailing newline -> line 5 is the directive)
  googlebot::ParseRobotsTxt(kBody, &report);

  const std::vector<RobotsParsedLine> results = report.parse_results();
  ASSERT_EQ(results.size(), 5u);

  // Strictly ascending, contiguous one-based line numbers: the sorted-by-line
  // contract the std::map replacement must preserve.
  for (size_t i = 0; i < results.size(); ++i) {
    EXPECT_EQ(results[i].line_num, static_cast<int>(i) + 1)
        << "parse_results() not sorted-by-line at index " << i;
    if (i > 0) {
      EXPECT_LT(results[i - 1].line_num, results[i].line_num);
    }
  }

  // Directive tags land at their ascending source lines.
  EXPECT_EQ(results[1].tag_name, RobotsParsedLine::kDisallow);  // line 2
  EXPECT_EQ(results[3].tag_name, RobotsParsedLine::kAllow);     // line 4
  EXPECT_EQ(results[4].tag_name, RobotsParsedLine::kDisallow);  // line 5
}

}  // namespace
