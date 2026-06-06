# CardOS SDK Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the CardOS SDK: key press/long-press/release input, mic recording (streaming + WAV), unified `/flash`+`/sd` filesystem, HTTP(S) GET/POST/upload with progress, TextView/ProgressBar/Label widgets, umbrella header, docs, and a Recorder example app exercising everything.

**Architecture:** Incremental evolution per the spec — input upgrade happens in `core/` (pure `KeyTracker` + InputRouter rewrite); new capabilities live in `src/sdk/` under `namespace cardos`; new widgets in `src/ui/`. Hardware-independent logic (key tracking, WAV format, path routing, multipart encoding, text wrapping) is pure and native-tested; device code is build-verified then exercised on hardware by the Recorder app.

**Tech Stack:** C++17, Arduino/espressif32, M5Cardputer (M5Unified Mic_Class), LittleFS + SD (ESP-IDF VFS), HTTPClient + WiFiClientSecure, Unity native tests.

**Spec:** `docs/superpowers/specs/2026-06-06-cardos-sdk-design.md`
**Branch:** create `sdk` off `main` before Task 1.

---

## File Map

| Path | Responsibility | Native-tested |
|---|---|---|
| `src/core/KeyEvent.h` | + `KeyAction` enum, `action` field | ✅ (existing tests still pass) |
| `src/core/KeyTracker.h` | NEW pure per-key state machine → Press/LongPress/Release | ✅ |
| `src/core/InputRouter.{h,cpp}` | Rewritten: full matrix snapshot → KeyTracker → events | — |
| `src/core/AppManager.cpp` | Esc-pop guarded to `Press` only | — |
| `src/ui/MenuList.cpp`, `src/ui/TextInput.cpp`, `src/ui/Dialog.cpp` | + Press-only guards | — |
| `src/apps/LauncherApp.cpp`, `src/apps/WiFiApp.cpp`, `src/apps/HttpDemoApp.cpp`, `src/apps/SysInfoApp.*` | + Press-only guards (where handleKey exists) | — |
| `src/sdk/FsPath.h` | NEW pure: mount-prefix routing | ✅ |
| `src/sdk/Fs.{h,cpp}` | NEW `cardos::fs`: LittleFS@/flash + SD@/sd + helpers | — |
| `src/sdk/WavWriter.{h,cpp}` | NEW WAV file writer (cstdio only — native-safe) | ✅ |
| `src/sdk/Audio.{h,cpp}` | NEW `cardos::audio` over M5Unified Mic | — |
| `src/sdk/Multipart.h` | NEW pure multipart/form-data prefix/suffix builders | ✅ |
| `src/sdk/Http.{h,cpp}` | NEW `cardos::http` GET/POST/JSON/upload, HTTP+HTTPS | — |
| `src/ui/textwrap.h` | NEW pure UTF-8 word-wrap with injected measurer | ✅ |
| `src/ui/TextView.{h,cpp}` | NEW scrollable multi-line text widget | — |
| `src/ui/ProgressBar.{h,cpp}` | NEW progress bar widget | — |
| `src/ui/Label.h` | NEW one-line text helper (header-only) | — |
| `src/sdk/CardOS.h` | NEW umbrella header | — |
| `src/apps/RecorderApp.{h,cpp}` | NEW end-to-end example app | — |
| `docs/sdk.md` | NEW SDK reference + tutorial | — |
| `platformio.ini` | littlefs filesystem; native filter + WavWriter.cpp | — |
| `src/main.cpp` | fs::begin(), audio::tick(), Recorder registration | — |

Pure headers must not include Arduino/M5 headers. `pio` is at `/opt/homebrew/bin/pio`. Work from `/Users/dongxu/cardos`. Commit per task; do NOT push until the final task.

---

### Task 0: Branch

- [ ] **Step 1:** `git checkout main && git pull && git checkout -b sdk`

---

### Task 1: KeyAction + KeyTracker (pure, TDD)

**Files:**
- Modify: `src/core/KeyEvent.h`
- Create: `src/core/KeyTracker.h`
- Modify: `test/test_native/test_main.cpp`

- [ ] **Step 1: Add `KeyAction` to `src/core/KeyEvent.h`**

Insert after the `KeyCode` enum:

```cpp
// How the key transitioned this frame.
enum class KeyAction : uint8_t { Press, LongPress, Release };
```

And add the field to `KeyEvent` (after `bool fn = false;`):

```cpp
  KeyAction action = KeyAction::Press;
```

- [ ] **Step 2: Add failing KeyTracker tests to `test/test_native/test_main.cpp`**

Add include:

```cpp
#include "../../src/core/KeyTracker.h"
```

Add tests before `main`:

```cpp
static std::vector<KeyTracker::Out> upd(KeyTracker& t,
                                        std::initializer_list<uint16_t> keys,
                                        uint32_t now) {
  std::vector<uint16_t> v(keys);
  return t.update(v.data(), v.size(), now);
}

void test_tracker_press_and_release() {
  KeyTracker t;
  auto e1 = upd(t, {'a'}, 0);
  TEST_ASSERT_EQUAL(1, (int)e1.size());
  TEST_ASSERT_EQUAL('a', e1[0].id);
  TEST_ASSERT_EQUAL((int)KeyAction::Press, (int)e1[0].action);
  auto e2 = upd(t, {'a'}, 100);   // still held, below threshold
  TEST_ASSERT_EQUAL(0, (int)e2.size());
  auto e3 = upd(t, {}, 200);      // released
  TEST_ASSERT_EQUAL(1, (int)e3.size());
  TEST_ASSERT_EQUAL((int)KeyAction::Release, (int)e3[0].action);
}

void test_tracker_long_press_fires_once() {
  KeyTracker t;
  upd(t, {'a'}, 0);
  auto e1 = upd(t, {'a'}, 599);
  TEST_ASSERT_EQUAL(0, (int)e1.size());
  auto e2 = upd(t, {'a'}, 600);   // threshold
  TEST_ASSERT_EQUAL(1, (int)e2.size());
  TEST_ASSERT_EQUAL((int)KeyAction::LongPress, (int)e2[0].action);
  auto e3 = upd(t, {'a'}, 1200);  // no repeat
  TEST_ASSERT_EQUAL(0, (int)e3.size());
  auto e4 = upd(t, {}, 1300);
  TEST_ASSERT_EQUAL((int)KeyAction::Release, (int)e4[0].action);
}

void test_tracker_multiple_keys() {
  KeyTracker t;
  auto e1 = upd(t, {'a', 'b'}, 0);
  TEST_ASSERT_EQUAL(2, (int)e1.size());
  auto e2 = upd(t, {'b'}, 100);   // 'a' released, 'b' held
  TEST_ASSERT_EQUAL(1, (int)e2.size());
  TEST_ASSERT_EQUAL('a', e2[0].id);
  TEST_ASSERT_EQUAL((int)KeyAction::Release, (int)e2[0].action);
}

void test_tracker_special_ids() {
  KeyTracker t;
  auto e1 = upd(t, {KeyTracker::kEnterId}, 0);
  TEST_ASSERT_EQUAL((int)KeyTracker::kEnterId, (int)e1[0].id);
  TEST_ASSERT_EQUAL((int)KeyAction::Press, (int)e1[0].action);
}

void test_tracker_overflow_ignored() {
  KeyTracker t;
  // 9 keys; capacity is 8 — the 9th must be silently ignored.
  auto e1 = upd(t, {'a','b','c','d','e','f','g','h','i'}, 0);
  TEST_ASSERT_EQUAL(8, (int)e1.size());
}
```

Register in `main`:

```cpp
  RUN_TEST(test_tracker_press_and_release);
  RUN_TEST(test_tracker_long_press_fires_once);
  RUN_TEST(test_tracker_multiple_keys);
  RUN_TEST(test_tracker_special_ids);
  RUN_TEST(test_tracker_overflow_ignored);
```

