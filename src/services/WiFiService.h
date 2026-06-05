#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "WiFiStore.h"

// Async WiFi state machine over the Arduino WiFi library. All calls are
// non-blocking; the main loop drives progress via tick(). UI apps read
// state()/scanResults() and render accordingly.
enum class WifiState : uint8_t { Idle, Scanning, Connecting, Connected, Failed };
enum class WifiError : uint8_t { None, AuthFail, NoApFound, Timeout, Other };

struct ScanResult {
  std::string ssid;
  int32_t rssi = 0;
  bool secured = false;
  bool saved = false;
};

class WiFiService {
 public:
  static constexpr uint32_t kConnectTimeoutMs = 15000;

  void begin(WiFiStore* store);            // registers WiFi event handlers
  void tick(uint32_t nowMs);               // drive timeouts + auto-connect

  void startScan();
  bool scanFinished();                     // true once after a scan completes
  const std::vector<ScanResult>& scanResults() const { return results_; }

  // save=true persists the credential on success (manual connects).
  void connect(const std::string& ssid, const std::string& password, bool save);
  void disconnect();
  void autoConnect();                      // boot flow: scan -> best saved

  WifiState state() const { return state_; }
  WifiError lastError() const { return lastError_; }
  std::string currentSsid() const;
  std::string ip() const;
  int rssi() const;
  bool busy() const {                      // suppress deep sleep while true
    return state_ == WifiState::Scanning || state_ == WifiState::Connecting;
  }

 private:
  void onScanDone();
  void tryNextCandidate();
  // Event handlers below run on the Arduino WiFi event task — must NOT mutate
  // candidates_ directly; use advancePending_ to defer to tick() (loop task).
  void onGotIp();
  void onDisconnected(uint8_t reason);

  WiFiStore* store_ = nullptr;
  WifiState state_ = WifiState::Idle;
  WifiError lastError_ = WifiError::None;
  std::vector<ScanResult> results_;
  bool scanJustFinished_ = false;  // latch cleared by scanFinished(); shared by autoConnect()'s boot scan and the UI's manual scan — single consumer

  std::string pendingSsid_, pendingPw_;
  bool pendingSave_ = false;
  uint32_t connectStartMs_ = 0;
  uint32_t nowMs_ = 0;

  bool autoConnecting_ = false;
  volatile bool advancePending_ = false;  // set by event task, consumed by tick()
  std::vector<std::pair<std::string, std::string>> candidates_;  // ssid, pw
  size_t candidateIdx_ = 0;
};
