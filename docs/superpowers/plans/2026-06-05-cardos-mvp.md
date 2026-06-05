# CardOS MVP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build CardOS for the M5Stack Cardputer: a launcher + app framework with a WiFi management app (scan/connect/remember/auto-connect) and tiered power management (dim at 60s idle, deep sleep at 5min, G0 wake).

**Architecture:** Single cooperative main loop. A scene-stack `AppManager` hosts `App` instances (launcher at the bottom). Hardware-independent logic (key mapping, scroll math, idle policy, credential store) lives in pure C++ files unit-tested on the host (`pio test -e native`). Device-only code (rendering, WiFi, NVS, sleep) is verified on hardware via an acceptance checklist.

**Tech Stack:** C++17, Arduino framework via PlatformIO (`espressif32`), `m5stack/M5Cardputer` (M5Unified/M5GFX), `bblanchon/ArduinoJson`, Unity test framework (PIO native env).

**Spec:** `docs/superpowers/specs/2026-06-05-cardos-design.md`

---

## File Map

| Path | Responsibility | Testable on host |
|---|---|---|
| `platformio.ini` | Build envs: `m5stack-cardputer` (device), `native` (tests) | — |
| `src/core/KeyEvent.h` | `KeyCode` enum, `KeyEvent` struct, `mapKey()` pure mapping | ✅ |
| `src/core/App.h` | `App` base class (scene interface) | — |
| `src/core/AppManager.{h,cpp}` | Scene stack, dispatch/update/render, status bar composition | — |
| `src/core/InputRouter.{h,cpp}` | Polls M5Cardputer keyboard/buttons → `KeyEvent`s | — |
| `src/core/IdlePolicy.h` | Pure idle state machine (Active/Dimmed/SleepPending) | ✅ |
| `src/core/PowerManager.{h,cpp}` | Brightness, sleep notice, deep sleep + G0 wake | — |
| `src/ui/scroll.h` | Pure scroll-window math for lists | ✅ |
| `src/ui/Theme.h` | Colors, fonts, metrics constants | — |
| `src/ui/StatusBar.{h,cpp}` | Top bar: title, WiFi icon, battery bars | — |
| `src/ui/MenuList.{h,cpp}` | Scrollable selectable list widget | — |
| `src/ui/TextInput.{h,cpp}` | Single-line text input (password entry) | — |
| `src/ui/Dialog.{h,cpp}` | Toast + two-option confirm dialog | — |
| `src/services/WiFiStore.{h,cpp}` | Credential list: JSON (de)serialize, upsert, evict, remove | ✅ |
| `src/services/NvsStorage.{h,cpp}` | `StorageBackend` impl over NVS `Preferences` | — |
| `src/services/WiFiService.{h,cpp}` | Async WiFi state machine: scan/connect/auto-connect | — |
| `src/apps/LauncherApp.{h,cpp}` | Root menu | — |
| `src/apps/SysInfoApp.{h,cpp}` | IP / heap / battery info page | — |
| `src/apps/WiFiApp.{h,cpp}` | WiFi management UI (home/scan/password/saved) | — |
| `src/main.cpp` | Wiring + main loop | — |
| `test/test_native/test_main.cpp` | All native unit tests | ✅ |

Pure-logic headers must not include `Arduino.h` or M5 headers.

---

### Task 1: Project Scaffolding

**Files:**
- Create: `platformio.ini`
- Create: `src/main.cpp`

- [ ] **Step 1: Write `platformio.ini`**

```ini
[env:m5stack-cardputer]
platform = espressif32@6.12.0
board = m5stack-stamps3
framework = arduino
monitor_speed = 115200
build_flags =
    -std=gnu++17
    -DARDUINO_USB_CDC_ON_BOOT=1
build_unflags = -std=gnu++11
lib_deps =
    m5stack/M5Cardputer@^1.1.0
    bblanchon/ArduinoJson@^7.4.0

[env:native]
platform = native
build_flags = -std=c++17
lib_deps = bblanchon/ArduinoJson@^7.4.0
test_framework = unity
```

- [ ] **Step 2: Write minimal `src/main.cpp`**

```cpp
#include <M5Cardputer.h>

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);  // true = enable keyboard
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(200);
  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.drawString("CardOS boot", 10, 10);
  Serial.begin(115200);
  Serial.println("[cardos] boot");
}

void loop() {
  M5Cardputer.update();
  delay(10);
}
```

- [ ] **Step 3: Build for device**

Run: `pio run -e m5stack-cardputer`
Expected: `SUCCESS` (first run downloads toolchain + libs, takes a few minutes)

- [ ] **Step 4: Flash and verify (device must be plugged in; check `ls /dev/cu.usbmodem*`)**

Run: `pio run -e m5stack-cardputer -t upload && pio device monitor`
Expected: screen shows "CardOS boot"; serial prints `[cardos] boot`. If no `/dev/cu.usbmodem*` device exists, stop and ask the user to check the USB connection.

- [ ] **Step 5: Commit**

```bash
git add platformio.ini src/main.cpp
git commit -m "feat: project scaffolding, boots on Cardputer"
```

---

### Task 2: Key Mapping (pure logic, TDD)

**Files:**
- Create: `src/core/KeyEvent.h`
- Create: `test/test_native/test_main.cpp`

- [ ] **Step 1: Write the failing tests**

Create `test/test_native/test_main.cpp`:

```cpp
#include <unity.h>
#include "../../src/core/KeyEvent.h"

void test_enter_key() {
  KeyEvent ev = mapKey(0, false, true, false, false);
  TEST_ASSERT_EQUAL((int)KeyCode::Enter, (int)ev.code);
}

void test_backspace_key() {
  KeyEvent ev = mapKey(0, false, false, true, false);
  TEST_ASSERT_EQUAL((int)KeyCode::Backspace, (int)ev.code);
}

void test_esc_is_backtick() {
  KeyEvent ev = mapKey('`', false, false, false, false);
  TEST_ASSERT_EQUAL((int)KeyCode::Esc, (int)ev.code);
}

void test_nav_keys_carry_char() {
  KeyEvent up = mapKey(';', false, false, false, false);
  TEST_ASSERT_EQUAL((int)KeyCode::Up, (int)up.code);
  TEST_ASSERT_EQUAL(';', up.ch);  // TextInput can still type ';'
  KeyEvent down = mapKey('.', false, false, false, false);
  TEST_ASSERT_EQUAL((int)KeyCode::Down, (int)down.code);
}

void test_printable_char() {
  KeyEvent ev = mapKey('a', false, false, false, false);
  TEST_ASSERT_EQUAL((int)KeyCode::Char, (int)ev.code);
  TEST_ASSERT_EQUAL('a', ev.ch);
}

void test_fn_flag_passthrough() {
  KeyEvent ev = mapKey('a', true, false, false, false);
  TEST_ASSERT_TRUE(ev.fn);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_enter_key);
  RUN_TEST(test_backspace_key);
  RUN_TEST(test_esc_is_backtick);
  RUN_TEST(test_nav_keys_carry_char);
  RUN_TEST(test_printable_char);
  RUN_TEST(test_fn_flag_passthrough);
  return UNITY_END();
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native`
Expected: compile error, `KeyEvent.h: No such file or directory`

- [ ] **Step 3: Write `src/core/KeyEvent.h`**

```cpp
#pragma once
#include <cstdint>

// Semantic key codes for CardOS. Pure header: no Arduino/M5 includes.
enum class KeyCode : uint8_t {
  None, Up, Down, Left, Right, Enter, Esc, Backspace, Tab, Char
};

struct KeyEvent {
  KeyCode code = KeyCode::None;
  char ch = 0;       // raw printable char (valid for Char and nav keys)
  bool fn = false;   // Fn modifier held
};

