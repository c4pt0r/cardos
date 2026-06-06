# CardOS App SDK

The CardOS SDK is the set of headers an app uses to draw to the screen,
read the keyboard, record audio, talk to the network, and read/write
files on the M5Cardputer. This document is the reference and a short
tutorial.

## Overview

An **app** is a `class` deriving from `App` (a "scene"). The launcher
pushes your app onto a stack; while it's on top it receives input and
renders the content area below the status bar. Pressing **Esc** (the
backtick key or the G0 button) that your app doesn't consume pops it and
returns to the launcher.

**One include.** Everything below is available through a single header:

```cpp
#include "../sdk/CardOS.h"
```

**Cooperative main loop.** CardOS is single-threaded and cooperative.
`handleKey()`, `update()`, and `render()` are called from one loop and
must return quickly — **never block** in them, with one documented
exception: the `cardos::http` calls block while a request completes
(seconds). Render a "Requesting…" frame *before* calling them so the user
sees feedback (see [HTTP](#http)). Long-running work that isn't a single
HTTP call should be sliced across `update()` calls.

The loop, each frame, does: `M5Cardputer.update()` → `cardos::audio::tick()`
→ deliver input events → `app->update(dt)` → `app->render(canvas)`.

## Your first app

```cpp
#include "../sdk/CardOS.h"

class HelloApp : public App {
 public:
  const char* title() const override { return "Hello"; }
  bool handleKey(const KeyEvent& ev) override {
    if (ev.action != KeyAction::Press) return false;
    if (ev.code == KeyCode::Enter) { count_++; requestRedraw(); return true; }
    return false;  // Esc falls through -> framework pops back to launcher
  }
  void render(M5Canvas& gfx) override {
    label::draw(gfx, "Enter pressed " + std::to_string(count_) + "x",
                theme::kPadX, theme::kStatusBarH + 8);
  }
 private:
  int count_ = 0;
};
```

Register it in `src/main.cpp`: declare `HelloApp hello;` as a global and add
`launcher.addEntry("Hello", &hello);` in `setup()`.

**App lifecycle hooks** (all optional except `title()` and `render()`):

| Method | When |
|---|---|
| `const char* title()` | shown in the status bar (required) |
| `void onEnter()` | each time the app becomes the top scene |
| `void onExit()` | when it is popped |
| `bool handleKey(const KeyEvent&)` | one call per input event; return `true` if consumed |
| `void update(uint32_t dtMs)` | every frame with elapsed milliseconds |
| `void render(M5Canvas& gfx)` | every frame the screen is dirty (required) |

Call `requestRedraw()` whenever your state changes so the next frame
repaints (rendering is skipped when nothing is dirty).

## Input

Each event is a `KeyEvent`:

```cpp
struct KeyEvent {
  KeyCode code;     // semantic key
  char ch;          // raw printable char (Char and nav keys)
  bool fn;          // Fn modifier held
  KeyAction action; // Press / LongPress / Release
};
```

`KeyCode`: `None, Up, Down, Left, Right, Enter, Esc, Backspace, Tab, Char`.

The Cardputer has no arrow keys, so navigation is mapped onto punctuation
(the char is still delivered in `ch` so a focused `TextInput` can type it):

| Key | KeyCode | `ch` |
|---|---|---|
| `;` | `Up` | `;` |
| `.` | `Down` | `.` |
| `,` | `Left` | `,` |
| `/` | `Right` | `/` |
| `` ` `` | `Esc` | — |
| any other printable | `Char` | the char |
| G0 button | `Esc` | — |

**KeyAction semantics:**

- `Press` — fired once when a key goes down. **This is what you almost
  always want.** Most handlers start with the guard
  `if (ev.action != KeyAction::Press) return false;`.
- `LongPress` — fired once, ~600 ms after a key goes down while still held
  (`KeyTracker::kLongPressMs`). Used for push-to-talk and similar.
- `Release` — fired once when the key comes up.

Up to 8 keys are tracked concurrently; extra simultaneous keys are
ignored. If you don't handle `LongPress`/`Release`, the press guard above
silently drops them — exactly what menu-style apps want.

## Audio

`namespace cardos::audio` records from the built-in PDM microphone:
16 kHz, mono, 16-bit. Recording is **non-blocking** — capture happens in
the background and `tick()` (already called by the main loop) drains
finished chunks.

```cpp
bool  start(uint32_t sampleRate = 16000, ChunkCallback cb = nullptr);
bool  startToWav(const std::string& path, uint32_t sampleRate = 16000);
void  stop();          // finalizes the WAV header when recording to a file
bool  isRecording();
float level();         // peak amplitude of the last chunk, 0..1
void  tick();          // pump; the main loop already calls this
```

`ChunkCallback` is `void(const int16_t* samples, size_t count)` — invoked
~16 times/second (1024-sample chunks ≈ 64 ms each) for streaming uses.
`startToWav()` writes a valid `.wav` to a `/flash` or `/sd` path; `stop()`
patches the header sizes. `level()` drives a VU meter.

The mic and speaker share the I2S peripheral, so `start()` calls
`Speaker.end()` first — you cannot record and play simultaneously.

```cpp
cardos::audio::startToWav("/flash/rec/memo.wav");
// ... later, on key release ...
cardos::audio::stop();
```

## Filesystem

`namespace cardos::fs` presents two mounts behind one API:

- `/flash` — internal LittleFS (formatted on first boot, always present)
- `/sd` — microSD, mounted lazily the first time it's used

```cpp
bool begin();        // mounts /flash; called once from setup()
bool sdAvailable();  // probes/mounts the SD card on demand

std::string readFile(const std::string& path);             // "" on error
bool writeFile(const std::string& path, const std::string& data,
               bool append = false);
bool exists(const std::string& path);
bool remove(const std::string& path);
bool mkdir(const std::string& path);
std::vector<Entry> list(const std::string& dir);  // Entry{name,size,isDir}
uint64_t freeBytes(const std::string& mount);      // "/flash" or "/sd"
```

Paths must start with `/flash` or `/sd`; anything else fails. After
`begin()`, plain POSIX `fopen()/fread()` also works on both mounts via the
ESP-IDF VFS — that's how `WavWriter` and HTTP uploads stream files.

```cpp
if (cardos::fs::sdAvailable())
  cardos::fs::writeFile("/sd/notes.txt", "hello\n", /*append=*/true);
for (auto& e : cardos::fs::list("/flash/rec"))
  Serial.printf("%s  %u bytes\n", e.name.c_str(), (unsigned)e.size);
```

## HTTP

`namespace cardos::http` is a **blocking** HTTP(S) client. `https://`
URLs use TLS without certificate verification (pragmatic for a hobby
device).

```cpp
struct Response {
  int status;          // -1 = transport failure (see error)
  std::string body;
  std::string error;
  bool ok() const;     // 200..299
};

Response get(const std::string& url, const Headers& h = {});
Response post(const std::string& url, const std::string& body,
              const std::string& contentType, const Headers& h = {});
Response postJson(const std::string& url, const std::string& json,
                  const Headers& h = {});
Response uploadFile(const std::string& url, const std::string& filePath,
                    const std::string& fieldName = "file",
                    const Headers& h = {}, Progress onProgress = nullptr);
void setTimeout(uint32_t ms);  // default 10000
```

`Headers` is `std::vector<std::pair<std::string,std::string>>`. `Progress`
is `void(size_t sent, size_t total)`, called periodically during an
upload. `uploadFile` streams the file as `multipart/form-data` straight
from the VFS — no full-file buffer in RAM.

**Blocking caveat / one-frame-delay pattern.** Because these calls block
the loop for seconds, paint a "working" frame first, then fire the request
on the *next* `update()`:

```cpp
bool handleKey(const KeyEvent& ev) override {
  if (ev.action != KeyAction::Press) return true;
  if (ev.code == KeyCode::Enter) {
    pendingUrl_ = url; pendingDelay_ = 1;   // run next frame
    status_ = "Requesting...";              // shown this frame
    requestRedraw();
    return true;
  }
  return false;
}
void update(uint32_t) override {
  if (pendingUrl_.empty()) return;
  if (pendingDelay_ > 0) { pendingDelay_--; return; }  // let the frame paint
  auto r = cardos::http::get(pendingUrl_);             // blocks ~seconds
  pendingUrl_.clear();
  status_ = r.ok() ? "HTTP " + std::to_string(r.status) : "err: " + r.error;
  requestRedraw();
}
```

For uploads, the `onProgress` callback can paint a `ProgressBar` directly
to the display while the loop is blocked (see `RecorderApp`).

## Widgets

All widgets render into the app's full-screen `M5Canvas` and take a
rectangle. Most consume only `Press` events.

**MenuList** — scrollable, selectable list:
```cpp
menu_.setItems({{"One", "", 0}, {"Two", "12K", theme::kMuted}});
if (menu_.handleKey(ev)) { requestRedraw(); return true; }
if (ev.code == KeyCode::Enter) doThing(menu_.selected());
menu_.render(gfx, 0, theme::kStatusBarH, gfx.width(), gfx.height());
```

**TextInput** — one-line editor (Enter=submit, Esc=cancel, Tab=toggle mask):
```cpp
input_.reset("Password:");
switch (input_.handleKey(ev)) {
  case TextInput::Result::Submit: use(input_.text()); break;
  case TextInput::Result::Cancel: close(); break;
  default: break;
}
input_.render(gfx, theme::kPadX, 40, gfx.width() - 2 * theme::kPadX);
```

**ConfirmDialog** — two-option modal (`reset(msg, first, second)`):
```cpp
confirm_.reset("Delete?", "Upload", "Delete");
switch (confirm_.handleKey(ev)) {
  case ConfirmDialog::Result::First:  upload(); break;
  case ConfirmDialog::Result::Second: del();    break;
  case ConfirmDialog::Result::Cancel: break;
  default: break;
}
confirm_.render(gfx);  // draws centered over your content
```

**Toast** — non-interactive overlay: `dialog::paintToast(gfx, "Saving…", true)`.

**TextView** — scrollable wrapped text (`;`/`.` scroll):
```cpp
view_.setText(longString);
if (view_.handleKey(ev)) requestRedraw();
view_.render(gfx, 0, theme::kStatusBarH, gfx.width(), gfx.height());
```

**ProgressBar** — `bar_.setValue(pct);` then
`bar_.render(gfx, x, y, w, h, /*showPercent=*/true);`.

**Label** — one line of text:
```cpp
label::draw(gfx, "Hold SPACE to record", theme::kPadX, top, theme::kMuted);
```

Colors and metrics live in `namespace theme` (`kFg`, `kMuted`, `kAccent`,
`kDanger`, `kOk`, `kPadX`, `kRowH`, `kStatusBarH`, `font()`).

## Power notes

CardOS dims the screen after 60 s idle and deep-sleeps after 5 min,
waking on the G0 button. Any input resets the timer. If your app does
something the user is watching without pressing keys (a long download, a
slideshow), keep the device awake — power management exposes a
`keepAwake` hook the main loop already feeds with WiFi activity; follow
that pattern for your own busy state.

**Deep sleep is a full power-down**: on wake the firmware boots fresh and
the launcher starts at the top — your app does **not** resume where it
left off. Persist anything you need to survive sleep to `cardos::fs`.
