# Transcript Webhook Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fire-and-forget POST of each transcript (full payload) to an optional configurable webhook from the cardos-voice Worker.

**Architecture:** New pure-ish `src/webhook.js` (payload builder + injected-fetch sender, vitest-covered); `/upload` builds the payload when `raw` exists and defers delivery via `ctx.waitUntil`. No device changes.

**Tech Stack:** Cloudflare Workers, vitest.

**Spec:** `docs/superpowers/specs/2026-06-07-transcript-webhook-design.md`
**Working dir:** `/Users/dongxu/cardos`, branch `main`. Commit per task; push in the final task (triggers CI deploy).

---

### Task 1: webhook.js (TDD)

**Files:**
- Create: `backend/voice-worker/src/webhook.js`
- Create: `backend/voice-worker/test/webhook.test.js`

- [ ] **Step 1: Failing tests `backend/voice-worker/test/webhook.test.js`**

```js
import { describe, it, expect } from "vitest";
import { buildWebhookPayload, sendWebhook } from "../src/webhook.js";

const meta = {
  id: "11111111-1111-1111-1111-111111111111",
  device: "cardputer",
  size_bytes: 188460,
};

describe("buildWebhookPayload", () => {
  it("builds the full payload with audio_url from origin", () => {
    const p = buildWebhookPayload(
      meta,
      { raw: "r", corrected: "c", cleaned: "cl" },
      "https://w.example"
    );
    expect(p.id).toBe(meta.id);
    expect(p.raw).toBe("r");
    expect(p.corrected).toBe("c");
    expect(p.cleaned).toBe("cl");
    expect(p.device).toBe("cardputer");
    expect(p.size_bytes).toBe(188460);
    expect(p.audio_url).toBe(`https://w.example/audio/${meta.id}`);
    expect(typeof p.created_at).toBe("string");
    expect(p.created_at).toMatch(/^\d{4}-\d{2}-\d{2}T/);
  });
  it("passes null fix fields through", () => {
    const p = buildWebhookPayload(meta, { raw: "r", corrected: null, cleaned: null }, "https://w");
    expect(p.corrected).toBeNull();
    expect(p.cleaned).toBeNull();
  });
});

describe("sendWebhook", () => {
  const payload = { id: "x" };

  it("skips when WEBHOOK_URL unset", async () => {
    const out = await sendWebhook(async () => {
      throw new Error("must not fetch");
    }, {}, payload);
    expect(out).toBe("skipped");
  });

  it("posts JSON with key header when configured", async () => {
    let got;
    const f = async (url, init) => {
      got = { url, init };
      return new Response("", { status: 200 });
    };
    const env = { WEBHOOK_URL: "https://hook.example/x", WEBHOOK_KEY: "k1" };
    const out = await sendWebhook(f, env, payload);
    expect(out).toBe("ok");
    expect(got.url).toBe("https://hook.example/x");
    expect(got.init.method).toBe("POST");
    expect(got.init.headers["Content-Type"]).toBe("application/json");
    expect(got.init.headers["X-Webhook-Key"]).toBe("k1");
    expect(JSON.parse(got.init.body)).toEqual(payload);
  });

  it("omits the key header when WEBHOOK_KEY unset", async () => {
    let got;
    const f = async (url, init) => {
      got = init;
      return new Response("", { status: 200 });
    };
    await sendWebhook(f, { WEBHOOK_URL: "https://h" }, payload);
    expect(got.headers["X-Webhook-Key"]).toBeUndefined();
  });

  it("reports non-200 and thrown fetch as failed, never throws", async () => {
    const bad = async () => new Response("nope", { status: 500 });
    expect(await sendWebhook(bad, { WEBHOOK_URL: "https://h" }, payload)).toMatch(/^failed: 500/);
    const boom = async () => {
      throw new Error("conn reset");
    };
    expect(await sendWebhook(boom, { WEBHOOK_URL: "https://h" }, payload)).toMatch(/^failed: conn reset/);
  });
});
```

- [ ] **Step 2: Run to verify failure** — `cd backend/voice-worker && npm test` → FAIL (no webhook.js)

- [ ] **Step 3: Write `backend/voice-worker/src/webhook.js`**

```js
// Generic transcript webhook: optional, fire-and-forget, never throws.
// WEBHOOK_URL unset -> skipped. WEBHOOK_KEY -> X-Webhook-Key header.