// Map one raw key report to a semantic event. The M5Cardputer keyboard
// reports Enter/Backspace/Tab as separate booleans, printables as chars.
// Nav keys (; . , /) keep their char so a focused TextInput can use them
// as literal input; ` doubles as ESC and is not typeable.
inline KeyEvent mapKey(char raw, bool fn, bool enter, bool del, bool tab) {
  KeyEvent ev;
  ev.fn = fn;
  if (enter) { ev.code = KeyCode::Enter; return ev; }
  if (del)   { ev.code = KeyCode::Backspace; return ev; }
  if (tab)   { ev.code = KeyCode::Tab; return ev; }
  switch (raw) {
    case '`': ev.code = KeyCode::Esc; return ev;
    case ';': ev.code = KeyCode::Up; break;
    case '.': ev.code = KeyCode::Down; break;
    case ',': ev.code = KeyCode::Left; break;
    case '/': ev.code = KeyCode::Right; break;
    default:  ev.code = KeyCode::Char; break;
  }
  ev.ch = raw;
  return ev;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native`
Expected: `6 Tests 0 Failures 0 Ignored: PASSED`

- [ ] **Step 5: Commit**

```bash
git add src/core/KeyEvent.h test/test_native/test_main.cpp
git commit -m "feat: semantic key mapping with native tests"
```

---

### Task 3: Scroll Window Math (pure logic, TDD)

**Files:**
- Create: `src/ui/scroll.h`
- Modify: `test/test_native/test_main.cpp`

- [ ] **Step 1: Add failing tests to `test/test_native/test_main.cpp`**

Add after the existing includes:

```cpp
#include "../../src/ui/scroll.h"
```

Add test functions before `main`:

```cpp
void test_scroll_fits_on_one_page() {
  TEST_ASSERT_EQUAL(0, scrollFirstVisible(3, 4, 6));  // 4 items, 6 visible
}

void test_scroll_centers_selection() {
  // 20 items, 6 visible, selected 10 -> window starts at 10 - 3 = 7
  TEST_ASSERT_EQUAL(7, scrollFirstVisible(10, 20, 6));
}

void test_scroll_clamps_top() {
  TEST_ASSERT_EQUAL(0, scrollFirstVisible(1, 20, 6));
}

void test_scroll_clamps_bottom() {
  TEST_ASSERT_EQUAL(14, scrollFirstVisible(19, 20, 6));  // 20 - 6 = 14
}
```

Register them in `main` before `return UNITY_END();`:

```cpp
  RUN_TEST(test_scroll_fits_on_one_page);
  RUN_TEST(test_scroll_centers_selection);
  RUN_TEST(test_scroll_clamps_top);
  RUN_TEST(test_scroll_clamps_bottom);
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native`
Expected: compile error, `scroll.h: No such file or directory`

- [ ] **Step 3: Write `src/ui/scroll.h`**

```cpp
#pragma once

// Index of the first visible row of a scrolling list. Keeps the selected
// row centered where possible, clamped to the list bounds. Pure header.
inline int scrollFirstVisible(int selected, int count, int visible) {
  if (count <= visible) return 0;
  int first = selected - visible / 2;
  if (first < 0) first = 0;
  if (first > count - visible) first = count - visible;
  return first;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native`
Expected: `10 Tests 0 Failures 0 Ignored: PASSED`

- [ ] **Step 5: Commit**

```bash
git add src/ui/scroll.h test/test_native/test_main.cpp
git commit -m "feat: scroll window math with native tests"
```

---

### Task 4: Idle Policy (pure logic, TDD)

**Files:**
- Create: `src/core/IdlePolicy.h`
- Modify: `test/test_native/test_main.cpp`

- [ ] **Step 1: Add failing tests to `test/test_native/test_main.cpp`**

Add include:

```cpp
#include "../../src/core/IdlePolicy.h"
```

Add test functions before `main`:

```cpp
void test_idle_starts_active() {
  IdlePolicy p(60000, 300000);
  TEST_ASSERT_EQUAL((int)IdleState::Active, (int)p.state(0));
}

void test_idle_dims_after_dim_timeout() {
  IdlePolicy p(60000, 300000);
  p.onInput(1000);
  TEST_ASSERT_EQUAL((int)IdleState::Active, (int)p.state(60999));
  TEST_ASSERT_EQUAL((int)IdleState::Dimmed, (int)p.state(61000));
}

void test_idle_sleeps_after_sleep_timeout() {
  IdlePolicy p(60000, 300000);
  p.onInput(1000);
  TEST_ASSERT_EQUAL((int)IdleState::SleepPending, (int)p.state(301000));
}

void test_input_resets_timer() {
  IdlePolicy p(60000, 300000);
  p.onInput(1000);
  p.onInput(200000);
  TEST_ASSERT_EQUAL((int)IdleState::Active, (int)p.state(259999));
}

void test_wake_input_is_swallowed_when_dimmed() {
  IdlePolicy p(60000, 300000);
  p.onInput(0);
  TEST_ASSERT_TRUE(p.onInput(61000));    // dimmed -> swallow this key
  TEST_ASSERT_FALSE(p.onInput(62000));   // active again -> forward
}

void test_keep_awake_suppresses_sleep_not_dim() {
  IdlePolicy p(60000, 300000);
  p.onInput(0);
  p.keepAwake(true);
  TEST_ASSERT_EQUAL((int)IdleState::Dimmed, (int)p.state(301000));
  p.keepAwake(false);
  TEST_ASSERT_EQUAL((int)IdleState::SleepPending, (int)p.state(301000));
}
```

Register in `main`:

```cpp
  RUN_TEST(test_idle_starts_active);
  RUN_TEST(test_idle_dims_after_dim_timeout);
  RUN_TEST(test_idle_sleeps_after_sleep_timeout);
  RUN_TEST(test_input_resets_timer);
  RUN_TEST(test_wake_input_is_swallowed_when_dimmed);
  RUN_TEST(test_keep_awake_suppresses_sleep_not_dim);
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native`
Expected: compile error, `IdlePolicy.h: No such file or directory`

- [ ] **Step 3: Write `src/core/IdlePolicy.h`**

```cpp
#pragma once
#include <cstdint>

// Idle state machine. Pure header: takes time as a parameter, no millis().
enum class IdleState : uint8_t { Active, Dimmed, SleepPending };

class IdlePolicy {
 public:
  IdlePolicy(uint32_t dimAfterMs, uint32_t sleepAfterMs)
      : dimAfterMs_(dimAfterMs), sleepAfterMs_(sleepAfterMs) {}

  // Record user input at `now`. Returns true if the key should be swallowed
  // (it only woke the dimmed screen and must not reach the app).
  bool onInput(uint32_t now) {
    bool swallow = state(now) == IdleState::Dimmed;
    lastInputMs_ = now;
    return swallow;
  }

  // Suppresses SleepPending (e.g. WiFi connecting); dimming still applies.
  void keepAwake(bool on) { keepAwake_ = on; }

  IdleState state(uint32_t now) const {
    uint32_t idle = now - lastInputMs_;
    if (!keepAwake_ && idle >= sleepAfterMs_) return IdleState::SleepPending;
    if (idle >= dimAfterMs_) return IdleState::Dimmed;
    return IdleState::Active;
  }

 private:
  uint32_t dimAfterMs_, sleepAfterMs_;
  uint32_t lastInputMs_ = 0;
  bool keepAwake_ = false;
};
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native`
Expected: `16 Tests 0 Failures 0 Ignored: PASSED`

- [ ] **Step 5: Commit**

```bash
git add src/core/IdlePolicy.h test/test_native/test_main.cpp
git commit -m "feat: idle policy state machine with native tests"
```

---

### Task 5: WiFiStore (pure logic, TDD)

**Files:**
- Create: `src/services/WiFiStore.h`
- Create: `src/services/WiFiStore.cpp`
- Modify: `test/test_native/test_main.cpp`
- Modify: `platformio.ini` (native env needs `src/services` compiled into tests)

- [ ] **Step 1: Point the native test build at the pure sources**

In `platformio.ini`, replace the `[env:native]` section with:

```ini
[env:native]
platform = native
build_flags = -std=c++17 -I src
lib_deps = bblanchon/ArduinoJson@^7.4.0
test_framework = unity
build_src_filter = +<services/WiFiStore.cpp>
test_build_src = yes
```

- [ ] **Step 2: Add failing tests to `test/test_native/test_main.cpp`**

Add include:

```cpp
#include "../../src/services/WiFiStore.h"
```

Add test support + functions before `main`:

```cpp
struct InMemoryStorage : StorageBackend {
  std::string data;
  std::string load() override { return data; }
  void save(const std::string& s) override { data = s; }
};

void test_store_starts_empty() {
  InMemoryStorage mem;
  WiFiStore store(mem);
  store.load();
  TEST_ASSERT_EQUAL(0, (int)store.networks().size());
}

void test_store_upsert_and_find() {
  InMemoryStorage mem;
  WiFiStore store(mem);
  store.upsert("home", "pw123");
  const WifiNetwork* n = store.find("home");
  TEST_ASSERT_NOT_NULL(n);
  TEST_ASSERT_EQUAL_STRING("pw123", n->password.c_str());
}

void test_store_upsert_updates_password() {
  InMemoryStorage mem;
  WiFiStore store(mem);
  store.upsert("home", "old");
  store.upsert("home", "new");
  TEST_ASSERT_EQUAL(1, (int)store.networks().size());
  TEST_ASSERT_EQUAL_STRING("new", store.find("home")->password.c_str());
}

void test_store_persists_roundtrip() {
  InMemoryStorage mem;
  {
    WiFiStore store(mem);
    store.upsert("home", "pw123");
    store.touch("home", 42);
  }
  WiFiStore store2(mem);
  store2.load();
  TEST_ASSERT_EQUAL(1, (int)store2.networks().size());
  TEST_ASSERT_EQUAL_STRING("pw123", store2.find("home")->password.c_str());
  TEST_ASSERT_EQUAL(42, (int)store2.find("home")->lastOkTs);
}

void test_store_evicts_oldest_when_full() {
  InMemoryStorage mem;
  WiFiStore store(mem);
  for (int i = 0; i < 8; i++) {
    std::string ssid = "net" + std::to_string(i);
    store.upsert(ssid, "pw");
    store.touch(ssid, 100 + i);  // net0 oldest
  }
  store.upsert("net8", "pw");    // 9th entry -> evict net0
  TEST_ASSERT_EQUAL(8, (int)store.networks().size());
  TEST_ASSERT_NULL(store.find("net0"));
  TEST_ASSERT_NOT_NULL(store.find("net8"));
}

void test_store_remove() {
  InMemoryStorage mem;
  WiFiStore store(mem);
  store.upsert("home", "pw");
  store.remove("home");
  TEST_ASSERT_NULL(store.find("home"));
  TEST_ASSERT_EQUAL(0, (int)store.networks().size());
}

void test_store_load_garbage_is_empty() {
  InMemoryStorage mem;
  mem.data = "not json {{{";
  WiFiStore store(mem);
  store.load();
  TEST_ASSERT_EQUAL(0, (int)store.networks().size());
}
```

Register in `main`:

```cpp
  RUN_TEST(test_store_starts_empty);
  RUN_TEST(test_store_upsert_and_find);
  RUN_TEST(test_store_upsert_updates_password);
  RUN_TEST(test_store_persists_roundtrip);
  RUN_TEST(test_store_evicts_oldest_when_full);
  RUN_TEST(test_store_remove);
  RUN_TEST(test_store_load_garbage_is_empty);
```

- [ ] **Step 3: Run tests to verify they fail**

Run: `pio test -e native`
Expected: compile error, `WiFiStore.h: No such file or directory`

- [ ] **Step 4: Write `src/services/WiFiStore.h`**

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Saved WiFi credential list. Pure logic over an injected storage backend:
// the device uses NVS (NvsStorage), tests use an in-memory mock.
struct WifiNetwork {
  std::string ssid;
  std::string password;
  uint32_t lastOkTs = 0;  // last successful connect (epoch s, 0 = never)
};

class StorageBackend {
 public:
  virtual ~StorageBackend() = default;
  virtual std::string load() = 0;               // "" when nothing stored
  virtual void save(const std::string& data) = 0;
};

class WiFiStore {
 public:
  static constexpr size_t kCapacity = 8;

  explicit WiFiStore(StorageBackend& backend) : backend_(backend) {}

  void load();  // deserialize from backend; garbage -> empty list
  const std::vector<WifiNetwork>& networks() const { return networks_; }
  const WifiNetwork* find(const std::string& ssid) const;

  // Insert or update a credential; evicts the oldest-lastOkTs entry when
  // full. Persists immediately.
  void upsert(const std::string& ssid, const std::string& password);
  void remove(const std::string& ssid);          // persists
  void touch(const std::string& ssid, uint32_t ts);  // mark success; persists

 private:
  void persist();
  StorageBackend& backend_;
  std::vector<WifiNetwork> networks_;
};
```

- [ ] **Step 5: Write `src/services/WiFiStore.cpp`**

```cpp
#include "WiFiStore.h"

#include <ArduinoJson.h>
#include <algorithm>

void WiFiStore::load() {
  networks_.clear();
  JsonDocument doc;
  if (deserializeJson(doc, backend_.load()) != DeserializationError::Ok)
    return;
  for (JsonObject o : doc.as<JsonArray>()) {
    WifiNetwork n;
    n.ssid = o["ssid"].as<std::string>();
    n.password = o["pw"].as<std::string>();
    n.lastOkTs = o["ts"] | 0;
    if (!n.ssid.empty()) networks_.push_back(std::move(n));
  }
}

const WifiNetwork* WiFiStore::find(const std::string& ssid) const {
  for (const auto& n : networks_)
    if (n.ssid == ssid) return &n;
  return nullptr;
}

void WiFiStore::upsert(const std::string& ssid, const std::string& password) {
  for (auto& n : networks_) {
    if (n.ssid == ssid) {
      n.password = password;
      persist();
      return;
    }
  }
  if (networks_.size() >= kCapacity) {
    auto oldest = std::min_element(
        networks_.begin(), networks_.end(),
        [](const WifiNetwork& a, const WifiNetwork& b) {
          return a.lastOkTs < b.lastOkTs;
        });
    networks_.erase(oldest);
  }
  networks_.push_back({ssid, password, 0});
  persist();
}

void WiFiStore::remove(const std::string& ssid) {
  networks_.erase(
      std::remove_if(networks_.begin(), networks_.end(),
                     [&](const WifiNetwork& n) { return n.ssid == ssid; }),
      networks_.end());
  persist();
}

void WiFiStore::touch(const std::string& ssid, uint32_t ts) {
  for (auto& n : networks_) {
    if (n.ssid == ssid) {
      n.lastOkTs = ts;
      persist();
      return;
    }
  }
}

void WiFiStore::persist() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (const auto& n : networks_) {
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = n.ssid;
    o["pw"] = n.password;
    o["ts"] = n.lastOkTs;
  }
  std::string out;
  serializeJson(doc, out);
  backend_.save(out);
}
```

- [ ] **Step 6: Run tests to verify they pass**

Run: `pio test -e native`
Expected: `23 Tests 0 Failures 0 Ignored: PASSED`

- [ ] **Step 7: Verify the device build still compiles**

Run: `pio run -e m5stack-cardputer`
Expected: `SUCCESS`

- [ ] **Step 8: Commit**

```bash
git add src/services/WiFiStore.h src/services/WiFiStore.cpp test/test_native/test_main.cpp platformio.ini
git commit -m "feat: WiFi credential store with eviction and native tests"
```

---

### Task 6: Core Framework — App, AppManager, InputRouter

Device-only code (no native tests). Verified by building and flashing a stub app that echoes key presses.

**Files:**
- Create: `src/core/App.h`
- Create: `src/core/AppManager.h`
- Create: `src/core/AppManager.cpp`
- Create: `src/core/InputRouter.h`
- Create: `src/core/InputRouter.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Write `src/core/App.h`**

```cpp
#pragma once
#include <M5GFX.h>

#include "KeyEvent.h"

class AppManager;

// One screen/scene. Apps are pushed onto AppManager's stack; the top app
// receives input and renders into the content area below the status bar.
class App {
 public:
  virtual ~App() = default;
  virtual const char* title() const = 0;
  virtual void onEnter() {}
  virtual void onExit() {}
  // Return true if the key was consumed. Unconsumed Esc pops the app.
  virtual bool handleKey(const KeyEvent& ev) { (void)ev; return false; }
  virtual void update(uint32_t dtMs) { (void)dtMs; }
  virtual void render(M5Canvas& gfx) = 0;  // gfx is the full-screen canvas

  void requestRedraw() { dirty_ = true; }
  bool consumeDirty() { bool d = dirty_; dirty_ = false; return d; }
  void attach(AppManager* mgr) { mgr_ = mgr; }

 protected:
  AppManager* mgr_ = nullptr;
  bool dirty_ = true;
};
```

- [ ] **Step 2: Write `src/core/AppManager.h`**

```cpp
#pragma once
#include <M5Cardputer.h>

#include <vector>

#include "App.h"
#include "KeyEvent.h"

// Scene stack + render loop. Owns the off-screen canvas; composes the
// status bar (drawn by a callback so core/ does not depend on ui/).
class AppManager {
 public:
  using StatusBarPainter = void (*)(M5Canvas& gfx, const char* title);

  void begin(M5GFX& display, StatusBarPainter painter);
  void push(App* app);   // apps are statically allocated; not owned
  void pop();
  App* top() const { return stack_.empty() ? nullptr : stack_.back(); }

  void dispatch(const KeyEvent& ev);
  void update(uint32_t dtMs);
  void render();              // redraws only when the top app is dirty
  void requestRedraw();       // force a redraw (e.g. status bar changed)

 private:
  std::vector<App*> stack_;
  M5Canvas canvas_;
  M5GFX* display_ = nullptr;
  StatusBarPainter statusBar_ = nullptr;
  bool forceRedraw_ = true;
};
```

- [ ] **Step 3: Write `src/core/AppManager.cpp`**

```cpp
#include "AppManager.h"

void AppManager::begin(M5GFX& display, StatusBarPainter painter) {
  display_ = &display;
  statusBar_ = painter;
  canvas_.setColorDepth(16);
  canvas_.createSprite(display.width(), display.height());
}

void AppManager::push(App* app) {
  app->attach(this);
  stack_.push_back(app);
  app->onEnter();
  forceRedraw_ = true;
}

void AppManager::pop() {
  if (stack_.size() <= 1) return;  // launcher stays at the bottom
  stack_.back()->onExit();
  stack_.pop_back();
  forceRedraw_ = true;
}

void AppManager::dispatch(const KeyEvent& ev) {
  App* app = top();
  if (!app) return;
  if (!app->handleKey(ev) && ev.code == KeyCode::Esc) pop();
}

void AppManager::update(uint32_t dtMs) {
  if (App* app = top()) app->update(dtMs);
}

void AppManager::requestRedraw() { forceRedraw_ = true; }

void AppManager::render() {
  App* app = top();
  if (!app) return;
  if (!forceRedraw_ && !app->consumeDirty()) return;
  forceRedraw_ = false;
  app->consumeDirty();
  canvas_.fillSprite(TFT_BLACK);
  app->render(canvas_);
  if (statusBar_) statusBar_(canvas_, app->title());
  canvas_.pushSprite(display_, 0, 0);
}
```

- [ ] **Step 4: Write `src/core/InputRouter.h`**

```cpp
#pragma once
#include <vector>

#include "KeyEvent.h"

// Polls the Cardputer keyboard matrix and G0 button, emitting semantic
// KeyEvents once per physical key press (no auto-repeat in MVP).
class InputRouter {
 public:
  // Call once per loop after M5Cardputer.update(). Returns the events
  // generated by this frame's state change (usually 0 or 1).
  std::vector<KeyEvent> poll();
};
```

- [ ] **Step 5: Write `src/core/InputRouter.cpp`**

```cpp
#include "InputRouter.h"

#include <M5Cardputer.h>

std::vector<KeyEvent> InputRouter::poll() {
  std::vector<KeyEvent> out;

  // G0/BtnA acts as Esc when running (it is also the deep-sleep wake pin).
  if (M5Cardputer.BtnA.wasPressed()) {
    KeyEvent ev;
    ev.code = KeyCode::Esc;
    out.push_back(ev);
  }

  if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed())
    return out;

  auto st = M5Cardputer.Keyboard.keysState();
  if (st.enter || st.del || st.tab) {
    out.push_back(mapKey(0, st.fn, st.enter, st.del, st.tab));
    return out;
  }
  for (char c : st.word) out.push_back(mapKey(c, st.fn, false, false, false));
  return out;
}
```

- [ ] **Step 6: Rewrite `src/main.cpp` with the main loop and a stub app**

```cpp
#include <M5Cardputer.h>

#include "core/AppManager.h"
#include "core/InputRouter.h"

namespace {

// Temporary key-echo app, replaced by LauncherApp in a later task.
class EchoApp : public App {
 public:
  const char* title() const override { return "CardOS"; }
  bool handleKey(const KeyEvent& ev) override {
    last_ = ev;
    requestRedraw();
    return ev.code != KeyCode::Esc;  // keep Esc from popping the root
  }
  void render(M5Canvas& gfx) override {
    gfx.setTextSize(2);
    gfx.setTextColor(TFT_WHITE);
    gfx.setCursor(10, 40);
    switch (last_.code) {
      case KeyCode::None: gfx.print("press a key"); break;
      case KeyCode::Up: gfx.print("UP"); break;
      case KeyCode::Down: gfx.print("DOWN"); break;
      case KeyCode::Enter: gfx.print("ENTER"); break;
      case KeyCode::Esc: gfx.print("ESC"); break;
      case KeyCode::Backspace: gfx.print("BACKSPACE"); break;
      default: gfx.printf("char: %c", last_.ch); break;
    }
  }
 private:
  KeyEvent last_;
};

void paintStatusBar(M5Canvas& gfx, const char* title) {
  gfx.fillRect(0, 0, gfx.width(), 16, TFT_NAVY);
  gfx.setTextSize(1);
  gfx.setTextColor(TFT_WHITE);
  gfx.setCursor(4, 4);
  gfx.print(title);
}

AppManager apps;
InputRouter input;
EchoApp echo;
uint32_t lastMs = 0;

}  // namespace

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(200);
  Serial.begin(115200);
  Serial.println("[cardos] boot");
  apps.begin(M5Cardputer.Display, paintStatusBar);
  apps.push(&echo);
  lastMs = millis();
}

