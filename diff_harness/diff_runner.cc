// Differential harness runner.
//
// This single source is compiled TWICE into two separate executables:
//   * diff_runner_replica  -- links the de-Abseiled product engine (repo root)
//   * diff_runner_pristine -- links the pristine upstream engine + Abseil,
//                             with .upstream-pinned/ on the include path so
//                             `#include "robots.h"` resolves to the pristine
//                             headers.
//
// The two engines share the namespace `googlebot` and the type name
// `RobotsMatcher`, so they CANNOT be linked into one executable (ODR / symbol
// collision). Instead each runner emits a canonical, deterministic serialized
// line per case for the shared (seed, count) case stream, and the comparator
// diffs the two output files.
//
// The only source-level difference between the engines' public API is
// `absl::string_view` vs `std::string_view` on the body parameter; a
// `std::string` argument converts implicitly to either, so this file is
// engine-agnostic.
//
// Usage: diff_runner <out_file> <seed> <generated_count>

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "reporting_robots.h"
#include "robots.h"

#include "case_generator.h"
#include "corpus_data.h"

using robotstxt_diff::DiffCase;

namespace {

// Serializes one engine's output for a single case into `out`.
void RunCase(const DiffCase& c, std::string* out) {
  // --- Matcher decision ---
  googlebot::RobotsMatcher matcher;
  const bool allowed =
      matcher.OneAgentAllowedByRobots(c.robots_body, c.user_agent, c.url);
  out->append("allowed=");
  out->append(allowed ? "1" : "0");
  out->append(";line=");
  out->append(std::to_string(matcher.matching_line()));
  out->append(";seen=");
  out->append(matcher.ever_seen_specific_agent() ? "1" : "0");

  // --- Reporting layer ---
  googlebot::RobotsParsingReporter reporter;
  googlebot::ParseRobotsTxt(c.robots_body, &reporter);
  const std::vector<googlebot::RobotsParsedLine> lines =
      reporter.parse_results();

  out->append("|last=");
  out->append(std::to_string(reporter.last_line_seen()));
  out->append(";valid=");
  out->append(std::to_string(reporter.valid_directives()));
  out->append(";unused=");
  out->append(std::to_string(reporter.unused_directives()));
  out->append(";n=");
  out->append(std::to_string(lines.size()));
  out->append("|");

  for (const auto& l : lines) {
    out->append(std::to_string(l.line_num));
    out->append(",");
    out->append(std::to_string(static_cast<int>(l.tag_name)));
    out->append(",");
    out->append(l.is_typo ? "1" : "0");
    out->append(",");
    const auto& m = l.metadata;
    out->push_back(m.is_empty ? '1' : '0');
    out->push_back(m.has_comment ? '1' : '0');
    out->push_back(m.is_comment ? '1' : '0');
    out->push_back(m.has_directive ? '1' : '0');
    out->push_back(m.is_acceptable_typo ? '1' : '0');
    out->push_back(m.is_line_too_long ? '1' : '0');
    out->push_back(m.is_missing_colon_separator ? '1' : '0');
    out->append(";");
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 4) {
    std::fprintf(stderr, "usage: %s <out_file> <seed> <generated_count>\n",
                 argv[0]);
    return 2;
  }
  const std::string out_path = argv[1];
  const std::uint64_t seed =
      std::strtoull(argv[2], nullptr, 0);
  const int count = std::atoi(argv[3]);

  std::vector<DiffCase> cases = robotstxt_diff::UpstreamCorpus();
  std::vector<DiffCase> gen = robotstxt_diff::GenerateCases(seed, count);
  cases.insert(cases.end(), gen.begin(), gen.end());

  std::ofstream os(out_path, std::ios::binary);
  if (!os) {
    std::fprintf(stderr, "cannot open output file: %s\n", out_path.c_str());
    return 2;
  }

  std::string line;
  for (const auto& c : cases) {
    line.clear();
    RunCase(c, &line);
    os << line << '\n';
  }
  os.flush();
  return 0;
}
