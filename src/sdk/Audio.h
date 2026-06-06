#pragma once
#include <cstdint>
#include <functional>
#include <string>

// Microphone recording over M5Unified Mic_Class (PDM mic).
// Non-blocking: start*() begins capture, tick() (called from the main
// loop) drains completed chunks into the callback and/or the WAV file.
namespace cardos::audio {

using ChunkCallback = std::function<void(const int16_t* samples, size_t count)>;

bool start(uint32_t sampleRate = 16000, ChunkCallback cb = nullptr);
bool startToWav(const std::string& path, uint32_t sampleRate = 16000);
void stop();          // finalizes the WAV header when recording to file
bool isRecording();
float level();        // peak amplitude of the last chunk, 0..1
void tick();          // pump; call once per loop

}  // namespace cardos::audio