void loop() {
  M5Cardputer.update();
  uint32_t now = millis();
  for (const KeyEvent& ev : input.poll()) apps.dispatch(ev);
  apps.update(now - lastMs);
  apps.render();
  lastMs = now;
  delay(5);
}
```

- [ ] **Step 7: Build**

Run: `pio run -e m5stack-cardputer`
Expected: `SUCCESS`

- [ ] **Step 8: Flash and verify on device**

Run: `pio run -e m5stack-cardputer -t upload`
Expected: status bar reads "CardOS"; pressing letters shows `char: x`; `;`/`.` show UP/DOWN; Enter shows ENTER; backtick shows ESC (EchoApp consumes everything except Esc, and the root app never pops).

- [ ] **Step 9: Verify native tests still pass**

Run: `pio test -e native`
Expected: `23 Tests 0 Failures 0 Ignored: PASSED`

- [ ] **Step 10: Commit**

```bash
git add src/core src/main.cpp
git commit -m "feat: app framework with scene stack and input routing"
```

---

### Task 7: Theme + StatusBar

**Files:**
- Create: `src/ui/Theme.h`
- Create: `src/ui/StatusBar.h`
- Create: `src/ui/StatusBar.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Write `src/ui/Theme.h`**

```cpp
#pragma once
#include <M5GFX.h>

// All UI constants in one place.
namespace theme {
constexpr int kStatusBarH = 16;
constexpr int kRowH = 18;             // menu row height
constexpr int kPadX = 6;

constexpr uint16_t kBg = TFT_BLACK;
constexpr uint16_t kFg = TFT_WHITE;
constexpr uint16_t kMuted = 0x8410;   // mid gray
constexpr uint16_t kAccent = 0x05FF;  // cyan-ish
constexpr uint16_t kBarBg = 0x0926;   // dark navy
constexpr uint16_t kDanger = TFT_RED;
constexpr uint16_t kOk = TFT_GREEN;

// CJK-capable font for SSIDs and labels.
inline const lgfx::IFont* font() { return &fonts::efontCN_12; }
inline const lgfx::IFont* fontBold() { return &fonts::efontCN_12_b; }
}  // namespace theme
```