- [ ] **Step 3: Run tests — expect compile failure** (`KeyTracker.h: No such file`)

Run: `pio test -e native`

- [ ] **Step 4: Write `src/core/KeyTracker.h`**

```cpp
#pragma once
#include <cstdint>
#include <vector>

#include "KeyEvent.h"

// Per-key hold tracker. Fed the set of currently-held key ids each frame,
// emits Press / LongPress (once per hold) / Release transitions.
// Pure header: time is a parameter; no Arduino includes.
// Ids: printable keys use their char value; special keys use the
// constants below (outside the char range).
class KeyTracker {
 public:
  static constexpr uint32_t kLongPressMs = 600;
  static constexpr size_t kMaxKeys = 8;  // extra concurrent keys are ignored
  static constexpr uint16_t kEnterId = 0x100;
  static constexpr uint16_t kBackspaceId = 0x101;
  static constexpr uint16_t kTabId = 0x102;

  struct Out {
    uint16_t id;
    KeyAction action;
  };

  std::vector<Out> update(const uint16_t* held, size_t count, uint32_t now) {
    std::vector<Out> out;
    // Releases: slots whose id is no longer held.
    for (auto& s : slots_) {
      if (!s.used) continue;
      bool still = false;
      for (size_t i = 0; i < count; i++)
        if (held[i] == s.id) { still = true; break; }
      if (!still) {
        out.push_back({s.id, KeyAction::Release});
        s.used = false;
      }
    }
    // Presses and long-presses.
    for (size_t i = 0; i < count; i++) {
      Slot* slot = find(held[i]);
      if (!slot) {
        if (Slot* free = alloc()) {
          *free = {held[i], now, false, true};
          out.push_back({held[i], KeyAction::Press});
        }
        continue;  // full: ignore overflow keys
      }
      if (!slot->longFired && now - slot->since >= kLongPressMs) {
        slot->longFired = true;
        out.push_back({slot->id, KeyAction::LongPress});
      }
    }
    return out;
  }

 private:
  struct Slot {
    uint16_t id = 0;
    uint32_t since = 0;
    bool longFired = false;
    bool used = false;
  };
  Slot* find(uint16_t id) {
    for (auto& s : slots_)
      if (s.used && s.id == id) return &s;
    return nullptr;
  }
  Slot* alloc() {
    for (auto& s : slots_)
      if (!s.used) return &s;
    return nullptr;
  }
  Slot slots_[kMaxKeys];
};
```

- [ ] **Step 5: Run tests — expect 30/30 pass** (25 existing + 5 new)

Run: `pio test -e native`

- [ ] **Step 6: Device build still green:** `pio run -e m5stack-cardputer` → SUCCESS

- [ ] **Step 7: Commit**

```bash
git add src/core/KeyEvent.h src/core/KeyTracker.h test/test_native/test_main.cpp
git commit -m "feat(sdk): KeyAction and pure KeyTracker with native tests"
```

---

### Task 2: InputRouter Rewrite + Press Guards

**Files:**
- Modify: `src/core/InputRouter.h`, `src/core/InputRouter.cpp`
- Modify: `src/core/AppManager.cpp` (Esc-pop guard)
- Modify: `src/ui/MenuList.cpp`, `src/ui/TextInput.cpp`, `src/ui/Dialog.cpp`
- Modify: `src/apps/LauncherApp.cpp`, `src/apps/WiFiApp.cpp`, `src/apps/HttpDemoApp.cpp`

- [ ] **Step 1: Rewrite `src/core/InputRouter.h`**

```cpp
#pragma once
#include <vector>

#include "KeyEvent.h"
#include "KeyTracker.h"

// Polls the Cardputer keyboard matrix and G0 button every frame, feeding
// the full held-key snapshot into KeyTracker to derive Press / LongPress /
// Release events.
class InputRouter {
 public:
  // Call once per loop after M5Cardputer.update().
  std::vector<KeyEvent> poll();

 private:
  KeyTracker tracker_;
};
```

- [ ] **Step 2: Rewrite `src/core/InputRouter.cpp`**

```cpp
#include "InputRouter.h"

#include <M5Cardputer.h>

namespace {
KeyEvent escEvent(KeyAction action) {
  KeyEvent ev;
  ev.code = KeyCode::Esc;
  ev.action = action;
  return ev;
}
}  // namespace

std::vector<KeyEvent> InputRouter::poll() {
  std::vector<KeyEvent> out;

  // G0/BtnA: M5Unified tracks press/hold/release for us.
  auto& btn = M5Cardputer.BtnA;
  if (btn.wasPressed()) out.push_back(escEvent(KeyAction::Press));
  if (btn.wasHold()) out.push_back(escEvent(KeyAction::LongPress));
  if (btn.wasReleased()) out.push_back(escEvent(KeyAction::Release));

  // Keyboard matrix: full held-key snapshot every frame.
  uint16_t held[KeyTracker::kMaxKeys];
  size_t n = 0;
  bool fn = false;
  if (M5Cardputer.Keyboard.isPressed()) {
    auto st = M5Cardputer.Keyboard.keysState();
    fn = st.fn;
    if (st.enter && n < KeyTracker::kMaxKeys) held[n++] = KeyTracker::kEnterId;
    if (st.del && n < KeyTracker::kMaxKeys) held[n++] = KeyTracker::kBackspaceId;
    if (st.tab && n < KeyTracker::kMaxKeys) held[n++] = KeyTracker::kTabId;
    for (char c : st.word) {
      if (n >= KeyTracker::kMaxKeys) break;
      held[n++] = (uint16_t)(uint8_t)c;
    }
  }

  for (const auto& o : tracker_.update(held, n, millis())) {
    KeyEvent ev;
    if (o.id == KeyTracker::kEnterId) ev = mapKey(0, fn, true, false, false);
    else if (o.id == KeyTracker::kBackspaceId) ev = mapKey(0, fn, false, true, false);
    else if (o.id == KeyTracker::kTabId) ev = mapKey(0, fn, false, false, true);
    else ev = mapKey((char)o.id, fn, false, false, false);
    ev.action = o.action;
    out.push_back(ev);
  }
  return out;
}
```

- [ ] **Step 3: Guard Esc-pop in `src/core/AppManager.cpp`**

Replace the `dispatch` body line:

```cpp
  if (!app->handleKey(ev) && ev.code == KeyCode::Esc) pop();
```

with:

```cpp
  if (!app->handleKey(ev) && ev.code == KeyCode::Esc &&
      ev.action == KeyAction::Press)
    pop();
```

- [ ] **Step 4: Add Press-only guards (one line at the top of each handleKey)**

The guard line (identical everywhere):

```cpp
  if (ev.action != KeyAction::Press) return false;
```

Add as the FIRST line of:
- `MenuList::handleKey` (src/ui/MenuList.cpp)
- `TextInput::handleKey` (src/ui/TextInput.cpp) — return `Result::None` instead of `false`:
  `if (ev.action != KeyAction::Press) return Result::None;`
- `ConfirmDialog::handleKey` (src/ui/Dialog.cpp) — return `Result::None` likewise
- `LauncherApp::handleKey` (src/apps/LauncherApp.cpp) — return `true` (the root app swallows everything non-Press): `if (ev.action != KeyAction::Press) return true;`
- `WiFiApp::handleKey` (src/apps/WiFiApp.cpp) — return `true` (its modal logic must not see phantom events): `if (ev.action != KeyAction::Press) return true;`
- `HttpDemoApp::handleKey` (src/apps/HttpDemoApp.cpp) — return `true` similarly

- [ ] **Step 5: Build + tests**

`pio run -e m5stack-cardputer` → SUCCESS; `pio test -e native` → 30/30.

- [ ] **Step 6: Flash and spot-check (device required)**

