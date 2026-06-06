# CardOS SDK Design Document

Date: 2026-06-06
Builds on: CardOS MVP (see `2026-06-05-cardos-design.md`)

## 1. Goal

A simple app-development SDK for CardOS. Form: a **static C++ API compiled into the firmware** (apps live in `src/apps/` as before). The SDK is the existing framework (App/AppManager, widgets, services) plus five new capability areas, one umbrella header, documentation, and an end-to-end example app.

Capabilities:

1. **Input** — key press / long-press / release events
2. **Audio** — microphone recording: streaming chunks + record-to-WAV
3. **Filesystem** — unified VFS over internal flash (`/flash`, LittleFS) and microSD (`/sd`)
4. **HTTP** — GET / POST / JSON / multipart file upload, HTTP + HTTPS (no cert verification), progress callbacks
5. **UI widgets** — existing set plus TextView (scrollable multi-line), ProgressBar, Label

## 2. Architecture: Incremental Evolution

Existing `core/`, `ui/`, `services/` stay in place — they are already part of the SDK surface. New capabilities go to `src/sdk/` under `namespace cardos`. New widgets go to `src/ui/`.

```
src/
├─ core/      (in place; input upgrade happens here)
│   ├─ KeyEvent.h     + KeyAction {Press, LongPress, Release}
│   ├─ KeyTracker.h   NEW: pure per-key state machine (native-testable)
│   └─ InputRouter    upgraded: full matrix state -> KeyTracker -> events
├─ ui/        (in place + TextView, ProgressBar, Label)
├─ services/  (unchanged)
├─ sdk/       NEW
│   ├─ CardOS.h       umbrella header: includes core/ui/sdk APIs
│   ├─ Audio.{h,cpp}  namespace cardos::audio
│   ├─ WavWriter.{h,cpp}
│   ├─ Fs.{h,cpp}     namespace cardos::fs
│   └─ Http.{h,cpp}   namespace cardos::http
└─ apps/
    └─ RecorderApp    NEW: end-to-end SDK example
```

## 3. Input: Press / LongPress / Release

`KeyEvent` gains `KeyAction action` (default `Press`). New pure header `core/KeyTracker.h`: fed the set of currently-held keys plus a timestamp each frame, it emits:

- key newly held → `Press`
- held ≥ **600 ms** → `LongPress` (fired once per hold)
- key no longer held → `Release`

Tracks up to 8 concurrent keys. `InputRouter` switches from `isChange()` edge-polling to feeding the full `keysState()` snapshot into KeyTracker every frame; special keys (Enter/Backspace/Tab) tracked the same way; G0/BtnA maps M5Unified's `wasPressed`/`wasHold`/`wasReleased` directly.

**Compatibility:** every event flows through the single `App::handleKey()`. Existing widgets and apps add a one-line guard (`if (ev.action != KeyAction::Press) return false;`) so behavior is unchanged; apps that want long-press handle the new actions explicitly. No dual handler API.

## 4. Audio: `cardos::audio`

```cpp
namespace cardos::audio {
using ChunkCallback = std::function<void(const int16_t* samples, size_t count)>;
bool start(uint32_t sampleRate = 16000, ChunkCallback cb = nullptr);  // streaming
bool startToWav(const std::string& path, uint32_t rate = 16000);      // convenience
void stop();          // finalizes WAV header when recording to file
bool isRecording();
float level();        // peak of last chunk, 0..1 (UI meters)
void tick();          // pump; called from the main loop
}
```

- Backed by M5Unified `Mic_Class` (PDM microphone). Double buffer, 2 × 1024 samples (64 ms cadence at 16 kHz); `tick()` collects completed buffers, fires the callback and/or appends to the WAV file.
- `WavWriter`: `fopen` → 44-byte placeholder header → append PCM → patch sizes on close. Header construction is a pure function (native-tested).
- 16 kHz / 16-bit / mono ≈ 32 KB/s (~45 s fits in flash; SD for longer).

## 5. Filesystem: `cardos::fs`

LittleFS mounts at `/flash`, SD at `/sd` via ESP-IDF VFS — after mounting, standard `fopen`/`fread`/`fwrite` work on both transparently (WavWriter relies on this). Convenience layer:

```cpp
namespace cardos::fs {
bool begin();                 // mounts /flash (format on first boot); SD is lazy
bool sdAvailable();           // probe / mount SD on demand
std::string readFile(const std::string& path);   // "" on error
bool writeFile(const std::string& path, const std::string& data, bool append = false);
bool exists(const std::string& path);
bool remove(const std::string& path);
bool mkdir(const std::string& path);
struct Entry { std::string name; size_t size; bool isDir; };
std::vector<Entry> list(const std::string& dir);
uint64_t freeBytes(const std::string& mount);    // "/flash" or "/sd"
}
```