- [ ] **Step 2: Write `src/ui/StatusBar.h`**

```cpp
#pragma once
#include <M5GFX.h>

// Framework-owned top bar. WiFi/battery state is pushed in by main.cpp
// (the status bar does not query services itself).
namespace statusbar {

enum class WifiIcon { Off, Connecting, Bars1, Bars2, Bars3 };

void setWifi(WifiIcon icon);
void setBattery(int pct);            // 0..100
bool changedSinceLastPaint();        // true -> AppManager.requestRedraw()
void paint(M5Canvas& gfx, const char* title);

}  // namespace statusbar
```

- [ ] **Step 3: Write `src/ui/StatusBar.cpp`**

```cpp
#include "StatusBar.h"

#include "Theme.h"

namespace statusbar {
namespace {
WifiIcon wifi_ = WifiIcon::Off;
int batteryPct_ = 100;
bool dirty_ = true;
uint8_t spinnerFrame_ = 0;
}  // namespace

void setWifi(WifiIcon icon) {
  if (wifi_ != icon) { wifi_ = icon; dirty_ = true; }
  if (icon == WifiIcon::Connecting) dirty_ = true;  // animate spinner
}

void setBattery(int pct) {
  int bars = pct / 26;  // 0..3, only repaint when the bar count changes
  if (bars != batteryPct_ / 26) dirty_ = true;
  batteryPct_ = pct;
}

bool changedSinceLastPaint() { return dirty_; }

void paint(M5Canvas& gfx, const char* title) {
  dirty_ = false;
  gfx.fillRect(0, 0, gfx.width(), theme::kStatusBarH, theme::kBarBg);
  gfx.setFont(theme::fontBold());
  gfx.setTextColor(theme::kFg);
  gfx.setCursor(theme::kPadX, 2);
  gfx.print(title);

  // Battery: 4-segment bar at the right edge.
  int bx = gfx.width() - 26, by = 4;
  gfx.drawRect(bx, by, 20, 8, theme::kFg);
  gfx.fillRect(bx + 20, by + 2, 2, 4, theme::kFg);
  int seg = (batteryPct_ + 25) / 26;  // 0..4
  for (int i = 0; i < seg && i < 4; i++)
    gfx.fillRect(bx + 2 + i * 4, by + 2, 3, 4,
                 batteryPct_ <= 20 ? theme::kDanger : theme::kOk);

  // WiFi: 3 ascending bars, gray when off; spinner dot when connecting.
  int wx = gfx.width() - 48, wy = 12;
  if (wifi_ == WifiIcon::Connecting) {
    spinnerFrame_ = (spinnerFrame_ + 1) % 4;
    gfx.fillCircle(wx + 4 + spinnerFrame_ * 3, wy - 4, 1, theme::kAccent);
  } else {
    int bars = wifi_ == WifiIcon::Bars1 ? 1
             : wifi_ == WifiIcon::Bars2 ? 2
             : wifi_ == WifiIcon::Bars3 ? 3 : 0;
    for (int i = 0; i < 3; i++) {
      uint16_t c = i < bars ? theme::kFg : theme::kMuted;
      int h = 3 + i * 3;
      gfx.fillRect(wx + i * 5, wy - h, 3, h, c);
    }
  }
}

}  // namespace statusbar
```

- [ ] **Step 4: Wire into `src/main.cpp`**

Replace the local `paintStatusBar` function with the real one and feed it battery data. Remove the old `paintStatusBar` and change:

```cpp
#include "ui/StatusBar.h"
```

In `setup()`, change the `apps.begin` line to:

```cpp
  apps.begin(M5Cardputer.Display, statusbar::paint);
```

In `loop()`, before `apps.render();` add:

```cpp
  static uint32_t lastBatteryMs = 0;
  if (now - lastBatteryMs > 5000) {
    lastBatteryMs = now;
    statusbar::setBattery(M5Cardputer.Power.getBatteryLevel());
  }
  if (statusbar::changedSinceLastPaint()) apps.requestRedraw();
```

- [ ] **Step 5: Build, flash, verify**

Run: `pio run -e m5stack-cardputer -t upload`
Expected: navy status bar with "CardOS", gray WiFi bars, green battery indicator.

- [ ] **Step 6: Commit**

```bash
git add src/ui/Theme.h src/ui/StatusBar.h src/ui/StatusBar.cpp src/main.cpp
git commit -m "feat: theme constants and status bar"
```

---

### Task 8: MenuList Widget

**Files:**
- Create: `src/ui/MenuList.h`
- Create: `src/ui/MenuList.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Write `src/ui/MenuList.h`**

```cpp
#pragma once
#include <M5GFX.h>

#include <string>
#include <vector>

#include "../core/KeyEvent.h"

// Scrollable, selectable vertical list. Up/Down moves the selection;
// the caller reacts to Enter itself (via selected()).
struct MenuItem {
  std::string label;
  std::string note;      // right-aligned annotation (signal bars, lock, ...)
  uint16_t noteColor = 0xFFFF;
};

class MenuList {
 public:
  void setItems(std::vector<MenuItem> items);
  bool handleKey(const KeyEvent& ev);  // true if consumed (selection moved)
  int selected() const { return selected_; }
  void setSelected(int idx);
  // Draws into the given rect; rows of theme::kRowH, ~6 visible rows.
  void render(M5Canvas& gfx, int x, int y, int w, int h) const;

 private:
  std::vector<MenuItem> items_;
  int selected_ = 0;
};
```

- [ ] **Step 2: Write `src/ui/MenuList.cpp`**

```cpp
#include "MenuList.h"

#include "Theme.h"
#include "scroll.h"

void MenuList::setItems(std::vector<MenuItem> items) {
  items_ = std::move(items);
  if (selected_ >= (int)items_.size()) selected_ = 0;
}

void MenuList::setSelected(int idx) {
  if (idx >= 0 && idx < (int)items_.size()) selected_ = idx;
}

bool MenuList::handleKey(const KeyEvent& ev) {
  if (items_.empty()) return false;
  if (ev.code == KeyCode::Up && selected_ > 0) { selected_--; return true; }
  if (ev.code == KeyCode::Down && selected_ < (int)items_.size() - 1) {
    selected_++;
    return true;
  }
  return false;
}

void MenuList::render(M5Canvas& gfx, int x, int y, int w, int h) const {
  gfx.setFont(theme::font());
  int visible = h / theme::kRowH;
  int first = scrollFirstVisible(selected_, items_.size(), visible);
  for (int row = 0; row < visible; row++) {
    int idx = first + row;
    if (idx >= (int)items_.size()) break;
    int ry = y + row * theme::kRowH;
    bool sel = idx == selected_;
    if (sel) gfx.fillRect(x, ry, w, theme::kRowH, theme::kAccent);
    gfx.setTextColor(sel ? TFT_BLACK : theme::kFg);
    gfx.setCursor(x + theme::kPadX, ry + 3);
    gfx.print(items_[idx].label.c_str());
    if (!items_[idx].note.empty()) {
      int nw = gfx.textWidth(items_[idx].note.c_str());
      gfx.setTextColor(sel ? TFT_BLACK : items_[idx].noteColor);
      gfx.setCursor(x + w - nw - theme::kPadX, ry + 3);
      gfx.print(items_[idx].note.c_str());
    }
  }
  // Scroll hint: thin bar on the right edge.
  if ((int)items_.size() > visible) {
    int barH = h * visible / items_.size();
    int barY = y + (h - barH) * first / (items_.size() - visible);
    gfx.fillRect(x + w - 2, barY, 2, barH, theme::kMuted);
  }
}
```

- [ ] **Step 3: Smoke-test in `src/main.cpp` (temporary)**

Replace `EchoApp` with a menu demo (this entire class is deleted in Task 10):

```cpp
#include "ui/MenuList.h"
#include "ui/Theme.h"

class EchoApp : public App {
 public:
  const char* title() const override { return "CardOS"; }
  void onEnter() override {
    menu_.setItems({{"WiFi Settings", "", 0},
                    {"System Info", "", 0},
                    {"中文条目测试", "ok", theme::kOk},
                    {"Item 4", "", 0},
                    {"Item 5", "", 0},
                    {"Item 6", "", 0},
                    {"Item 7", "", 0},
                    {"Item 8", "", 0}});
  }
  bool handleKey(const KeyEvent& ev) override {
    if (menu_.handleKey(ev)) { requestRedraw(); return true; }
    return ev.code != KeyCode::Esc;
  }
  void render(M5Canvas& gfx) override {
    menu_.render(gfx, 0, theme::kStatusBarH, gfx.width(),
                 gfx.height() - theme::kStatusBarH);
  }
 private:
  MenuList menu_;
};
```

- [ ] **Step 4: Build, flash, verify**

Run: `pio run -e m5stack-cardputer -t upload`
Expected: 8-item menu; `;`/`.` moves the highlight; list scrolls past row 6; the Chinese label renders; scrollbar visible on the right.

- [ ] **Step 5: Commit**

```bash
git add src/ui/MenuList.h src/ui/MenuList.cpp src/main.cpp
git commit -m "feat: scrollable menu list widget"
```

---

### Task 9: TextInput + Dialog Widgets

**Files:**
- Create: `src/ui/TextInput.h`
- Create: `src/ui/TextInput.cpp`
- Create: `src/ui/Dialog.h`
- Create: `src/ui/Dialog.cpp`

- [ ] **Step 1: Write `src/ui/TextInput.h`**

```cpp
#pragma once
#include <M5GFX.h>

#include <string>

#include "../core/KeyEvent.h"

// Single-line text input. Plaintext by default; Tab toggles masking.
class TextInput {
 public:
  enum class Result { None, Submit, Cancel };

  void reset(const std::string& prompt);
  Result handleKey(const KeyEvent& ev);
  const std::string& text() const { return text_; }
  void render(M5Canvas& gfx, int x, int y, int w) const;

 private:
  std::string prompt_;
  std::string text_;
  bool masked_ = false;
};
```

- [ ] **Step 2: Write `src/ui/TextInput.cpp`**

```cpp
#include "TextInput.h"

#include "Theme.h"

