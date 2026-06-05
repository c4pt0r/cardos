# CardOS Design Document

Date: 2026-06-05
Target device: M5Stack Cardputer (ESP32-S3FN8, 240×135 TFT, QWERTY keyboard matrix, G0/BtnA button, 120mAh battery, 8MB flash, no PSRAM)

## 1. Project Goal

A **small, extensible OS framework** for the Cardputer: a launcher + app architecture. The first app is WiFi management; the framework provides clear extension points for future apps (clock, file manager, etc.). Includes simple power management (idle dimming, idle deep sleep).

**Tech stack**: C++ / Arduino framework + M5Unified/M5GFX (via the `m5stack/M5Cardputer` library) + PlatformIO build system.

## 2. Architecture Overview

Single main loop, cooperative scheduling, no extra FreeRTOS tasks.

```
loop():
  M5Cardputer.update()          // poll keyboard matrix and buttons
  InputRouter                   // raw keys → KeyEvent
  PowerManager.tick()           // KeyEvent resets idle timer; timeout → sleep flow
  AppManager.dispatch(event)    // deliver events to the top-of-stack app
  AppManager.update(dt)         // logic frame for the active app
  AppManager.render(canvas)     // redraw only when dirty → pushSprite
```

### App Model — Scene Stack

- `App` base class: `onEnter() / onExit() / handleKey(ev) / update(dt) / render(gfx)` plus `requestRedraw()` to mark dirty.
- `AppManager` owns an app stack: `LauncherApp` sits at the bottom; entering a feature does `push(app)`, ESC does `pop()`. Sub-pages inside an app (e.g. scan list → password input) reuse the same stack mechanism.
- **Services are separate from UI**: `WiFiService` and `PowerManager` are global singleton services; apps are merely their UI front-ends. Adding a new app = implement the `App` interface + one registration line in the launcher.

### Rendering

- M5GFX `M5Canvas` off-screen sprite (240×135×16bit ≈ 64KB, fits in SRAM); each frame is drawn fully into the sprite then pushed once — no flicker.
- The status bar is rendered by the framework (top ~16px): app title on the left; WiFi icon (disconnected / connecting animation / connected + RSSI bars) and battery bars on the right.
- CJK support: M5GFX built-in `efontCN_12/16` fonts, so Chinese SSIDs render correctly.

## 3. UI Widgets and Keyboard Input

Three reusable widgets:

1. **MenuList** — vertical list with scrolling (~6 rows per page), highlighted selection, optional right-side annotation per item (signal bars, 🔒, saved ✓).
2. **TextInput** — single-line input (for passwords): printable characters, Backspace, Enter to submit, ESC to cancel; plaintext display by default, Tab toggles masking.
3. **Toast/Dialog** — centered overlays: connecting spinner, success/failure messages, two-option confirmation dialogs.

Keyboard mapping (translated centrally by `InputRouter`; apps never touch matrix scanning):

| Physical key | Meaning |
|---|---|
| `;` / `.` | Up / Down |
| `,` / `/` | Left / Right (reserved) |
| Enter | Confirm |
| `` ` `` (ESC) | Back / Cancel |
| Fn+key | Reserved for future global shortcuts |
| Other printable keys | Literal input when a TextInput is focused |

## 4. WiFi App and Persistence

### Page Flow

```
Launcher ─ select "WiFi Settings" → WiFi home
  WiFi home (MenuList)
    ├─ Status row: "Connected: <SSID> (<IP>)" / "Not connected"
    ├─ [Scan networks] → spinner → result list (SSID + signal bars + 🔒 + saved ✓)
    │     └─ select AP → saved? connect directly : TextInput for password → connect
    │         └─ Toast: connecting… → success (save & return) / failure (retry or give up)
    ├─ [Saved networks] → list → select → Dialog: [Connect] [Delete]
    └─ [Disconnect] (shown only while connected)