export function buildWebhookPayload(meta, texts, origin) {
  return {
    id: meta.id,
    raw: texts.raw ?? null,
    corrected: texts.corrected ?? null,
    cleaned: texts.cleaned ?? null,
    device: meta.device ?? null,
    size_bytes: meta.size_bytes ?? null,
    created_at: new Date().toISOString(),
    audio_url: `${origin}/audio/${meta.id}`,
  };
}

// Returns "skipped" | "ok" | "failed: <reason>" for logging; never throws.
export async function sendWebhook(fetchFn, env, payload) {
  if (!env.WEBHOOK_URL) return "skipped";
  const headers = { "Content-Type": "application/json" };
  if (env.WEBHOOK_KEY) headers["X-Webhook-Key"] = env.WEBHOOK_KEY;
  try {
    const resp = await fetchFn(env.WEBHOOK_URL, {
      method: "POST",
      headers,
      body: JSON.stringify(payload),
    });
    if (!resp.ok) return `failed: ${resp.status}`;
    return "ok";
  } catch (e) {
    return `failed: ${String((e && e.message) || e)}`;
  }
}
```

- [ ] **Step 4: Run to verify pass** — `npm test` → all green (22 existing + 6 new = 28)

- [ ] **Step 5: Commit**

```bash
git add backend/voice-worker/src/webhook.js backend/voice-worker/test/webhook.test.js
git commit -m "feat(voice): generic transcript webhook payload + sender"
```

---

### Task 2: Wire into /upload with ctx.waitUntil

**Files:**
- Modify: `backend/voice-worker/src/index.js`
- Modify: `backend/voice-worker/wrangler.toml` (secret docs)

- [ ] **Step 1: Thread `ctx` through**

Change the fetch handler signature `async fetch(request, env)` →
`async fetch(request, env, ctx)`, and the `/upload` dispatch
`handleUpload(request, env)` → `handleUpload(request, env, ctx)`; update the
`handleUpload` definition likewise.

- [ ] **Step 2: Fire the webhook after the pipeline**

Add to the imports: `import { buildWebhookPayload, sendWebhook } from "./webhook.js";`

In `handleUpload`, immediately BEFORE the final `const out = { id, key, size, raw, corrected, cleaned };` add:

```js
  if (raw !== null) {
    const payload = buildWebhookPayload(
      { id, device, size_bytes: size },
      { raw, corrected, cleaned },
      new URL(request.url).origin
    );
    ctx.waitUntil(
      sendWebhook(fetch, env, payload).then((r) => {
        if (r !== "skipped") console.log(`[webhook] ${r}`);
      })
    );
  }
```

- [ ] **Step 3: wrangler.toml secret docs** — extend the secrets comment block:

```toml
#   WEBHOOK_URL     - optional; transcript webhook endpoint (skip when unset)
#   WEBHOOK_KEY     - optional; sent as X-Webhook-Key when set
```

- [ ] **Step 4: Tests green** — `npm test` → 28/28

- [ ] **Step 5: Commit**

```bash
git add backend/voice-worker/src/index.js backend/voice-worker/wrangler.toml
git commit -m "feat(voice): fire transcript webhook via ctx.waitUntil after upload"
```

---

### Task 3: Deploy + E2E

- [ ] **Step 1: Push** — `git push origin main`; watch the CI run
(`gh run list/watch`) → green.

- [ ] **Step 2: E2E with webhook.site**

Get a fresh URL from https://webhook.site (the user opens it, or use its API
`curl -s https://webhook.site/token | jq -r .uuid`). Then:

```bash
cd backend/voice-worker && npx wrangler secret put WEBHOOK_URL   # paste the URL
```

(If local wrangler is unauthenticated, ask the user to run it, or set the
GitHub secret + add to workflow sync as an alternative.)

Upload a memo (device or curl re-upload of an existing WAV with the
UPLOAD_KEY from src/apps/Secrets.h) → webhook.site shows the JSON payload
with all fields. Then per user instruction either keep, replace, or delete
the secret (`npx wrangler secret delete WEBHOOK_URL`).

- [ ] **Step 3: PROGRESS.md** — one bullet under the transcription section;
commit `docs: PROGRESS — transcript webhook` and push.

---

## Plan Self-Review Notes

- Spec coverage: config/skip (T1 sender), trigger-on-raw + waitUntil + log
  (T2), payload incl. audio_url/nulls (T1), no device changes (none), E2E
  (T3). Out-of-scope items absent.
- Type consistency: `buildWebhookPayload(meta, texts, origin)` /
  `sendWebhook(fetchFn, env, payload)` match between T1 and T2.
- `device` and `size` are in scope at the insertion point in handleUpload
  (defined earlier in the function); `request` likewise.
