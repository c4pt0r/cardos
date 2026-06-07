# CardOS Progress

Last updated: 2026-06-07

## Status at a Glance

| Milestone | State |
|---|---|
| MVP (launcher + WiFi + power management) | ✅ Merged to `main`, running on hardware |
| HTTP Demo app | ✅ Merged to `main`, verified on hardware |
| App SDK | ✅ Merged to `main`, running on hardware |
| Voice Memo app + cardos-voice backend | ✅ Merged to `main`; backend deployed + verified |
| Lua app loader + serial install tool | ✅ Merged to `main`, running on hardware |
| App Uploader (Lua apps over WiFi) | ✅ Merged to `main`, running on hardware |
| File Explorer + storage fixes | ✅ On `main`, flashed and in use |
| Lua REPL | ✅ On `main`, flashed; on-device acceptance pending |
| Storage expansion (/flash 1.5→4.7MB) + serial tool fixes | ✅ On `main`, flashed and verified |
| Voice transcription (Whisper + conservative LLM fix) | ✅ Worker deployed via CI, curl E2E verified; device flash pending |

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

## Voice Memo app + cardos-voice backend (branch `sdk`)

Spec: `docs/superpowers/specs/2026-06-06-voice-memo-design.md`

- **Device app** `src/apps/VoiceMemoApp.{h,cpp}` — hold any non-Fn key →
  record to `/flash/voice/memo-*.wav`; release → multipart upload to the
  Worker with a progress bar; Esc/G0 exits. Registered in the launcher.
  Firmware builds clean; native still 41/41.
- **Backend** `backend/voice-worker/` — Cloudflare Worker `cardos-voice`,
  **deployed** at `https://cardos-voice.db9.workers.dev`.
  - `POST /upload` (X-Upload-Key) → R2 `cardos-voice` + db9 INSERT;
    `GET /recordings`, `GET /audio/:id`.
  - db9 DB `cardos_voice` (id `emwisr1axc1o`), table `recordings`.
  - Secrets in the Worker only: `DB9_TOKEN` (scoped rw), `UPLOAD_KEY`.
  - vitest 6/6 for the pure SQL builders.
  - **Verified end-to-end via curl**: upload → R2 object + db9 row →
    `/audio/:id` streams the WAV back (16044 B); wrong key → 401.
- **Deferred to hardware**: flash firmware, hold a key to record on the
  Cardputer, confirm a real memo lands in R2 + db9 (`db9 sql cardos_voice
  -q "SELECT * FROM recordings"`).

## Lua app loader + serial install (branch `lua-apps`)

Spec: `docs/superpowers/specs/2026-06-06-lua-apps-design.md`;
guide: `docs/lua-apps.md`.

- **Interpreter** `src/lua/` — from-scratch Lua-subset (Value/Lexer/Parser/
  Interp/Lua facade), pure C++17, **native-tested** (closures, tables,
  pairs/ipairs, multi-return, pcall, stdlib). ~97 KB flash.
- **Runtime** `src/apps/ScriptApp` runs a `.lua` file as an App with a
  `cardos.*` binding surface; `ScriptHost` owns launched apps;
  `LuaAppsApp` lists/launches `/flash/apps/*.lua`.
- **Serial** `src/core/SerialControl` — PING/LIST/PUT(crc32)/DEL/RUN over
  USB CDC; `crc32` + `SerialProto` parser pure + native-tested.
- **Tool** `tools/cardos-app.py` push/list/rm/run (autodetect, `--selftest`).
- **Examples** `apps/hello.lua`, `apps/bounce.lua`.
- **Status**: 56/56 native tests; firmware builds (60.7% flash); Python
  selftest passes.
- **Deferred to hardware**: `push`→`run` over USB, on-screen Lua app, live
  RAM headroom of a `lua_State`.

## File Explorer, Lua REPL, storage fixes (2026-06-06/07, main)

Specs: `docs/superpowers/specs/2026-06-06-file-explorer-design.md`,
`docs/superpowers/specs/2026-06-07-lua-repl-design.md`.

- **Files app** `src/apps/FileExplorerApp` — browse `/flash` + `/sd`,
  delete files and empty dirs (ConfirmDialog; `fs::rmdir` added).
  `parentPath()` pure helper in `FsPath.h`, native-tested.
- **Lua REPL** `src/apps/LuaReplApp` — persistent on-device session;
  expression-first eval (`ReplEval.h`, parse-only trial so statement
  side effects never run twice), 50-line wrapped scrollback,
  Fn+arrows for history/paging. Platform bindings extracted to shared
  `LuaBindings` (used by ScriptApp + REPL; drawing stays in ScriptApp).
