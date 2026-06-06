#pragma once
#include <memory>
#include <string>
#include <vector>

class AppManager;
class ScriptApp;

// Owns dynamically-launched Lua apps and pushes them onto the AppManager.
// Apps live in /flash/apps/*.lua.
class ScriptHost {
 public:
  ScriptHost();
  ~ScriptHost();  // out-of-line: unique_ptr<ScriptApp> needs the complete type

  void begin(AppManager* apps);
  static const char* dir() { return "/flash/apps"; }

  std::vector<std::string> listFiles();    // "*.lua" filenames, sorted
  bool launch(const std::string& name);    // construct + push (takes ownership)
  void pruneDetached();                    // free owned apps no longer on the stack

 private:
  AppManager* apps_ = nullptr;
  std::vector<std::unique_ptr<ScriptApp>> owned_;
};
