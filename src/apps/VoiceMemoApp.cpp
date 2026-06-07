#include "VoiceMemoApp.h"

#include <ArduinoJson.h>
#include <M5Cardputer.h>
#include <WiFi.h>

#include "../sdk/Fs.h"

namespace {
// Deployed cardos-voice Worker (see backend/voice-worker). The upload key
// is a shared secret compiled into firmware — it deters casual abuse of a
// public endpoint; it is not extraction-proof on a hobby device.
constexpr const char* kWorkerUrl = "https://cardos-voice.db9.workers.dev";
constexpr const char* kUploadKey = "2553a72b9ac8d2caa3e112d440c69bb7";
constexpr const char* kRecDir = "/flash/voice";
}  // namespace

void VoiceMemoApp::onEnter() {
  cardos::http::setTimeout(30000);
  mode_ = Mode::Idle;
  if (status_.empty()) status_ = "Hold any key to record";
  requestRedraw();
}

void VoiceMemoApp::onExit() {
  cardos::http::setTimeout(10000);  // restore the global default
  // Don't leave the mic running if we're popped mid-recording.
  if (mode_ == Mode::Recording) cardos::audio::stop();
}

bool VoiceMemoApp::isRecordKey(const KeyEvent& ev) const {
  return ev.code == recCode_ && ev.ch == recCh_;
}

void VoiceMemoApp::startRecording(const KeyEvent& ev) {
  cardos::fs::mkdir(kRecDir);
  recPath_ = std::string(kRecDir) + "/memo-" +
             std::to_string(millis() / 1000) + ".wav";
  if (!cardos::audio::startToWav(recPath_)) {
    status_ = "mic start failed";
    recPath_.clear();
    requestRedraw();
    return;
  }
  mode_ = Mode::Recording;
  recCode_ = ev.code;
  recCh_ = ev.ch;
  meter_.setValue(0);
  requestRedraw();
}

bool VoiceMemoApp::handleKey(const KeyEvent& ev) {
  if (ev.fn) return true;  // function-modified keys are excluded

  // Esc / G0 exits. Stop any recording first, then let the framework pop.
  if (ev.code == KeyCode::Esc) {
    if (ev.action != KeyAction::Press) return true;  // swallow long/release
    if (mode_ == Mode::Recording) cardos::audio::stop();
    return false;  // unconsumed Esc -> AppManager pops to launcher
  }

  if (mode_ == Mode::Uploading) return true;  // busy: ignore input

  // Release of the recording key -> stop and queue the upload.
  if (mode_ == Mode::Recording) {
    if (ev.action == KeyAction::Release && isRecordKey(ev)) {
      cardos::audio::stop();
      if (WiFi.status() != WL_CONNECTED) {
        mode_ = Mode::Result;
        result_.setText("Saved " + recPath_ +
                        "\n\nWiFi not connected - upload skipped.");
        recPath_.clear();
        requestRedraw();
        return true;
      }
      mode_ = Mode::Uploading;
      pendingDelay_ = 1;
      result_.setText("Uploading...");
      requestRedraw();
    }
    return true;
  }

  // Idle or showing a result: any key press starts a new recording.
  if (ev.action == KeyAction::Press) startRecording(ev);
  return true;
}

void VoiceMemoApp::update(uint32_t dtMs) {
  if (mode_ == Mode::Recording) {
    sinceMeter_ += dtMs;
    if (sinceMeter_ >= 100) {  // 10 Hz level meter
      sinceMeter_ = 0;
      meter_.setValue((int)(cardos::audio::level() * 100));
      requestRedraw();
    }
    return;
  }

  if (mode_ == Mode::Uploading && !recPath_.empty()) {
    if (pendingDelay_ > 0) { pendingDelay_--; return; }  // let frame paint
    std::string path = recPath_;
    recPath_.clear();

    // Progress paints straight to the display: the loop is blocked here.
    ProgressBar bar;
    auto progress = [&](size_t sent, size_t total) {
      bar.setValue((int)(sent * 100 / (total ? total : 1)));
      M5Canvas tmp(&M5Cardputer.Display);
      tmp.createSprite(M5Cardputer.Display.width(), 24);
      tmp.fillSprite(TFT_BLACK);
      bar.render(tmp, theme::kPadX, 2,
                 M5Cardputer.Display.width() - 2 * theme::kPadX, 20, true);
      tmp.pushSprite(0, M5Cardputer.Display.height() - 24);
      tmp.deleteSprite();
    };

    cardos::http::Headers h = {{"X-Upload-Key", kUploadKey},
                               {"X-Device", "cardputer"},
                               {"X-Sample-Rate", "16000"}};
    auto r = cardos::http::uploadFile(std::string(kWorkerUrl) + "/upload",
                                      path, "file", h, progress);
    if (r.ok()) {
      cardos::fs::remove(path);  // uploaded: drop the local copy (1.5MB fs)
      JsonDocument doc;
      std::string text, err;
      if (deserializeJson(doc, r.body) == DeserializationError::Ok) {
        auto pick = [&](const char* k) -> std::string {
          const char* v = doc[k].as<const char*>();
          return v ? std::string(v) : std::string();
        };
        text = pick("cleaned");
        if (text.empty()) text = pick("corrected");
        if (text.empty()) text = pick("raw");
        err = pick("error");
      }
      if (!text.empty()) {
        status_ = "Transcribed";
        result_.setText(text + (err.empty() ? "" : "\n[warn: " + err + "]"));
      } else {
        status_ = "Uploaded (no transcript)";
        result_.setText(err.empty() ? r.body.substr(0, 200)
                                    : "Pipeline error:\n" + err);
      }
    } else {
      status_ = "Upload failed";
      result_.setText(r.status > 0
                          ? "HTTP " + std::to_string(r.status) + "\n" +
                                r.body.substr(0, 400)
                          : "Upload failed: " + r.error);
    }
    mode_ = Mode::Result;
    requestRedraw();
  }
}

void VoiceMemoApp::render(M5Canvas& gfx) {
  int top = theme::kStatusBarH;
  if (mode_ == Mode::Recording) {
    label::draw(gfx, "REC - release to send", theme::kPadX, top + 16,
                theme::kDanger);
    meter_.render(gfx, theme::kPadX, top + 40,
                  gfx.width() - 2 * theme::kPadX, 16);
    return;
  }
  if (mode_ == Mode::Uploading) {
    label::draw(gfx, "Uploading + transcribing...", theme::kPadX, top + 16, theme::kAccent);
    return;
  }
  if (mode_ == Mode::Result) {
    result_.render(gfx, 0, top, gfx.width(), gfx.height() - top);
    return;
  }
  // Idle
  label::draw(gfx, "Hold any key to record", theme::kPadX, top + 8,
              theme::kFg);
  label::draw(gfx, status_, theme::kPadX, top + 30, theme::kMuted);
  label::draw(gfx, "Esc: back", theme::kPadX, gfx.height() - 16,
              theme::kMuted);
}