- **Storage** System Info shows flash free/total; Voice Memo deletes
  the local WAV after a successful upload (the 1.5MB LittleFS fills in
  <1 min of audio otherwise); SD probe backs off 10s after a failure.
- **Status**: 71/71 native tests; flashed to hardware. REPL on-device
  acceptance pending (keyboard feel, Fn-layer keys).

## Storage expansion + serial hardening (2026-06-07, main)

- **Partition reclaim**: custom `partitions_cardos_8MB.csv` drops the idle
  OTA slot (`app1`/`otadata`) — `/flash` grows **1.5MB → 4.69MB** (verified
  on hardware: `[fs] /flash mounted (16/4800 KB used)`). `nvs` keeps its
  stock offset; **saved WiFi credentials survived the repartition**
  (auto-connect found and tried a saved network after the flash). No OTA
  with this table — by design.
- **Fixes landed**: SD probe 10s backoff (re-applied after merge loss);
  first-boot NVS error log silenced (read-write open creates the
  namespace); `fs::begin` logs used/total KB.
- **Serial install pipeline verified on hardware**: `cardos-app.py`
  push/list/run all work end-to-end. Two real bugs fixed: opening the CDC
  port reboots the board (tool now pings until the device settles, and
  throttles the payload), and LIST sent its `OK <n>` terminator before the
  ITEM lines (host stops at the first terminator — items now precede END).
  All three example Lua apps reinstalled and `run hello.lua` launches
  remotely; `/flash` contents persist across reflashes.

## Acceptance scoreboard (hardware)

Verified: boot/launcher/status bar; scan; connect+password (IP);
reboot auto-connect (NVS intact — connected NETGEAR16 unattended);
HTTP/HTTPS GETs; Voice Memo end-to-end (record → 219KB WAV → Worker 200
→ R2+db9); Recorder record-to-WAV; Lua push/list/run over serial;
/flash 4.69MB; **full power cycle**: deep sleep entered at exactly
303s idle (300s + 3s notice, serial-logged), USB powered off, G0 woke
it in ~1s, WiFi auto-reconnected (verified by ping from the host).
Pending (human-at-keyboard, low risk): 60s dim visual, key-release
feel, wrong-password retry, saved-network view/delete, Recorder
httpbin upload, Lua REPL keyboard feel, SD-card paths (no card on hand).

## Voice transcription pipeline (2026-06-07, main)

Spec: `docs/superpowers/specs/2026-06-07-voice-transcription-design.md`

- `/upload` now: R2 + db9 INSERT (unchanged) → OpenAI STT (`STT_MODEL`,
  default `gpt-4o-transcribe`) → conservative fix via `FIX_MODEL`
  (default `gpt-4o-mini`, one call, JSON `{corrected, cleaned}`) →
  db9 UPDATE (`raw_text`/`corrected_text`/`cleaned_text`) → response carries
  all three. Failures degrade the response, never lose the recording.
- Prompt contract: fix ONLY obvious ASR errors (中文同音字, 配森→Python,
  杰森→JSON), no summarizing; `cleaned` additionally drops fillers
  (这个/那个/嗯…) for intent use.
- Deploys: GitHub Actions (`deploy-voice-worker.yml`) — tests gate the
  deploy; `OPENAI_API_KEY` synced from GitHub Secrets each run.
- Verified: 22 vitest; CI run green; curl E2E (86KB WAV → raw/corrected
  persisted in db9; noise clip → cleaned="" as expected).
- Device: VoiceMemoApp shows cleaned text (fallback corrected→raw→error),
  30s HTTP timeout inside the app. **Flash + live mixed zh/en speech test
  pending** (device was deep-sleeping at flash time — by design).
- ⚠️ Security note: `UPLOAD_KEY` sits in plaintext in VoiceMemoApp.cpp in a
  public repo — rotate it and move it out of committed source.

## Upcoming Plan

1. **Close the pending acceptance items** above (most need a human at the
   keyboard; deep-sleep entry is being watched over serial right now).
2. **USB drops during WiFi/SD activity** — at least one earlier "drop" was
   actually deep sleep working as designed; for the rest, try a beefier
   USB-C cable before suspecting firmware.
3. **Candidate ideas (not committed)**
   - Audio playback (speaker) + voice-assistant demo (record → upload → TTS reply)
   - Clock/NTP app
   - OTA firmware updates (would need a partition rethink after the
     storage expansion); auto-repeat keys; certificate verification option
