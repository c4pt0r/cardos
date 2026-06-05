#include "HttpDemoApp.h"

#include <HTTPClient.h>
#include <WiFi.h>

#include "../ui/Theme.h"

namespace {
struct Endpoint {
  const char* label;
  const char* url;
};
// Plain HTTP only — TLS is out of scope for this demo.
const Endpoint kEndpoints[] = {
    {"My public IP", "http://ifconfig.me/ip"},
    {"GET example.com", "http://example.com/"},
};
}  // namespace

void HttpDemoApp::onEnter() {
  std::vector<MenuItem> items;
  for (const auto& e : kEndpoints) items.push_back({e.label, "", 0});
  menu_.setItems(std::move(items));
  status_.clear();
  body_.clear();
  pendingUrl_.clear();
  requestRedraw();
}

bool HttpDemoApp::handleKey(const KeyEvent& ev) {
  if (!pendingUrl_.empty()) return true;  // ignore keys mid-request
  if (menu_.handleKey(ev)) { requestRedraw(); return true; }
  if (ev.code == KeyCode::Enter) {
    if (WiFi.status() != WL_CONNECTED) {
      status_ = "WiFi not connected";
      body_ = "Connect via WiFi Settings first.";
    } else {
      pendingUrl_ = kEndpoints[menu_.selected()].url;
      pendingDelay_ = 1;  // let "Requesting..." reach the screen first
      status_ = "Requesting...";
      body_ = pendingUrl_;
    }
    requestRedraw();
    return true;
  }
  return false;  // Esc -> pop back to launcher
}

void HttpDemoApp::update(uint32_t) {
  if (pendingUrl_.empty()) return;
  if (pendingDelay_ > 0) {
    pendingDelay_--;
    return;
  }
  doRequest(pendingUrl_);  // blocks up to ~10s; acceptable for a demo
  pendingUrl_.clear();
  requestRedraw();
}

void HttpDemoApp::doRequest(const std::string& url) {
  Serial.printf("[http] GET %s\n", url.c_str());
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  http.setUserAgent("CardOS/0.1");
  if (!http.begin(url.c_str())) {
    status_ = "http.begin() failed";
    body_.clear();
    return;
  }
  uint32_t t0 = millis();
  int code = http.GET();
  uint32_t dt = millis() - t0;
  if (code > 0) {
    String payload = http.getString();
    status_ = "HTTP " + std::to_string(code) + "  " + std::to_string(dt) +
              " ms  " + std::to_string(payload.length()) + " B";
    payload.replace("\r", "");
    body_ = std::string(payload.c_str()).substr(0, 160);
  } else {
    status_ = std::string("error: ") + HTTPClient::errorToString(code).c_str();
    body_.clear();
  }
  http.end();
  Serial.printf("[http] %s\n", status_.c_str());
}

void HttpDemoApp::render(M5Canvas& gfx) {
  int top = theme::kStatusBarH;
  int menuH = 2 * theme::kRowH;
  menu_.render(gfx, 0, top, gfx.width(), menuH);

  gfx.setFont(theme::font());
  int y = top + menuH + 4;
  gfx.drawFastHLine(0, y - 2, gfx.width(), theme::kMuted);
  if (!status_.empty()) {
    gfx.setTextColor(status_.rfind("HTTP 2", 0) == 0 ? theme::kOk
                                                     : theme::kAccent);
    gfx.setCursor(theme::kPadX, y);
    gfx.print(status_.c_str());
    y += 16;
  }
  if (!body_.empty()) {
    gfx.setTextColor(theme::kFg);
    gfx.setCursor(theme::kPadX, y);
    gfx.print(body_.c_str());  // wraps automatically
  }
}
