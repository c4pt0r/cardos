#pragma once
#include <string>

#include "../sdk/CardOS.h"

// SDK showcase: push-to-talk voice memos with upload.
// Hold SPACE to record (LongPress starts, Release stops); recordings are
// listed below; Enter on one -> [Upload] [Delete].
class RecorderApp : public App {
 public:
  const char* title() const override { return "Recorder"; }
  void onEnter() override;
  bool handleKey(const KeyEvent& ev) override;
  void update(uint32_t dtMs) override;
  void render(M5Canvas& gfx) override;

 private:
  enum class Mode { List, Recording, Result };

  void refreshList();
  std::string recDir() const;   // "/sd/rec" when card present else "/flash/rec"
  void startUpload(const std::string& path);

  Mode mode_ = Mode::List;
  MenuList menu_;
  ProgressBar meter_;           // live level while recording
  TextView result_;             // upload response / errors
  ConfirmDialog confirm_;
  bool confirmOpen_ = false;
  std::vector<std::string> files_;  // full VFS paths, parallel to menu
  std::string pendingUpload_;   // one-frame delay before blocking upload
  int pendingDelay_ = 0;
  uint32_t sinceMeter_ = 0;
};
