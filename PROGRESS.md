# CardOS Progress

Last updated: 2026-06-06

## Status at a Glance

| Milestone | State |
|---|---|
| MVP (launcher + WiFi + power management) | ✅ Merged to `main`, running on hardware |
| HTTP Demo app | ✅ Merged to `main`, verified on hardware |
| App SDK | 🚧 On branch `sdk` — code complete (tasks 1–9); pending on-device acceptance + merge (task 10) |

## Done

### MVP (2026-06-05, merged)

- Launcher + scene-stack app framework (`core/`), double-buffered rendering, status bar
- UI widgets: MenuList (scroll/CJK/clipped labels), TextInput (masking), ConfirmDialog, Toast
- WiFi management: async scan/connect state machine, password entry, multi-credential NVS store (8 max, oldest-eviction), boot auto-connect to strongest saved network
- Power management: dim at 60s idle, deep sleep at 5min with 3s cancellable notice, G0/EXT0 wake
- HTTP Demo app (plain HTTP GET smoke tests)
- 25 native unit tests; per-task spec + quality reviews (7 real bugs found and fixed)

### Hardware acceptance: 7/10 verified

Verified: boot/launcher, scan (10–16 APs), connect+password (got IP), HTTP 200 × 2 endpoints, status bar, framework extensibility, native tests.
Remaining (user-side, battery-powered ok): wrong-password retry flow, saved-network view/delete, reboot auto-connect, 60s dim, 5min deep sleep + G0 wake.

## In Progress: App SDK (branch `sdk`)

Spec: `docs/superpowers/specs/2026-06-06-cardos-sdk-design.md`
Plan: `docs/superpowers/plans/2026-06-06-cardos-sdk.md`

| # | Task | State |
|---|---|---|
| 1 | KeyAction + pure KeyTracker (press/long-press/release), native tests | ✅ done (30/30 tests) |
| 2 | InputRouter rewrite + press guards in widgets/apps | ✅ done (device build + 30/30) |
| 3 | Filesystem: `cardos::fs` over LittleFS `/flash` + SD `/sd` | ✅ done (33/33) |
| 4 | WavWriter (pure, native-tested) | ✅ done (35/35) |
| 5 | Audio: `cardos::audio` (streaming chunks + record-to-WAV) | ✅ done (build only; mic semantics need device) |
| 6 | HTTP(S): `cardos::http` GET/POST/JSON/multipart upload + HttpDemo refactor | ✅ done (37/37) |
| 7 | Widgets: TextView (UTF-8 wrap), ProgressBar, Label | ✅ done (41/41) |
| 8 | Umbrella header `sdk/CardOS.h` + `docs/sdk.md` | ✅ done |
| 9 | Recorder example app (push-to-talk → WAV → upload with progress) | ✅ done (build only; needs device) |
| 10 | On-device acceptance + README + merge to `main` | 🚧 README done; acceptance + merge pending |

All 41 native tests pass; `pio run -e m5stack-cardputer` builds clean.

### Deferred to hardware (no device connected during build-out)

- Task 2: input feel — no double-trigger on key release.
- Task 5/9: mic ping-pong chunk semantics (`Mic.isRecording()` queue) and
  push-to-talk record → WAV (~32 KB/s) → httpbin upload with progress bar.
- Task 3/9: SD SPI pins (SCK=40/MISO=39/MOSI=14/CS=12); `recDir()` flash/SD switch.
- Task 10 acceptance checklist (see plan) + merge `sdk` → `main` after it passes.

## Upcoming Plan

1. **Finish SDK tasks 2–10** (above). Tasks needing the device for verification: 2 (input feel), 5 (mic chunk semantics — flagged risk), 9/10 (Recorder end-to-end, SD pin check SCK=40/MISO=39/MOSI=14/CS=12).
2. **Close out MVP acceptance** — the 5 remaining user-side checks; reboot auto-connect now testable since a network is saved.
3. **Known small fixes queued**
   - Suppress the harmless first-boot `nvs_open failed: NOT_FOUND` log line
   - Investigate USB drops during WiFi activity (suspected cable/power; recommend a better USB-C cable)
4. **Candidate ideas after SDK (not committed)**
   - Audio playback (speaker) + voice-assistant demo (record → upload → TTS reply)
   - Clock/NTP app; file manager app on `cardos::fs`
   - OTA firmware updates; auto-repeat keys; certificate verification option
