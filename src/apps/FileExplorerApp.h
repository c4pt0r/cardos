#pragma once
#include <string>
#include <vector>

#include "../core/App.h"
#include "../sdk/Fs.h"
#include "../ui/Dialog.h"
#include "../ui/MenuList.h"

// Minimal file manager: browse /flash and /sd, delete files and empty
// directories. path_ == "" is the mounts screen.
class FileExplorerApp : public App {
 public:
  const char* title() const override {
    return path_.empty() ? "Files" : path_.c_str();
  }
  void onEnter() override;
  bool handleKey(const KeyEvent& ev) override;
  void update(uint32_t dtMs) override;
  void render(M5Canvas& gfx) override;

 private:
  void refresh();
  void navigateUp();
  void deleteSelected();
  void showNotice(const std::string& msg);

  std::string path_;                       // current dir; "" = mounts screen
  std::vector<cardos::fs::Entry> entries_; // mirrors menu rows (dirs first)
  MenuList menu_;
  ConfirmDialog confirm_;
  bool confirmOpen_ = false;
  std::string notice_;
  int noticeMs_ = 0;
};
