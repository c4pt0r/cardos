# CardOS Lua Apps

Write apps in Lua and install them over USB serial — no firmware reflash.
A small Lua-compatible interpreter (`src/lua/`, ~100 KB flash) runs each
script; the host tool `tools/cardos-app.py` pushes/lists/deletes/runs them.

## Writing an app

A script lives in `/flash/apps/<name>.lua` and may define:

| Global | When |
|---|---|
| `title` (string) | shown in the status bar (defaults to the filename) |
| `on_enter()` | each time the app opens |
| `on_key(code, ch, action)` → truthy = consumed | per input event |
| `on_update(dt_ms)` | every frame |
| `on_render()` | every frame the screen is dirty; draw here |

`code` is one of `"up" "down" "left" "right" "enter" "esc" "backspace"
"tab" "char" "none"`; `action` is `"press" "long" "release"`. Return a
falsey value from `on_key` for an `esc` press to let the launcher pop your
app. Call `cardos.redraw()` after changing state so the next frame repaints.

```lua
title = "Hello Lua"
local count = 0
function on_key(code, ch, action)
  if action ~= "press" then return false end
  if code == "enter" then count = count + 1; cardos.redraw(); return true end
  return false  -- Esc exits
end
function on_render()
  cardos.text(8, 22, "Hello from Lua!", cardos.CYAN)
  cardos.text(8, 46, "Enter pressed " .. count .. " times")
end
```

## `cardos.*` API

Drawing (only valid during `on_render`):
`text(x,y,s[,color])`, `rect(x,y,w,h[,color])`, `fillrect(...)`,
`line(x0,y0,x1,y1[,color])`, `width()`, `height()`; colors
`cardos.WHITE/BLACK/GRAY/CYAN/RED/GREEN/YELLOW/ORANGE`.

Platform: `cardos.log(s)`, `cardos.millis()`, `cardos.redraw()`,
`cardos.fs_read(path)`, `cardos.fs_write(path,data)`, `cardos.fs_list(dir)`,
`cardos.http_get(url)` → `status, body`, `cardos.record_start(path)`,
`cardos.record_stop()`, `cardos.level()`.

## Language subset

Lua-compatible syntax, trimmed for size. **Supported:** numbers, strings,
booleans, nil, tables, functions/closures; `local`/global vars, multiple
assignment; full operators incl. `//`, `..`, `#`, `^`; `if/while/for`
(numeric and generic `for k,v in pairs`), `repeat`, `break`, `return`;
`string.*` (format/sub/len/upper/lower/rep/find-plain), `math.*`,
`table.insert/remove/concat`, `pairs/ipairs/pcall/error/assert/type/
tostring/tonumber`. **Not supported:** metatables, coroutines, varargs
`...`, goto, Lua pattern matching, integer/float distinction, `io/os/
require`.

## Bundled examples

The `apps/` directory ships ready-to-push scripts:

| Script | Shows |
|---|---|
| `apps/hello.lua` | state + input + drawing (a counter) |
| `apps/bounce.lua` | animation via `on_update` (a bouncing box) |
| `apps/audio.lua` | the recorder demo: hold-to-record, live level meter, `on_exit` cleanup |

```sh
tools/cardos-app.py push apps/audio.lua && tools/cardos-app.py run audio.lua
```

## Installing over serial

`tools/cardos-app.py` (needs `pyserial`) speaks the device's
`#CTRL#`-framed protocol over USB CDC:

```sh
tools/cardos-app.py push apps/hello.lua    # upload (CRC32-verified)
tools/cardos-app.py list                   # installed apps
tools/cardos-app.py run hello.lua          # launch on device
tools/cardos-app.py rm hello.lua           # delete
tools/cardos-app.py --selftest             # offline checks, no device
```

The port is auto-detected (`/dev/cu.usbmodem*` etc.); override with
`--port`. Files must end in `.lua` and are stored in `/flash/apps`.

## Installing over WiFi (App Uploader)

No USB cable needed once WiFi is connected. On the device, open
**App Uploader** — it starts a small HTTP server and shows a URL + QR code.

- Scan the QR code (or open `http://<device-ip>/`) to get a web page that
  lists installed apps and has a file-upload form (and a delete link each).
- Or use `curl`:

```sh
curl -F "app=@apps/hello.lua" http://<device-ip>/upload   # install
curl http://<device-ip>/api/apps                          # list (JSON)
curl "http://<device-ip>/delete?name=hello.lua"           # delete
```

The server runs only while the App Uploader screen is open (Esc stops it).
Uploaded files are validated (`.lua`, no path escape) and stored in
`/flash/apps`; run them from **Lua Apps**. Like the USB tool, the endpoint
is unauthenticated — it trusts your local network.

## Limits / notes

- The board has no PSRAM (320 KB RAM); keep app state small. One app runs
  at a time.
- Deep recursion in a script can overflow the loop-task stack (16 KB).
- A script error is caught and shown on screen; press Esc to exit.
