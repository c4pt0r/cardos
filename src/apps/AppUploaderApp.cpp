#include "AppUploaderApp.h"

#include <M5Cardputer.h>
#include <WiFi.h>

#include "../ui/Theme.h"

namespace {
bool wifiUp() { return WiFi.status() == WL_CONNECTED; }
std::string url() {
  return "http://" + std::string(WiFi.localIP().toString().c_str()) + "/";
}
}  // namespace

void AppUploaderApp::onEnter() {
  if (wifiUp()) server_.begin();
  sinceRefresh_ = 0;
  requestRedraw();
}

void AppUploaderApp::onExit() { server_.stop(); }

bool AppUploaderApp::handleKey(const KeyEvent& ev) {
  if (ev.action != KeyAction::Press) return true;
  if (ev.code == KeyCode::Esc) return false;  // stop+exit (onExit stops server)
  return true;
}

void AppUploaderApp::update(uint32_t dtMs) {
  // Start lazily once WiFi comes up (app opened before connecting).
  if (!server_.running() && wifiUp()) {
    server_.begin();
    requestRedraw();
  }
  server_.tick();
  sinceRefresh_ += dtMs;
  if (sinceRefresh_ >= 1000) {  // refresh status line / heap
    sinceRefresh_ = 0;
    requestRedraw();
  }
}

void AppUploaderApp::render(M5Canvas& gfx) {
  int top = theme::kStatusBarH;
  gfx.setFont(theme::font());

  if (!server_.running()) {
    gfx.setTextColor(theme::kDanger);
    gfx.setCursor(theme::kPadX, top + 10);
    gfx.print("WiFi not connected");
    gfx.setTextColor(theme::kMuted);
    gfx.setCursor(theme::kPadX, top + 30);
    gfx.print("Connect in WiFi Settings;");
    gfx.setCursor(theme::kPadX, top + 46);
    gfx.print("the server starts automatically.");
    gfx.setCursor(theme::kPadX, gfx.height() - 16);
    gfx.print("Esc: back");
    return;
  }

  std::string u = url();
  // QR code on the right so a phone can open the page instantly.
  int qr = 78;
  int qx = gfx.width() - qr - 4;
  int qy = top + 6;
  gfx.qrcode(u.c_str(), qx, qy, qr, 3);

  gfx.setTextColor(theme::kOk);
  gfx.setCursor(theme::kPadX, top + 8);
  gfx.print("Server running");
  gfx.setTextColor(theme::kFg);
  gfx.setCursor(theme::kPadX, top + 28);
  gfx.print(u.c_str());
  gfx.setTextColor(theme::kMuted);
  gfx.setCursor(theme::kPadX, top + 48);
  gfx.print("Scan or open to upload .lua");

  gfx.setCursor(theme::kPadX, top + 72);
  gfx.setTextColor(theme::kFg);
  gfx.print(("Uploaded: " + std::to_string(server_.uploadCount())).c_str());
  if (!server_.lastStatus().empty()) {
    gfx.setTextColor(theme::kMuted);
    gfx.setCursor(theme::kPadX, top + 88);
    gfx.print(server_.lastStatus().c_str());
  }
  gfx.setTextColor(theme::kMuted);
  gfx.setCursor(theme::kPadX, gfx.height() - 16);
  gfx.print("Esc: stop & exit");
}
