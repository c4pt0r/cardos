#pragma once
#include <string>

class ScriptHost;

// Host-controlled app management over USB serial. Drains the serial port
// each loop and dispatches line commands (PING/LIST/PUT/DEL/RUN). All
// replies are prefixed "#CTRL# " so the Python tool can ignore interleaved
// firmware log lines. See tools/cardos-app.py and SerialProto.h.
class SerialControl {
 public:
  void begin(ScriptHost* host) { host_ = host; }
  void tick();  // call once per loop

 private:
  void handleLine(const std::string& line);
  void doPut(const std::string& name, size_t size, uint32_t expectCrc);

  ScriptHost* host_ = nullptr;
  std::string line_;
};
