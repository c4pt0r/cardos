# Lua App Loader + Serial App Manager

**Goal:** Install and run new CardOS apps at runtime without reflashing.
Apps are **Lua scripts** interpreted by a small, from-scratch
Lua-compatible interpreter (trimmed for size); a Python tool pushes /
lists / deletes / runs them over USB serial.

## Why a custom interpreter

Full Lua 5.4 is ~200 KB and not host-testable. A trimmed tree-walking
interpreter in pure C++17 is ~30–60 KB, gives us full control of the
binding surface and memory, and — crucially — the lexer/parser/evaluator
run in the **native test suite** (no device needed). The board is an
ESP32-S3 with **no PSRAM** (320 KB RAM), so footprint and control matter.

## Language subset (`minlua`)

Lua-compatible syntax. **Supported:** nil/bool/number(double)/string/table/
function; local & global vars; multiple assignment; operators
`+ - * / % ^ // ..`, `== ~= < <= > >=`, `and or not`, unary `-`, `#`;
`if/elseif/else/end`, `while`, numeric `for i=a,b[,c]`, generic
`for k,v in pairs(t)`, `repeat/until`, `break`, `return`; named/local/
anonymous functions with **closures** (lexical upvalues); table
constructors `{1,2,k=v,[e]=v}`, `t.f`, `t[k]`, `#t`; `--` and `--[[ ]]`
comments; basic multiple return values.

**Builtins:** `print, tostring, tonumber, type, pairs, ipairs, pcall,
error, assert`; `string.format/sub/len/upper/lower/rep/find(plain)`;
`math.floor/ceil/abs/min/max/random/randomseed/sqrt/sin/cos/pi/huge`;
`table.insert/remove/concat`.

**Trimmed out (documented):** metatables, coroutines, varargs `...`, goto,
Lua pattern matching (find is plain substring only), integer/float split
(one number type), `io/os/require`. Scripts needing these won't run.

### Files (interpreter — all pure, native-tested)

| File | Responsibility |
|---|---|
| `src/lua/Value.h` | tagged value (nil/bool/num/string/table/function), shared_ptr tables/functions |
| `src/lua/Lexer.{h,cpp}` | source → tokens |
| `src/lua/Ast.h` | expression/statement node types |
| `src/lua/Parser.{h,cpp}` | tokens → AST |
| `src/lua/Interp.{h,cpp}` | tree-walking evaluator, env chain, closures, builtins |
| `src/lua/Lua.h` | facade: `Lua` class — `load(src)`, `call(fn,args)`, `setGlobal`, register native fns |

Errors surface as a `LuaError` (message + line); never throw past the
facade — `Lua::run` returns `{ok, error}`.

## ScriptApp (device, implements `App`)

`src/apps/ScriptApp.{h,cpp}` owns one `Lua` state loaded from a `.lua`
file and forwards the app lifecycle to Lua callbacks:

- globals `title` (string) names the app;
- `on_enter()`, `on_key(code, ch, action)` → truthy = consumed,
  `on_update(dt_ms)`, `on_render()`.

`code` is a lowercase string (`"up"`,`"down"`,`"left"`,`"right"`,`"enter"`,
`"esc"`,`"backspace"`,`"tab"`,`"char"`); `action` is `"press"`/`"long"`/
`"release"`. Every Lua call goes through the facade's protected call; an
error is logged + shown on screen, the app keeps running. During
`render(gfx)` the active `M5Canvas*` is stored so draw bindings target it
(and draw functions error if called outside render).

### `cardos.*` bindings

- Draw (render only): `cardos.text(x,y,s[,color])`,
  `cardos.rect/fillrect(x,y,w,h[,color])`, `cardos.line(x0,y0,x1,y1[,color])`,
  `cardos.width()`, `cardos.height()`, color constants
  `cardos.WHITE/BLACK/RED/GREEN/CYAN/GRAY`.
