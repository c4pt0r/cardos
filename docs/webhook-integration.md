# CardOS Transcript Webhook — Integration Guide

When the `cardos-voice` Worker finishes transcribing a recording, it POSTs a
JSON payload to a configurable endpoint. This document is the contract for
building a receiver — e.g. an agent that acts on the spoken intent.

You do not need any CardOS source to integrate; you only implement an HTTP
endpoint that accepts the payload below.

## 1. HTTP contract

| | |
|---|---|
| Method | `POST` |
| Content-Type | `application/json` |
| Auth header | `X-Webhook-Key: <shared secret>` — present only if the operator set `WEBHOOK_KEY` |
| Body | the JSON object in §2 |
| Expected response | any `2xx`. Non-2xx is logged on the sender and dropped. |
| Timeout / retry | **none** — at-most-once, fire-and-forget (see §4) |

Your endpoint must be reachable over public HTTPS (the sender is a
Cloudflare Worker).

## 2. Payload schema

```json
{
  "id": "9944c5b6-223e-435e-ab9f-d23f22b5b511",
  "raw": "测试测试，使用Python写一个JSON解析器。",
  "corrected": "测试测试，使用Python写一个JSON解析器。",
  "cleaned": "使用Python写一个JSON解析器。",
  "device": "cardputer",
  "size_bytes": 188460,
  "created_at": "2026-06-07T17:20:00.236Z",
  "audio_url": "https://cardos-voice.db9.workers.dev/audio/9944c5b6-223e-435e-ab9f-d23f22b5b511"
}
```

| Field | Type | Notes |
|---|---|---|
| `id` | string (UUID v4) | Recording id; stable primary key. Also the `audio_url` suffix. |
| `raw` | string | Verbatim speech-to-text output. Always present (the webhook only fires when STT produced text). |
| `corrected` | string \| null | `raw` with **only obvious ASR errors** fixed (Chinese homophones; English tech terms mis-heard as Chinese, e.g. 配森→Python, 杰森→JSON). Same wording/meaning, never summarized. `null` if the correction step failed. |
| `cleaned` | string \| null | `corrected` with filler words / false starts / verbal tics removed (这个, 那个, 嗯, …). **This is the intent-ready text** most agents should consume. `null` if the correction step failed. |
| `device` | string \| null | Source device id (`X-Device` header at upload), or `"unknown"`. |
| `size_bytes` | number \| null | WAV byte size. |
| `created_at` | string | ISO 8601 UTC, set by the Worker at delivery time. |
| `audio_url` | string | Fetch the original WAV (16 kHz mono PCM) here; see §5. |

**Which text to use:** for intent/command agents, use `cleaned` (fall back to
`corrected`, then `raw` when a field is null). Keep `raw` if you need the
exact spoken form. Always tolerate `corrected`/`cleaned` being `null`.

## 3. Authentication

If `WEBHOOK_KEY` is configured, every request carries
`X-Webhook-Key: <secret>`. Verify it with a constant-time compare and reject
mismatches with `401`. If no key is configured the header is absent — in
that case restrict exposure another way (obscure URL, network ACL), since
the endpoint is otherwise unauthenticated.

The key is a static shared secret, not a signature; there is no HMAC over
the body (out of scope for now).

## 4. Delivery semantics

- **At-most-once.** The Worker sends once via `ctx.waitUntil` after it has
  already responded to the device. There is **no retry** and no delivery
  receipt. If your endpoint is down, that transcript is lost from the
  webhook's perspective (the recording itself is still in R2 + db9 and can
  be re-fetched via the Worker's `/recordings` route).
- **No ordering or deduplication guarantees.** Treat `id` as an idempotency
  key: if you persist or act on transcripts, ignore an `id` you have already
  processed.
- **Fired only when `raw` exists.** A recording whose STT step failed
  produces no webhook.
- **Latency:** delivery happens just after upload; expect a payload within a
  few seconds of the user finishing a recording.

Respond quickly with `2xx`. Do heavy work (LLM calls, side effects) after
acknowledging, or the Worker's own request budget may lapse.

## 5. Fetching the audio

`GET {audio_url}` streams the original recording as `audio/wav`
(16 kHz, mono, 16-bit PCM). It currently requires no auth, so treat the URL
as a capability — anyone with the `id` can fetch it. Use it when you need to
re-transcribe with a different model, archive the audio, or play it back.

## 6. Minimal receivers

### Node / Cloudflare Worker

```js
export default {
  async fetch(req, env) {
    if (req.method !== "POST") return new Response("ok"); // health check
    if (env.WEBHOOK_KEY && req.headers.get("X-Webhook-Key") !== env.WEBHOOK_KEY)
      return new Response("unauthorized", { status: 401 });
    const t = await req.json();
    const text = t.cleaned ?? t.corrected ?? t.raw;
    // ... dispatch `text` (intent id `t.id`) to your agent ...
    return new Response("ok"); // 2xx ack
  },
};
```

### Python (FastAPI)

```python
import os
from fastapi import FastAPI, Request, Header, HTTPException

app = FastAPI()
KEY = os.environ.get("WEBHOOK_KEY")

@app.post("/cardos")
async def cardos(req: Request, x_webhook_key: str | None = Header(default=None)):
    if KEY and x_webhook_key != KEY:
        raise HTTPException(401)
    t = await req.json()
    text = t.get("cleaned") or t.get("corrected") or t.get("raw")
    # ... hand `text` + t["id"] to your agent (dedupe on id) ...
    return {"ok": True}
```

### curl smoke test (simulate the sender)

```sh
curl -X POST "$YOUR_ENDPOINT" \
  -H "Content-Type: application/json" \
  -H "X-Webhook-Key: $WEBHOOK_KEY" \
  -d '{"id":"test-1","raw":"用配森写代码","corrected":"用Python写代码","cleaned":"用Python写代码","device":"curl","size_bytes":0,"created_at":"2026-06-07T00:00:00Z","audio_url":"https://example/audio/test-1"}'
```

## 7. Pointing the Worker at your endpoint

The endpoint URL and optional key are Worker secrets, synced from GitHub
repository secrets on deploy:

```sh
gh secret set WEBHOOK_URL --repo c4pt0r/cardos     # https URL of your receiver
gh secret set WEBHOOK_KEY --repo c4pt0r/cardos     # optional shared secret
gh workflow run deploy-voice-worker --repo c4pt0r/cardos
```

`WEBHOOK_KEY` is only delivered to the Worker if it is listed in the
`secrets:` block of `.github/workflows/deploy-voice-worker.yml` — add it
there alongside `WEBHOOK_URL` when you start using a key. Unset `WEBHOOK_URL`
to disable webhooks entirely (the pipeline then just stores transcripts).

## 8. Versioning

Fields may be **added** to the payload without notice; build receivers that
ignore unknown keys. Removals or renames will be called out in this file's
git history.
