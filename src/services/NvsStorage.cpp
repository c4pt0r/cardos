#include "NvsStorage.h"

#include <Preferences.h>

namespace {
constexpr const char* kNamespace = "cardos.wifi";  // <= 15 chars (NVS limit)
constexpr const char* kKey = "networks";
}  // namespace

std::string NvsStorage::load() {
  Preferences p;
  p.begin(kNamespace, /*readOnly=*/true);
  String s = p.getString(kKey, "");
  p.end();
  return std::string(s.c_str());
}

void NvsStorage::save(const std::string& data) {
  Preferences p;
  p.begin(kNamespace, /*readOnly=*/false);
  p.putString(kKey, data.c_str());
  p.end();
}