void TextInput::reset(const std::string& prompt) {
  prompt_ = prompt;
  text_.clear();
  masked_ = false;
}

TextInput::Result TextInput::handleKey(const KeyEvent& ev) {
  switch (ev.code) {
    case KeyCode::Enter:
      return Result::Submit;
    case KeyCode::Esc:
      return Result::Cancel;
    case KeyCode::Backspace:
      if (!text_.empty()) text_.pop_back();
      return Result::None;
    case KeyCode::Tab:
      masked_ = !masked_;
      return Result::None;
    default:
      // Nav keys carry their literal char; accept any printable.
      if (ev.ch >= 0x20 && ev.ch < 0x7F && text_.size() < 63)
        text_.push_back(ev.ch);
      return Result::None;
  }
}

void TextInput::render(M5Canvas& gfx, int x, int y, int w) const {
  gfx.setFont(theme::font());
  gfx.setTextColor(theme::kMuted);
  gfx.setCursor(x, y);
  gfx.print(prompt_.c_str());

  int by = y + 16;
  gfx.drawRect(x, by, w, 20, theme::kAccent);
  gfx.setTextColor(theme::kFg);
  gfx.setCursor(x + 4, by + 4);
  if (masked_) {
    for (size_t i = 0; i < text_.size(); i++) gfx.print('*');
  } else {
    gfx.print(text_.c_str());
  }
  gfx.print('_');  // cursor
  gfx.setTextColor(theme::kMuted);
  gfx.setCursor(x, by + 26);
  gfx.print("Enter=OK  Esc=Cancel  Tab=Mask");
}
```

- [ ] **Step 3: Write `src/ui/Dialog.h`**

```cpp
#pragma once
#include <M5GFX.h>

#include <string>

#include "../core/KeyEvent.h"

// Centered overlays drawn on top of whatever the app rendered.
namespace dialog {

// Non-interactive toast (message + optional spinner). Caller manages
// how long it stays up.
void paintToast(M5Canvas& gfx, const std::string& msg, bool spinner);

}  // namespace dialog

// Two-option modal confirm. Left/Right (or Up/Down) switches the option,
// Enter confirms, Esc cancels.
class ConfirmDialog {
 public:
  enum class Result { None, First, Second, Cancel };

  void reset(const std::string& msg, const std::string& first,
             const std::string& second);
  Result handleKey(const KeyEvent& ev);
  void render(M5Canvas& gfx) const;

 private:
  std::string msg_, first_, second_;
  int selected_ = 0;
};
```

- [ ] **Step 4: Write `src/ui/Dialog.cpp`**

```cpp
#include "Dialog.h"

#include "Theme.h"

namespace {
void paintBox(M5Canvas& gfx, int w, int h, int& x, int& y) {
  x = (gfx.width() - w) / 2;
  y = (gfx.height() - h) / 2;
  gfx.fillRect(x, y, w, h, theme::kBarBg);
  gfx.drawRect(x, y, w, h, theme::kAccent);
}
}  // namespace

namespace dialog {

void paintToast(M5Canvas& gfx, const std::string& msg, bool spinner) {
  int x, y;
  paintBox(gfx, 180, 50, x, y);
  gfx.setFont(theme::font());
  gfx.setTextColor(theme::kFg);
  gfx.setTextDatum(middle_center);
  gfx.drawString(msg.c_str(), gfx.width() / 2, y + (spinner ? 18 : 25));
  gfx.setTextDatum(top_left);
  if (spinner) {
    static uint8_t frame = 0;
    frame = (frame + 1) % 8;
    for (int i = 0; i < 8; i++) {
      uint16_t c = i == frame ? theme::kAccent : theme::kMuted;
      gfx.fillCircle(gfx.width() / 2 - 28 + i * 8, y + 38, 2, c);
    }
  }
}

}  // namespace dialog

void ConfirmDialog::reset(const std::string& msg, const std::string& first,
                          const std::string& second) {
  msg_ = msg;
  first_ = first;
  second_ = second;
  selected_ = 0;
}

ConfirmDialog::Result ConfirmDialog::handleKey(const KeyEvent& ev) {
  switch (ev.code) {
    case KeyCode::Left:
    case KeyCode::Right:
    case KeyCode::Up:
    case KeyCode::Down:
      selected_ = 1 - selected_;
      return Result::None;
    case KeyCode::Enter:
      return selected_ == 0 ? Result::First : Result::Second;
    case KeyCode::Esc:
      return Result::Cancel;
    default:
      return Result::None;
  }
}

void ConfirmDialog::render(M5Canvas& gfx) const {
  int x, y;
  paintBox(gfx, 200, 70, x, y);
  gfx.setFont(theme::font());
  gfx.setTextColor(theme::kFg);
  gfx.setTextDatum(middle_center);
  gfx.drawString(msg_.c_str(), gfx.width() / 2, y + 18);
  int cx = gfx.width() / 2;
  const std::string* labels[2] = {&first_, &second_};
  for (int i = 0; i < 2; i++) {
    int bx = cx - 90 + i * 95;
    bool sel = selected_ == i;
    gfx.fillRect(bx, y + 40, 85, 20, sel ? theme::kAccent : theme::kBarBg);
    gfx.drawRect(bx, y + 40, 85, 20, theme::kMuted);
    gfx.setTextColor(sel ? TFT_BLACK : theme::kFg);
    gfx.drawString(labels[i]->c_str(), bx + 42, y + 50);
  }
  gfx.setTextDatum(top_left);
}
```

- [ ] **Step 5: Build (widgets are exercised by WiFiApp in Task 13)**

Run: `pio run -e m5stack-cardputer`
Expected: `SUCCESS`

- [ ] **Step 6: Commit**

```bash
git add src/ui/TextInput.h src/ui/TextInput.cpp src/ui/Dialog.h src/ui/Dialog.cpp
git commit -m "feat: text input and dialog widgets"
```

---

### Task 10: LauncherApp + SysInfoApp

**Files:**
- Create: `src/apps/LauncherApp.h`
- Create: `src/apps/LauncherApp.cpp`
- Create: `src/apps/SysInfoApp.h`
- Create: `src/apps/SysInfoApp.cpp`
- Modify: `src/main.cpp` (delete EchoApp, wire launcher)

- [ ] **Step 1: Write `src/apps/SysInfoApp.h`**

```cpp
#pragma once
#include "../core/App.h"

class SysInfoApp : public App {
 public:
  const char* title() const override { return "System Info"; }
  void update(uint32_t dtMs) override;
  void render(M5Canvas& gfx) override;

 private:
  uint32_t sinceRefresh_ = 0;
};
```

- [ ] **Step 2: Write `src/apps/SysInfoApp.cpp`**

```cpp
#include "SysInfoApp.h"

#include <M5Cardputer.h>
#include <WiFi.h>

#include "../ui/Theme.h"

void SysInfoApp::update(uint32_t dtMs) {
  sinceRefresh_ += dtMs;
  if (sinceRefresh_ >= 1000) {  // refresh once per second
    sinceRefresh_ = 0;
    requestRedraw();
  }
}

void SysInfoApp::render(M5Canvas& gfx) {
  gfx.setFont(theme::font());
  gfx.setTextColor(theme::kFg);
  int y = theme::kStatusBarH + 8;
  auto line = [&](const char* k, const String& v) {
    gfx.setCursor(theme::kPadX, y);
    gfx.setTextColor(theme::kMuted);
    gfx.print(k);
    gfx.setTextColor(theme::kFg);
    gfx.print(v);
    y += 16;
  };
  bool up = WiFi.status() == WL_CONNECTED;
  line("WiFi:    ", up ? WiFi.SSID() : String("not connected"));
  line("IP:      ", up ? WiFi.localIP().toString() : String("-"));
  line("RSSI:    ", up ? String(WiFi.RSSI()) + " dBm" : String("-"));
  line("Heap:    ", String(ESP.getFreeHeap() / 1024) + " KB free");
  line("Battery: ", String(M5Cardputer.Power.getBatteryLevel()) + " %");
  line("Uptime:  ", String(millis() / 1000) + " s");
}
```

- [ ] **Step 3: Write `src/apps/LauncherApp.h`**

```cpp
#pragma once
#include <vector>

#include "../core/App.h"
#include "../ui/MenuList.h"

// Root menu. Other apps register once at startup via addEntry().
class LauncherApp : public App {
 public:
  void addEntry(const char* label, App* app);
  const char* title() const override { return "CardOS"; }
  void onEnter() override;
  bool handleKey(const KeyEvent& ev) override;
  void render(M5Canvas& gfx) override;

 private:
  void rebuild();
  std::vector<std::pair<const char*, App*>> entries_;
  MenuList menu_;
};
```

- [ ] **Step 4: Write `src/apps/LauncherApp.cpp`**

```cpp
#include "LauncherApp.h"

#include "../core/AppManager.h"
#include "../ui/Theme.h"

void LauncherApp::addEntry(const char* label, App* app) {
  entries_.push_back({label, app});
}

void LauncherApp::onEnter() { rebuild(); }

void LauncherApp::rebuild() {
  std::vector<MenuItem> items;
  for (auto& e : entries_) items.push_back({e.first, "", 0});
  menu_.setItems(std::move(items));
  requestRedraw();
}

bool LauncherApp::handleKey(const KeyEvent& ev) {
  if (menu_.handleKey(ev)) { requestRedraw(); return true; }
  if (ev.code == KeyCode::Enter && !entries_.empty()) {
    mgr_->push(entries_[menu_.selected()].second);
    return true;
  }
  return ev.code == KeyCode::Esc;  // swallow Esc at the root
}

void LauncherApp::render(M5Canvas& gfx) {
  menu_.render(gfx, 0, theme::kStatusBarH, gfx.width(),
               gfx.height() - theme::kStatusBarH);
}
```

- [ ] **Step 5: Rewrite `src/main.cpp` — delete EchoApp, wire the launcher**

```cpp
#include <M5Cardputer.h>

#include "apps/LauncherApp.h"
#include "apps/SysInfoApp.h"
#include "core/AppManager.h"
#include "core/InputRouter.h"
#include "ui/StatusBar.h"

namespace {
AppManager apps;
InputRouter input;
LauncherApp launcher;
SysInfoApp sysinfo;
uint32_t lastMs = 0;
}  // namespace

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(200);
  Serial.begin(115200);
  Serial.println("[cardos] boot");

  launcher.addEntry("System Info", &sysinfo);
  apps.begin(M5Cardputer.Display, statusbar::paint);
  apps.push(&launcher);
  lastMs = millis();
}

