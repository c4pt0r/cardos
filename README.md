# CardOS

A small, extensible OS with a menu-based GUI for the [M5Stack Cardputer](https://docs.m5stack.com/en/core/Cardputer) (ESP32-S3).

## Features (planned)

- **Launcher + app framework** — a scene-stack app model with reusable UI widgets (menu list, text input, dialogs) rendered double-buffered on the 240×135 TFT
- **WiFi management** — scan networks, connect with on-device password entry, remember up to 8 networks, auto-connect to the strongest known network on boot
- **Power management** — screen dimming after 60s idle, deep sleep after 5min idle, wake via the G0 button

## Status

MVP working on hardware: launcher + app framework, WiFi management
(scan / connect / remember / auto-connect), HTTP demo app, idle dimming
and deep sleep with G0 wake.
Docs: [design](docs/superpowers/specs/2026-06-05-cardos-design.md) ·
[implementation plan](docs/superpowers/plans/2026-06-05-cardos-mvp.md)

## Apps

| App | What it does |
|---|---|
| WiFi Settings | Scan, connect (on-device password entry), saved networks (8 max), auto-connect on boot |
| HTTP Demo | HTTP/HTTPS GET smoke tests (public IP, example.com, httpbin TLS) with status display |
| Recorder | Push-to-talk voice memos (hold Space), saved to /flash or /sd, multipart upload with progress |
| Voice Memo | Hold any key to record, release to upload to a Cloudflare Worker (audio → R2, metadata → db9) |
| Lua Apps | Lists and runs Lua scripts in `/flash/apps`, installed over serial — no reflashing |
| Lua REPL | Interactive Lua prompt with a persistent session; standard library + `cardos.*` platform calls (Fn+arrows: history / scrollback paging) |
| App Uploader | Starts a local HTTP server (with QR code) to install Lua apps over WiFi from a browser |
| Files | File manager for /flash and /sd: browse directories, delete files and empty dirs (Backspace + confirm) |
| System Info | SSID / IP / RSSI / heap / flash free/total / battery / uptime |

## Tech Stack

- C++ / Arduino framework
- [M5Cardputer](https://github.com/m5stack/M5Cardputer) (M5Unified / M5GFX)
- PlatformIO

## Building

```sh
pio run                 # build
pio run -t upload       # flash over USB
pio device monitor      # serial logs
pio test -e native      # run unit tests on the host
```

## Writing apps

See [docs/sdk.md](docs/sdk.md) for the app SDK — input, audio, filesystem,
HTTP, and widgets, all behind a single `#include "../sdk/CardOS.h"`.

## Lua apps (dynamic install)

New apps can be written in Lua and installed over USB serial without
reflashing. A small built-in Lua-compatible interpreter (`src/lua/`) runs
each script; scripts define `on_key` / `on_update` / `on_render` and draw
through the `cardos.*` API. See [docs/lua-apps.md](docs/lua-apps.md).

```sh
tools/cardos-app.py push apps/hello.lua   # install over serial
tools/cardos-app.py list                  # list installed apps
tools/cardos-app.py run hello.lua         # launch on the device
tools/cardos-app.py rm hello.lua          # delete
```

Then open **Lua Apps** in the launcher (or use `run`). Requires `pyserial`.

## Backend

The Voice Memo app uploads to a Cloudflare Worker in
[backend/voice-worker](backend/voice-worker) that stores audio in R2 and
metadata in [db9](https://db9.ai) (serverless Postgres). See its README
for routes, config, and deploy steps.

## License

MIT
