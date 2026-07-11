// Maintainer-only differential harness driver.
//
// Orchestrates the two per-engine runners and diffs their output. Because the
// pristine and replica engines cannot be linked together (shared `googlebot`
// namespace / `RobotsMatcher` type), each runs as its own subprocess and writes
// a canonical serialized line per case; this driver regenerates the identical
// case stream (so it can print offending case details), reads both output
// files, and compares them line by line.
//
// The runner executable paths are injected at compile time by CMake via the
// REPLICA_RUNNER / PRISTINE_RUNNER definitions (absolute $<TARGET_FILE:...>
// paths), so the harness is a single self-contained command.
//
// Exit status: 0 iff ZERO matcher differences AND ZERO reporting differences.
//
// Usage: differential_harness [--seed <n>] [--count <n>] [--max-report <n>]

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>  // getpid

#include "case_generator.h"
#include "corpus_data.h"

#ifndef REPLICA_RUNNER
#error "REPLICA_RUNNER must be defined by the build"
#endif
#ifndef PRISTINE_RUNNER
#error "PRISTINE_RUNNER must be defined by the build"
#endif

namespace {

std::string TempPath(const char* tag) {
  const char* dir = std::getenv("TMPDIR");
  std::string base = dir && *dir ? dir : "/tmp";
  if (base.back() != '/') base.push_back('/');
  return base + "robotstxt_diff_" + tag + "_" +
         std::to_string(static_cast<long>(::getpid())) + ".out";
}

// Runs one runner subprocess. Returns true on success.
bool RunRunner(const std::string& exe, const std::string& out,
               std::uint64_t seed, int count) {
  std::string cmd = "\"" + exe + "\" \"" + out + "\" " +
                    std::to_string(seed) + " " + std::to_string(count);
  const int rc = std::system(cmd.c_str());
  if (rc != 0) {
    std::fprintf(stderr, "runner failed (rc=%d): %s\n", rc, cmd.c_str());
    return false;
  }
  return true;
}

std::vector<std::string> ReadLines(const std::string& path) {
  std::vector<std::string> lines;
  std::ifstream in(path, std::ios::binary);
  std::string line;
  while (std::getline(in, line)) lines.push_back(line);
  return lines;
}

// Splits a serialized case line into (matcher, reporting) halves at the first
// '|'. Everything after the first '|' is the reporting payload.
void SplitHalves(const std::string& s, std::string* matcher,
                 std::string* reporting) {
  const auto bar = s.find('|');
  if (bar == std::string::npos) {
    *matcher = s;
    reporting->clear();
  } else {
    *matcher = s.substr(0, bar);
    *reporting = s.substr(bar + 1);
  }
}

std::string Truncate(const std::string& s, std::size_t n = 200) {
  if (s.size() <= n) return s;
  return s.substr(0, n) + "...(truncated)";
}

}  // namespace

int main(int argc, char** argv) {
  std::uint64_t seed = robotstxt_diff::kDefaultSeed;
  int count = robotstxt_diff::kDefaultGeneratedCount;
  int max_report = 20;

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto next = [&]() -> std::string {
      return (i + 1 < argc) ? argv[++i] : "";
    };
    if (a == "--seed") seed = std::strtoull(next().c_str(), nullptr, 0);
    else if (a == "--count") count = std::atoi(next().c_str());
    else if (a == "--max-report") max_report = std::atoi(next().c_str());
    else {
      std::fprintf(stderr, "unknown argument: %s\n", a.c_str());
      return 2;
    }
  }

  // Regenerate the identical case stream for reporting offending cases.
  std::vector<robotstxt_diff::DiffCase> cases = robotstxt_diff::UpstreamCorpus();
  const std::size_t corpus_n = cases.size();
  {
    auto gen = robotstxt_diff::GenerateCases(seed, count);
    cases.insert(cases.end(), gen.begin(), gen.end());
  }

  std::printf("robotstxt differential harness\n");
  std::printf("  corpus version : %s\n", robotstxt_diff::kCorpusVersion);
  std::printf("  seed           : %llu (0x%llx)\n",
              static_cast<unsigned long long>(seed),
              static_cast<unsigned long long>(seed));
  std::printf("  upstream corpus: %zu cases\n", corpus_n);
  std::printf("  generated      : %d cases\n", count);
  std::printf("  total          : %zu cases\n", cases.size());
  std::printf("  replica runner : %s\n", REPLICA_RUNNER);
  std::printf("  pristine runner: %s\n", PRISTINE_RUNNER);
  std::fflush(stdout);

  const std::string replica_out = TempPath("replica");
  const std::string pristine_out = TempPath("pristine");

  if (!RunRunner(REPLICA_RUNNER, replica_out, seed, count)) return 2;
  if (!RunRunner(PRISTINE_RUNNER, pristine_out, seed, count)) return 2;

  const std::vector<std::string> rep = ReadLines(replica_out);
  const std::vector<std::string> pri = ReadLines(pristine_out);
  std::remove(replica_out.c_str());
  std::remove(pristine_out.c_str());

  if (rep.size() != cases.size() || pri.size() != cases.size()) {
    std::fprintf(stderr,
                 "line-count mismatch: replica=%zu pristine=%zu expected=%zu\n",
                 rep.size(), pri.size(), cases.size());
    return 2;
  }

  int matcher_diffs = 0;
  int reporting_diffs = 0;
  int reported = 0;

  for (std::size_t i = 0; i < cases.size(); ++i) {
    if (rep[i] == pri[i]) continue;

    std::string rm, rr, pm, pr;
    SplitHalves(rep[i], &rm, &rr);
    SplitHalves(pri[i], &pm, &pr);
    const bool m_diff = (rm != pm);
    const bool r_diff = (rr != pr);
    if (m_diff) ++matcher_diffs;
    if (r_diff) ++reporting_diffs;

    if (reported < max_report) {
      ++reported;
      const auto& c = cases[i];
      const char* kind = i < corpus_n ? "upstream" : "generated";
      std::fprintf(stderr, "\n=== DIFF case #%zu (%s)%s%s ===\n", i, kind,
                   m_diff ? " [matcher]" : "", r_diff ? " [reporting]" : "");
      std::fprintf(stderr, "  body : %s\n", Truncate(c.robots_body).c_str());
      std::fprintf(stderr, "  agent: %s\n", c.user_agent.c_str());
      std::fprintf(stderr, "  url  : %s\n", c.url.c_str());
      std::fprintf(stderr, "  replica : %s\n", Truncate(rep[i]).c_str());
      std::fprintf(stderr, "  pristine: %s\n", Truncate(pri[i]).c_str());
    }
  }

  std::printf("\nresult: matcher diffs = %d, reporting diffs = %d\n",
              matcher_diffs, reporting_diffs);
  if (matcher_diffs == 0 && reporting_diffs == 0) {
    std::printf("PASS: zero differences across %zu cases.\n", cases.size());
    return 0;
  }
  std::printf("FAIL: differences found (showed %d of %d differing cases).\n",
              reported, matcher_diffs + reporting_diffs);
  return 1;
}
