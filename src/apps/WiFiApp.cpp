#include "WiFiApp.h"

#include "../ui/Theme.h"

namespace {
const char* kScanLabel = "Scan networks";
const char* kSavedLabel = "Saved networks";
const char* kDisconnectLabel = "Disconnect";
}  // namespace

std::string WiFiApp::bars(int rssi) {
  if (rssi >= -55) return "|||";
  if (rssi >= -70) return "||";
  return "|";
}

void WiFiApp::onEnter() {
  page_ = Page::Home;
  modal_ = Modal::None;
  showHome();
}

void WiFiApp::showHome() {
  page_ = Page::Home;
  std::vector<MenuItem> items;
  if (wifi_.state() == WifiState::Connected) {
    items.push_back({"Connected: " + wifi_.currentSsid(),
                     wifi_.ip(), theme::kOk});
  } else {
    items.push_back({"Not connected", "", theme::kMuted});
  }
  items.push_back({kScanLabel, "", 0});
  items.push_back({kSavedLabel, std::to_string(store_.networks().size()),
                   theme::kMuted});
  if (wifi_.state() == WifiState::Connected)
    items.push_back({kDisconnectLabel, "", 0});
  menu_.setItems(std::move(items));
  menu_.setSelected(1);  // first actionable row
  requestRedraw();
}

void WiFiApp::showScanResults() {
  page_ = Page::Scan;
  std::vector<MenuItem> items;
  for (const auto& r : wifi_.scanResults()) {
    std::string note = bars(r.rssi);
    if (r.secured) note += " *";
    if (r.saved) note += " v";
    items.push_back({r.ssid, note, theme::kMuted});
  }
  if (items.empty()) items.push_back({"(no networks found)", "", theme::kMuted});
  menu_.setItems(std::move(items));
  requestRedraw();
}

void WiFiApp::showSaved() {
  page_ = Page::Saved;
  std::vector<MenuItem> items;
  for (const auto& n : store_.networks()) items.push_back({n.ssid, "", 0});
  if (items.empty()) items.push_back({"(none saved)", "", theme::kMuted});
  menu_.setItems(std::move(items));
  requestRedraw();
}

void WiFiApp::toast(const std::string& msg, bool spinner, uint32_t autoHideMs) {
  modal_ = Modal::Toast;
  toastMsg_ = msg;
  toastSpinner_ = spinner;
  toastTimerMs_ = autoHideMs;
  requestRedraw();
}

void WiFiApp::startConnect(const std::string& ssid, const std::string& pw,
                           bool save) {
  targetSsid_ = ssid;
  wifi_.connect(ssid, pw, save);
  toast("Connecting: " + ssid, /*spinner=*/true, /*autoHideMs=*/0);
}

