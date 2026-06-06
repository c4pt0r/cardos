#pragma once
#include <WebServer.h>

#include <cstdio>
#include <string>

// Tiny HTTP server for installing Lua apps over WiFi (the network sibling of
// tools/cardos-app.py). Serves an upload page + REST endpoints; writes to
// /flash/apps. No auth — same trust model as the USB tool (local network).
class AppServer {
 public:
  void begin();   // mkdir /flash/apps, register routes, start listening on :80
  void stop();
  void tick();    // pump; call from the owning app's update()
  bool running() const { return running_; }
  const std::string& lastStatus() const { return lastStatus_; }
  int uploadCount() const { return uploadCount_; }

 private:
  void handleRoot();
  void handleList();
  void handleDelete();
  void handleUpload();          // multipart chunk callback

  WebServer server_{80};
  bool running_ = false;
  std::string lastStatus_;
  int uploadCount_ = 0;

  FILE* uploadFile_ = nullptr;  // open during a multipart upload
  std::string uploadName_;
};
