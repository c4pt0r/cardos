#include "HttpDemoApp.h"

#include <WiFi.h>

#include "../sdk/Http.h"
#include "../ui/Theme.h"

namespace {
struct Endpoint {
  const char* label;
  const char* url;
};
const Endpoint kEndpoints[] = {
    {"My public IP", "http://ifconfig.me/ip"},
    {"GET example.com", "http://example.com/"},
    {"HTTPS httpbin", "https://httpbin.org/get"},
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
  if (ev.action != KeyAction::Press) return true;
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
  cardos::http::Response r = cardos::http::get(url);
  if (r.status > 0) {
    status_ = "HTTP " + std::to_string(r.status) + "  " +
              std::to_string(r.body.size()) + " B";
    body_ = r.body.substr(0, 160);
  } else {
    status_ = "error: " + r.error;
    body_.clear();
  }
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