bool WiFiApp::handleKey(const KeyEvent& ev) {
  if (ev.action != KeyAction::Press) return true;  // ignore long-press/release
  // Modal layers swallow input first.
  if (modal_ == Modal::Toast) {
    if (ev.code == KeyCode::Esc && toastSpinner_) {
      wifi_.disconnect();  // cancel in-flight connect
      modal_ = Modal::None;
      showHome();
    }
    return true;  // toast blocks everything else
  }
  if (modal_ == Modal::Confirm) {
    auto r = confirm_.handleKey(ev);
    if (r == ConfirmDialog::Result::First) {        // [Connect]
      modal_ = Modal::None;
      const WifiNetwork* n = store_.find(targetSsid_);
      if (n) startConnect(n->ssid, n->password, false);
    } else if (r == ConfirmDialog::Result::Second) { // [Delete]
      store_.remove(targetSsid_);
      modal_ = Modal::None;
      showSaved();
    } else if (r == ConfirmDialog::Result::Cancel) {
      modal_ = Modal::None;
    }
    requestRedraw();
    return true;
  }

  if (page_ == Page::Password) {
    auto r = input_.handleKey(ev);
    if (r == TextInput::Result::Submit && !input_.text().empty()) {
      startConnect(targetSsid_, input_.text(), /*save=*/true);
    } else if (r == TextInput::Result::Cancel) {
      showScanResults();
    }
    requestRedraw();
    return true;
  }

  if (menu_.handleKey(ev)) { requestRedraw(); return true; }

  if (ev.code == KeyCode::Enter) {
    if (page_ == Page::Home) {
      int sel = menu_.selected();
      bool connected = wifi_.state() == WifiState::Connected;
      if (sel == 1) {  // Scan
        if (wifi_.busy()) {
          // Boot auto-connect (or another scan) still in flight; don't
          // show a spinner that nothing will ever dismiss.
          toast("WiFi busy, try again", false, 1500);
        } else {
          wifi_.startScan();
          toast("Scanning...", true, 0);
        }
      } else if (sel == 2) {  // Saved
        showSaved();
      } else if (sel == 3 && connected) {  // Disconnect
        wifi_.disconnect();
        showHome();
      }
    } else if (page_ == Page::Scan && !wifi_.scanResults().empty()) {
      const ScanResult& r = wifi_.scanResults()[menu_.selected()];
      targetSsid_ = r.ssid;
      if (const WifiNetwork* n = store_.find(r.ssid)) {
        startConnect(n->ssid, n->password, false);
      } else if (!r.secured) {
        startConnect(r.ssid, "", true);
      } else {
        page_ = Page::Password;
        input_.reset("Password for " + r.ssid + ":");
        requestRedraw();
      }
    } else if (page_ == Page::Saved && !store_.networks().empty()) {
      targetSsid_ = store_.networks()[menu_.selected()].ssid;
      confirm_.reset(targetSsid_, "Connect", "Delete");
      modal_ = Modal::Confirm;
      requestRedraw();
    }
    return true;
  }

  if (ev.code == KeyCode::Esc && page_ != Page::Home) {
    showHome();  // back to home instead of popping the app
    return true;
  }
  return false;  // Esc on Home -> AppManager pops back to launcher
}

void WiFiApp::update(uint32_t dtMs) {
  WifiState s = wifi_.state();

  if (wifi_.scanFinished() && modal_ == Modal::Toast && toastSpinner_) {
    modal_ = Modal::None;
    showScanResults();
  }

  if (s != lastSeenState_) {
    lastSeenState_ = s;
    if (s == WifiState::Connected && modal_ == Modal::Toast) {
      toast("Connected: " + wifi_.currentSsid(), false, 1500);
    } else if (s == WifiState::Failed && modal_ == Modal::Toast) {
      WifiError e = wifi_.lastError();
      toast(e == WifiError::AuthFail   ? "Wrong password"
            : e == WifiError::NoApFound ? "Network not found"
            : e == WifiError::Timeout   ? "Connection timed out"
                                        : "Connection failed",
            false, 2000);
    }
    requestRedraw();
  }

  if (modal_ == Modal::Toast) {
    if (toastSpinner_) {
      sinceAnim_ += dtMs;
      if (sinceAnim_ > 120) { sinceAnim_ = 0; requestRedraw(); }  // spinner
    } else if (toastTimerMs_ > 0) {
      toastTimerMs_ = toastTimerMs_ > dtMs ? toastTimerMs_ - dtMs : 0;
      if (toastTimerMs_ == 0) {
        modal_ = Modal::None;
        bool wasAuthFail = wifi_.lastError() == WifiError::AuthFail;
        if (lastSeenState_ == WifiState::Failed && wasAuthFail &&
            page_ != Page::Saved && !targetSsid_.empty()) {
          page_ = Page::Password;  // wrong password -> retry input
          input_.reset("Password for " + targetSsid_ + ":");
        } else {
          showHome();
        }
        requestRedraw();
      }
    }
  }
}

void WiFiApp::render(M5Canvas& gfx) {
  int top = theme::kStatusBarH;
  int h = gfx.height() - top;
  if (page_ == Page::Password) {
    input_.render(gfx, theme::kPadX, top + 12, gfx.width() - 2 * theme::kPadX);
  } else {
    menu_.render(gfx, 0, top, gfx.width(), h);
  }
  if (modal_ == Modal::Toast) dialog::paintToast(gfx, toastMsg_, toastSpinner_);
  if (modal_ == Modal::Confirm) confirm_.render(gfx);
}