`pio run -e m5stack-cardputer -t upload`. Verify: launcher navigation unchanged (`;`/`.`/Enter/Esc work; nothing double-triggers on key release). If the device is disconnected, mark this step deferred and continue.

- [ ] **Step 7: Commit**

```bash
git add src/core src/ui src/apps
git commit -m "feat(sdk): press/long-press/release input pipeline with press guards"
```

---

### Task 3: Filesystem — FsPath (pure, TDD) + cardos::fs

**Files:**
- Modify: `platformio.ini`
- Create: `src/sdk/FsPath.h`
- Create: `src/sdk/Fs.h`, `src/sdk/Fs.cpp`
- Modify: `src/main.cpp`
- Modify: `test/test_native/test_main.cpp`

- [ ] **Step 1: `platformio.ini` — littlefs for the device env**

Add to `[env:m5stack-cardputer]`:

```ini
board_build.filesystem = littlefs
```

- [ ] **Step 2: Failing tests for FsPath**

Add include to `test/test_native/test_main.cpp`:

```cpp
#include "../../src/sdk/FsPath.h"
```

Tests:

```cpp
void test_fspath_flash() {
  std::string mount, rel;
  TEST_ASSERT_TRUE(cardos::fs::splitPath("/flash/rec/a.wav", mount, rel));
  TEST_ASSERT_EQUAL_STRING("/flash", mount.c_str());
  TEST_ASSERT_EQUAL_STRING("/rec/a.wav", rel.c_str());
}

void test_fspath_sd_root() {
  std::string mount, rel;
  TEST_ASSERT_TRUE(cardos::fs::splitPath("/sd", mount, rel));
  TEST_ASSERT_EQUAL_STRING("/sd", mount.c_str());
  TEST_ASSERT_EQUAL_STRING("/", rel.c_str());
}

void test_fspath_invalid() {
  std::string mount, rel;
  TEST_ASSERT_FALSE(cardos::fs::splitPath("/nvs/x", mount, rel));
  TEST_ASSERT_FALSE(cardos::fs::splitPath("flash/x", mount, rel));
  TEST_ASSERT_FALSE(cardos::fs::splitPath("/flashy/x", mount, rel));
}
```

Register:

```cpp
  RUN_TEST(test_fspath_flash);
  RUN_TEST(test_fspath_sd_root);
  RUN_TEST(test_fspath_invalid);
```

- [ ] **Step 3: Run — expect compile failure.** `pio test -e native`

- [ ] **Step 4: Write `src/sdk/FsPath.h`**

```cpp
#pragma once
#include <string>

// Pure mount-prefix routing for the CardOS VFS. No Arduino includes.
namespace cardos::fs {

// Splits "/flash/rec/a.wav" into mount="/flash", rel="/rec/a.wav".
// Only "/flash" and "/sd" are valid mounts. rel is "/" for the bare mount.
inline bool splitPath(const std::string& path, std::string& mount,
                      std::string& rel) {
  for (const char* m : {"/flash", "/sd"}) {
    size_t len = std::string(m).size();
    if (path.compare(0, len, m) != 0) continue;
    if (path.size() == len) { mount = m; rel = "/"; return true; }
    if (path[len] == '/') { mount = m; rel = path.substr(len); return true; }
  }
  return false;
}

}  // namespace cardos::fs
```

- [ ] **Step 5: Run — 33/33 pass.** `pio test -e native`

- [ ] **Step 6: Write `src/sdk/Fs.h`**

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "FsPath.h"

// Unified filesystem API over internal flash (LittleFS at /flash) and
// microSD (at /sd). After begin(), POSIX fopen()/fread() also work on
// both mounts via the ESP-IDF VFS (WavWriter relies on this).
namespace cardos::fs {

bool begin();         // mounts /flash (formats on first boot); SD is lazy
bool sdAvailable();   // probes / mounts the SD card on demand

std::string readFile(const std::string& path);  // "" on error
bool writeFile(const std::string& path, const std::string& data,
               bool append = false);
bool exists(const std::string& path);
bool remove(const std::string& path);
bool mkdir(const std::string& path);

struct Entry {
  std::string name;  // bare name, no directory prefix
  size_t size = 0;
  bool isDir = false;
};
std::vector<Entry> list(const std::string& dir);

uint64_t freeBytes(const std::string& mount);  // "/flash" or "/sd"

}  // namespace cardos::fs
```

- [ ] **Step 7: Write `src/sdk/Fs.cpp`**

```cpp
#include "Fs.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <SD.h>
#include <SPI.h>

namespace cardos::fs {
namespace {
// Cardputer microSD over SPI. VERIFY ON HARDWARE (spec section 5 risk).
constexpr int kSdSck = 40, kSdMiso = 39, kSdMosi = 14, kSdCs = 12;
bool sdMounted = false;

// Returns the Arduino FS backend for a path, and the path relative to it.
FS* backendFor(const std::string& path, std::string& rel) {
  std::string mount;
  if (!splitPath(path, mount, rel)) return nullptr;
  if (mount == "/flash") return &LittleFS;
  return sdAvailable() ? (FS*)&SD : nullptr;
}
}  // namespace

bool begin() {
  // base_path "/flash" registers the POSIX VFS mount point.
  bool ok = LittleFS.begin(/*formatOnFail=*/true, "/flash");
  Serial.printf("[fs] /flash %s\n", ok ? "mounted" : "MOUNT FAILED");
  return ok;
}

bool sdAvailable() {
  if (sdMounted && SD.cardType() != CARD_NONE) return true;
  SPI.begin(kSdSck, kSdMiso, kSdMosi, kSdCs);
  sdMounted = SD.begin(kSdCs, SPI, 25000000, "/sd");
  if (sdMounted) Serial.println("[fs] /sd mounted");
  return sdMounted;
}

std::string readFile(const std::string& path) {
  std::string rel;
  FS* be = backendFor(path, rel);
  if (!be) return "";
  File f = be->open(rel.c_str(), "r");
  if (!f || f.isDirectory()) return "";
  std::string out;
  out.reserve(f.size());
  uint8_t buf[256];
  while (f.available()) {
    size_t n = f.read(buf, sizeof(buf));
    out.append((const char*)buf, n);
  }
  f.close();
  return out;
}

bool writeFile(const std::string& path, const std::string& data, bool append) {
  std::string rel;
  FS* be = backendFor(path, rel);
  if (!be) return false;
  File f = be->open(rel.c_str(), append ? "a" : "w");
  if (!f) return false;
  size_t n = f.write((const uint8_t*)data.data(), data.size());
  f.close();
  return n == data.size();
}

bool exists(const std::string& path) {
  std::string rel;
  FS* be = backendFor(path, rel);
  return be && be->exists(rel.c_str());
}

bool remove(const std::string& path) {
  std::string rel;
  FS* be = backendFor(path, rel);
  return be && be->remove(rel.c_str());
}

bool mkdir(const std::string& path) {
  std::string rel;
  FS* be = backendFor(path, rel);
  if (!be) return false;
  return be->exists(rel.c_str()) || be->mkdir(rel.c_str());
}

std::vector<Entry> list(const std::string& dir) {
  std::vector<Entry> out;
  std::string rel;
  FS* be = backendFor(dir, rel);
  if (!be) return out;
  File d = be->open(rel.c_str());
  if (!d || !d.isDirectory()) return out;
  for (File f = d.openNextFile(); f; f = d.openNextFile()) {
    Entry e;
    const char* n = f.name();             // may include a path prefix
    const char* slash = strrchr(n, '/');
    e.name = slash ? slash + 1 : n;
    e.size = f.size();
    e.isDir = f.isDirectory();
    out.push_back(std::move(e));
    f.close();
  }
  d.close();
  return out;
}

uint64_t freeBytes(const std::string& mount) {
  if (mount == "/flash") return LittleFS.totalBytes() - LittleFS.usedBytes();
  if (mount == "/sd" && sdAvailable()) return SD.totalBytes() - SD.usedBytes();
  return 0;
}

}  // namespace cardos::fs
```

- [ ] **Step 8: Wire into `src/main.cpp`**

Add include `#include "sdk/Fs.h"`. In `setup()`, right after `power.begin();` block add:

