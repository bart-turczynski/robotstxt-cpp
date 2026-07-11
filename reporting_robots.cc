#include "reporting_robots.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace googlebot {
namespace {
// ASCII-only lowercase of a whole string (replaces absl::AsciiStrToLower).
// Byte-accurate: only 'A'..'Z' are shifted; every other byte (including
// non-ASCII) is left unchanged, exactly as the Abseil helper behaved.
std::string AsciiStrToLower(std::string_view s) {
  std::string result(s);
  for (char& c : result) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c + ('a' - 'A'));
    }
  }
  return result;
}
}  // namespace

// The kUnsupportedTags tags are popular tags in robots.txt files, but Google
// doesn't use them for anything. Other search engines may, however, so we
// parse them out so users of the library can highlight them for their own
// users if they so wish.
// We are using the HTTP Archive custom_metrics.robots_txt dataset to identify
// the top 10 tags and append new tags to this list.
// https://github.com/HTTPArchive/custom-metrics
// These are different from the "unknown" tags, since we know that these may
// have some use cases; to the best of our knowledge other tags we find, don't.
// (for example, "unicorn" from "unicorn: /value")
static const std::vector<std::string> kUnsupportedTags = {
    "clean-param", "content-signal", "content-usage", "crawl-delay",
    "domain",      "host",           "noarchive",     "nofollow",
    "noindex",     "request-rate",   "revisit-after", "visit-time"};

void RobotsParsingReporter::Digest(int line_num,
                                   RobotsParsedLine::RobotsTagName parsed_tag) {
  if (line_num > last_line_seen_) {
    last_line_seen_ = line_num;
  }
  if (parsed_tag != RobotsParsedLine::kUnknown &&
      parsed_tag != RobotsParsedLine::kUnused) {
    ++valid_directives_;
  }

  RobotsParsedLine& line = robots_parse_results_[line_num];
  line.line_num = line_num;
  line.tag_name = parsed_tag;
}

void RobotsParsingReporter::ReportLineMetadata(int line_num,
                                               const LineMetadata& metadata) {
  if (line_num > last_line_seen_) {
    last_line_seen_ = line_num;
  }
  RobotsParsedLine& line = robots_parse_results_[line_num];
  line.line_num = line_num;
  line.is_typo = metadata.is_acceptable_typo;
  line.metadata = metadata;
}

void RobotsParsingReporter::HandleRobotsStart() {
  last_line_seen_ = 0;
  valid_directives_ = 0;
  unused_directives_ = 0;
}
void RobotsParsingReporter::HandleRobotsEnd() {}
void RobotsParsingReporter::HandleUserAgent(int line_num,
                                            std::string_view line_value) {
  Digest(line_num, RobotsParsedLine::kUserAgent);
}
void RobotsParsingReporter::HandleAllow(int line_num,
                                        std::string_view line_value) {
  Digest(line_num, RobotsParsedLine::kAllow);
}
void RobotsParsingReporter::HandleDisallow(int line_num,
                                           std::string_view line_value) {
  Digest(line_num, RobotsParsedLine::kDisallow);
}
void RobotsParsingReporter::HandleSitemap(int line_num,
                                          std::string_view line_value) {
  Digest(line_num, RobotsParsedLine::kSitemap);
}
void RobotsParsingReporter::HandleUnknownAction(int line_num,
                                                std::string_view action,
                                                std::string_view line_value) {
  RobotsParsedLine::RobotsTagName rtn =
      std::count(kUnsupportedTags.begin(), kUnsupportedTags.end(),
                 AsciiStrToLower(action)) > 0
          ? RobotsParsedLine::kUnused
          : RobotsParsedLine::kUnknown;
  unused_directives_++;
  Digest(line_num, rtn);
}

}  // namespace googlebot
