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
| HTTP Demo | Plain-HTTP GET smoke tests (public IP, example.com) with latency/status display |
| System Info | SSID / IP / RSSI / heap / battery / uptime |

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

## License

MIT