```cpp
  cardos::fs::begin();
```

- [ ] **Step 9: Build + tests.** `pio run -e m5stack-cardputer` → SUCCESS; `pio test -e native` → 33/33.

- [ ] **Step 10: Commit**

```bash
git add platformio.ini src/sdk/FsPath.h src/sdk/Fs.h src/sdk/Fs.cpp src/main.cpp test/test_native/test_main.cpp
git commit -m "feat(sdk): unified /flash + /sd filesystem"
```

---

### Task 4: WavWriter (pure, TDD)

**Files:**
- Create: `src/sdk/WavWriter.h`, `src/sdk/WavWriter.cpp`
- Modify: `platformio.ini` (add WavWriter.cpp to the native filter)
- Modify: `test/test_native/test_main.cpp`

- [ ] **Step 1: Add to `[env:native]` in `platformio.ini`**

```ini
build_src_filter = +<services/WiFiStore.cpp> +<sdk/WavWriter.cpp>
```

(replacing the existing single-entry `build_src_filter` line)

- [ ] **Step 2: Failing tests**

Include:

```cpp
#include "../../src/sdk/WavWriter.h"
```

Tests:

```cpp
void test_wav_header_fields() {
  uint8_t h[44];
  cardos::audio::writeWavHeader(h, 16000, 1, 3200);  // 0.1s of 16k mono
  TEST_ASSERT_EQUAL_MEMORY("RIFF", h, 4);
  TEST_ASSERT_EQUAL_MEMORY("WAVE", h + 8, 4);
  TEST_ASSERT_EQUAL_MEMORY("data", h + 36, 4);
  uint32_t riffSize;  memcpy(&riffSize, h + 4, 4);
  TEST_ASSERT_EQUAL(36 + 3200, (int)riffSize);
  uint32_t rate;      memcpy(&rate, h + 24, 4);
  TEST_ASSERT_EQUAL(16000, (int)rate);
  uint32_t byteRate;  memcpy(&byteRate, h + 28, 4);
  TEST_ASSERT_EQUAL(32000, (int)byteRate);   // 16k * 1ch * 2B
  uint16_t bits;      memcpy(&bits, h + 34, 2);
  TEST_ASSERT_EQUAL(16, (int)bits);
  uint32_t dataSize;  memcpy(&dataSize, h + 40, 4);
  TEST_ASSERT_EQUAL(3200, (int)dataSize);
}

void test_wav_writer_roundtrip() {
  const char* path = "/tmp/cardos_test.wav";
  cardos::audio::WavWriter w;
  TEST_ASSERT_TRUE(w.open(path, 16000));
  int16_t samples[256];
  for (int i = 0; i < 256; i++) samples[i] = (int16_t)(i * 100);
  w.write(samples, 256);
  w.write(samples, 256);
  TEST_ASSERT_EQUAL(1024, (int)w.dataBytes());  // 512 samples * 2B
  w.close();
  FILE* f = fopen(path, "rb");
  TEST_ASSERT_NOT_NULL(f);
  fseek(f, 0, SEEK_END);
  TEST_ASSERT_EQUAL(44 + 1024, (int)ftell(f));
  uint8_t h[44];
  fseek(f, 0, SEEK_SET);
  fread(h, 1, 44, f);
  uint32_t dataSize;  memcpy(&dataSize, h + 40, 4);
  TEST_ASSERT_EQUAL(1024, (int)dataSize);       // header patched on close
  fclose(f);
  ::remove(path);
}
```

Register both. Also add `#include <cstring>` and `#include <cstdio>` at the top of the test file if not present.

- [ ] **Step 3: Run — expect compile failure.** `pio test -e native`

- [ ] **Step 4: Write `src/sdk/WavWriter.h`**

```cpp
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
```

- [ ] **Step 5: Write `src/sdk/WavWriter.cpp`**

```cpp
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
```

- [ ] **Step 6: Run — 35/35 pass.** `pio test -e native`. Device build: `pio run -e m5stack-cardputer` → SUCCESS.

- [ ] **Step 7: Commit**

```bash
git add src/sdk/WavWriter.h src/sdk/WavWriter.cpp platformio.ini test/test_native/test_main.cpp
git commit -m "feat(sdk): WAV writer with native tests"
```

---

### Task 5: Audio — cardos::audio

**Files:**
- Create: `src/sdk/Audio.h`, `src/sdk/Audio.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Write `src/sdk/Audio.h`**

```cpp
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
```

- [ ] **Step 2: Write `src/sdk/Audio.cpp`**

NOTE FOR IMPLEMENTER: the exact `Mic_Class` record semantics must be
checked against the vendored header
(`.pio/libdeps/m5stack-cardputer/M5Unified/src/utility/Mic_Class.hpp`) and
the official M5 mic example. The contract below assumes the documented
M5Unified pattern: `record(buf, len, rate)` enqueues a capture into `buf`
and returns immediately; the data for a buffer is complete when
`isRecording()` reflects the queue draining — the standard ping-pong
pattern processes buffer A while B records. Keep this file's PUBLIC
behavior identical even if internals need adapting; report deviations.

```cpp
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
```

- [ ] **Step 3: Wire into `src/main.cpp`**

Add `#include "sdk/Audio.h"`. In `loop()`, right after `wifiService.tick(now);` add:

```cpp
  cardos::audio::tick();
```

- [ ] **Step 4: Build + tests.** `pio run -e m5stack-cardputer` → SUCCESS; `pio test -e native` → 35/35.

- [ ] **Step 5: Commit**

```bash
git add src/sdk/Audio.h src/sdk/Audio.cpp src/main.cpp
git commit -m "feat(sdk): microphone recording with streaming chunks and WAV output"
```

---

### Task 6: HTTP — Multipart (pure, TDD) + cardos::http + HttpDemo refactor

**Files:**
- Create: `src/sdk/Multipart.h`
- Create: `src/sdk/Http.h`, `src/sdk/Http.cpp`
- Modify: `src/apps/HttpDemoApp.cpp` (refactor onto cardos::http)
- Modify: `test/test_native/test_main.cpp`

- [ ] **Step 1: Failing tests for Multipart**

Include:

```cpp
#include "../../src/sdk/Multipart.h"
```

Tests:

```cpp
void test_multipart_prefix() {
  std::string p = cardos::http::multipartPrefix("BNDRY", "file", "a.wav");
  TEST_ASSERT_EQUAL_STRING(
      "--BNDRY\r\n"
      "Content-Disposition: form-data; name=\"file\"; filename=\"a.wav\"\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n",
      p.c_str());
}

void test_multipart_suffix() {
  std::string s = cardos::http::multipartSuffix("BNDRY");
  TEST_ASSERT_EQUAL_STRING("\r\n--BNDRY--\r\n", s.c_str());
}
```

Register both.

- [ ] **Step 2: Run — expect compile failure.** `pio test -e native`

- [ ] **Step 3: Write `src/sdk/Multipart.h`**

```cpp
#pragma once
#include <string>

// Pure multipart/form-data encoding helpers. No Arduino includes.
namespace cardos::http {

inline std::string multipartPrefix(
    const std::string& boundary, const std::string& fieldName,
    const std::string& filename,
    const std::string& contentType = "application/octet-stream") {
  return "--" + boundary + "\r\n" +
         "Content-Disposition: form-data; name=\"" + fieldName +
         "\"; filename=\"" + filename + "\"\r\n" +
         "Content-Type: " + contentType + "\r\n\r\n";
}

inline std::string multipartSuffix(const std::string& boundary) {
  return "\r\n--" + boundary + "--\r\n";
}

}  // namespace cardos::http
```

