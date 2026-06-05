#pragma once
#include <string>

#include "../core/App.h"
#include "../ui/MenuList.h"

// Network smoke-test app: fires plain-HTTP GETs at known endpoints and
// shows status code, latency, and a body snippet. Demonstrates how little
// code a new CardOS app needs.
class HttpDemoApp : public App {
 public:
  const char* title() const override { return "HTTP Demo"; }
  void onEnter() override;
  bool handleKey(const KeyEvent& ev) override;
  void update(uint32_t dtMs) override;
  void render(M5Canvas& gfx) override;

 private:
  void doRequest(const std::string& url);

  MenuList menu_;
  std::string pendingUrl_;  // non-empty: request queued
  int pendingDelay_ = 0;    // frames to wait so "Requesting..." paints first
  std::string status_;      // result summary line
  std::string body_;        // body snippet
};
