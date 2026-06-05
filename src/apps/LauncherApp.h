#pragma once
#include <vector>

#include "../core/App.h"
#include "../ui/MenuList.h"

// Root menu. Other apps register once at startup via addEntry().
class LauncherApp : public App {
 public:
  void addEntry(const char* label, App* app);
  const char* title() const override { return "CardOS"; }
  void onEnter() override;
  bool handleKey(const KeyEvent& ev) override;
  void render(M5Canvas& gfx) override;

 private:
  void rebuild();
  std::vector<std::pair<const char*, App*>> entries_;
  MenuList menu_;
};
