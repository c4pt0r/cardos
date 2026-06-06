#pragma once
#include <cstdint>
#include <cstdio>

// WAV (16-bit mono PCM) file writer. Uses only cstdio so it runs in
// native tests; on-device it works through the ESP-IDF VFS (/flash, /sd).
namespace cardos::audio {

// Fills out[44] with a canonical PCM WAV header.
void writeWavHeader(uint8_t out[44], uint32_t sampleRate, uint16_t channels,
                    uint32_t dataBytes);

class WavWriter {
 public:
  bool open(const char* path, uint32_t sampleRate);  // writes placeholder header
  void write(const int16_t* samples, size_t count);
  uint32_t dataBytes() const { return dataBytes_; }
  void close();                                      // patches sizes
  bool isOpen() const { return f_ != nullptr; }

 private:
  FILE* f_ = nullptr;
  uint32_t sampleRate_ = 16000;
  uint32_t dataBytes_ = 0;
};

}  // namespace cardos::audio