```

### WiFiService State Machine

- `IDLE → SCANNING → CONNECTING → CONNECTED / FAILED`
- Fully asynchronous: `WiFi.scanNetworks(true)` + WiFi event callbacks; the main loop never blocks, and connecting can be cancelled with ESC.
- 15s connect timeout counts as failure; `AUTH_FAIL` (wrong password) and `NO_AP_FOUND` produce distinct error messages.

### Credential Persistence (NVS / Preferences)

- Namespace `cardos.wifi`, an ArduinoJson-serialized array: `[{ssid, password, last_ok_ts}]`.
- Capacity **8 entries**; when full, evict the entry with the oldest successful connection (`last_ok_ts`).
- Passwords stored in plaintext (device-local, no security boundary requirement; no encryption complexity).

### Auto-Connect on Boot

boot → `autoConnect()` → async scan → connect to the strongest-RSSI network in "saved ∩ visible" → on failure, retry remaining known networks in RSSI order → if all fail, stay disconnected (no infinite retry). Runs entirely in the background without blocking the launcher; the status bar reflects progress in real time.

## 5. Power Management

Tiered power saving (PowerManager):

| Idle time | Action |
|---|---|
| 0–60s | Normal brightness (~80%) |
| 60s | Dim to ~20%; any key restores brightness and is **not** forwarded to the app |
| 5min | Deep sleep flow |

Deep sleep flow:

1. Full-screen notice "Sleeping soon — press G0 to wake" for ~3s (any key cancels);
2. `WiFi.disconnect()` + radio off;
3. Backlight off, display controller to sleep;
4. `esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0)` (G0 button wake);
5. `esp_deep_sleep_start()`.

After wake: deep sleep wake is a fresh boot (RAM is not retained) — normal boot flow + WiFi auto-connect. Use `esp_reset_reason()` to distinguish cold boot from wake; skip the boot logo on wake for a faster feel.

Idle timer rules: any KeyEvent resets the timer; apps may call `keepAwake()` to suppress sleep; an in-progress WiFi connection also suppresses sleep.

Battery display: read battery voltage via ADC divider → lookup table → 4-bar icon; trend accuracy only, not precise percentages.

## 6. Project Layout and Dependencies

```
cardos/
├── platformio.ini          # env:m5stack-cardputer (arduino) + env:native (unit tests)
├── src/
│   ├── main.cpp
│   ├── core/    App.h, AppManager, InputRouter, PowerManager
│   ├── ui/      Theme.h, StatusBar, MenuList, TextInput, Dialog
│   ├── services/ WiFiService, WiFiStore   # WiFiStore: credential list logic
│   │                                      #   (serialization/eviction) over a storage
│   │                                      #   interface (device=NVS, tests=in-memory mock)
│   └── apps/    LauncherApp, WiFiApp, SysInfoApp
├── test/                   # PIO native unit tests
└── docs/superpowers/specs/
```

Dependencies: `m5stack/M5Cardputer` (bundles M5Unified/M5GFX), `bblanchon/ArduinoJson`.

Build/flash: `pio run -t upload` (USB CDC); debugging via `pio device monitor` plus a built-in `LOG()` serial macro.

## 7. Testing Strategy

- **Native unit tests** (`pio test -e native`): WiFiStore add/remove/eviction (with the in-memory mock storage backend), InputRouter key mapping, MenuList scroll-window math — all hardware-independent pure logic.
- **On-device acceptance checklist**:
  1. Boots into the launcher with a working status bar;
  2. Scans nearby APs; Chinese SSIDs render correctly;
  3. Select AP, enter password, connect successfully; status bar shows connected + IP;
  4. Wrong password produces a "wrong password" error and allows retry;
  5. After reboot, auto-connects to a saved network;
  6. With multiple saved networks, connects to the strongest RSSI;
  7. Saved networks can be viewed and deleted;
  8. Screen dims after 60s idle; any key restores it;
  9. After 5min idle, shows the sleep notice then enters deep sleep;
  10. G0 wakes the device and WiFi reconnects automatically.

## 8. Out of Scope

OTA updates, Bluetooth, language switching, SD card, sound, encrypted credential storage, any-key deep-sleep wake (keyboard matrix hardware limitation — G0 instead).

## 9. Known Risks

- The device does not currently show up under `/dev/cu.*`; verify the USB connection/driver before flashing;
- ADC-based battery estimation has limited accuracy; the battery icon is indicative only;
- M5Cardputer library keyboard mapping details (the `;`/`.` arrow-key convention) to be verified on hardware.
