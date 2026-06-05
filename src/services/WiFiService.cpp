#include "WiFiService.h"

#include <Arduino.h>
#include <WiFi.h>

#include <algorithm>

void WiFiService::begin(WiFiStore* store) {
  store_ = store;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);  // CardOS owns the retry policy

  WiFi.onEvent([this](WiFiEvent_t, WiFiEventInfo_t) { onGotIp(); },
               ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(
      [this](WiFiEvent_t, WiFiEventInfo_t info) {
        onDisconnected(info.wifi_sta_disconnected.reason);
      },
      ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
}

void WiFiService::tick(uint32_t nowMs) {
  nowMs_ = nowMs;
  if (state_ == WifiState::Scanning) {
    int16_t n = WiFi.scanComplete();
    if (n >= 0) onScanDone();
  }
  if (state_ == WifiState::Connecting &&
      nowMs - connectStartMs_ >= kConnectTimeoutMs) {
    Serial.printf("[wifi] connect timeout: %s\n", pendingSsid_.c_str());
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    lastError_ = WifiError::Timeout;
    state_ = WifiState::Failed;
    if (autoConnecting_) tryNextCandidate();
  }
  if (advancePending_) {
    advancePending_ = false;
    tryNextCandidate();
  }
}

void WiFiService::startScan() {
  if (busy()) return;
  Serial.println("[wifi] scan start");
  results_.clear();
  state_ = WifiState::Scanning;
  WiFi.scanNetworks(/*async=*/true);
}

bool WiFiService::scanFinished() {
  bool f = scanJustFinished_;
  scanJustFinished_ = false;
  return f;
}

void WiFiService::onScanDone() {
  int16_t n = WiFi.scanComplete();
  results_.clear();
  for (int16_t i = 0; i < n; i++) {
    if (WiFi.SSID(i).isEmpty()) continue;  // skip hidden SSIDs
    ScanResult r;
    r.ssid = WiFi.SSID(i).c_str();
    r.rssi = WiFi.RSSI(i);
    r.secured = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    r.saved = store_ && store_->find(r.ssid) != nullptr;
    // Dedup: keep the strongest entry per SSID (APs with multiple bands).
    auto dup = std::find_if(results_.begin(), results_.end(),
                            [&](const ScanResult& e) { return e.ssid == r.ssid; });
    if (dup != results_.end()) {
      if (r.rssi > dup->rssi) *dup = r;
      continue;
    }
    results_.push_back(std::move(r));
  }
  std::sort(results_.begin(), results_.end(),
            [](const ScanResult& a, const ScanResult& b) {
              return a.rssi > b.rssi;
            });
  WiFi.scanDelete();
  Serial.printf("[wifi] scan done: %d APs\n", (int)results_.size());
  state_ = WiFi.status() == WL_CONNECTED ? WifiState::Connected : WifiState::Idle;
  scanJustFinished_ = true;

  if (autoConnecting_) {
    // Candidates = saved ∩ visible, in scan order (strongest RSSI first).
    candidates_.clear();
    candidateIdx_ = 0;
    for (const auto& r : results_) {
      if (const WifiNetwork* n = store_->find(r.ssid))
        candidates_.push_back({n->ssid, n->password});
    }
    Serial.printf("[wifi] auto-connect: %d candidates\n", (int)candidates_.size());
    tryNextCandidate();
  }
}

void WiFiService::tryNextCandidate() {
  if (candidateIdx_ >= candidates_.size()) {
    if (!candidates_.empty() || autoConnecting_)
      Serial.println("[wifi] auto-connect: exhausted");
    autoConnecting_ = false;
    if (state_ != WifiState::Connected && state_ != WifiState::Failed)
      state_ = WifiState::Idle;
    return;
  }
  auto& c = candidates_[candidateIdx_++];
  connect(c.first, c.second, /*save=*/false);
}

void WiFiService::connect(const std::string& ssid, const std::string& password,
                          bool save) {
  Serial.printf("[wifi] connecting: %s\n", ssid.c_str());
  pendingSsid_ = ssid;
  pendingPw_ = password;
  pendingSave_ = save;
  lastError_ = WifiError::None;
  state_ = WifiState::Connecting;
  connectStartMs_ = nowMs_;
  WiFi.begin(ssid.c_str(), password.empty() ? nullptr : password.c_str());
}

void WiFiService::disconnect() {
  autoConnecting_ = false;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  state_ = WifiState::Idle;
  Serial.println("[wifi] disconnected by user");
}

void WiFiService::autoConnect() {
  if (!store_ || store_->networks().empty()) return;
  autoConnecting_ = true;
  startScan();
}

void WiFiService::onGotIp() {
  if (state_ != WifiState::Connecting) return;  // not ours (late/spurious)
  Serial.printf("[wifi] connected: %s ip=%s\n", pendingSsid_.c_str(),
                WiFi.localIP().toString().c_str());
  state_ = WifiState::Connected;
  autoConnecting_ = false;
  if (store_) {
    if (pendingSave_) store_->upsert(pendingSsid_, pendingPw_);
    store_->touch(pendingSsid_, nowMs_ / 1000);  // monotonic-enough ordering
  }
}

void WiFiService::onDisconnected(uint8_t reason) {
  if (state_ != WifiState::Connecting) {
    // Lost an established connection; reflect reality, stay passive.
    if (state_ == WifiState::Connected) state_ = WifiState::Idle;
    return;
  }
  // 202 = AUTH_FAIL, 201 = NO_AP_FOUND (esp_wifi reason codes)
  Serial.printf("[wifi] connect failed: %s reason=%d\n", pendingSsid_.c_str(),
                reason);
  // 202 AUTH_FAIL; 15/204 handshake timeouts — typical wrong-password
  // symptoms on consumer APs. 201 NO_AP_FOUND.
  lastError_ = (reason == 202 || reason == 15 || reason == 204)
                   ? WifiError::AuthFail
             : reason == 201 ? WifiError::NoApFound
                             : WifiError::Other;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  state_ = WifiState::Failed;
  if (autoConnecting_) advancePending_ = true;
}

std::string WiFiService::currentSsid() const {
  return WiFi.status() == WL_CONNECTED ? std::string(WiFi.SSID().c_str()) : "";
}

std::string WiFiService::ip() const {
  return WiFi.status() == WL_CONNECTED
             ? std::string(WiFi.localIP().toString().c_str())
             : "";
}

int WiFiService::rssi() const {
  return WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
}
