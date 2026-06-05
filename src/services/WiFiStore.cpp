#include "WiFiStore.h"

#include <ArduinoJson.h>
#include <algorithm>

void WiFiStore::load() {
  networks_.clear();
  JsonDocument doc;
  if (deserializeJson(doc, backend_.load()) != DeserializationError::Ok)
    return;
  for (JsonObject o : doc.as<JsonArray>()) {
    WifiNetwork n;
    n.ssid = o["ssid"].as<std::string>();
    n.password = o["pw"].as<std::string>();
    n.lastOkTs = o["ts"] | 0;
    if (!n.ssid.empty()) networks_.push_back(std::move(n));
  }
}

const WifiNetwork* WiFiStore::find(const std::string& ssid) const {
  for (const auto& n : networks_)
    if (n.ssid == ssid) return &n;
  return nullptr;
}

void WiFiStore::upsert(const std::string& ssid, const std::string& password) {
  for (auto& n : networks_) {
    if (n.ssid == ssid) {
      n.password = password;
      persist();
      return;
    }
  }
  if (networks_.size() >= kCapacity) {
    auto oldest = std::min_element(
        networks_.begin(), networks_.end(),
        [](const WifiNetwork& a, const WifiNetwork& b) {
          return a.lastOkTs < b.lastOkTs;
        });
    networks_.erase(oldest);
  }
  networks_.push_back({ssid, password, 0});
  persist();
}

void WiFiStore::remove(const std::string& ssid) {
  networks_.erase(
      std::remove_if(networks_.begin(), networks_.end(),
                     [&](const WifiNetwork& n) { return n.ssid == ssid; }),
      networks_.end());
  persist();
}

void WiFiStore::touch(const std::string& ssid, uint32_t ts) {
  for (auto& n : networks_) {
    if (n.ssid == ssid) {
      n.lastOkTs = ts;
      persist();
      return;
    }
  }
}

void WiFiStore::persist() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (const auto& n : networks_) {
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = n.ssid;
    o["pw"] = n.password;
    o["ts"] = n.lastOkTs;
  }
  std::string out;
  serializeJson(doc, out);
  backend_.save(out);
}
