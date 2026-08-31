#ifndef _H_EMBER_SHORT_MSG_CHECK
#define _H_EMBER_SHORT_MSG_CHECK

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <string>

namespace SST {
namespace Ember {

// Patched by launchAll before each sstsim.x run; Firefly shortMsgLength
// is in bytes (count * dtypeSize).
inline constexpr const char *kEmberDefaultParamsPath =
    "/home/gera/torus-allreduce/scratch/src/sst-elements/src/sst/elements/"
    "ember/test/defaultParams.py";

inline uint64_t emberParseValueShortExpr(std::string expr) {
  expr.erase(std::remove_if(expr.begin(), expr.end(),
                            [](unsigned char c) { return std::isspace(c); }),
             expr.end());
  auto pow_pos = expr.find("**");
  if (pow_pos != std::string::npos) {
    uint64_t base = std::stoull(expr.substr(0, pow_pos));
    uint64_t exp = std::stoull(expr.substr(pow_pos + 2));
    uint64_t v = 1;
    for (uint64_t i = 0; i < exp; ++i) {
      v *= base;
    }
    return v;
  }
  return std::stoull(expr);
}

// Read last valueShort = ... assignment from defaultParams.py.
inline uint64_t emberLoadValueShort(
    const char *path = kEmberDefaultParamsPath) {
  std::ifstream in(path);
  if (!in) {
    fprintf(stderr, "emberLoadValueShort: cannot open %s\n", path);
    exit(1);
  }
  std::regex re(R"(^\s*valueShort\s*=\s*(.+?)\s*$)");
  std::string line;
  bool found = false;
  uint64_t value = 0;
  while (std::getline(in, line)) {
    std::smatch m;
    if (std::regex_match(line, m, re)) {
      value = emberParseValueShortExpr(m[1].str());
      found = true;
    }
  }
  if (!found) {
    fprintf(stderr, "emberLoadValueShort: no valueShort in %s\n", path);
    exit(1);
  }
  return value;
}

// Firefly short path uses length <= shortMsgLength (bytes).
inline void emberAssertMsgFitsShort(uint64_t bytes, uint64_t value_short,
                                    const char *where) {
  if (bytes > value_short) {
    fprintf(stderr,
            "FATAL [%s]: send %llu bytes > valueShort=%llu "
            "(from defaultParams.py)\n",
            where, (unsigned long long)bytes,
            (unsigned long long)value_short);
  }
  assert(bytes <= value_short);
}

} // namespace Ember
} // namespace SST

#endif
