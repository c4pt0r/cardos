# Voice Memo: device → Cloudflare Worker → R2 + db9

**Goal:** A small CardOS app that records audio while a key is held and
uploads it on release to a Cloudflare Worker, which stores the audio in R2
and the metadata in db9 (serverless Postgres). Exercises the CardOS SDK
(input long-press/release, audio→WAV, HTTP multipart upload, widgets)
end-to-end against a real backend.

## Components

### 1. Device app — `VoiceMemoApp` (`src/apps/VoiceMemoApp.{h,cpp}`)

Built on `#include "../sdk/CardOS.h"`.

- **Idle (List/Idle mode):** shows a hint "Hold any key to record" and the
  last upload result.
- **Press-and-hold any non-Fn key** → start recording to
  `/flash/voice/memo-<uptime_s>.wav` via `cardos::audio::startToWav`.
  Screen switches to `REC` with a live level meter (`ProgressBar` fed by
  `cardos::audio::level()` at ~10 Hz from `update()`).
- **Release the same key** → `cardos::audio::stop()`, then upload.
- **Esc / G0** exits to launcher. Keys with `ev.fn == true` are ignored
  (function keys excluded per the requirement).
- Upload uses `cardos::http::uploadFile(kWorkerUrl + "/upload", path,
  "file", headers, progress)` with a bottom progress bar painted from the
  progress callback. Result (status + id, or error) shown in a `TextView`.
- Headers: `X-Upload-Key: <kUploadKey>`, `X-Device: cardputer`,
  `X-Sample-Rate: 16000`.
- `kWorkerUrl` and `kUploadKey` are compile-time constants in the .cpp,
  filled in after the Worker is deployed.

Interaction detail: the app remembers the key id that started recording
(`KeyEvent` Press) and only stops on that key's Release, so chord noise
doesn't cut a recording short. The push-to-talk Press/LongPress/Release
events come from the SDK's `KeyTracker` pipeline.

### 2. Cloudflare Worker — `cardos-voice` (`backend/voice-worker/`)

ES-module Worker (`src/index.js`), `wrangler.toml`, `package.json`,
`vitest` test.

Routes:
- `POST /upload` — verify `X-Upload-Key` against `env.UPLOAD_KEY` (401 on
  mismatch); parse multipart, require field `file` (400 if missing);
  `id = crypto.randomUUID()`, `key = recordings/<id>.wav`;
  `env.BUCKET.put(key, file.stream(), { httpMetadata: { contentType }})`;
  then INSERT metadata into db9. On db9 failure after the R2 put, return
  502 with detail (object is kept; reconcilable). Success → 200
  `{ id, key, size }`.
- `GET /recordings?limit=N` — `SELECT ... ORDER BY created_at DESC LIMIT N`
  from db9; returns JSON array. For verification.
- `GET /audio/:id` — look up `r2_key` in db9, stream the object from R2
  (`Content-Type: audio/wav`). 404 if absent.
- `GET /` — tiny health/info text.

db9 access: `POST https://api.db9.ai/customer/databases/${env.DB9_DB_ID}/sql`
with `Authorization: Bearer ${env.DB9_TOKEN}` and body `{"query": "..."}`.
A **pure** helper `buildInsertSql(meta)` builds a safe INSERT (string
values single-quote-escaped) and is unit-tested with vitest.

Bindings (wrangler.toml):
- R2: `BUCKET` → bucket `cardos-voice`
- Vars: `DB9_DB_ID`
- Secrets: `DB9_TOKEN` (scoped db9 rw token), `UPLOAD_KEY`

### 3. db9 database — `cardos_voice`

```sql
CREATE TABLE IF NOT EXISTS recordings (
  id           uuid PRIMARY KEY,
  r2_key       text NOT NULL,
  filename     text,
  content_type text,
  size_bytes   bigint,
  sample_rate  int,
  device       text,
  created_at   timestamptz NOT NULL DEFAULT now()
);
```

The Worker authenticates with a scoped rw token
(`db9 token create --name voice-worker --scope cardos_voice:rw`), kept only
as a Worker secret.

## Data flow

device records WAV → `POST /upload` (multipart + headers) → Worker checks
upload key → `R2.put` → db9 `INSERT` via SQL HTTP API → `{id,key,size}` →
device shows `HTTP 200 · id=…`.

## Security notes

- `X-Upload-Key` is a shared secret compiled into firmware: it deters
  casual abuse of a public endpoint but is not extractable-proof on a
  hobby device. Acceptable for this use; documented.
- `DB9_TOKEN` is scoped rw to the single database and never shipped to the
  device — only the Worker holds it.
- TLS: device uses `WiFiClientSecure::setInsecure()` (no cert
  verification), consistent with the rest of the SDK.

## Verification

- Worker: `vitest` for `buildInsertSql`; `wrangler dev` + `curl -F` upload
  of a generated test WAV → assert 200, then `db9 sql -q "SELECT * FROM
  recordings"` shows the row and `GET /audio/:id` streams bytes back.
- Firmware: `pio run -e m5stack-cardputer` green; `pio test -e native`
  still 41/41 (app is device-only; no new native-testable logic beyond the
  Worker's vitest).
- On-device end-to-end (hold key → REC → release → HTTP 200) deferred to
  the user — no Cardputer is connected during build-out.

## Names

Worker `cardos-voice`, R2 bucket `cardos-voice`, db9 DB `cardos_voice`,
backend dir `backend/voice-worker/`. Work continues on branch `sdk`
(extends PR #1).
