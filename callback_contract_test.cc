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
// File: callback_contract_test.cc  (slice C4)
// -----------------------------------------------------------------------------
//
// The callback-contract test (PRD v3 §6.6, "Correlation mechanism", lines
// ~529-544). It proves that ParseRobotsTxt() emits Allow/Disallow directive
// values keyed by the SAME one-based line numbers that RobotsMatcher's
// matching_line() returns, so R (slice R3) can correlate a winning match to its
// directive type and value purely by line number.
//
// Mechanism, exactly as the PRD prescribes:
//   1. Run the matcher for the URL and read matching_line() (a positive line).
//   2. Run ParseRobotsTxt() once over the SAME body with a private, read-only
//      RobotsParseHandler collector that records Allow/Disallow callback type
//      and value keyed by line number.
//   3. Join matching_line() into that per-source lookup and assert type, value,
//      AND line.
//
// Value contract asserted here: the collected value is the directive value the
// parse callback emits -- comments already removed and surrounding whitespace
// already stripped by ParseRobotsTxt(), and NOT reconstructed by R from the
// URL. For the percent-escape case the escape sequence is preserved verbatim.
// (The chosen percent-escape input is invariant under the parser's internal
// MaybeEscapePattern canonicalization -- already-uppercase, ASCII-only hex --
// so the asserted value is unambiguously the pre-escape directive text; see
// _worklog/C4.md for why that input was chosen.)

#include <map>
#include <string>
#include <string_view>

#include "gtest/gtest.h"
#include "robots.h"

using ::googlebot::RobotsMatcher;

namespace {

// A private, read-only parse handler that records only what the correlation
// needs: the type and value of each Allow/Disallow callback, keyed by the
// one-based line number ParseRobotsTxt() reports. It never matches, escapes, or
// mutates anything -- it is the C++ analogue of the R3 collector.
class CallbackCollector : public googlebot::RobotsParseHandler {
 public:
  enum class Kind { kAllow, kDisallow };
  struct Entry {
    Kind kind;
    std::string value;
  };

  void HandleRobotsStart() override {}
  void HandleRobotsEnd() override {}
  void HandleUserAgent(int, std::string_view) override {}
  void HandleAllow(int line_num, std::string_view value) override {
    entries_[line_num] = Entry{Kind::kAllow, std::string(value)};
  }
  void HandleDisallow(int line_num, std::string_view value) override {
    entries_[line_num] = Entry{Kind::kDisallow, std::string(value)};
  }
  void HandleSitemap(int, std::string_view) override {}
  void HandleUnknownAction(int, std::string_view, std::string_view) override {}

  const std::map<int, Entry>& entries() const { return entries_; }

 private:
  std::map<int, Entry> entries_;
};

const char* KindName(CallbackCollector::Kind kind) {
  return kind == CallbackCollector::Kind::kAllow ? "allow" : "disallow";
}

// Runs the full correlation for one (robots.txt, url) pair and asserts the
// joined callback entry has the expected line, type, and value.
void ExpectCorrelated(const std::string& robots, const std::string& url,
                      int expected_line, CallbackCollector::Kind expected_kind,
                      const std::string& expected_value) {
  // Step 1: match, and obtain the winning one-based line.
  RobotsMatcher matcher;
  matcher.OneAgentAllowedByRobots(robots, "FooBot", url);
  const int line = matcher.matching_line();
  ASSERT_GT(line, 0) << "expected a positive matching line for " << url;
  EXPECT_EQ(line, expected_line);

  // Step 2: collect Allow/Disallow callback values keyed by line, read-only.
  CallbackCollector collector;
  googlebot::ParseRobotsTxt(robots, &collector);

  // Step 3: join matching_line() into the per-source callback lookup.
  const auto it = collector.entries().find(line);
  ASSERT_NE(it, collector.entries().end())
      << "matching_line() == " << line
      << " has no Allow/Disallow callback entry (correlation would break)";

  EXPECT_EQ(KindName(it->second.kind), KindName(expected_kind));
  EXPECT_EQ(it->second.value, expected_value);
}

// A Disallow whose directive carries a trailing comment: the callback value has
// the comment removed. Type=disallow, value="/foo", line=2.
TEST(CallbackContract, DisallowWithComment) {
  ExpectCorrelated("user-agent: *\ndisallow: /foo  # secret area\n",
                   "http://example.com/foo",
                   /*line=*/2, CallbackCollector::Kind::kDisallow, "/foo");
}

// A Disallow with leading and trailing whitespace around the value: the
// callback value is whitespace-stripped. Type=disallow, value="/bar/baz",
// line=2.
TEST(CallbackContract, DisallowWithSurroundingWhitespace) {
  ExpectCorrelated("user-agent: *\ndisallow:  \t /bar/baz \t \n",
                   "http://example.com/bar/baz",
                   /*line=*/2, CallbackCollector::Kind::kDisallow, "/bar/baz");
}

// A Disallow with a percent-escaped path. The callback value preserves the
// escape sequence verbatim and comes from the DIRECTIVE, not the URL: the
// directive is "/a%2F" (a prefix) while the matched URL path is "/a%2Fbcd", so
// the asserted value can only have come from the parse callback. Type=disallow,
// value="/a%2F", line=2.
TEST(CallbackContract, DisallowWithPercentEscapedPath) {
  ExpectCorrelated("user-agent: *\ndisallow: /a%2F\n",
                   "http://example.com/a%2Fbcd",
                   /*line=*/2, CallbackCollector::Kind::kDisallow, "/a%2F");
}

// An Allow that wins by longest match over a blanket Disallow, with a trailing
// comment on the Allow line. Covers the Allow callback type and confirms the
// winning line is the Allow line (3), not the Disallow line (2).
TEST(CallbackContract, AllowWinsOverDisallowWithComment) {
  ExpectCorrelated("user-agent: *\ndisallow: /\nallow: /public # ok\n",
                   "http://example.com/public",
                   /*line=*/3, CallbackCollector::Kind::kAllow, "/public");
}

}  // namespace
