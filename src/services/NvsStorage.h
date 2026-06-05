#pragma once
#include "WiFiStore.h"

// StorageBackend over ESP32 NVS (Preferences). Namespace "cardos.wifi",
// key "networks" (a JSON array string).
class NvsStorage : public StorageBackend {
 public:
  std::string load() override;
  void save(const std::string& data) override;
};