- [ ] **Step 4: Run — 37/37 pass.** `pio test -e native`

- [ ] **Step 5: Write `src/sdk/Http.h`**

```cpp
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Blocking HTTP(S) client API. https:// URLs use TLS without certificate
// verification (pragmatic for a hobby device). Calls block the caller —
// render a "Requesting..." frame before invoking (see HttpDemoApp).
namespace cardos::http {

using Headers = std::vector<std::pair<std::string, std::string>>;

struct Response {
  int status = -1;        // -1 = transport failure (see error)
  std::string body;
  std::string error;
  bool ok() const { return status >= 200 && status < 300; }
};

using Progress = std::function<void(size_t sent, size_t total)>;

Response get(const std::string& url, const Headers& h = {});
Response post(const std::string& url, const std::string& body,
              const std::string& contentType, const Headers& h = {});
Response postJson(const std::string& url, const std::string& json,
                  const Headers& h = {});
// Streams filePath (a /flash or /sd VFS path) as multipart/form-data.
Response uploadFile(const std::string& url, const std::string& filePath,
                    const std::string& fieldName = "file",
                    const Headers& h = {}, Progress onProgress = nullptr);
void setTimeout(uint32_t ms);  // default 10000

}  // namespace cardos::http
```

- [ ] **Step 6: Write `src/sdk/Http.cpp`**

```cpp
#include "Http.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "Multipart.h"

namespace cardos::http {
namespace {
uint32_t timeoutMs_ = 10000;

bool isHttps(const std::string& url) { return url.rfind("https://", 0) == 0; }

// One client per request; secure clients skip cert verification.
struct ClientBox {
  WiFiClient plain;
  WiFiClientSecure secure;
  WiFiClient& pick(const std::string& url) {
    if (!isHttps(url)) return plain;
    secure.setInsecure();
    return secure;
  }
};

void applyHeaders(HTTPClient& http, const Headers& h) {
  for (const auto& kv : h) http.addHeader(kv.first.c_str(), kv.second.c_str());
}

Response finish(HTTPClient& http, int code) {
  Response r;
  if (code > 0) {
    r.status = code;
    r.body = std::string(http.getString().c_str());
  } else {
    r.error = std::string(HTTPClient::errorToString(code).c_str());
  }
  http.end();
  Serial.printf("[http] -> %d %s\n", r.status, r.error.c_str());
  return r;
}

// Streams: multipart prefix + FILE* + multipart suffix, with progress.
class MultipartStream : public Stream {
 public:
  MultipartStream(std::string prefix, FILE* f, size_t fileSize,
                  std::string suffix, Progress progress)
      : prefix_(std::move(prefix)), f_(f), fileSize_(fileSize),
        suffix_(std::move(suffix)), progress_(std::move(progress)) {
    total_ = prefix_.size() + fileSize_ + suffix_.size();
  }
  size_t totalSize() const { return total_; }

  int available() override {
    size_t remaining = total_ - pos_;
    return remaining > 0x7FFFFFFF ? 0x7FFFFFFF : (int)remaining;
  }
  int read() override {
    uint8_t b;
    return readBytes((char*)&b, 1) == 1 ? b : -1;
  }
  size_t readBytes(char* dst, size_t len) override {
    size_t n = 0;
    while (n < len && pos_ < total_) {
      if (pos_ < prefix_.size()) {                       // prefix region
        size_t take = std::min(len - n, prefix_.size() - pos_);
        memcpy(dst + n, prefix_.data() + pos_, take);
        n += take; pos_ += take;
      } else if (pos_ < prefix_.size() + fileSize_) {    // file region
        size_t take = fread(dst + n, 1, len - n, f_);
        if (take == 0) break;
        n += take; pos_ += take;
      } else {                                           // suffix region
        size_t off = pos_ - prefix_.size() - fileSize_;
        size_t take = std::min(len - n, suffix_.size() - off);
        memcpy(dst + n, suffix_.data() + off, take);
        n += take; pos_ += take;
      }
    }
    if (progress_ && pos_ - lastReport_ >= 4096) {
      lastReport_ = pos_;
      progress_(pos_, total_);
    }
    return n;
  }
  int peek() override { return -1; }
  size_t write(uint8_t) override { return 0; }  // read-only stream

 private:
  std::string prefix_;
  FILE* f_;
  size_t fileSize_;
  std::string suffix_;
  Progress progress_;
  size_t total_ = 0, pos_ = 0, lastReport_ = 0;
};
}  // namespace

void setTimeout(uint32_t ms) { timeoutMs_ = ms; }

Response get(const std::string& url, const Headers& h) {
  Serial.printf("[http] GET %s\n", url.c_str());
  ClientBox box;
  HTTPClient http;
  http.setConnectTimeout(timeoutMs_);
  http.setTimeout(timeoutMs_);
  Response r;
  if (!http.begin(box.pick(url), url.c_str())) {
    r.error = "begin() failed";
    return r;
  }
  applyHeaders(http, h);
  return finish(http, http.GET());
}

Response post(const std::string& url, const std::string& body,
              const std::string& contentType, const Headers& h) {
  Serial.printf("[http] POST %s (%u B)\n", url.c_str(), (unsigned)body.size());
  ClientBox box;
  HTTPClient http;
  http.setConnectTimeout(timeoutMs_);
  http.setTimeout(timeoutMs_);
  Response r;
  if (!http.begin(box.pick(url), url.c_str())) {
    r.error = "begin() failed";
    return r;
  }
  http.addHeader("Content-Type", contentType.c_str());
  applyHeaders(http, h);
  return finish(http, http.POST((uint8_t*)body.data(), body.size()));
}

Response postJson(const std::string& url, const std::string& json,
                  const Headers& h) {
  return post(url, json, "application/json", h);
}

Response uploadFile(const std::string& url, const std::string& filePath,
                    const std::string& fieldName, const Headers& h,
                    Progress onProgress) {
  Response r;
  FILE* f = fopen(filePath.c_str(), "rb");
  if (!f) {
    r.error = "cannot open " + filePath;
    return r;
  }
  fseek(f, 0, SEEK_END);
  size_t fileSize = ftell(f);
  fseek(f, 0, SEEK_SET);

  std::string boundary = "----cardos";
  char rnd[9];
  snprintf(rnd, sizeof(rnd), "%08x", (unsigned)esp_random());
  boundary += rnd;

  const char* slash = strrchr(filePath.c_str(), '/');
  std::string filename = slash ? slash + 1 : filePath;

  Serial.printf("[http] UPLOAD %s (%u B) -> %s\n", filePath.c_str(),
                (unsigned)fileSize, url.c_str());
  MultipartStream stream(multipartPrefix(boundary, fieldName, filename), f,
                         fileSize, multipartSuffix(boundary), onProgress);
  ClientBox box;
  HTTPClient http;
  http.setConnectTimeout(timeoutMs_);
  http.setTimeout(timeoutMs_);
  if (!http.begin(box.pick(url), url.c_str())) {
    fclose(f);
    r.error = "begin() failed";
    return r;
  }
  http.addHeader("Content-Type",
                 ("multipart/form-data; boundary=" + boundary).c_str());
  applyHeaders(http, h);
  r = finish(http, http.sendRequest("POST", &stream, stream.totalSize()));
  fclose(f);
  return r;
}

}  // namespace cardos::http
```

- [ ] **Step 7: Refactor `src/apps/HttpDemoApp.cpp` onto cardos::http**

Replace the includes `<HTTPClient.h>` and `<WiFi.h>` with:

```cpp
#include <WiFi.h>

#include "../sdk/Http.h"
```

