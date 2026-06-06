#include "RecorderApp.h"

#include <M5Cardputer.h>

std::string RecorderApp::recDir() const {
  return cardos::fs::sdAvailable() ? "/sd/rec" : "/flash/rec";
}

void RecorderApp::onEnter() {
  mode_ = Mode::List;
  confirmOpen_ = false;
  refreshList();
}

void RecorderApp::refreshList() {
  std::string dir = recDir();
  cardos::fs::mkdir(dir);
  files_.clear();
  std::vector<MenuItem> items;
  for (const auto& e : cardos::fs::list(dir)) {
    if (e.isDir) continue;
    files_.push_back(dir + "/" + e.name);
    items.push_back({e.name, std::to_string(e.size / 1024) + "K",
                     theme::kMuted});
  }
  if (items.empty()) items.push_back({"(no recordings)", "", theme::kMuted});
  menu_.setItems(std::move(items));
  requestRedraw();
}

bool RecorderApp::handleKey(const KeyEvent& ev) {
  // Push-to-talk: explicit LongPress/Release handling BEFORE the guard.
  if (ev.code == KeyCode::Char && ev.ch == ' ') {
    if (ev.action == KeyAction::LongPress && mode_ == Mode::List) {
      std::string path = recDir() + "/rec-" + std::to_string(millis() / 1000) +
                         ".wav";
      if (cardos::audio::startToWav(path)) {
        mode_ = Mode::Recording;
        requestRedraw();
      }
      return true;
    }
    if (ev.action == KeyAction::Release && mode_ == Mode::Recording) {
      cardos::audio::stop();
      mode_ = Mode::List;
      refreshList();
      return true;
    }
  }
  if (ev.action != KeyAction::Press) return true;  // standard guard
  if (!pendingUpload_.empty()) return true;        // uploading: ignore keys

  if (confirmOpen_) {
    auto r = confirm_.handleKey(ev);
    if (r == ConfirmDialog::Result::First) {        // Upload
      confirmOpen_ = false;
      startUpload(files_[menu_.selected()]);
    } else if (r == ConfirmDialog::Result::Second) { // Delete
      cardos::fs::remove(files_[menu_.selected()]);
      confirmOpen_ = false;
      refreshList();
    } else if (r == ConfirmDialog::Result::Cancel) {
      confirmOpen_ = false;
    }
    requestRedraw();
    return true;
  }

  if (mode_ == Mode::Result) {
    if (result_.handleKey(ev)) { requestRedraw(); return true; }
    if (ev.code == KeyCode::Esc || ev.code == KeyCode::Enter) {
      mode_ = Mode::List;
      requestRedraw();
      return true;
    }
    return true;
  }

  if (menu_.handleKey(ev)) { requestRedraw(); return true; }
  if (ev.code == KeyCode::Enter && !files_.empty()) {
    confirm_.reset(menu_.selected() < (int)files_.size()
                       ? files_[menu_.selected()]
                       : "",
                   "Upload", "Delete");
    confirmOpen_ = !files_.empty();
    requestRedraw();
    return true;
  }
  return false;  // Esc -> launcher
}

void RecorderApp::startUpload(const std::string& path) {
  pendingUpload_ = path;
  pendingDelay_ = 1;
  result_.setText("Uploading " + path + " ...");
  mode_ = Mode::Result;
  requestRedraw();
}

void RecorderApp::update(uint32_t dtMs) {
  if (mode_ == Mode::Recording) {
    sinceMeter_ += dtMs;
    if (sinceMeter_ >= 100) {   // 10 Hz level meter refresh
      sinceMeter_ = 0;
      meter_.setValue((int)(cardos::audio::level() * 100));
      requestRedraw();
    }
  }
  if (!pendingUpload_.empty()) {
    if (pendingDelay_ > 0) { pendingDelay_--; return; }
    std::string path = pendingUpload_;
    // Progress paints straight to the display: the loop is blocked.
    ProgressBar bar;
    auto progress = [&](size_t sent, size_t total) {
      bar.setValue((int)(sent * 100 / (total ? total : 1)));
      M5Canvas tmp(&M5Cardputer.Display);
      tmp.createSprite(M5Cardputer.Display.width(), 24);
      tmp.fillSprite(TFT_BLACK);
      bar.render(tmp, theme::kPadX, 2,
                 M5Cardputer.Display.width() - 2 * theme::kPadX, 20, true);
      tmp.pushSprite(0, M5Cardputer.Display.height() - 24);
      tmp.deleteSprite();
    };
    auto r = cardos::http::uploadFile("https://httpbin.org/post", path,
                                      "file", {}, progress);
    pendingUpload_.clear();
    std::string text = r.status > 0
        ? "HTTP " + std::to_string(r.status) + "\n" + r.body.substr(0, 400)
        : "Upload failed: " + r.error;
    result_.setText(text);
    requestRedraw();
  }
}

void RecorderApp::render(M5Canvas& gfx) {
  int top = theme::kStatusBarH;
  if (mode_ == Mode::Recording) {
    label::draw(gfx, "REC - release SPACE to stop", theme::kPadX, top + 16,
                theme::kDanger);
    meter_.render(gfx, theme::kPadX, top + 40,
                  gfx.width() - 2 * theme::kPadX, 16);
    return;
  }
  if (mode_ == Mode::Result) {
    result_.render(gfx, 0, top, gfx.width(), gfx.height() - top);
    return;
  }
  label::draw(gfx, "Hold SPACE to record", theme::kPadX, top + 2,
              theme::kMuted);
  menu_.render(gfx, 0, top + 18, gfx.width(), gfx.height() - top - 18);
  if (confirmOpen_) confirm_.render(gfx);
}
