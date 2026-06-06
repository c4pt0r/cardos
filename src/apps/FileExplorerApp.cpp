#include "FileExplorerApp.h"

#include <algorithm>

#include "../sdk/FsPath.h"
#include "../ui/Theme.h"

namespace {
std::string fmtSize(size_t bytes) {
  if (bytes < 1024) return std::to_string(bytes) + "B";
  return std::to_string(bytes / 1024) + "K";
}
}  // namespace

void FileExplorerApp::onEnter() {
  path_.clear();
  confirmOpen_ = false;
  notice_.clear();
  noticeMs_ = 0;
  refresh();
}

void FileExplorerApp::refresh() {
  std::vector<MenuItem> items;
  entries_.clear();
  if (path_.empty()) {  // mounts screen
    bool sd = cardos::fs::sdAvailable();
    items.push_back({"/flash", "", 0});
    items.push_back({"/sd", sd ? "" : "no card", theme::kMuted});
  } else {
    entries_ = cardos::fs::list(path_);
    std::sort(entries_.begin(), entries_.end(),
              [](const cardos::fs::Entry& a, const cardos::fs::Entry& b) {
                if (a.isDir != b.isDir) return a.isDir;  // dirs first
                return a.name < b.name;
              });
    for (const auto& e : entries_) {
      if (e.isDir)
        items.push_back({e.name + "/", "", 0});
      else
        items.push_back({e.name, fmtSize(e.size), theme::kMuted});
    }
    if (items.empty()) items.push_back({"(empty)", "", theme::kMuted});
  }
  menu_.setItems(std::move(items));
  menu_.setSelected(0);
  requestRedraw();
}

void FileExplorerApp::navigateUp() {
  path_ = cardos::fs::parentPath(path_);
  refresh();
}

void FileExplorerApp::deleteSelected() {
  const auto& e = entries_[menu_.selected()];
  std::string full = path_ + "/" + e.name;
  bool ok;
  if (e.isDir) {
    if (!cardos::fs::list(full).empty()) {
      showNotice("dir not empty");
      return;
    }
    ok = cardos::fs::rmdir(full);
  } else {
    ok = cardos::fs::remove(full);
  }
  if (!ok) showNotice("delete failed");
  refresh();
}

void FileExplorerApp::showNotice(const std::string& msg) {
  notice_ = msg;
  noticeMs_ = 1500;
  requestRedraw();
}

bool FileExplorerApp::handleKey(const KeyEvent& ev) {
  if (ev.action != KeyAction::Press) return true;  // standard guard

  if (confirmOpen_) {
    auto r = confirm_.handleKey(ev);
    if (r == ConfirmDialog::Result::First) {
      confirmOpen_ = false;
      deleteSelected();
    } else if (r != ConfirmDialog::Result::None) {  // Cancel either way
      confirmOpen_ = false;
    }
    requestRedraw();
    return true;
  }

  if (menu_.handleKey(ev)) { requestRedraw(); return true; }

  bool atRoot = path_.empty();
  if (ev.code == KeyCode::Enter) {
    if (atRoot) {
      if (menu_.selected() == 0) { path_ = "/flash"; refresh(); }
      else if (cardos::fs::sdAvailable()) { path_ = "/sd"; refresh(); }
      else showNotice("no SD card");
    } else if (!entries_.empty()) {
      const auto& e = entries_[menu_.selected()];
      if (e.isDir) { path_ += "/" + e.name; refresh(); }
    }
    return true;
  }
  if (ev.code == KeyCode::Backspace) {
    if (!atRoot && !entries_.empty()) {
      confirm_.reset("Delete " + entries_[menu_.selected()].name + "?",
                     "Delete", "Cancel");
      confirmOpen_ = true;
      requestRedraw();
    }
    return true;
  }
  if (ev.code == KeyCode::Esc || ev.code == KeyCode::Left) {
    if (atRoot) return false;  // Esc at root -> launcher
    navigateUp();
    return true;
  }
  return false;
}

void FileExplorerApp::update(uint32_t dtMs) {
  if (noticeMs_ > 0) {
    noticeMs_ -= (int)dtMs;
    if (noticeMs_ <= 0) { notice_.clear(); requestRedraw(); }
  }
}

void FileExplorerApp::render(M5Canvas& gfx) {
  int top = theme::kStatusBarH;
  menu_.render(gfx, 0, top + 2, gfx.width(), gfx.height() - top - 2);
  if (confirmOpen_) confirm_.render(gfx);
  if (!notice_.empty()) dialog::paintToast(gfx, notice_, false);
}
