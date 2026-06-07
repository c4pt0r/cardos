# Transcript Webhook Design

Date: 2026-06-07
Builds on: voice transcription pipeline (`2026-06-07-voice-transcription-design.md`)

## 1. Goal

After the cardos-voice Worker obtains a transcript, POST it to a
configurable HTTP endpoint (generic webhook) for downstream consumers
(intent execution, notifications, …). The receiver is undecided — keep the
contract generic.

## 2. Configuration

Two optional Worker secrets (set manually via `wrangler secret put`; not in
the CI sync until a receiver is chosen):

- `WEBHOOK_URL` — absent → the feature is silently skipped.
- `WEBHOOK_KEY` — when present, sent as `X-Webhook-Key` for the receiver to
  verify.

## 3. Trigger & Delivery

- Fires from `/upload` only when transcription produced at least `raw`
  (STT failure → nothing to send → skip).
- Delivered via `ctx.waitUntil()` after the device response is sent —
  fire-and-forget, zero added latency, no retry (MVP).
- Failures are `console.log`-ged (visible in `wrangler tail`) and never
  affect the upload response or persistence — same isolation principle as
  the transcription pipeline.
- `index.js` fetch handler gains the `ctx` parameter and passes it down.

## 4. Payload

`POST WEBHOOK_URL`, `Content-Type: application/json` (+ `X-Webhook-Key` when
configured):

```json
{
  "id": "<uuid>",
  "raw": "测试测试，使用Python写一个JSON解析器。",
  "corrected": "...",
  "cleaned": "使用Python写一个JSON解析器。",
  "device": "cardputer",
  "size_bytes": 188460,
  "created_at": "2026-06-07T17:20:00Z",
  "audio_url": "https://<worker-origin>/audio/<uuid>"
}
```

`corrected`/`cleaned` are null when the fix step failed. `audio_url` is
derived from the request origin. `created_at` is the Worker's current time
(ISO 8601).

## 5. Implementation Structure

- `src/webhook.js`:
  - `buildWebhookPayload(meta, texts, origin)` — pure, vitest-covered.
  - `sendWebhook(fetchFn, env, payload)` — injected fetch; returns
    `"skipped" | "ok" | "failed: <reason>"` (for logging); never throws.
- `src/index.js`: pass `ctx`; after the pipeline, build payload and
  `ctx.waitUntil(sendWebhook(fetch, env, payload).then(console.log))`.
- Device: zero changes.

## 6. Testing

- vitest: payload shape (incl. null fix fields, audio_url derivation);
  sendWebhook skip-when-unset, key header presence/absence, non-200 and
  thrown-fetch → "failed", never throws.
- Post-deploy E2E: set `WEBHOOK_URL` to a webhook.site URL, upload a memo,
  observe the JSON arrive; then leave the secrets unset/real per user.

## 7. Out of Scope

Retries/queues, multiple endpoints, payload templating per receiver
(Slack/飞书 adapters), signature schemes (HMAC) — add when a real receiver
exists.
