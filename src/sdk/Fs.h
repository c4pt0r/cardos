#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "FsPath.h"

// Unified filesystem API over internal flash (LittleFS at /flash) and
// microSD (at /sd). After begin(), POSIX fopen()/fread() also work on
// both mounts via the ESP-IDF VFS (WavWriter relies on this).
namespace cardos::fs {

bool begin();         // mounts /flash (formats on first boot); SD is lazy
bool sdAvailable();   // probes / mounts the SD card on demand

std::string readFile(const std::string& path);  // "" on error
bool writeFile(const std::string& path, const std::string& data,
               bool append = false);
bool exists(const std::string& path);
bool remove(const std::string& path);
bool mkdir(const std::string& path);
bool rmdir(const std::string& path);  // empty directories only

struct Entry {
  std::string name;  // bare name, no directory prefix
  size_t size = 0;
  bool isDir = false;
};
std::vector<Entry> list(const std::string& dir);

uint64_t freeBytes(const std::string& mount);  // "/flash" or "/sd"

}  // namespace cardos::fs
