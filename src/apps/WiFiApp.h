#pragma once
#include <string>

#include "../core/App.h"
#include "../services/WiFiService.h"
#include "../ui/Dialog.h"
#include "../ui/MenuList.h"
#include "../ui/TextInput.h"

class WiFiApp : public App {
 public:
  explicit WiFiApp(WiFiService& wifi, WiFiStore& store)
      : wifi_(wifi), store_(store) {}

  const char* title() const override { return "WiFi"; }
  void onEnter() override;
  bool handleKey(const KeyEvent& ev) override;
  void update(uint32_t dtMs) override;
  void render(M5Canvas& gfx) override;

 private:
  enum class Page { Home, Scan, Password, Saved };
  enum class Modal { None, Toast, Confirm };

  void showHome();
  void showScanResults();
  void showSaved();
  void startConnect(const std::string& ssid, const std::string& pw, bool save);
  void toast(const std::string& msg, bool spinner, uint32_t autoHideMs);
  static std::string bars(int rssi);

  WiFiService& wifi_;
  WiFiStore& store_;
  Page page_ = Page::Home;
  Modal modal_ = Modal::None;
  MenuList menu_;
  TextInput input_;
  ConfirmDialog confirm_;

  std::string toastMsg_;
  bool toastSpinner_ = false;
  uint32_t toastTimerMs_ = 0;  // >0: auto-hide countdown

  std::string targetSsid_;     // AP being connected / acted upon
  WifiState lastSeenState_ = WifiState::Idle;
  uint32_t sinceAnim_ = 0;
};
