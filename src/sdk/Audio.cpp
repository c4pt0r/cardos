#include "Audio.h"

#include <M5Cardputer.h>

#include "WavWriter.h"

namespace cardos::audio {
namespace {
constexpr size_t kChunk = 1024;        // samples per buffer (64 ms @ 16 kHz)
int16_t buf_[2][kChunk];
int cur_ = 0;                           // buffer currently being recorded
bool recording_ = false;
uint32_t rate_ = 16000;
ChunkCallback cb_;
WavWriter wav_;
float level_ = 0.f;

void processChunk(const int16_t* s, size_t n) {
  int16_t peak = 0;
  for (size_t i = 0; i < n; i++) {
    int16_t v = s[i] < 0 ? -s[i] : s[i];
    if (v > peak) peak = v;
  }
  level_ = peak / 32768.0f;
  if (cb_) cb_(s, n);
  if (wav_.isOpen()) wav_.write(s, n);
}
}  // namespace

bool start(uint32_t sampleRate, ChunkCallback cb) {
  if (recording_) return false;
  M5Cardputer.Speaker.end();   // mic and speaker share the I2S peripheral
  if (!M5Cardputer.Mic.begin()) {
    Serial.println("[audio] mic begin failed");
    return false;
  }
  rate_ = sampleRate;
  cb_ = std::move(cb);
  recording_ = true;
  cur_ = 0;
  level_ = 0.f;
  M5Cardputer.Mic.record(buf_[cur_], kChunk, rate_);  // prime first buffer
  Serial.printf("[audio] recording @%u Hz\n", (unsigned)rate_);
  return true;
}

bool startToWav(const std::string& path, uint32_t sampleRate) {
  if (recording_) return false;
  if (!wav_.open(path.c_str(), sampleRate)) {
    Serial.printf("[audio] cannot open %s\n", path.c_str());
    return false;
  }
  if (!start(sampleRate, nullptr)) {
    wav_.close();
    return false;
  }
  Serial.printf("[audio] -> %s\n", path.c_str());
  return true;
}

void tick() {
  if (!recording_) return;
  // When the in-flight buffer completes, queue the other one and process
  // the finished data.
  if (!M5Cardputer.Mic.isRecording()) {
    int done = cur_;
    cur_ ^= 1;
    M5Cardputer.Mic.record(buf_[cur_], kChunk, rate_);
    processChunk(buf_[done], kChunk);
  }
}

void stop() {
  if (!recording_) return;
  recording_ = false;
  M5Cardputer.Mic.end();
  if (wav_.isOpen()) {
    wav_.close();
    Serial.println("[audio] wav finalized");
  }
  cb_ = nullptr;
  level_ = 0.f;
}

bool isRecording() { return recording_; }
float level() { return level_; }

}  // namespace cardos::audio