- `platformio.ini`: `board_build.filesystem = littlefs` (~1.5 MB region in the default 8 MB partition table).
- SD over SPI — Cardputer pins SCK=40, MISO=39, MOSI=14, CS=12 (**verify on hardware**). Missing card → `/sd/...` operations fail gracefully (no crash). Hot-insert supported; hot-removal not guaranteed safe.
- Path-prefix routing is pure logic (native-tested).

## 6. HTTP: `cardos::http`

```cpp
namespace cardos::http {
using Headers = std::vector<std::pair<std::string, std::string>>;
struct Response {
  int status = -1;            // -1 = transport failure (see error)
  std::string body, error;
  bool ok() const { return status >= 200 && status < 300; }
};
using Progress = std::function<void(size_t sent, size_t total)>;

Response get(const std::string& url, const Headers& h = {});
Response post(const std::string& url, const std::string& body,
              const std::string& contentType, const Headers& h = {});
Response postJson(const std::string& url, const std::string& json, const Headers& h = {});
Response uploadFile(const std::string& url, const std::string& filePath,  // multipart/form-data
                    const std::string& fieldName = "file",
                    const Headers& h = {}, Progress onProgress = nullptr);
void setTimeout(uint32_t ms);   // default 10000
}
```

- `https://` → `WiFiClientSecure` + `setInsecure()` (encrypted, unverified — pragmatic for a hobby device); `http://` → plain client. Transparent to callers.
- Upload: custom `MultipartStream` (prefix + `FILE*` + suffix as one `Stream`) with `HTTPClient::sendRequest(method, Stream*, size)` — streams from disk (no full-file buffering), fires `Progress` as bytes go out. Multipart prefix/suffix builders are pure functions (native-tested).
- **Calls are blocking** (consistent with the existing HttpDemo). Documented pattern: render a "Requesting..." frame first (one-frame delay), and progress callbacks may paint directly to the display.
- `HttpDemoApp` is refactored onto `cardos::http` as a live validation of the GET path.

## 7. New Widgets

- **TextView** — scrollable multi-line text display. Word-wraps to its width using an injected measure function (`std::function<int(const char*)>` over `gfx.textWidth`) so the wrap algorithm is pure and native-testable; CJK-safe (wraps on any glyph). `;`/`.` scroll (Press only); reuses the scrollbar style.
- **ProgressBar** — `setValue(0..100)`, `render(gfx, x, y, w, h)`, accent fill on muted track, optional percent label.
- **Label** — single line with color/alignment; thin wrapper over font + cursor + print.

All widgets (existing + new) gain the `Press`-only guard from section 3.

## 8. Umbrella Header, Wiring, Docs

- `sdk/CardOS.h` includes: App/AppManager/KeyEvent, all widgets, Theme, `cardos::audio/fs/http`, WiFiService accessor. A new app includes this one header.
- `main.cpp`: `cardos::fs::begin()` in `setup()`; `cardos::audio::tick()` in `loop()`.
- `docs/sdk.md` (English): API reference per module + a "write your first app" tutorial (the minimal App subclass + launcher registration).

## 9. Example App: Recorder (end-to-end SDK validation)

Push-to-talk voice memo + upload:

- Home: Label hint ("Hold SPACE to record"), live level meter (ProgressBar) while recording, MenuList of saved recordings.
- **Hold Space** (`LongPress`) → `audio::startToWav("/sd/rec/NNN.wav")` (falls back to `/flash/rec/` when no card); **Release** → `stop()`, list refreshes. Exercises long-press/release + audio + fs.
- Select a recording → ConfirmDialog **[Upload] [Delete]**. Upload → `http::uploadFile("https://httpbin.org/post", path, "file", {}, progress)` with ProgressBar; response summary (status, bytes) shown in a TextView. Delete → `fs::remove`.
- Exercises every SDK capability in one app.

## 10. Testing Strategy

- **Native unit tests** (extend the existing suite): KeyTracker sequences (press → long → release; multi-key; 600 ms boundary), WAV header bytes, fs path routing, multipart prefix/suffix, TextView wrap algorithm (injected measurer).
- **Existing 25 tests must stay green**; widget Press-guard changes covered by updated expectations where applicable.
- **On-device acceptance**: record → playback file exists with correct size; upload to httpbin returns 200 with file echoed; SD present/absent paths; long-press in Recorder; HttpDemo still works on `cardos::http`; WiFi/launcher regression pass.

## 11. Out of Scope

Audio playback (speaker), multi-line text editor, dynamic app loading, scripting runtimes, certificate verification, OTA, auto-repeat keys, hot-unplug-safe SD.

## 12. Risks

- M5Unified `Mic_Class` double-buffer semantics need on-device validation (chunk cadence, dropouts while writing to SD).
- SD SPI pin mapping verified on hardware in the first fs task.
- `LongPress` threshold (600 ms) may need tuning against the matrix scan feel.
- Blocking HTTP freezes the UI during requests/uploads; acceptable for MVP, documented.
- Flash budget: TLS (~150 KB) + LittleFS driver + SD already partially linked via M5 libs; current firmware at 49.6% of 3.3 MB app partition — headroom is ample.
