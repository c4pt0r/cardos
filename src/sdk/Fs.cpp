#include "Fs.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <SD.h>
#include <SPI.h>

namespace cardos::fs {
namespace {
// Cardputer microSD over SPI. VERIFY ON HARDWARE (spec section 5 risk).
constexpr int kSdSck = 40, kSdMiso = 39, kSdMosi = 14, kSdCs = 12;
bool sdMounted = false;

// Returns the Arduino FS backend for a path, and the path relative to it.
FS* backendFor(const std::string& path, std::string& rel) {
  std::string mount;
  if (!splitPath(path, mount, rel)) return nullptr;
  if (mount == "/flash") return &LittleFS;
  return sdAvailable() ? (FS*)&SD : nullptr;
}
}  // namespace

bool begin() {
  // base_path "/flash" registers the POSIX VFS mount point.
  bool ok = LittleFS.begin(/*formatOnFail=*/true, "/flash");
  Serial.printf("[fs] /flash %s\n", ok ? "mounted" : "MOUNT FAILED");
  return ok;
}

bool sdAvailable() {
  if (sdMounted && SD.cardType() != CARD_NONE) return true;
  SPI.begin(kSdSck, kSdMiso, kSdMosi, kSdCs);
  sdMounted = SD.begin(kSdCs, SPI, 25000000, "/sd");
  if (sdMounted) Serial.println("[fs] /sd mounted");
  return sdMounted;
}

std::string readFile(const std::string& path) {
  std::string rel;
  FS* be = backendFor(path, rel);
  if (!be) return "";
  File f = be->open(rel.c_str(), "r");
  if (!f || f.isDirectory()) return "";
  std::string out;
  out.reserve(f.size());
  uint8_t buf[256];
  while (f.available()) {
    size_t n = f.read(buf, sizeof(buf));
    out.append((const char*)buf, n);
  }
  f.close();
  return out;
}

bool writeFile(const std::string& path, const std::string& data, bool append) {
  std::string rel;
  FS* be = backendFor(path, rel);
  if (!be) return false;
  File f = be->open(rel.c_str(), append ? "a" : "w");
  if (!f) return false;
  size_t n = f.write((const uint8_t*)data.data(), data.size());
  f.close();
  return n == data.size();
}

bool exists(const std::string& path) {
  std::string rel;
  FS* be = backendFor(path, rel);
  return be && be->exists(rel.c_str());
}

bool remove(const std::string& path) {
  std::string rel;
  FS* be = backendFor(path, rel);
  return be && be->remove(rel.c_str());
}

bool mkdir(const std::string& path) {
  std::string rel;
  FS* be = backendFor(path, rel);
  if (!be) return false;
  return be->exists(rel.c_str()) || be->mkdir(rel.c_str());
}

std::vector<Entry> list(const std::string& dir) {
  std::vector<Entry> out;
  std::string rel;
  FS* be = backendFor(dir, rel);
  if (!be) return out;
  File d = be->open(rel.c_str());
  if (!d || !d.isDirectory()) return out;
  for (File f = d.openNextFile(); f; f = d.openNextFile()) {
    Entry e;
    const char* n = f.name();             // may include a path prefix
    const char* slash = strrchr(n, '/');
    e.name = slash ? slash + 1 : n;
    e.size = f.size();
    e.isDir = f.isDirectory();
    out.push_back(std::move(e));
    f.close();
  }
  d.close();
  return out;
}

uint64_t freeBytes(const std::string& mount) {
  if (mount == "/flash") return LittleFS.totalBytes() - LittleFS.usedBytes();
  if (mount == "/sd" && sdAvailable()) return SD.totalBytes() - SD.usedBytes();
  return 0;
}

}  // namespace cardos::fs