Replace the whole `doRequest` body with:

```cpp
void HttpDemoApp::doRequest(const std::string& url) {
  cardos::http::Response r = cardos::http::get(url);
  if (r.status > 0) {
    status_ = "HTTP " + std::to_string(r.status) + "  " +
              std::to_string(r.body.size()) + " B";
    body_ = r.body.substr(0, 160);
  } else {
    status_ = "error: " + r.error;
    body_.clear();
  }
}
```

(The latency line moves out of scope — cardos::http logs to serial; keep the app simple. Update the `kEndpoints` list to add one HTTPS entry proving TLS works:)

```cpp
const Endpoint kEndpoints[] = {
    {"My public IP", "http://ifconfig.me/ip"},
    {"GET example.com", "http://example.com/"},
    {"HTTPS httpbin", "https://httpbin.org/get"},
};
```

- [ ] **Step 8: Build + tests.** `pio run -e m5stack-cardputer` → SUCCESS; `pio test -e native` → 37/37.

- [ ] **Step 9: Commit**

```bash
git add src/sdk/Multipart.h src/sdk/Http.h src/sdk/Http.cpp src/apps/HttpDemoApp.cpp test/test_native/test_main.cpp
git commit -m "feat(sdk): HTTP(S) client with multipart upload; HttpDemo on cardos::http"
```

---

### Task 7: Widgets — textwrap (pure, TDD) + TextView + ProgressBar + Label

**Files:**
- Create: `src/ui/textwrap.h`
- Create: `src/ui/TextView.h`, `src/ui/TextView.cpp`
- Create: `src/ui/ProgressBar.h`, `src/ui/ProgressBar.cpp`
- Create: `src/ui/Label.h`
- Modify: `test/test_native/test_main.cpp`

- [ ] **Step 1: Failing tests for textwrap**

Include:

```cpp
#include "../../src/ui/textwrap.h"
```

Tests (fake measurer: 6 px per byte — multi-byte glyphs are naturally wider):

```cpp
static int fakeMeasure(const std::string& s) { return (int)s.size() * 6; }

void test_wrap_simple() {
  auto lines = wrapText("hello world", 36, fakeMeasure);  // 6 chars/line
  TEST_ASSERT_EQUAL(2, (int)lines.size());
  TEST_ASSERT_EQUAL_STRING("hello ", lines[0].c_str());
  TEST_ASSERT_EQUAL_STRING("world", lines[1].c_str());
}

void test_wrap_honors_newlines() {
  auto lines = wrapText("ab\ncd", 600, fakeMeasure);
  TEST_ASSERT_EQUAL(2, (int)lines.size());
  TEST_ASSERT_EQUAL_STRING("ab", lines[0].c_str());
  TEST_ASSERT_EQUAL_STRING("cd", lines[1].c_str());
}

void test_wrap_utf8_not_split() {
  // "中" is 3 bytes; width 18px per glyph under fakeMeasure. Width 20
  // fits exactly one glyph per line — multibyte sequences must not be cut.
  auto lines = wrapText("中文", 20, fakeMeasure);
  TEST_ASSERT_EQUAL(2, (int)lines.size());
  TEST_ASSERT_EQUAL_STRING("中", lines[0].c_str());
  TEST_ASSERT_EQUAL_STRING("文", lines[1].c_str());
}

void test_wrap_empty() {
  auto lines = wrapText("", 100, fakeMeasure);
  TEST_ASSERT_EQUAL(0, (int)lines.size());
}
```

Register all four.

- [ ] **Step 2: Run — expect compile failure.** `pio test -e native`

- [ ] **Step 3: Write `src/ui/textwrap.h`**

```cpp
#pragma once
#include <functional>
#include <string>
#include <vector>

// Pure greedy word-wrap with an injected width-measure function so the
// algorithm is host-testable (the device passes gfx.textWidth).
// UTF-8 aware: multi-byte sequences are never split. Wraps at spaces when
// possible, otherwise at any glyph (CJK text wraps per glyph naturally).
using MeasureFn = std::function<int(const std::string&)>;

inline std::vector<std::string> wrapText(const std::string& text, int width,
                                         const MeasureFn& measure) {
  std::vector<std::string> lines;
  std::string line;
  size_t lastSpace = std::string::npos;  // byte index in `line`

  auto flush = [&]() {
    lines.push_back(line);
    line.clear();
    lastSpace = std::string::npos;
  };

  for (size_t i = 0; i < text.size();) {
    if (text[i] == '\n') {
      flush();
      i++;
      continue;
    }
    // Glyph = one UTF-8 sequence.
    size_t glyphLen = 1;
    uint8_t lead = (uint8_t)text[i];
    if (lead >= 0xF0) glyphLen = 4;
    else if (lead >= 0xE0) glyphLen = 3;
    else if (lead >= 0xC0) glyphLen = 2;
    std::string glyph = text.substr(i, glyphLen);

    if (measure(line + glyph) > width && !line.empty()) {
      if (glyph == " ") {  // wrap point itself: drop the space, new line
        flush();
        i += glyphLen;
        continue;
      }
      if (lastSpace != std::string::npos) {
        // Break at the last space; carry the partial word to the next line.
        std::string carry = line.substr(lastSpace + 1);
        line.resize(lastSpace + 1);
        flush();
        line = carry;
      } else {
        flush();
      }
    }
    if (glyph == " ") lastSpace = line.size();
    line += glyph;
    i += glyphLen;
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}
```

- [ ] **Step 4: Run — 41/41 pass.** `pio test -e native`

- [ ] **Step 5: Write `src/ui/TextView.h`**

```cpp
#pragma once
#include <M5GFX.h>

#include <string>
#include <vector>

#include "../core/KeyEvent.h"

// Scrollable multi-line text display. Wraps on first render (or width
// change); ;/. scroll by line.
class TextView {
 public:
  void setText(const std::string& text);
  bool handleKey(const KeyEvent& ev);  // consumes Up/Down (Press only)
  void render(M5Canvas& gfx, int x, int y, int w, int h);

 private:
  std::string text_;
  std::vector<std::string> lines_;
  int wrappedWidth_ = -1;  // width lines_ was computed for; -1 = dirty
  int scroll_ = 0;         // first visible line
};
```

- [ ] **Step 6: Write `src/ui/TextView.cpp`**

```cpp
#include "TextView.h"

#include "Theme.h"
#include "scroll.h"
#include "textwrap.h"

void TextView::setText(const std::string& text) {
  text_ = text;
  wrappedWidth_ = -1;
  scroll_ = 0;
}

bool TextView::handleKey(const KeyEvent& ev) {
  if (ev.action != KeyAction::Press) return false;
  if (ev.code == KeyCode::Up && scroll_ > 0) { scroll_--; return true; }
  if (ev.code == KeyCode::Down && scroll_ < (int)lines_.size() - 1) {
    scroll_++;
    return true;
  }
  return false;
}

void TextView::render(M5Canvas& gfx, int x, int y, int w, int h) {
  gfx.setFont(theme::font());
  int textW = w - 2 * theme::kPadX - 2;  // leave room for the scrollbar
  if (wrappedWidth_ != textW) {
    lines_ = wrapText(text_, textW,
                      [&](const std::string& s) {
                        return (int)gfx.textWidth(s.c_str());
                      });
    wrappedWidth_ = textW;
    if (scroll_ >= (int)lines_.size()) scroll_ = 0;
  }
  const int lineH = 14;
  int visible = h / lineH;
  gfx.setTextColor(theme::kFg);
  for (int row = 0; row < visible; row++) {
    int idx = scroll_ + row;
    if (idx >= (int)lines_.size()) break;
    gfx.setCursor(x + theme::kPadX, y + row * lineH);
    gfx.print(lines_[idx].c_str());
  }
  if ((int)lines_.size() > visible) {
    int count = (int)lines_.size();
    int barH = h * visible / count;
    int barY = y + (h - barH) * scroll_ / (count - 1);
    gfx.fillRect(x + w - 2, barY, 2, barH, theme::kMuted);
  }
}
```

