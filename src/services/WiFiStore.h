#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Saved WiFi credential list. Pure logic over an injected storage backend:
// the device uses NVS (NvsStorage), tests use an in-memory mock.
struct WifiNetwork {
  std::string ssid;
  std::string password;
  uint32_t lastOkTs = 0;  // last successful connect (epoch s, 0 = never)
};

class StorageBackend {
 public:
  virtual ~StorageBackend() = default;
  virtual std::string load() = 0;               // "" when nothing stored
  virtual void save(const std::string& data) = 0;
};

class WiFiStore {
 public:
  static constexpr size_t kCapacity = 8;

  explicit WiFiStore(StorageBackend& backend) : backend_(backend) {}

  void load();  // deserialize from backend; garbage -> empty list
  const std::vector<WifiNetwork>& networks() const { return networks_; }
  // Returned pointer is valid only until the next mutation (upsert/remove/load).
  const WifiNetwork* find(const std::string& ssid) const;

  // Insert or update a credential; evicts the oldest-lastOkTs entry when
  // full. Persists immediately.
  void upsert(const std::string& ssid, const std::string& password);
  void remove(const std::string& ssid);          // persists
  void touch(const std::string& ssid, uint32_t ts);  // mark success; persists

 private:
  void persist();
  StorageBackend& backend_;
  std::vector<WifiNetwork> networks_;
};
