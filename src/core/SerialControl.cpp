#include "SerialControl.h"

#include <Arduino.h>

#include <cstdlib>

#include "../apps/ScriptHost.h"
#include "../sdk/Fs.h"
#include "SerialProto.h"
#include "crc32.h"

namespace {
void reply(const std::string& s) {
  Serial.print("#CTRL# ");
  Serial.println(s.c_str());
}
std::string appPath(const std::string& name) {
  return std::string(ScriptHost::dir()) + "/" + name;
}
}  // namespace

void SerialControl::tick() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      std::string line = line_;
      line_.clear();
      handleLine(line);
    } else if (line_.size() < 256) {
      line_ += c;
    }
  }
}

void SerialControl::handleLine(const std::string& line) {
  serialproto::Command cmd = serialproto::parse(line);
  if (cmd.empty()) return;
  const auto& v = cmd.verb;
  const auto& a = cmd.args;

  if (v == "PING") {
    reply("PONG");
  } else if (v == "LIST") {
    auto entries = cardos::fs::list(ScriptHost::dir());
    int n = 0;
    for (auto& e : entries)
      if (!e.isDir && serialproto::validAppName(e.name)) n++;
    reply("OK " + std::to_string(n));
    for (auto& e : entries) {
      if (e.isDir || !serialproto::validAppName(e.name)) continue;
      reply("ITEM " + e.name + " " + std::to_string(e.size));
    }
    reply("END");
  } else if (v == "DEL") {
    if (a.empty() || !serialproto::validAppName(a[0])) { reply("ERR bad name"); return; }
    reply(cardos::fs::remove(appPath(a[0])) ? "OK" : "ERR delete failed");
  } else if (v == "RUN") {
    if (a.empty() || !serialproto::validAppName(a[0])) { reply("ERR bad name"); return; }
    if (!cardos::fs::exists(appPath(a[0]))) { reply("ERR no such app"); return; }
    reply(host_ && host_->launch(a[0]) ? "OK" : "ERR launch failed");
  } else if (v == "PUT") {
    if (a.size() < 3) { reply("ERR usage: PUT <name> <size> <crc32hex>"); return; }
    if (!serialproto::validAppName(a[0])) { reply("ERR bad name"); return; }
    size_t size = (size_t)strtoul(a[1].c_str(), nullptr, 10);
    uint32_t crc = (uint32_t)strtoul(a[2].c_str(), nullptr, 16);
    if (size == 0 || size > 64 * 1024) { reply("ERR bad size"); return; }
    doPut(a[0], size, crc);
  } else {
    reply("ERR unknown command");
  }
}

void SerialControl::doPut(const std::string& name, size_t size, uint32_t expectCrc) {
  reply("READY");
  std::string data;
  data.reserve(size);
  uint8_t buf[256];
  uint32_t lastByte = millis();
  while (data.size() < size) {
    size_t want = size - data.size();
    size_t got = Serial.readBytes(buf, want < sizeof(buf) ? want : sizeof(buf));
    if (got > 0) {
      data.append(reinterpret_cast<char*>(buf), got);
      lastByte = millis();
    } else if (millis() - lastByte > 8000) {
      reply("ERR timeout");
      return;
    }
  }
  uint32_t crc = crc32::of(data);
  if (crc != expectCrc) {
    char msg[48];
    snprintf(msg, sizeof(msg), "ERR crc mismatch got %08x", (unsigned)crc);
    reply(msg);
    return;
  }
  reply(cardos::fs::writeFile(appPath(name), data) ? "OK" : "ERR write failed");
}
