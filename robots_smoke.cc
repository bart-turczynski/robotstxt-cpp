// Copyright 2026 The robotstxt-cpp authors.
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
// File: robots_smoke.cc
// -----------------------------------------------------------------------------
//
// Tiny offline smoke binary for slice C1. It exercises the de-Abseiled matcher
// end to end and prints the word "allow" or "disallow" for one disallowed and
// one allowed URL, so a human (or CI) can confirm a real decision runs.

#include <iostream>
#include <string>
#include <vector>

#include "robots.h"

namespace {

// Returns "allow" or "disallow" for a single (robots_body, agent, url) case.
const char* Decision(const std::string& robots_body, const std::string& agent,
                     const std::string& url) {
  std::vector<std::string> agents = {agent};
  googlebot::RobotsMatcher matcher;
  return matcher.AllowedByRobots(robots_body, &agents, url) ? "allow"
                                                            : "disallow";
}

}  // namespace

int main() {
  const std::string robots = "user-agent: *\ndisallow: /private";

  // Disallowed case: /private/x is under the disallowed /private prefix.
  const char* disallowed =
      Decision(robots, "crawler", "http://example.com/private/x");
  std::cout << disallowed << "\n";

  // Allowed case: /public/x is not covered by any disallow rule.
  const char* allowed =
      Decision(robots, "crawler", "http://example.com/public/x");
  std::cout << allowed << "\n";

  // Non-zero exit if either decision is wrong, so the tracer bullet is
  // self-checking when run from a script.
  const bool ok = std::string(disallowed) == "disallow" &&
                  std::string(allowed) == "allow";
  return ok ? 0 : 1;
}
