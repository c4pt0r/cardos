#include "AppServer.h"

#include <Arduino.h>
#include <WiFi.h>

#include "../apps/ScriptHost.h"
#include "../core/SerialProto.h"
#include "../sdk/Fs.h"

namespace {
std::string appPath(const std::string& name) {
  return std::string(ScriptHost::dir()) + "/" + name;
}
std::string htmlEscape(const std::string& s) {
  std::string o;
  for (char c : s) {
    if (c == '&') o += "&amp;";
    else if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else o += c;
  }
  return o;
}
}  // namespace

void AppServer::begin() {
  if (running_) return;
  cardos::fs::mkdir(ScriptHost::dir());
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/api/apps", HTTP_GET, [this]() { handleList(); });
  server_.on("/delete", HTTP_GET, [this]() { handleDelete(); });
  server_.on(
      "/upload", HTTP_POST,
      [this]() {  // request complete: report + link back
        std::string body =
            "<!doctype html><meta charset=utf-8><body style='font-family:sans-serif'>" +
            (uploadName_.empty() ? std::string("<p>upload rejected (name must end .lua)</p>")
                                 : "<p>uploaded " + htmlEscape(uploadName_) + "</p>") +
            "<p><a href='/'>&larr; back</a></p></body>";
        server_.send(uploadName_.empty() ? 400 : 200, "text/html", body.c_str());
        uploadName_.clear();
      },
      [this]() { handleUpload(); });
  server_.begin();
  running_ = true;
  Serial.println("[appsrv] listening on :80");
}

void AppServer::stop() {
  if (!running_) return;
  server_.stop();
  if (uploadFile_) { fclose(uploadFile_); uploadFile_ = nullptr; }
  running_ = false;
  Serial.println("[appsrv] stopped");
}

void AppServer::tick() {
  if (running_) server_.handleClient();
}

void AppServer::handleRoot() {
  std::string h =
      "<!doctype html><html><head><meta charset=utf-8>"
      "<meta name=viewport content='width=device-width,initial-scale=1'>"
      "<title>CardOS Apps</title><style>"
      "body{font-family:sans-serif;margin:1.5em;max-width:34em;color:#222}"
      "h2{color:#0a7}li{margin:.3em 0}a{color:#c33}"
      "form{margin:1em 0;padding:1em;background:#f4f4f4;border-radius:8px}"
      "code{background:#eee;padding:.1em .3em}</style></head><body>"
      "<h2>CardOS App Uploader</h2>"
      "<form method=POST action=/upload enctype=multipart/form-data>"
      "<input type=file name=app accept=.lua required> "
      "<button>Upload</button></form>"
      "<h3>Installed apps</h3><ul>";
  bool any = false;
  for (const auto& e : cardos::fs::list(ScriptHost::dir())) {
    if (e.isDir || !serialproto::validAppName(e.name)) continue;
    any = true;
    h += "<li>" + htmlEscape(e.name) + " (" + std::to_string(e.size) + " B) "
         "<a href='/delete?name=" + htmlEscape(e.name) + "'>delete</a></li>";
  }
  if (!any) h += "<li><i>(none yet)</i></li>";
  h += "</ul><p><small>CLI: <code>curl -F \"app=@hello.lua\" "
       "http://" + std::string(WiFi.localIP().toString().c_str()) +
       "/upload</code></small></p></body></html>";
  server_.send(200, "text/html", h.c_str());
}

void AppServer::handleList() {
  std::string j = "{\"apps\":[";
  bool first = true;
  for (const auto& e : cardos::fs::list(ScriptHost::dir())) {
    if (e.isDir || !serialproto::validAppName(e.name)) continue;
    if (!first) j += ",";
    first = false;
    j += "{\"name\":\"" + e.name + "\",\"size\":" + std::to_string(e.size) + "}";
  }
  j += "]}";
  server_.send(200, "application/json", j.c_str());
}

void AppServer::handleDelete() {
  std::string name = server_.arg("name").c_str();
  if (!serialproto::validAppName(name)) {
    server_.send(400, "text/plain", "bad name");
    return;
  }
  cardos::fs::remove(appPath(name));
  server_.sendHeader("Location", "/");
  server_.send(303, "text/plain", "deleted");
}

void AppServer::handleUpload() {
  HTTPUpload& up = server_.upload();
  if (up.status == UPLOAD_FILE_START) {
    uploadName_.clear();
    std::string name = serialproto::uploadName(up.filename.c_str());
    if (name.empty()) { uploadFile_ = nullptr; return; }
    uploadFile_ = fopen(appPath(name).c_str(), "wb");
    if (uploadFile_) uploadName_ = name;
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (uploadFile_) fwrite(up.buf, 1, up.currentSize, uploadFile_);
  } else if (up.status == UPLOAD_FILE_END) {
    if (uploadFile_) {
      fclose(uploadFile_);
      uploadFile_ = nullptr;
      uploadCount_++;
      lastStatus_ = uploadName_ + " (" + std::to_string(up.totalSize) + " B)";
    }
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (uploadFile_) { fclose(uploadFile_); uploadFile_ = nullptr; }
    uploadName_.clear();
  }
}