- [ ] **Step 7: Write `src/ui/ProgressBar.h`**

```cpp
#pragma once
#include <M5GFX.h>

// Horizontal progress bar, 0..100.
class ProgressBar {
 public:
  void setValue(int pct);  // clamped to 0..100
  int value() const { return pct_; }
  void render(M5Canvas& gfx, int x, int y, int w, int h,
              bool showPercent = false) const;

 private:
  int pct_ = 0;
};
```

- [ ] **Step 8: Write `src/ui/ProgressBar.cpp`**

```cpp
#include "ProgressBar.h"

#include "Theme.h"

void ProgressBar::setValue(int pct) {
  pct_ = pct < 0 ? 0 : pct > 100 ? 100 : pct;
}

void ProgressBar::render(M5Canvas& gfx, int x, int y, int w, int h,
                         bool showPercent) const {
  gfx.drawRect(x, y, w, h, theme::kMuted);
  int fill = (w - 4) * pct_ / 100;
  if (fill > 0) gfx.fillRect(x + 2, y + 2, fill, h - 4, theme::kAccent);
  if (showPercent) {
    gfx.setFont(theme::font());
    gfx.setTextDatum(middle_center);
    gfx.setTextColor(theme::kFg);
    gfx.drawString((std::to_string(pct_) + "%").c_str(), x + w / 2,
                   y + h / 2);
    gfx.setTextDatum(top_left);
  }
}
```

- [ ] **Step 9: Write `src/ui/Label.h`** (header-only)

```cpp
#pragma once
#include <M5GFX.h>

#include <string>

#include "Theme.h"

// One-line text helper.
namespace label {

inline void draw(M5Canvas& gfx, const std::string& text, int x, int y,
                 uint16_t color = theme::kFg,
                 textdatum_t datum = top_left) {
  gfx.setFont(theme::font());
  gfx.setTextColor(color);
  gfx.setTextDatum(datum);
  gfx.drawString(text.c_str(), x, y);
  gfx.setTextDatum(top_left);
}

}  // namespace label
```

- [ ] **Step 10: Build + tests.** `pio run -e m5stack-cardputer` → SUCCESS; `pio test -e native` → 41/41.

- [ ] **Step 11: Commit**

```bash
git add src/ui/textwrap.h src/ui/TextView.h src/ui/TextView.cpp src/ui/ProgressBar.h src/ui/ProgressBar.cpp src/ui/Label.h test/test_native/test_main.cpp
git commit -m "feat(sdk): TextView, ProgressBar, Label widgets with wrap tests"
```

---

### Task 8: Umbrella Header + SDK Docs

**Files:**
- Create: `src/sdk/CardOS.h`
- Create: `docs/sdk.md`

- [ ] **Step 1: Write `src/sdk/CardOS.h`**

```cpp
#pragma once
// CardOS SDK umbrella header. A new app needs only this include.

// App framework
#include "../core/App.h"
#include "../core/AppManager.h"
#include "../core/KeyEvent.h"

// Widgets
#include "../ui/Dialog.h"
#include "../ui/Label.h"
#include "../ui/MenuList.h"
#include "../ui/ProgressBar.h"
#include "../ui/TextInput.h"
#include "../ui/TextView.h"
#include "../ui/Theme.h"

// Platform services
#include "Audio.h"
#include "Fs.h"
#include "Http.h"
```

- [ ] **Step 2: Write `docs/sdk.md`** — an English reference with these sections (write real content, not stubs; pull signatures from the headers built in Tasks 1–7):

1. **Overview** — what the SDK is, the one-include rule (`#include "../sdk/CardOS.h"`), the cooperative main-loop model (never block in handleKey/update except documented HTTP calls).
2. **Your first app** — complete minimal example (the canonical pattern):

````markdown
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
Register it in `src/main.cpp`: declare `HelloApp hello;` and add
`launcher.addEntry("Hello", &hello);` in `setup()`.
````

3. **Input** — KeyEvent fields, KeyCode table (from the MVP design), KeyAction semantics (Press default; LongPress at 600 ms once; Release), the press-guard idiom.
4. **Audio** — start/startToWav/stop/level/isRecording; chunk cadence; 16 kHz mono 16-bit; mic/speaker exclusivity.
5. **Filesystem** — mounts, lazy SD, API table, POSIX interop note.
6. **HTTP** — Response struct, get/post/postJson/uploadFile signatures, blocking caveat + the one-frame-delay pattern (show the HttpDemoApp pendingDelay_ snippet), progress callback.
7. **Widgets** — one short usage snippet per widget (MenuList, TextInput, ConfirmDialog/Toast, TextView, ProgressBar, Label).
8. **Power notes** — keepAwake, what deep sleep does to your app (fresh boot).

- [ ] **Step 3: Build sanity** (header compiles standalone): `pio run -e m5stack-cardputer` → SUCCESS

- [ ] **Step 4: Commit**

```bash
git add src/sdk/CardOS.h docs/sdk.md
git commit -m "feat(sdk): umbrella header and SDK documentation"
```

---

### Task 9: RecorderApp — End-to-End Example

**Files:**
- Create: `src/apps/RecorderApp.h`, `src/apps/RecorderApp.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Write `src/apps/RecorderApp.h`**

```cpp
#pragma once
#include <string>

#include "../sdk/CardOS.h"

// SDK showcase: push-to-talk voice memos with upload.
// Hold SPACE to record (LongPress starts, Release stops); recordings are
// listed below; Enter on one -> [Upload] [Delete].
class RecorderApp : public App {
 public:
  const char* title() const override { return "Recorder"; }
  void onEnter() override;
  bool handleKey(const KeyEvent& ev) override;
  void update(uint32_t dtMs) override;
  void render(M5Canvas& gfx) override;

 private:
  enum class Mode { List, Recording, Result };

  void refreshList();
  std::string recDir() const;   // "/sd/rec" when card present else "/flash/rec"
  void startUpload(const std::string& path);

  Mode mode_ = Mode::List;
  MenuList menu_;
  ProgressBar meter_;           // live level while recording
  TextView result_;             // upload response / errors
  ConfirmDialog confirm_;
  bool confirmOpen_ = false;
  std::vector<std::string> files_;  // full VFS paths, parallel to menu
  std::string pendingUpload_;   // one-frame delay before blocking upload
  int pendingDelay_ = 0;
  uint32_t sinceMeter_ = 0;
};
```

- [ ] **Step 2: Write `src/apps/RecorderApp.cpp`**

```cpp
#include "RecorderApp.h"

#include <M5Cardputer.h>

std::string RecorderApp::recDir() const {
  return cardos::fs::sdAvailable() ? "/sd/rec" : "/flash/rec";
}

void RecorderApp::onEnter() {
  mode_ = Mode::List;
  confirmOpen_ = false;
  refreshList();
}

void RecorderApp::refreshList() {
  std::string dir = recDir();
  cardos::fs::mkdir(dir);
  files_.clear();
  std::vector<MenuItem> items;
  for (const auto& e : cardos::fs::list(dir)) {
    if (e.isDir) continue;
    files_.push_back(dir + "/" + e.name);
    items.push_back({e.name, std::to_string(e.size / 1024) + "K",
                     theme::kMuted});
  }
  if (items.empty()) items.push_back({"(no recordings)", "", theme::kMuted});
  menu_.setItems(std::move(items));
  requestRedraw();
}