void loop() {
  M5Cardputer.update();
  uint32_t now = millis();
  for (const KeyEvent& ev : input.poll()) apps.dispatch(ev);
  apps.update(now - lastMs);

  static uint32_t lastBatteryMs = 0;
  if (now - lastBatteryMs > 5000) {
    lastBatteryMs = now;
    statusbar::setBattery(M5Cardputer.Power.getBatteryLevel());
  }
  if (statusbar::changedSinceLastPaint()) apps.requestRedraw();

  apps.render();
  lastMs = now;
  delay(5);
}
```

- [ ] **Step 6: Build, flash, verify**

Run: `pio run -e m5stack-cardputer -t upload`
Expected: launcher shows "System Info"; Enter opens the info page (IP "-", heap, battery, uptime counting); Esc (backtick or G0) returns to the launcher; Esc at the launcher does nothing.

- [ ] **Step 7: Commit**

```bash
git add src/apps src/main.cpp
git commit -m "feat: launcher and system info apps"
```

---

### Task 11: NVS Storage Backend

**Files:**
- Create: `src/services/NvsStorage.h`
- Create: `src/services/NvsStorage.cpp`

- [ ] **Step 1: Write `src/services/NvsStorage.h`**

```cpp
#pragma once
#include "WiFiStore.h"

// StorageBackend over ESP32 NVS (Preferences). Namespace "cardos.wifi",
// key "networks" (a JSON array string).
class NvsStorage : public StorageBackend {
 public:
  std::string load() override;
  void save(const std::string& data) override;
};
```

- [ ] **Step 2: Write `src/services/NvsStorage.cpp`**

```cpp
#include "NvsStorage.h"

#include <Preferences.h>

namespace {
constexpr const char* kNamespace = "cardos.wifi";  // <= 15 chars (NVS limit)
constexpr const char* kKey = "networks";
}  // namespace

std::string NvsStorage::load() {
  Preferences p;
  p.begin(kNamespace, /*readOnly=*/true);
  String s = p.getString(kKey, "");
  p.end();
  return std::string(s.c_str());
}

void NvsStorage::save(const std::string& data) {
  Preferences p;
  p.begin(kNamespace, /*readOnly=*/false);
  p.putString(kKey, data.c_str());
  p.end();
}
```

- [ ] **Step 3: Build**

Run: `pio run -e m5stack-cardputer`
Expected: `SUCCESS`

- [ ] **Step 4: Commit**

```bash
git add src/services/NvsStorage.h src/services/NvsStorage.cpp
git commit -m "feat: NVS storage backend for WiFi credentials"
```

---

### Task 12: WiFiService State Machine

**Files:**
- Create: `src/services/WiFiService.h`
- Create: `src/services/WiFiService.cpp`

- [ ] **Step 1: Write `src/services/WiFiService.h`**

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "WiFiStore.h"

// Async WiFi state machine over the Arduino WiFi library. All calls are
// non-blocking; the main loop drives progress via tick(). UI apps read
// state()/scanResults() and render accordingly.
enum class WifiState : uint8_t { Idle, Scanning, Connecting, Connected, Failed };
enum class WifiError : uint8_t { None, AuthFail, NoApFound, Timeout, Other };

struct ScanResult {
  std::string ssid;
  int32_t rssi = 0;
  bool secured = false;
  bool saved = false;
};

class WiFiService {
 public:
  static constexpr uint32_t kConnectTimeoutMs = 15000;

  void begin(WiFiStore* store);            // registers WiFi event handlers
  void tick(uint32_t nowMs);               // drive timeouts + auto-connect

  void startScan();
  bool scanFinished();                     // true once after a scan completes
  const std::vector<ScanResult>& scanResults() const { return results_; }

  // save=true persists the credential on success (manual connects).
  void connect(const std::string& ssid, const std::string& password, bool save);
  void disconnect();
  void autoConnect();                      // boot flow: scan -> best saved

  WifiState state() const { return state_; }
  WifiError lastError() const { return lastError_; }
  std::string currentSsid() const;
  std::string ip() const;
  int rssi() const;
  bool busy() const {                      // suppress deep sleep while true
    return state_ == WifiState::Scanning || state_ == WifiState::Connecting;
  }

 private:
  void onScanDone();
  void tryNextCandidate();
  void onGotIp();
  void onDisconnected(uint8_t reason);

  WiFiStore* store_ = nullptr;
  WifiState state_ = WifiState::Idle;
  WifiError lastError_ = WifiError::None;
  std::vector<ScanResult> results_;
  bool scanJustFinished_ = false;

  std::string pendingSsid_, pendingPw_;
  bool pendingSave_ = false;
  uint32_t connectStartMs_ = 0;
  uint32_t nowMs_ = 0;

  bool autoConnecting_ = false;
  std::vector<std::pair<std::string, std::string>> candidates_;  // ssid, pw
  size_t candidateIdx_ = 0;
};
```

- [ ] **Step 2: Write `src/services/WiFiService.cpp`**

```cpp
#include "WiFiService.h"

#include <Arduino.h>
#include <WiFi.h>

#include <algorithm>

void WiFiService::begin(WiFiStore* store) {
  store_ = store;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);  // CardOS owns the retry policy

  WiFi.onEvent([this](WiFiEvent_t, WiFiEventInfo_t) { onGotIp(); },
               ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(
      [this](WiFiEvent_t, WiFiEventInfo_t info) {
        onDisconnected(info.wifi_sta_disconnected.reason);
      },
      ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
}

void WiFiService::tick(uint32_t nowMs) {
  nowMs_ = nowMs;
  if (state_ == WifiState::Scanning) {
    int16_t n = WiFi.scanComplete();
    if (n >= 0) onScanDone();
  }
  if (state_ == WifiState::Connecting &&
      nowMs - connectStartMs_ >= kConnectTimeoutMs) {
    Serial.printf("[wifi] connect timeout: %s\n", pendingSsid_.c_str());
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    lastError_ = WifiError::Timeout;
    state_ = WifiState::Failed;
    if (autoConnecting_) tryNextCandidate();
  }
}

void WiFiService::startScan() {
  if (busy()) return;
  Serial.println("[wifi] scan start");
  results_.clear();
  state_ = WifiState::Scanning;
  WiFi.scanNetworks(/*async=*/true);
}

bool WiFiService::scanFinished() {
  bool f = scanJustFinished_;
  scanJustFinished_ = false;
  return f;
}

void WiFiService::onScanDone() {
  int16_t n = WiFi.scanComplete();
  results_.clear();
  for (int16_t i = 0; i < n; i++) {
    if (WiFi.SSID(i).isEmpty()) continue;  // skip hidden SSIDs
    ScanResult r;
    r.ssid = WiFi.SSID(i).c_str();
    r.rssi = WiFi.RSSI(i);
    r.secured = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    r.saved = store_ && store_->find(r.ssid) != nullptr;
    // Dedup: keep the strongest entry per SSID (APs with multiple bands).
    auto dup = std::find_if(results_.begin(), results_.end(),
                            [&](const ScanResult& e) { return e.ssid == r.ssid; });
    if (dup != results_.end()) {
      if (r.rssi > dup->rssi) *dup = r;
      continue;
    }
    results_.push_back(std::move(r));
  }
  std::sort(results_.begin(), results_.end(),
            [](const ScanResult& a, const ScanResult& b) {
              return a.rssi > b.rssi;
            });
  WiFi.scanDelete();
  Serial.printf("[wifi] scan done: %d APs\n", (int)results_.size());
  state_ = WiFi.status() == WL_CONNECTED ? WifiState::Connected : WifiState::Idle;
  scanJustFinished_ = true;

  if (autoConnecting_) {
    // Candidates = saved ∩ visible, in scan order (strongest RSSI first).
    candidates_.clear();
    candidateIdx_ = 0;
    for (const auto& r : results_) {
      if (const WifiNetwork* n = store_->find(r.ssid))
        candidates_.push_back({n->ssid, n->password});
    }
    Serial.printf("[wifi] auto-connect: %d candidates\n", (int)candidates_.size());
    tryNextCandidate();
  }
}

void WiFiService::tryNextCandidate() {
  if (candidateIdx_ >= candidates_.size()) {
    if (!candidates_.empty() || autoConnecting_)
      Serial.println("[wifi] auto-connect: exhausted");
    autoConnecting_ = false;
    if (state_ != WifiState::Connected && state_ != WifiState::Failed)
      state_ = WifiState::Idle;
    return;
  }
  auto& c = candidates_[candidateIdx_++];
  connect(c.first, c.second, /*save=*/false);
}

void WiFiService::connect(const std::string& ssid, const std::string& password,
                          bool save) {
  Serial.printf("[wifi] connecting: %s\n", ssid.c_str());
  pendingSsid_ = ssid;
  pendingPw_ = password;
  pendingSave_ = save;
  lastError_ = WifiError::None;
  state_ = WifiState::Connecting;
  connectStartMs_ = nowMs_;
  WiFi.begin(ssid.c_str(), password.empty() ? nullptr : password.c_str());
}

void WiFiService::disconnect() {
  autoConnecting_ = false;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  state_ = WifiState::Idle;
  Serial.println("[wifi] disconnected by user");
}

void WiFiService::autoConnect() {
  if (!store_ || store_->networks().empty()) return;
  autoConnecting_ = true;
  startScan();
}

void WiFiService::onGotIp() {
  Serial.printf("[wifi] connected: %s ip=%s\n", pendingSsid_.c_str(),
                WiFi.localIP().toString().c_str());
  state_ = WifiState::Connected;
  autoConnecting_ = false;
  if (store_) {
    if (pendingSave_) store_->upsert(pendingSsid_, pendingPw_);
    store_->touch(pendingSsid_, nowMs_ / 1000);  // monotonic-enough ordering
  }
}

void WiFiService::onDisconnected(uint8_t reason) {
  if (state_ != WifiState::Connecting) {
    // Lost an established connection; reflect reality, stay passive.
    if (state_ == WifiState::Connected) state_ = WifiState::Idle;
    return;
  }
  // 202 = AUTH_FAIL, 201 = NO_AP_FOUND (esp_wifi reason codes)
  Serial.printf("[wifi] connect failed: %s reason=%d\n", pendingSsid_.c_str(),
                reason);
  lastError_ = reason == 202 ? WifiError::AuthFail
             : reason == 201 ? WifiError::NoApFound
                             : WifiError::Other;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  state_ = WifiState::Failed;
  if (autoConnecting_) tryNextCandidate();
}

std::string WiFiService::currentSsid() const {
  return WiFi.status() == WL_CONNECTED ? std::string(WiFi.SSID().c_str()) : "";
}

std::string WiFiService::ip() const {
  return WiFi.status() == WL_CONNECTED
             ? std::string(WiFi.localIP().toString().c_str())
             : "";
}

int WiFiService::rssi() const {
  return WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
}
```

