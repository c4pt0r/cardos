#pragma once
#include "../core/App.h"

class SysInfoApp : public App {
 public:
  const char* title() const override { return "System Info"; }
  void update(uint32_t dtMs) override;
  void render(M5Canvas& gfx) override;

 private:
  uint32_t sinceRefresh_ = 0;
};
