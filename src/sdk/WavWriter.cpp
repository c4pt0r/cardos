#include "WavWriter.h"

#include <cstring>

namespace cardos::audio {

void writeWavHeader(uint8_t out[44], uint32_t sampleRate, uint16_t channels,
                    uint32_t dataBytes) {
  const uint16_t bits = 16;
  const uint32_t byteRate = sampleRate * channels * (bits / 8);
  const uint16_t blockAlign = channels * (bits / 8);
  const uint32_t riffSize = 36 + dataBytes;
  const uint32_t fmtSize = 16;
  const uint16_t pcm = 1;
  memcpy(out, "RIFF", 4);
  memcpy(out + 4, &riffSize, 4);
  memcpy(out + 8, "WAVEfmt ", 8);
  memcpy(out + 16, &fmtSize, 4);
  memcpy(out + 20, &pcm, 2);
  memcpy(out + 22, &channels, 2);
  memcpy(out + 24, &sampleRate, 4);
  memcpy(out + 28, &byteRate, 4);
  memcpy(out + 32, &blockAlign, 2);
  memcpy(out + 34, &bits, 2);
  memcpy(out + 36, "data", 4);
  memcpy(out + 40, &dataBytes, 4);
}

bool WavWriter::open(const char* path, uint32_t sampleRate) {
  close();
  f_ = fopen(path, "wb");
  if (!f_) return false;
  sampleRate_ = sampleRate;
  dataBytes_ = 0;
  uint8_t h[44];
  writeWavHeader(h, sampleRate_, 1, 0);  // placeholder; patched on close
  fwrite(h, 1, 44, f_);
  return true;
}

void WavWriter::write(const int16_t* samples, size_t count) {
  if (!f_) return;
  fwrite(samples, 2, count, f_);
  dataBytes_ += count * 2;
}

void WavWriter::close() {
  if (!f_) return;
  uint8_t h[44];
  writeWavHeader(h, sampleRate_, 1, dataBytes_);
  fseek(f_, 0, SEEK_SET);
  fwrite(h, 1, 44, f_);
  fclose(f_);
  f_ = nullptr;
}

}  // namespace cardos::audio
