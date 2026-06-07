# Lua REPL — design

2026-06-07. Status: approved.

An on-device REPL app ("Lua REPL" in the Launcher): type a line of Lua
on the Cardputer keyboard, see results and print() output in a
scrollback area. Persistent session — globals survive across lines.

## Approach

New thin app (`src/apps/LuaReplApp.{h,cpp}`) owning one `minlua::Lua`
instance, plus one refactor: the platform-service bindings currently
registered inside ScriptApp (`fs_read/fs_write/fs_list`, `http_get`,
`millis`, `log`, `record_*`, `level`) move to a shared
`installPlatformBindings(minlua::Lua&)` in `src/apps/LuaBindings.{h,cpp}`,
called by both ScriptApp and LuaReplApp. Drawing and `redraw` bindings
stay in ScriptApp — they only make sense inside a script's render
context and are NOT exposed to the REPL.

## Eval semantics (pure, native-tested)

`replEval(Lua&, line)` in `src/apps/ReplEval.h` — no UI, no Arduino:

1. Try the line as an expression: run `__r = (<line>)`. On success,
   return `tostr(__r)`, or "" when the value is nil (no echo).
2. If that fails to parse, run the raw line as a statement chunk.
3. Errors come back as `Result.error` text.
4. `print()` output is captured via `onPrint` and interleaved before
   the result line.

The Lua instance is created in `onEnter()` (fresh session per app
entry) and torn down in `onExit()`.

## UI (240x135)

- Scrollback: the output area shows `> <input>` echo, print() lines,
  results, and errors (errors in `theme::kDanger`). Capped at the last
  50 lines; older lines drop off.
- Input: single `TextInput` row pinned to the bottom with a `> `
  prompt.
- Keys: Enter evaluates; Up/Down cycle input history (last 10);
  Fn+Up/Down scroll the scrollback; Esc exits the app.
- Evaluation blocks the main loop (same convention as the other apps;
  a long `http_get` just stalls the UI briefly).

## Testing

- Native unit tests for `replEval`: expression echo, nil no-echo,
  statement fallthrough (`x = 1` then `x` shows `1`), parse/runtime
  error text, print() capture, state persistence across calls.
- Bindings refactor is behavior-preserving for ScriptApp; existing
  Lua app examples re-verified on device.
- UI exercised on device.