- [ ] **Step 3: Build**

Run: `pio run -e m5stack-cardputer`
Expected: `SUCCESS`

- [ ] **Step 4: Run native tests (must be unaffected)**

Run: `pio test -e native`
Expected: `23 Tests 0 Failures 0 Ignored: PASSED`

- [ ] **Step 5: Commit**

```bash
git add src/services/WiFiService.h src/services/WiFiService.cpp
git commit -m "feat: async WiFi service state machine"
```

---

### Task 13: WiFiApp UI

One `App` with internal pages (Home / Scan / Password / Saved) — pages share too much state to justify separate stack scenes.

**Files:**
- Create: `src/apps/WiFiApp.h`
- Create: `src/apps/WiFiApp.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Write `src/apps/WiFiApp.h`**

```cpp
#pragma once
#include <string>

#include "../core/App.h"
#include "../services/WiFiService.h"
#include "../ui/Dialog.h"
#include "../ui/MenuList.h"
#include "../ui/TextInput.h"

class WiFiApp : public App {
 public:
  explicit WiFiApp(WiFiService& wifi, WiFiStore& store)
      : wifi_(wifi), store_(store) {}

  const char* title() const override { return "WiFi"; }
  void onEnter() override;
  bool handleKey(const KeyEvent& ev) override;
  void update(uint32_t dtMs) override;
  void render(M5Canvas& gfx) override;

 private:
  enum class Page { Home, Scan, Password, Saved };
  enum class Modal { None, Toast, Confirm };

  void showHome();
  void showScanResults();
  void showSaved();
  void startConnect(const std::string& ssid, const std::string& pw, bool save);
  void toast(const std::string& msg, bool spinner, uint32_t autoHideMs);
  static std::string bars(int rssi);

  WiFiService& wifi_;
  WiFiStore& store_;
  Page page_ = Page::Home;
  Modal modal_ = Modal::None;
  MenuList menu_;
  TextInput input_;
  ConfirmDialog confirm_;

  std::string toastMsg_;
  bool toastSpinner_ = false;
  uint32_t toastTimerMs_ = 0;  // >0: auto-hide countdown

  std::string targetSsid_;     // AP being connected / acted upon
  WifiState lastSeenState_ = WifiState::Idle;
  uint32_t sinceAnim_ = 0;
};
```

- [ ] **Step 2: Write `src/apps/WiFiApp.cpp`**

```cpp
#include "WiFiApp.h"

#include "../ui/Theme.h"

namespace {
const char* kScanLabel = "Scan networks";
const char* kSavedLabel = "Saved networks";
const char* kDisconnectLabel = "Disconnect";
}  // namespace

std::string WiFiApp::bars(int rssi) {
  if (rssi >= -55) return "|||";
  if (rssi >= -70) return "||";
  return "|";
}

void WiFiApp::onEnter() {
  page_ = Page::Home;
  modal_ = Modal::None;
  showHome();
}

void WiFiApp::showHome() {
  page_ = Page::Home;
  std::vector<MenuItem> items;
  if (wifi_.state() == WifiState::Connected) {
    items.push_back({"Connected: " + wifi_.currentSsid(),
                     wifi_.ip(), theme::kOk});
  } else {
    items.push_back({"Not connected", "", theme::kMuted});
  }
  items.push_back({kScanLabel, "", 0});
  items.push_back({kSavedLabel, std::to_string(store_.networks().size()),
                   theme::kMuted});
  if (wifi_.state() == WifiState::Connected)
    items.push_back({kDisconnectLabel, "", 0});
  menu_.setItems(std::move(items));
  menu_.setSelected(1);  // first actionable row
  requestRedraw();
}

void WiFiApp::showScanResults() {
  page_ = Page::Scan;
  std::vector<MenuItem> items;
  for (const auto& r : wifi_.scanResults()) {
    std::string note = bars(r.rssi);
    if (r.secured) note += " *";
    if (r.saved) note += " v";
    items.push_back({r.ssid, note, theme::kMuted});
  }
  if (items.empty()) items.push_back({"(no networks found)", "", theme::kMuted});
  menu_.setItems(std::move(items));
  requestRedraw();
}

void WiFiApp::showSaved() {
  page_ = Page::Saved;
  std::vector<MenuItem> items;
  for (const auto& n : store_.networks()) items.push_back({n.ssid, "", 0});
  if (items.empty()) items.push_back({"(none saved)", "", theme::kMuted});
  menu_.setItems(std::move(items));
  requestRedraw();
}

void WiFiApp::toast(const std::string& msg, bool spinner, uint32_t autoHideMs) {
  modal_ = Modal::Toast;
  toastMsg_ = msg;
  toastSpinner_ = spinner;
  toastTimerMs_ = autoHideMs;
  requestRedraw();
}

void WiFiApp::startConnect(const std::string& ssid, const std::string& pw,
                           bool save) {
  targetSsid_ = ssid;
  wifi_.connect(ssid, pw, save);
  toast("Connecting: " + ssid, /*spinner=*/true, /*autoHideMs=*/0);
}

bool WiFiApp::handleKey(const KeyEvent& ev) {
  // Modal layers swallow input first.
  if (modal_ == Modal::Toast) {
    if (ev.code == KeyCode::Esc && toastSpinner_) {
      wifi_.disconnect();  // cancel in-flight connect
      modal_ = Modal::None;
      showHome();
    }
    return true;  // toast blocks everything else
  }
  if (modal_ == Modal::Confirm) {
    auto r = confirm_.handleKey(ev);
    if (r == ConfirmDialog::Result::First) {        // [Connect]
      modal_ = Modal::None;
      const WifiNetwork* n = store_.find(targetSsid_);
      if (n) startConnect(n->ssid, n->password, false);
    } else if (r == ConfirmDialog::Result::Second) { // [Delete]
      store_.remove(targetSsid_);
      modal_ = Modal::None;
      showSaved();
    } else if (r == ConfirmDialog::Result::Cancel) {
      modal_ = Modal::None;
    }
    requestRedraw();
    return true;
  }

  if (page_ == Page::Password) {
    auto r = input_.handleKey(ev);
    if (r == TextInput::Result::Submit && !input_.text().empty()) {
      startConnect(targetSsid_, input_.text(), /*save=*/true);
    } else if (r == TextInput::Result::Cancel) {
      showScanResults();
    }
    requestRedraw();
    return true;
  }

  if (menu_.handleKey(ev)) { requestRedraw(); return true; }

  if (ev.code == KeyCode::Enter) {
    if (page_ == Page::Home) {
      int sel = menu_.selected();
      bool connected = wifi_.state() == WifiState::Connected;
      if (sel == 1) {  // Scan
        wifi_.startScan();
        toast("Scanning...", true, 0);
      } else if (sel == 2) {  // Saved
        showSaved();
      } else if (sel == 3 && connected) {  // Disconnect
        wifi_.disconnect();
        showHome();
      }
    } else if (page_ == Page::Scan && !wifi_.scanResults().empty()) {
      const ScanResult& r = wifi_.scanResults()[menu_.selected()];
      targetSsid_ = r.ssid;
      if (const WifiNetwork* n = store_.find(r.ssid)) {
        startConnect(n->ssid, n->password, false);
      } else if (!r.secured) {
        startConnect(r.ssid, "", true);
      } else {
        page_ = Page::Password;
        input_.reset("Password for " + r.ssid + ":");
        requestRedraw();
      }
    } else if (page_ == Page::Saved && !store_.networks().empty()) {
      targetSsid_ = store_.networks()[menu_.selected()].ssid;
      confirm_.reset(targetSsid_, "Connect", "Delete");
      modal_ = Modal::Confirm;
      requestRedraw();
    }
    return true;
  }

  if (ev.code == KeyCode::Esc && page_ != Page::Home) {
    showHome();  // back to home instead of popping the app
    return true;
  }
  return false;  // Esc on Home -> AppManager pops back to launcher
}

void WiFiApp::update(uint32_t dtMs) {
  WifiState s = wifi_.state();

  if (wifi_.scanFinished() && modal_ == Modal::Toast && toastSpinner_) {
    modal_ = Modal::None;
    showScanResults();
  }

  if (s != lastSeenState_) {
    lastSeenState_ = s;
    if (s == WifiState::Connected && modal_ == Modal::Toast) {
      toast("Connected: " + wifi_.currentSsid(), false, 1500);
    } else if (s == WifiState::Failed && modal_ == Modal::Toast) {
      WifiError e = wifi_.lastError();
      toast(e == WifiError::AuthFail   ? "Wrong password"
            : e == WifiError::NoApFound ? "Network not found"
            : e == WifiError::Timeout   ? "Connection timed out"
                                        : "Connection failed",
            false, 2000);
    }
    requestRedraw();
  }

  if (modal_ == Modal::Toast) {
    if (toastSpinner_) {
      sinceAnim_ += dtMs;
      if (sinceAnim_ > 120) { sinceAnim_ = 0; requestRedraw(); }  // spinner
    } else if (toastTimerMs_ > 0) {
      toastTimerMs_ = toastTimerMs_ > dtMs ? toastTimerMs_ - dtMs : 0;
      if (toastTimerMs_ == 0) {
        modal_ = Modal::None;
        bool wasAuthFail = wifi_.lastError() == WifiError::AuthFail;
        if (lastSeenState_ == WifiState::Failed && wasAuthFail &&
            page_ != Page::Saved) {
          page_ = Page::Password;  // wrong password -> retry input
          input_.reset("Password for " + targetSsid_ + ":");
        } else {
          showHome();
        }
        requestRedraw();
      }
    }
  }
}

void WiFiApp::render(M5Canvas& gfx) {
  int top = theme::kStatusBarH;
  int h = gfx.height() - top;
  if (page_ == Page::Password) {
    input_.render(gfx, theme::kPadX, top + 12, gfx.width() - 2 * theme::kPadX);
  } else {
    menu_.render(gfx, 0, top, gfx.width(), h);
  }
  if (modal_ == Modal::Toast) dialog::paintToast(gfx, toastMsg_, toastSpinner_);
  if (modal_ == Modal::Confirm) confirm_.render(gfx);
}
```

- [ ] **Step 3: Wire into `src/main.cpp`**

Add includes:

```cpp
#include "apps/WiFiApp.h"
#include "services/NvsStorage.h"
#include "services/WiFiService.h"
#include "ui/StatusBar.h"
```

Add globals (after `SysInfoApp sysinfo;`):

```cpp
NvsStorage nvs;
WiFiStore wifiStore(nvs);
WiFiService wifiService;
WiFiApp wifiApp(wifiService, wifiStore);
```

In `setup()`, before `launcher.addEntry("System Info", &sysinfo);` add:

```cpp
  wifiStore.load();
  wifiService.begin(&wifiStore);
  launcher.addEntry("WiFi Settings", &wifiApp);