bool RecorderApp::handleKey(const KeyEvent& ev) {
  // Push-to-talk: explicit LongPress/Release handling BEFORE the guard.
  if (ev.code == KeyCode::Char && ev.ch == ' ') {
    if (ev.action == KeyAction::LongPress && mode_ == Mode::List) {
      std::string path = recDir() + "/rec-" + std::to_string(millis() / 1000) +
                         ".wav";
      if (cardos::audio::startToWav(path)) {
        mode_ = Mode::Recording;
        requestRedraw();
      }
      return true;
    }
    if (ev.action == KeyAction::Release && mode_ == Mode::Recording) {
      cardos::audio::stop();
      mode_ = Mode::List;
      refreshList();
      return true;
    }
  }
  if (ev.action != KeyAction::Press) return true;  // standard guard
  if (!pendingUpload_.empty()) return true;        // uploading: ignore keys

  if (confirmOpen_) {
    auto r = confirm_.handleKey(ev);
    if (r == ConfirmDialog::Result::First) {        // Upload
      confirmOpen_ = false;
      startUpload(files_[menu_.selected()]);
    } else if (r == ConfirmDialog::Result::Second) { // Delete
      cardos::fs::remove(files_[menu_.selected()]);
      confirmOpen_ = false;
      refreshList();
    } else if (r == ConfirmDialog::Result::Cancel) {
      confirmOpen_ = false;
    }
    requestRedraw();
    return true;
  }

  if (mode_ == Mode::Result) {
    if (result_.handleKey(ev)) { requestRedraw(); return true; }
    if (ev.code == KeyCode::Esc || ev.code == KeyCode::Enter) {
      mode_ = Mode::List;
      requestRedraw();
      return true;
    }
    return true;
  }

  if (menu_.handleKey(ev)) { requestRedraw(); return true; }
  if (ev.code == KeyCode::Enter && !files_.empty()) {
    confirm_.reset(menu_.selected() < (int)files_.size()
                       ? files_[menu_.selected()]
                       : "",
                   "Upload", "Delete");
    confirmOpen_ = !files_.empty();
    requestRedraw();
    return true;
  }
  return false;  // Esc -> launcher
}

void RecorderApp::startUpload(const std::string& path) {
  pendingUpload_ = path;
  pendingDelay_ = 1;
  result_.setText("Uploading " + path + " ...");
  mode_ = Mode::Result;
  requestRedraw();
}

void RecorderApp::update(uint32_t dtMs) {
  if (mode_ == Mode::Recording) {
    sinceMeter_ += dtMs;
    if (sinceMeter_ >= 100) {   // 10 Hz level meter refresh
      sinceMeter_ = 0;
      meter_.setValue((int)(cardos::audio::level() * 100));
      requestRedraw();
    }
  }
  if (!pendingUpload_.empty()) {
    if (pendingDelay_ > 0) { pendingDelay_--; return; }
    std::string path = pendingUpload_;
    // Progress paints straight to the display: the loop is blocked.
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
    auto r = cardos::http::uploadFile("https://httpbin.org/post", path,
                                      "file", {}, progress);
    pendingUpload_.clear();
    std::string text = r.status > 0
        ? "HTTP " + std::to_string(r.status) + "\n" + r.body.substr(0, 400)
        : "Upload failed: " + r.error;
    result_.setText(text);
    requestRedraw();
  }
}

void RecorderApp::render(M5Canvas& gfx) {
  int top = theme::kStatusBarH;
  if (mode_ == Mode::Recording) {
    label::draw(gfx, "REC - release SPACE to stop", theme::kPadX, top + 16,
                theme::kDanger);
    meter_.render(gfx, theme::kPadX, top + 40,
                  gfx.width() - 2 * theme::kPadX, 16);
    return;
  }
  if (mode_ == Mode::Result) {
    result_.render(gfx, 0, top, gfx.width(), gfx.height() - top);
    return;
  }
  label::draw(gfx, "Hold SPACE to record", theme::kPadX, top + 2,
              theme::kMuted);
  menu_.render(gfx, 0, top + 18, gfx.width(), gfx.height() - top - 18);
  if (confirmOpen_) confirm_.render(gfx);
}
```

- [ ] **Step 3: Register in `src/main.cpp`**

Add `#include "apps/RecorderApp.h"`, global `RecorderApp recorder;` (after `httpDemo`), and in `setup()` after the HTTP Demo entry:

```cpp
  launcher.addEntry("Recorder", &recorder);
```

- [ ] **Step 4: Build + tests.** `pio run -e m5stack-cardputer` → SUCCESS; `pio test -e native` → 41/41.

- [ ] **Step 5: Flash + on-device smoke test (device required)**

`pio run -e m5stack-cardputer -t upload`. Then: open Recorder → hold Space ≥0.6 s → REC screen with moving level meter → release → file appears in list → Enter → Upload (needs WiFi) → progress bar at the bottom → TextView shows `HTTP 200` and the httpbin JSON echo. If device absent, defer.

- [ ] **Step 6: Commit**

```bash
git add src/apps/RecorderApp.h src/apps/RecorderApp.cpp src/main.cpp
git commit -m "feat(sdk): Recorder example app exercising the full SDK"
```

---

### Task 10: Acceptance + Docs Refresh + Merge

- [ ] **Step 1: On-device acceptance checklist**

1. Launcher regression: navigation, WiFi page, HTTP Demo (incl. the new HTTPS endpoint), SysInfo all behave as before;
2. No double-trigger on key release anywhere (press guards);
3. Hold-Space recording: starts at ~0.6 s, level meter moves, release stops; WAV exists in `/flash/rec` (or `/sd/rec` with a card) with plausible size (~32 KB per second);
4. Upload to httpbin.org returns HTTP 200; progress bar advances; response echoes the file;
5. SD present/absent: `recDir()` switches; no crash without a card;
6. `pio test -e native` → 41/41.

Record failures, fix (superpowers:systematic-debugging), re-verify.

- [ ] **Step 2: README update** — add to the Apps table:

```markdown
| Recorder | Push-to-talk voice memos (hold Space), saved to /flash or /sd, multipart upload with progress |
```

and a Development section pointer: `See [docs/sdk.md](docs/sdk.md) for the app SDK.`

- [ ] **Step 3: Commit, merge, push**

```bash
git add README.md
git commit -m "docs: README SDK and Recorder updates"
git checkout main && git merge sdk -m "Merge branch 'sdk': CardOS app SDK"
git push origin main sdk
```

---

## Plan Self-Review Notes

- **Spec coverage:** input Press/LongPress/Release (T1–T2), audio streaming + WAV (T4–T5), fs /flash+/sd (T3), HTTP GET/POST/JSON/upload/HTTPS/progress/timeout (T6), TextView/ProgressBar/Label (T7), umbrella header + docs (T8), Recorder example exercising all (T9), acceptance + merge (T10). Spec §11 out-of-scope items absent.
- **Type consistency:** `KeyTracker::Out{id, action}` consumed in T2; `cardos::audio::start/startToWav/stop/level/tick` (T5) used by Recorder (T9); `cardos::fs::Entry{name,size,isDir}` (T3) used in T9 `refreshList`; `cardos::http::Response{status,body,error,ok()}` and `Progress(sent,total)` (T6) used in T9; `wrapText(text,width,measure)` (T7) used by TextView. Test-count arithmetic: 25 → 30 (T1) → 33 (T3) → 35 (T4) → 37 (T6) → 41 (T7).
- **Known hardware-verification points:** Mic_Class record semantics (T5 implementer note), SD SPI pins (T3 comment), M5 Button_Class `wasHold` threshold vs our 600 ms KeyTracker constant (G0 long-press may fire at the M5 default ~500 ms — acceptable; only Esc semantics use it).
- **Deliberate simplifications:** upload progress paints via a transient sprite to the live display because the main loop is blocked (documented in sdk.md T8); Recorder ignores keys during upload; boundary randomness via `esp_random()` (device-only path, pure builders take boundary as a parameter).



