#include "ScriptHost.h"

#include <algorithm>

#include "../core/AppManager.h"
#include "../sdk/Fs.h"
#include "ScriptApp.h"

ScriptHost::ScriptHost() = default;
ScriptHost::~ScriptHost() = default;

void ScriptHost::begin(AppManager* apps) {
  apps_ = apps;
  cardos::fs::mkdir(dir());
}

std::vector<std::string> ScriptHost::listFiles() {
  std::vector<std::string> out;
  for (const auto& e : cardos::fs::list(dir())) {
    if (e.isDir) continue;
    if (e.name.size() > 4 && e.name.substr(e.name.size() - 4) == ".lua")
      out.push_back(e.name);
  }
  std::sort(out.begin(), out.end());
  return out;
}

bool ScriptHost::launch(const std::string& name) {
  if (!apps_) return false;
  pruneDetached();  // reclaim previously-exited apps before adding another
  auto app = std::make_unique<ScriptApp>(std::string(dir()) + "/" + name);
  ScriptApp* p = app.get();
  owned_.push_back(std::move(app));
  apps_->push(p);  // implicit upcast ScriptApp* -> App*
  return true;
}

// Free any owned ScriptApp that is no longer on the AppManager stack. Only
// detached apps are freed, so this can never invalidate a live stack frame.
void ScriptHost::pruneDetached() {
  if (!apps_) return;
  owned_.erase(
      std::remove_if(owned_.begin(), owned_.end(),
                     [&](const std::unique_ptr<ScriptApp>& a) {
                       return !apps_->onStack(a.get());
                     }),
      owned_.end());
}