```

In `loop()`, after `M5Cardputer.update();` and `uint32_t now = millis();` add:

```cpp
  wifiService.tick(now);
```

And extend the 5-second status block to also update the WiFi icon:

```cpp
  static uint32_t lastStatusMs = 0;
  if (now - lastStatusMs > 2000) {
    lastStatusMs = now;
    statusbar::setBattery(M5Cardputer.Power.getBatteryLevel());
    if (wifiService.state() == WifiState::Connected) {
      int r = wifiService.rssi();
      statusbar::setWifi(r >= -55 ? statusbar::WifiIcon::Bars3
                         : r >= -70 ? statusbar::WifiIcon::Bars2
                                    : statusbar::WifiIcon::Bars1);
    } else if (wifiService.busy()) {
      statusbar::setWifi(statusbar::WifiIcon::Connecting);
    } else {
      statusbar::setWifi(statusbar::WifiIcon::Off);
    }
  }
```

(Replace the previous `lastBatteryMs` block with this one.)

- [ ] **Step 4: Build, flash, full manual test**

Run: `pio run -e m5stack-cardputer -t upload && pio device monitor`
Expected, on device:
1. Launcher → "WiFi Settings" → home shows "Not connected";
2. "Scan networks" → spinner → AP list sorted by signal with `|||`/`*`/`v` notes;
3. Select a secured AP → password prompt → typing works, Tab masks, Backspace deletes;
4. Enter → "Connecting:" spinner → "Connected:" toast → home shows SSID + IP; status bar shows WiFi bars; serial logs `[wifi] connected:`;
5. Wrong password → "Wrong password" toast → returns to password input;
6. "Saved networks" → entry listed → [Connect] works, [Delete] removes it;
7. "Disconnect" appears while connected and works.

- [ ] **Step 5: Commit**

```bash
git add src/apps/WiFiApp.h src/apps/WiFiApp.cpp src/main.cpp
git commit -m "feat: WiFi management app"
```

---

### Task 14: PowerManager + Boot Auto-Connect

**Files:**
- Create: `src/core/PowerManager.h`
- Create: `src/core/PowerManager.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Write `src/core/PowerManager.h`**

```cpp
#pragma once
#include <cstdint>

#include "IdlePolicy.h"

// Device-side power control driven by IdlePolicy: brightness tiers, the
// pre-sleep notice, and the deep-sleep sequence (G0/EXT0 wake).
class PowerManager {
 public:
  static constexpr uint32_t kDimAfterMs = 60000;
  static constexpr uint32_t kSleepAfterMs = 300000;
  static constexpr uint8_t kBrightActive = 200;
  static constexpr uint8_t kBrightDimmed = 40;

  void begin();
  // Forward every key press here BEFORE dispatching to apps. Returns true
  // if the key must be swallowed (it only woke the dimmed screen).
  bool onInput(uint32_t nowMs);
  void keepAwake(bool on) { policy_.keepAwake(on); }
  // Call once per loop. May not return (deep sleep).
  void tick(uint32_t nowMs);
  static bool wokeFromDeepSleep();

 private:
  void showSleepNoticeAndSleep();  // 3s notice, any key cancels
  IdlePolicy policy_{kDimAfterMs, kSleepAfterMs};
  IdleState applied_ = IdleState::Active;
};
```

- [ ] **Step 2: Write `src/core/PowerManager.cpp`**

```cpp
#include "PowerManager.h"

#include <M5Cardputer.h>
#include <WiFi.h>
#include <esp_sleep.h>

void PowerManager::begin() {
  M5Cardputer.Display.setBrightness(kBrightActive);
}

bool PowerManager::onInput(uint32_t nowMs) {
  bool swallow = policy_.onInput(nowMs);
  if (applied_ != IdleState::Active) {
    M5Cardputer.Display.setBrightness(kBrightActive);
    applied_ = IdleState::Active;
  }
  return swallow;
}

void PowerManager::tick(uint32_t nowMs) {
  IdleState s = policy_.state(nowMs);
  if (s == applied_) return;
  applied_ = s;
  switch (s) {
    case IdleState::Active:
      M5Cardputer.Display.setBrightness(kBrightActive);
      break;
    case IdleState::Dimmed:
      M5Cardputer.Display.setBrightness(kBrightDimmed);
      break;
    case IdleState::SleepPending:
      showSleepNoticeAndSleep();
      // Only reached if the user cancelled within the notice window.
      policy_.onInput(nowMs);
      applied_ = IdleState::Active;
      M5Cardputer.Display.setBrightness(kBrightActive);
      break;
  }
}

void PowerManager::showSleepNoticeAndSleep() {
  auto& d = M5Cardputer.Display;
  d.setBrightness(kBrightActive);
  d.fillScreen(TFT_BLACK);
  d.setFont(&fonts::efontCN_16);
  d.setTextColor(TFT_WHITE);
  d.setTextDatum(middle_center);
  d.drawString("Sleeping soon...", d.width() / 2, d.height() / 2 - 12);
  d.drawString("Press G0 to wake", d.width() / 2, d.height() / 2 + 12);
  d.setTextDatum(top_left);

  uint32_t start = millis();
  while (millis() - start < 3000) {  // any key cancels
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed())
      return;
    if (M5Cardputer.BtnA.wasPressed()) return;
    delay(20);
  }

  Serial.println("[power] entering deep sleep");
  Serial.flush();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  d.sleep();           // display controller off + backlight off
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);  // G0 pressed = LOW
  esp_deep_sleep_start();
}

bool PowerManager::wokeFromDeepSleep() {
  return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0;
}
```

- [ ] **Step 3: Wire into `src/main.cpp`**

Add include and global:

```cpp
#include "core/PowerManager.h"
```

```cpp
PowerManager power;
```

In `setup()`, after `Serial.println("[cardos] boot");` add:

```cpp
  power.begin();
  if (PowerManager::wokeFromDeepSleep())
    Serial.println("[cardos] woke from deep sleep");
  wifiService.autoConnect();  // boot auto-connect (no-op when none saved)
```

Note: `wifiService.autoConnect()` must come AFTER `wifiStore.load()` and `wifiService.begin(&wifiStore)` — keep those lines above it.

In `loop()`, replace the input dispatch line with:

```cpp
  for (const KeyEvent& ev : input.poll()) {
    if (!power.onInput(now)) apps.dispatch(ev);
  }
```

And after `wifiService.tick(now);` add:

```cpp
  power.keepAwake(wifiService.busy());
  power.tick(now);
```

- [ ] **Step 4: Build, flash, verify power behavior**

Run: `pio run -e m5stack-cardputer -t upload && pio device monitor`
Expected:
1. After 60s without keys the screen dims; the next key press only restores brightness (menu selection does not move);
2. After 5min idle the sleep notice appears; pressing a key within 3s cancels it; otherwise serial logs `[power] entering deep sleep` and the screen turns off;
3. Pressing G0 reboots; serial logs `woke from deep sleep`; WiFi reconnects automatically (status bar progresses Off → Connecting → bars).

Tip: for a faster test cycle, temporarily set `kDimAfterMs = 10000` / `kSleepAfterMs = 20000`, verify, then restore the real values before committing.

- [ ] **Step 5: Verify boot auto-connect**

Power-cycle the device with a saved network in range.
Expected: launcher appears immediately (no blocking), status bar animates Connecting, then shows bars; serial logs `[wifi] auto-connect: N candidates` then `[wifi] connected:`.

- [ ] **Step 6: Run native tests**

Run: `pio test -e native`
Expected: `23 Tests 0 Failures 0 Ignored: PASSED`

- [ ] **Step 7: Commit**

```bash
git add src/core/PowerManager.h src/core/PowerManager.cpp src/main.cpp
git commit -m "feat: tiered power management with deep sleep and G0 wake"
```

---

### Task 15: Acceptance Run + Docs

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Run the full acceptance checklist from the spec (section 7) on hardware**

1. Boots into the launcher with a working status bar;
2. Scans nearby APs; Chinese SSIDs render correctly;
3. Select AP, enter password, connect successfully; status bar shows connected + IP on the WiFi home page;
4. Wrong password produces "Wrong password" and allows retry;
5. After reboot, auto-connects to a saved network;
6. With multiple saved networks, connects to the strongest RSSI;
7. Saved networks can be viewed and deleted;
8. Screen dims after 60s idle; any key restores it;
9. After 5min idle, shows the sleep notice then deep-sleeps;
10. G0 wakes the device and WiFi reconnects automatically.

Record any failures, fix them (using superpowers:systematic-debugging), and re-run the failed item before proceeding.

- [ ] **Step 2: Run the native suite one final time**

Run: `pio test -e native`
Expected: `23 Tests 0 Failures 0 Ignored: PASSED`

- [ ] **Step 3: Update `README.md` status section**

Replace:

```markdown
## Status

Design phase. See the [design document](docs/superpowers/specs/2026-06-05-cardos-design.md).
```

with:

```markdown
## Status

MVP working: launcher, WiFi management (scan/connect/remember/auto-connect),
idle dimming and deep sleep with G0 wake.
Docs: [design](docs/superpowers/specs/2026-06-05-cardos-design.md) ·
[implementation plan](docs/superpowers/plans/2026-06-05-cardos-mvp.md)
```

- [ ] **Step 4: Commit and push**

```bash
git add README.md
git commit -m "docs: mark MVP status in README"
git push origin main
```

---

## Plan Self-Review Notes

- **Spec coverage:** launcher+app framework (T6, T10), widgets (T8, T9), CJK font (T7 theme + T8 demo), WiFi scan/connect/password/saved/delete/disconnect (T12, T13), credential persistence + eviction (T5, T11), boot auto-connect strongest-RSSI (T12 `autoConnect`, T14 wiring), dim @60s / sleep @5min / notice / G0 wake / wake-reason (T4, T14), battery indicator (T7), status bar WiFi state (T13 step 3), native tests (T2–T5), acceptance checklist (T15). Out-of-scope items from spec section 8 are not implemented anywhere — correct.
- **Known hardware-verification points** (flagged in spec section 9): M5Cardputer `keysState()` field names and `;`/`.` mapping are verified live in Task 6 step 8; if the library differs, fix `InputRouter.cpp` only — `mapKey()` and everything above it stay stable.
- **Type consistency check:** `WiFiStore` API (`upsert/remove/touch/find/networks/load`) matches usage in T12/T13; `statusbar::` free functions match T13's calls; `IdlePolicy.onInput` swallow semantics match `PowerManager.onInput` usage in T14.