- `cardos.fs_read(path)`, `cardos.fs_write(path,data)`, `cardos.fs_list(dir)`
  (restricted to `/flash`,`/sd` by `cardos::fs`).
- `cardos.http_get(url)` → `status, body`.
- `cardos.record_start(path)`, `cardos.record_stop()`, `cardos.level()`.
- `cardos.millis()`, `cardos.log(s)`, `cardos.redraw()`.

## Install + launcher

Apps live in `/flash/apps/*.lua`. `src/apps/ScriptHost.{h,cpp}` scans that
directory, owns the `ScriptApp` instances (`vector<unique_ptr<ScriptApp>>`),
and exposes them to the launcher (rescanned on `RUN` and on demand). A
bundled example `apps/hello.lua` is uploaded by the user via the tool (or
pre-seeded). Launcher gains a dynamic section listing installed Lua apps.

## Serial protocol (`src/core/SerialControl.{h,cpp}`)

Line-based control over USB CDC; every response line is prefixed `#CTRL#`
so the host tool ignores interleaved firmware logs. Commands:

| Command | Response |
|---|---|
| `PING` | `#CTRL# PONG` |
| `LIST` | `#CTRL# OK <n>` then `#CTRL# ITEM <name> <size>` ×n, `#CTRL# END` |
| `PUT <name> <size> <crc32hex>` | `#CTRL# READY`, device reads exactly `<size>` raw bytes, writes `/flash/apps/<name>`, verifies CRC → `#CTRL# OK` / `#CTRL# ERR <msg>` |
| `DEL <name>` | `#CTRL# OK` / `#CTRL# ERR` |
| `RUN <name>` | load + push the app → `#CTRL# OK` / `#CTRL# ERR <msg>` |

During `PUT` the device blocks in a read loop for the payload (with a
timeout), so no other output interleaves. `name` is validated (no `/`,
`.lua` enforced). Pure helpers — **native-tested**:
`src/core/crc32.h` (CRC32) and a command-line parser
`SerialControl::parse(line)` → `{verb, args}`.

`SerialControl::tick()` is called each loop iteration from `main.cpp`,
draining available serial bytes and dispatching complete command lines.

## Python tool `tools/cardos-app.py`

`pyserial`-based. Autodetects the port (prefers `/dev/cu.usbmodem*` /
`/dev/ttyACM*`; `--port` overrides). Subcommands:

- `push <file.lua> [--name N]` — chunked send, CRC32, progress bar
- `list` — table of installed apps
- `rm <name>` — delete
- `run <name>` — launch on device

Protocol encoding (frame a PUT, parse responses) is factored into a pure
module so it's unit-testable with a fake serial loopback (`pytest` or a
`--selftest`).

## Testing

- **Native (Unity):** lexer, parser, and interpreter — arithmetic,
  strings, tables, control flow, functions/closures, multiple return,
  pairs/ipairs, pcall/error, a few whole-program snippets; CRC32 vectors;
  `SerialControl::parse`. Target: existing 41 + ~20 new.
- **Firmware:** `pio run -e m5stack-cardputer` green with the interpreter +
  ScriptApp + SerialControl linked; check RAM/flash headroom.
- **Python:** protocol-encode/parse self-test.
- **Deferred to hardware:** real `push`→`run` over USB, on-screen Lua app,
  RAM headroom of a live `lua_State`.

## Risks

- **RAM** for an interpreter state on no-PSRAM — small apps fine; large
  scripts/tables may OOM. Keep per-app state lean; one app active at a time.
- **C-stack depth** of recursive tree-walking on the loop task — bump
  `ARDUINO_LOOP_STACK_SIZE` to 16 KB; deep recursion in scripts can still
  overflow (documented).
- **No device** during build-out — load/serial paths build-verified only.

## Names / branch

Interpreter `minlua` under `src/lua/`; branch `lua-apps` off `main`.
