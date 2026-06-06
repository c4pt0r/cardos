#pragma once
#include "../core/App.h"
#include "../services/AppServer.h"

// Starts a local HTTP server (while open) for installing Lua apps over WiFi.
// Shows the URL + a QR code; Esc stops the server and exits.
class AppUploaderApp : public App {
 public:
  const char* title() const override { return "App Uploader"; }
  void onEnter() override;
  void onExit() override;
  bool handleKey(const KeyEvent& ev) override;
  void update(uint32_t dtMs) override;
  void render(M5Canvas& gfx) override;

 private:
  AppServer server_;
  uint32_t sinceRefresh_ = 0;
};
