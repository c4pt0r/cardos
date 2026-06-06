#pragma once
#include <string>

#include "../sdk/CardOS.h"

// Push-to-talk voice memo: hold any non-Fn key to record, release to
// upload the WAV to the cardos-voice Cloudflare Worker (which stores audio
// in R2 and metadata in db9). Esc / G0 exits.
class VoiceMemoApp : public App {
 public:
  const char* title() const override { return "Voice Memo"; }
  void onEnter() override;
  void onExit() override;
  bool handleKey(const KeyEvent& ev) override;
  void update(uint32_t dtMs) override;
  void render(M5Canvas& gfx) override;

 private:
  enum class Mode { Idle, Recording, Uploading, Result };

  void startRecording(const KeyEvent& ev);
  bool isRecordKey(const KeyEvent& ev) const;

  Mode mode_ = Mode::Idle;
  ProgressBar meter_;        // live level while recording
  TextView result_;          // upload response / errors
  std::string recPath_;      // file being recorded / pending upload
  std::string status_;       // one-line status for the idle screen
  KeyCode recCode_ = KeyCode::None;  // key that started the recording
  char recCh_ = 0;
  int pendingDelay_ = 0;     // one frame so "Uploading..." paints first
  uint32_t sinceMeter_ = 0;
};
