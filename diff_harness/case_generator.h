// Deterministic differential-case generator for the maintainer-only harness.
//
// This header is ENGINE-INDEPENDENT: it depends only on the C++ standard
// library and never includes robots.h / reporting_robots.h. Both the replica
// runner and the pristine runner include it, so from an identical (seed, count)
// pair they generate byte-for-byte identical case streams. The two runners then
// serialize each engine's output for that shared stream and the differences are
// compared downstream.
//
// Determinism contract:
//   * The PRNG is std::mt19937_64 seeded solely by the caller-supplied seed.
//   * The whole stream is a pure function of (seed, count): both runners and
//     the driver regenerate it identically by replaying GenerateOne against one
//     shared mt19937_64. Correctness only requires that the algorithm below is
//     stable and that both sides run the same binary logic; do not reorder or
//     insert PRNG draws without bumping kCorpusVersion.
//   * kCorpusVersion changes whenever the generation algorithm or the corpus
//     derivation changes, so a recorded (seed, version) pair reproduces a run.

#ifndef ROBOTSTXT_DIFF_HARNESS_CASE_GENERATOR_H_
#define ROBOTSTXT_DIFF_HARNESS_CASE_GENERATOR_H_

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace robotstxt_diff {

// A single differential case: identical bytes are fed to both engines.
struct DiffCase {
  std::string robots_body;
  std::string user_agent;
  std::string url;
};

// Recorded, committed provenance of a differential run. Keep in sync with
// diff_harness/corpus.lock.
inline constexpr std::uint64_t kDefaultSeed = 0xC5D1FF0FFEEDCAFEULL;
inline constexpr int kDefaultGeneratedCount = 10000;
inline constexpr const char* kCorpusVersion = "c5-corpus-v1";

// The building blocks below are deliberately small, ASCII-focused vocabularies
// that stress the parser and matcher surface: directive keys (incl. typos and
// unsupported/unknown tags), wildcards, anchors, percent escapes, BOMs, comment
// and whitespace noise, CR/LF/CRLF line endings, and odd user agents/URLs.

namespace detail {

inline const std::vector<std::string>& Keys() {
  static const std::vector<std::string> v = {
      "user-agent", "User-Agent", "USER-AGENT", "useragent",
      "allow",      "Allow",      "disallow",   "Disallow",
      "disalow",    "dissalow",   "allo",       "sitemap",
      "noindex",    "crawl-delay", "host",       "unicorn",
      "clean-param", ""};
  return v;
}

inline const std::vector<std::string>& Agents() {
  static const std::vector<std::string> v = {
      "*",       "FooBot",   "foobot",   "BarBot",  "Foo-Bar",
      "Foo_Bar", "Googlebot", "",        "Foobot/2.1", "Foo Bar",
      "\xe3\x83\x84"};  // a multibyte agent ("ツ")
  return v;
}

inline const std::vector<std::string>& PathPieces() {
  static const std::vector<std::string> v = {
      "/",      "/x",     "/x/",    "/page.html", "/fish",  "/*.php",
      "/*",     "/a$",    "/foo/",  "/%2f",       "/~user", "/path?q=1",
      "/a/b/c", "/index.html", ".html", "*",       "$",     "/ ",
      "/café",  "/foo%20bar"};
  return v;
}

inline const std::vector<std::string>& Values() {
  static const std::vector<std::string> v = {
      "/",     "/x/",    "/x/y",   "/*.html$", "/fish*",  "/private",
      "*",     "",       " /leadingspace", "/trailing ", "/CaseX/",
      "/a\tb", "12", "noarchive", "/foo$bar"};
  return v;
}

inline const std::vector<std::string>& Separators() {
  static const std::vector<std::string> v = {": ", ":", " :", " : ", "\t:\t",
                                             " "};
  return v;
}

inline const std::vector<std::string>& LineEndings() {
  static const std::vector<std::string> v = {"\n", "\r\n", "\r", "\n\n"};
  return v;
}

inline const std::vector<std::string>& Bom() {
  static const std::vector<std::string> v = {
      "", "\xef\xbb\xbf", "\xef\xbb", "\xef"};  // full/partial/broken BOM
  return v;
}

template <typename Rng>
std::size_t Pick(Rng& rng, std::size_t n) {
  return std::uniform_int_distribution<std::size_t>(0, n - 1)(rng);
}

template <typename Rng>
const std::string& PickFrom(Rng& rng, const std::vector<std::string>& v) {
  return v[Pick(rng, v.size())];
}

}  // namespace detail

// Generates one case from the given PRNG state. Advances `rng`.
template <typename Rng>
DiffCase GenerateOne(Rng& rng) {
  using namespace detail;
  DiffCase c;

  // Body: optional BOM + a handful of noisy lines.
  c.robots_body = PickFrom(rng, Bom());
  const int num_lines = 1 + static_cast<int>(Pick(rng, 8));
  for (int i = 0; i < num_lines; ++i) {
    const int kind = static_cast<int>(Pick(rng, 10));
    if (kind == 0) {
      // Blank / whitespace-only line.
      c.robots_body += (Pick(rng, 2) ? "   " : "");
    } else if (kind == 1) {
      // Comment line.
      c.robots_body += "# " + PickFrom(rng, Values());
    } else {
      // key <sep> value, optionally with a trailing comment.
      c.robots_body += PickFrom(rng, Keys());
      c.robots_body += PickFrom(rng, Separators());
      c.robots_body += PickFrom(rng, Values());
      if (Pick(rng, 4) == 0) {
        c.robots_body += "  # trailing";
      }
    }
    c.robots_body += PickFrom(rng, LineEndings());
  }

  // User agent.
  c.user_agent = PickFrom(rng, Agents());

  // URL: assemble a path from 0..3 pieces, then wrap in an origin (sometimes a
  // bare path or empty string, which the matcher treats specially).
  std::string path;
  const int num_pieces = static_cast<int>(Pick(rng, 4));
  for (int i = 0; i < num_pieces; ++i) path += PickFrom(rng, PathPieces());
  const int url_kind = static_cast<int>(Pick(rng, 6));
  if (url_kind == 0) {
    c.url = "";
  } else if (url_kind == 1) {
    c.url = path;  // relative / non-absolute
  } else if (url_kind == 2) {
    c.url = "https://foo.bar" + path;
  } else {
    c.url = "http://foo.bar" + path;
  }

  return c;
}

// Generates `count` deterministic cases from `seed`.
inline std::vector<DiffCase> GenerateCases(std::uint64_t seed, int count) {
  std::mt19937_64 rng(seed);
  std::vector<DiffCase> out;
  out.reserve(count);
  for (int i = 0; i < count; ++i) out.push_back(GenerateOne(rng));
  return out;
}

}  // namespace robotstxt_diff

#endif  // ROBOTSTXT_DIFF_HARNESS_CASE_GENERATOR_H_
