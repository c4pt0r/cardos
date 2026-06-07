# cardos-voice Worker

Receives WAV uploads from the CardOS **Voice Memo** app, stores audio in
**R2**, and writes metadata to **db9** (serverless Postgres).

## Routes

| Method | Path | Purpose |
|---|---|---|
| `POST` | `/upload` | multipart `file` upload → R2 + db9 row. Requires header `X-Upload-Key`. Returns `{id,key,size}`. |
| `GET` | `/recordings?limit=N` | recent metadata rows (JSON) |
| `GET` | `/audio/:id` | stream the WAV back from R2 |
| `GET` | `/` | health check |

Extra upload headers: `X-Device`, `X-Sample-Rate`.

## Config

`wrangler.toml` binds R2 bucket `cardos-voice` as `BUCKET` and sets
`DB9_DB_ID`. Two secrets are required:

```sh
npm install
npx wrangler secret put DB9_TOKEN    # scoped db9 rw token (cardos_voice:rw)
npx wrangler secret put UPLOAD_KEY   # shared key, also compiled into firmware
npx wrangler deploy
```

## db9 schema

```sql
CREATE TABLE recordings (
  id uuid PRIMARY KEY, r2_key text NOT NULL, filename text,
  content_type text, size_bytes bigint, sample_rate int,
  device text, created_at timestamptz NOT NULL DEFAULT now()
);
```

## Schema migrations

2026-06-07 — transcription text layers (run once, idempotent):

```sql
ALTER TABLE recordings ADD COLUMN IF NOT EXISTS raw_text TEXT;
ALTER TABLE recordings ADD COLUMN IF NOT EXISTS corrected_text TEXT;
ALTER TABLE recordings ADD COLUMN IF NOT EXISTS cleaned_text TEXT;
```

## Test

```sh
npm test         # vitest: pure SQL builders
```
