#pragma once
#include <string>

// Pure mount-prefix routing for the CardOS VFS. No Arduino includes.
namespace cardos::fs {

// Splits "/flash/rec/a.wav" into mount="/flash", rel="/rec/a.wav".
// Only "/flash" and "/sd" are valid mounts. rel is "/" for the bare mount.
inline bool splitPath(const std::string& path, std::string& mount,
                      std::string& rel) {
  for (const char* m : {"/flash", "/sd"}) {
    size_t len = std::string(m).size();
    if (path.compare(0, len, m) != 0) continue;
    if (path.size() == len) { mount = m; rel = "/"; return true; }
    if (path[len] == '/') { mount = m; rel = path.substr(len); return true; }
  }
  return false;
}

// Parent directory of a path: "/flash/rec/a.wav" -> "/flash/rec",
// "/flash/rec" -> "/flash". A bare mount has no parent and yields "".
inline std::string parentPath(const std::string& path) {
  std::string mount, rel;
  if (!splitPath(path, mount, rel) || rel == "/") return "";
  size_t slash = path.rfind('/');
  return path.substr(0, slash);
}

}  // namespace cardos::fs
