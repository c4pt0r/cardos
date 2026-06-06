#pragma once
#include <string>
#include <vector>

// Pure parsing for the serial control protocol (whitespace-separated
// command line). The verb is upper-cased; remaining tokens are args.
// Native-tested; no Arduino includes.
namespace serialproto {

struct Command {
  std::string verb;
  std::vector<std::string> args;
  bool empty() const { return verb.empty(); }
};

inline Command parse(const std::string& line) {
  Command c;
  size_t i = 0, n = line.size();
  std::vector<std::string> toks;
  while (i < n) {
    while (i < n && (line[i] == ' ' || line[i] == '\t' ||
                     line[i] == '\r' || line[i] == '\n')) i++;
    if (i >= n) break;
    size_t start = i;
    while (i < n && line[i] != ' ' && line[i] != '\t' &&
           line[i] != '\r' && line[i] != '\n') i++;
    toks.push_back(line.substr(start, i - start));
  }
  if (toks.empty()) return c;
  c.verb = toks[0];
  for (char& ch : c.verb) ch = (ch >= 'a' && ch <= 'z') ? ch - 32 : ch;
  c.args.assign(toks.begin() + 1, toks.end());
  return c;
}

// A valid uploaded app name: non-empty, ends in ".lua", no path separators
// or "..", reasonable length.
inline bool validAppName(const std::string& name) {
  if (name.size() < 5 || name.size() > 48) return false;
  if (name.find('/') != std::string::npos) return false;
  if (name.find("..") != std::string::npos) return false;
  return name.substr(name.size() - 4) == ".lua";
}

// Reduce an uploaded filename to a safe app name (strip any directory part,
// then validate). Returns "" if unacceptable. Used by the HTTP uploader so a
// crafted multipart filename can't escape /flash/apps.
inline std::string uploadName(const std::string& filename) {
  size_t s = filename.find_last_of("/\\");
  std::string base = s == std::string::npos ? filename : filename.substr(s + 1);
  return validAppName(base) ? base : std::string();
}

}  // namespace serialproto
