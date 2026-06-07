# Voice Transcription Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the cardos-voice Worker so `/upload` transcribes the WAV via OpenAI (`gpt-4o-transcribe`), conservatively fixes the transcript for mixed zh/en tech speech via `gpt-4o-mini` (corrected + cleaned variants), stores all three text layers in db9, and returns them to the Cardputer, which shows the cleaned text on screen.

**Architecture:** Synchronous pipeline inside the existing Worker `/upload` handler; recording persistence always precedes and survives OpenAI failures. Pure logic (prompt/request builders, response parsing, SQL) in separate modules with vitest coverage; OpenAI calls take an injected `fetchFn` for testability. Deploys move to GitHub Actions (`cloudflare/wrangler-action`) which syncs `OPENAI_API_KEY` from GitHub Secrets into the Worker.

**Tech Stack:** Cloudflare Workers (JS modules), OpenAI `/v1/audio/transcriptions` + `/v1/chat/completions`, db9 HTTP SQL, vitest, GitHub Actions, ArduinoJson on the device.

**Spec:** `docs/superpowers/specs/2026-06-07-voice-transcription-design.md`
**Working dir:** `/Users/dongxu/cardos` (branch `main`; commit per task, push only in the final task — pushing triggers the new deploy workflow).
**Prereqs already done:** GitHub secrets `OPENAI_API_KEY` + `CLOUDFLARE_API_TOKEN` are set.

---

## File Map

| Path | Responsibility |
|---|---|
| `backend/voice-worker/src/openai.js` | NEW pure: system prompt, chat request builder, LLM reply parser |
| `backend/voice-worker/src/pipeline.js` | NEW: `transcribe(fetchFn, env, ...)`, `fixTranscript(fetchFn, env, raw)` |
| `backend/voice-worker/src/sql.js` | + `buildUpdateTextSql` |
| `backend/voice-worker/src/index.js` | `/upload` pipeline wiring; `/recordings` returns text columns |
| `backend/voice-worker/test/openai.test.js` | NEW vitest: builders + parser |
| `backend/voice-worker/test/pipeline.test.js` | NEW vitest: mocked-fetch transcribe/fix branches |
| `backend/voice-worker/test/sql.test.js` | + UPDATE builder cases |
| `backend/voice-worker/wrangler.toml` | + `STT_MODEL`/`FIX_MODEL` vars, secret docs |
| `backend/voice-worker/README.md` | migration + secrets + CI notes |
| `.github/workflows/deploy-voice-worker.yml` | NEW: test + deploy + secret sync |
| `src/apps/VoiceMemoApp.cpp` | 30s timeout, parse response JSON, show cleaned text |

---

### Task 1: db9 Schema Migration (one-time)

- [ ] **Step 1: Run the idempotent migration against db9**

```bash
db9 sql cardos_voice -q "ALTER TABLE recordings ADD COLUMN IF NOT EXISTS raw_text TEXT; ALTER TABLE recordings ADD COLUMN IF NOT EXISTS corrected_text TEXT; ALTER TABLE recordings ADD COLUMN IF NOT EXISTS cleaned_text TEXT;"
```

If the `db9` CLI needs different invocation, check `backend/voice-worker/README.md` for how the database was created. Verify:

```bash
db9 sql cardos_voice -q "SELECT column_name FROM information_schema.columns WHERE table_name='recordings' ORDER BY 1"
```

Expected: includes `raw_text`, `corrected_text`, `cleaned_text`.

- [ ] **Step 2: Record the migration in `backend/voice-worker/README.md`**

Add under the schema/setup section (create a "## Schema migrations" section if none exists):

```markdown
## Schema migrations

2026-06-07 — transcription text layers (run once, idempotent):

```sql
ALTER TABLE recordings ADD COLUMN IF NOT EXISTS raw_text TEXT;
ALTER TABLE recordings ADD COLUMN IF NOT EXISTS corrected_text TEXT;
ALTER TABLE recordings ADD COLUMN IF NOT EXISTS cleaned_text TEXT;
```
```

- [ ] **Step 3: Commit**

```bash
git add backend/voice-worker/README.md
git commit -m "docs(voice): record transcription-columns migration"
```

---

### Task 2: openai.js — Prompt, Request Builder, Reply Parser (TDD)

**Files:**
- Create: `backend/voice-worker/src/openai.js`
- Create: `backend/voice-worker/test/openai.test.js`

- [ ] **Step 1: Write failing tests `backend/voice-worker/test/openai.test.js`**

```js
import { describe, it, expect } from "vitest";
import {
  FIX_SYSTEM_PROMPT,
  buildChatRequest,
  parseFixResult,
} from "../src/openai.js";

describe("FIX_SYSTEM_PROMPT", () => {
  it("encodes the conservative contract", () => {
    expect(FIX_SYSTEM_PROMPT).toContain("ONLY obvious ASR errors");
    expect(FIX_SYSTEM_PROMPT).toContain("配森");
    expect(FIX_SYSTEM_PROMPT).toContain("Do NOT summarize");
    expect(FIX_SYSTEM_PROMPT).toContain("leave it unchanged");
  });
});

describe("buildChatRequest", () => {
  it("builds a json_object request with system+user messages", () => {
    const r = buildChatRequest("gpt-4o-mini", "你好 world");
    expect(r.model).toBe("gpt-4o-mini");
    expect(r.response_format).toEqual({ type: "json_object" });
    expect(r.messages[0]).toEqual({
      role: "system",
      content: FIX_SYSTEM_PROMPT,
    });
    expect(r.messages[1]).toEqual({ role: "user", content: "你好 world" });
  });
});

describe("parseFixResult", () => {
  it("parses a plain JSON reply", () => {
    const p = parseFixResult('{"corrected":"用Python","cleaned":"用Python"}');
    expect(p).toEqual({ corrected: "用Python", cleaned: "用Python" });
  });
  it("strips markdown fences", () => {
    const p = parseFixResult('```json\n{"corrected":"a","cleaned":"b"}\n```');
    expect(p).toEqual({ corrected: "a", cleaned: "b" });
  });
  it("falls back to the other field when one is missing", () => {
    expect(parseFixResult('{"corrected":"only"}')).toEqual({
      corrected: "only",
      cleaned: "only",
    });
    expect(parseFixResult('{"cleaned":"only"}')).toEqual({
      corrected: "only",
      cleaned: "only",
    });
  });
  it("returns null for garbage / non-string / empty object", () => {
    expect(parseFixResult("not json at all")).toBeNull();
    expect(parseFixResult(undefined)).toBeNull();
    expect(parseFixResult("{}")).toBeNull();
  });
});
```

- [ ] **Step 2: Run to verify failure**

Run: `cd backend/voice-worker && npm test`
Expected: FAIL — cannot resolve `../src/openai.js`

- [ ] **Step 3: Write `backend/voice-worker/src/openai.js`**

```js
// Pure OpenAI request/response helpers for the transcription pipeline.
// No I/O — unit-tested. The system prompt is deliberately conservative:
// fix only obvious ASR errors, never summarize.

export const FIX_SYSTEM_PROMPT = `You fix speech-recognition transcripts. The speaker mixes Chinese and
English in a programming/technology context. Reply with JSON only:
{"corrected": "...", "cleaned": "..."}.

corrected: the transcript with ONLY obvious ASR errors fixed — Chinese
homophone mistakes, and English technical terms mis-transcribed as
Chinese (e.g. 配森→Python, 杰森→JSON, 吉特→git). Preserve the original
wording and meaning exactly. Do NOT summarize, rephrase, or translate.

cleaned: corrected, with filler words, false starts, and repeated verbal
tics removed (这个, 那个, 就是说, 嗯, 啊, and similar). This version is
for intent recognition. Only delete fillers — do not summarize or
rephrase.

If you are unsure whether something is an error, leave it unchanged.`;

export function buildChatRequest(model, rawTranscript) {
  return {
    model,
    response_format: { type: "json_object" },
    messages: [
      { role: "system", content: FIX_SYSTEM_PROMPT },
      { role: "user", content: rawTranscript },
    ],
  };
}

// Parse the model's reply into {corrected, cleaned}. Tolerates markdown
// fences and a single missing field (mirrors the other); returns null when
// nothing usable came back.
export function parseFixResult(text) {
  if (typeof text !== "string") return null;
  let t = text.trim();
  const fence = t.match(/^```(?:json)?\s*([\s\S]*?)\s*```$/);
  if (fence) t = fence[1];
  let obj;
  try {
    obj = JSON.parse(t);
  } catch {
    return null;
  }
  const corrected = typeof obj.corrected === "string" ? obj.corrected : null;
  const cleaned = typeof obj.cleaned === "string" ? obj.cleaned : null;
  if (corrected === null && cleaned === null) return null;
  return {
    corrected: corrected ?? cleaned,
    cleaned: cleaned ?? corrected,
  };
}
```

- [ ] **Step 4: Run to verify pass**

Run: `cd backend/voice-worker && npm test`
Expected: all suites pass (existing sql tests + new openai tests).

- [ ] **Step 5: Commit**

```bash
git add backend/voice-worker/src/openai.js backend/voice-worker/test/openai.test.js
git commit -m "feat(voice): conservative fix prompt, chat request builder, reply parser"
```

---

### Task 3: sql.js UPDATE Builder (TDD)

**Files:**
- Modify: `backend/voice-worker/src/sql.js`
- Modify: `backend/voice-worker/test/sql.test.js`

- [ ] **Step 1: Add failing tests to `backend/voice-worker/test/sql.test.js`**

Extend the import line to include `buildUpdateTextSql`, then append:

```js
describe("buildUpdateTextSql", () => {
  it("updates all three text columns with escaping", () => {
    const sql = buildUpdateTextSql(
      "11111111-1111-1111-1111-111111111111",
      "raw 'q'",
      "corrected",
      "cleaned"
    );
    expect(sql).toBe(
      "UPDATE recordings SET raw_text = 'raw ''q''', " +
        "corrected_text = 'corrected', cleaned_text = 'cleaned' " +
        "WHERE id = '11111111-1111-1111-1111-111111111111'"
    );
  });
  it("renders NULL for missing layers", () => {
    const sql = buildUpdateTextSql("x", "raw", null, null);
    expect(sql).toContain("corrected_text = NULL");
    expect(sql).toContain("cleaned_text = NULL");
  });
});
```

- [ ] **Step 2: Run to verify failure** — `cd backend/voice-worker && npm test` → FAIL (no export)

- [ ] **Step 3: Append to `backend/voice-worker/src/sql.js`**

```js
// Build the UPDATE that attaches transcription text layers to a recording.
export function buildUpdateTextSql(id, rawText, correctedText, cleanedText) {
  return (
    `UPDATE recordings SET raw_text = ${sqlStr(rawText)}, ` +
    `corrected_text = ${sqlStr(correctedText)}, ` +
    `cleaned_text = ${sqlStr(cleanedText)} ` +
    `WHERE id = ${sqlStr(id)}`
  );
}
```

- [ ] **Step 4: Run to verify pass** — `npm test` → all green

- [ ] **Step 5: Commit**

```bash
git add backend/voice-worker/src/sql.js backend/voice-worker/test/sql.test.js
git commit -m "feat(voice): UPDATE builder for transcription text columns"
```

---

### Task 4: pipeline.js — OpenAI Calls with Injected Fetch (TDD)

**Files:**
- Create: `backend/voice-worker/src/pipeline.js`
- Create: `backend/voice-worker/test/pipeline.test.js`

- [ ] **Step 1: Write failing tests `backend/voice-worker/test/pipeline.test.js`**

```js
import { describe, it, expect } from "vitest";
import { transcribe, fixTranscript } from "../src/pipeline.js";

const env = {
  OPENAI_API_KEY: "sk-test",
  STT_MODEL: "gpt-4o-transcribe",
  FIX_MODEL: "gpt-4o-mini",
};

function fakeFetch(status, bodyObj) {
  return async (url, init) => {
    fakeFetch.lastUrl = url;
    fakeFetch.lastInit = init;
    return new Response(JSON.stringify(bodyObj), { status });
  };
}

describe("transcribe", () => {
  const wav = new File([new Uint8Array([1, 2, 3])], "a.wav", {
    type: "audio/wav",
  });

  it("posts multipart to /audio/transcriptions and returns text", async () => {
    const f = fakeFetch(200, { text: "你好 world" });
    const out = await transcribe(f, env, wav);
    expect(out).toBe("你好 world");
    expect(fakeFetch.lastUrl).toBe(
      "https://api.openai.com/v1/audio/transcriptions"
    );
    expect(fakeFetch.lastInit.headers.Authorization).toBe("Bearer sk-test");
    expect(fakeFetch.lastInit.body).toBeInstanceOf(FormData);
    expect(fakeFetch.lastInit.body.get("model")).toBe("gpt-4o-transcribe");
  });

  it("throws on non-200", async () => {
    await expect(transcribe(fakeFetch(401, { error: "bad key" }), env, wav))
      .rejects.toThrow(/stt 401/);
  });

  it("throws when the reply lacks text", async () => {
    await expect(transcribe(fakeFetch(200, { nope: 1 }), env, wav))
      .rejects.toThrow(/no text/);
  });
});

describe("fixTranscript", () => {
  function chatReply(content) {
    return { choices: [{ message: { content } }] };
  }

  it("returns parsed corrected/cleaned", async () => {
    const f = fakeFetch(
      200,
      chatReply('{"corrected":"用Python","cleaned":"用Python写"}')
    );
    const out = await fixTranscript(f, env, "用配森写");
    expect(out).toEqual({ corrected: "用Python", cleaned: "用Python写" });
    expect(fakeFetch.lastUrl).toBe("https://api.openai.com/v1/chat/completions");
    const body = JSON.parse(fakeFetch.lastInit.body);
    expect(body.model).toBe("gpt-4o-mini");
    expect(body.messages[1].content).toBe("用配森写");
  });

  it("throws on non-200", async () => {
    await expect(fixTranscript(fakeFetch(500, {}), env, "x"))
      .rejects.toThrow(/fix 500/);
  });

  it("throws on unparseable model reply", async () => {
    const f = fakeFetch(200, chatReply("sorry, I cannot do that"));
    await expect(fixTranscript(f, env, "x")).rejects.toThrow(/unparseable/);
  });
});
```

- [ ] **Step 2: Run to verify failure** — `cd backend/voice-worker && npm test` → FAIL (no pipeline.js)

- [ ] **Step 3: Write `backend/voice-worker/src/pipeline.js`**

```js
// OpenAI pipeline steps. fetchFn is injected so tests run without network.
import { buildChatRequest, parseFixResult } from "./openai.js";

// Speech-to-text. `file` is a File/Blob (the uploaded WAV bytes).
export async function transcribe(fetchFn, env, file) {
  const form = new FormData();
  form.append("file", file, file.name || "audio.wav");
  form.append("model", env.STT_MODEL || "gpt-4o-transcribe");
  const resp = await fetchFn(
    "https://api.openai.com/v1/audio/transcriptions",
    {
      method: "POST",
      headers: { Authorization: `Bearer ${env.OPENAI_API_KEY}` },
      body: form,
    }
  );
  const text = await resp.text();
  if (!resp.ok) throw new Error(`stt ${resp.status}: ${text.slice(0, 200)}`);
  const obj = JSON.parse(text);
  if (typeof obj.text !== "string") throw new Error("stt: no text in reply");
  return obj.text;
}

// Conservative correction; returns {corrected, cleaned}.
export async function fixTranscript(fetchFn, env, raw) {
  const resp = await fetchFn("https://api.openai.com/v1/chat/completions", {
    method: "POST",
    headers: {
      Authorization: `Bearer ${env.OPENAI_API_KEY}`,
      "Content-Type": "application/json",
    },
    body: JSON.stringify(buildChatRequest(env.FIX_MODEL || "gpt-4o-mini", raw)),
  });
  const text = await resp.text();
  if (!resp.ok) throw new Error(`fix ${resp.status}: ${text.slice(0, 200)}`);
  const content = JSON.parse(text)?.choices?.[0]?.message?.content;
  const parsed = parseFixResult(content);
  if (!parsed) throw new Error("fix: unparseable model reply");
  return parsed;
}
```

- [ ] **Step 4: Run to verify pass** — `npm test` → all green

- [ ] **Step 5: Commit**

```bash
git add backend/voice-worker/src/pipeline.js backend/voice-worker/test/pipeline.test.js
git commit -m "feat(voice): transcribe + fixTranscript pipeline steps with mocked-fetch tests"
```

---

### Task 5: Wire the Pipeline into /upload + /recordings

**Files:**
- Modify: `backend/voice-worker/src/index.js`
- Modify: `backend/voice-worker/wrangler.toml`

- [ ] **Step 1: index.js — imports**

Add next to the existing import:

```js
import { buildInsertSql, buildUpdateTextSql } from "./sql.js";
import { transcribe, fixTranscript } from "./pipeline.js";
```

(replacing the current `import { buildInsertSql } from "./sql.js";`)

- [ ] **Step 2: index.js — make the upload bytes reusable**

In `handleUpload`, the `file` Blob is currently passed straight to R2. Read
the bytes once so both R2 and OpenAI get a deterministic copy. Replace:

```js
  // Store the audio. A File is a Blob, so R2 knows the size.
  await env.BUCKET.put(key, file, { httpMetadata: { contentType } });
  const size = file.size || 0;
```

with:

```js
  // Read once; R2 and the transcription call each get a deterministic copy.
  const bytes = await file.arrayBuffer();
  await env.BUCKET.put(key, bytes, { httpMetadata: { contentType } });
  const size = bytes.byteLength;
```

- [ ] **Step 3: index.js — pipeline after the INSERT**

Replace the final `return json({ id, key, size }, 200);` of `handleUpload` with:

```js
  // Transcription pipeline. The recording is already persisted — any
  // failure below degrades the response, never loses data.
  let raw = null;
  let corrected = null;
  let cleaned = null;
  let pipeErr = null;
  if (!env.OPENAI_API_KEY) {
    pipeErr = "OPENAI_API_KEY not configured";
  } else {
    try {
      const wav = new File([bytes], filename, { type: contentType });
      raw = await transcribe(fetch, env, wav);
      try {
        const fix = await fixTranscript(fetch, env, raw);
        corrected = fix.corrected;
        cleaned = fix.cleaned;
      } catch (e) {
        pipeErr = String((e && e.message) || e);
      }
      try {
        await db9(env, buildUpdateTextSql(id, raw, corrected, cleaned));
      } catch (e) {
        pipeErr = (pipeErr ? pipeErr + "; " : "") + "db9 text update failed";
      }
    } catch (e) {
      pipeErr = String((e && e.message) || e);
    }
  }
  const out = { id, key, size, raw, corrected, cleaned };
  if (pipeErr) out.error = pipeErr;
  return json(out, 200);
```

- [ ] **Step 4: index.js — expose text layers in /recordings**

In `handleList`, extend the SELECT column list:

```js
    "SELECT id, r2_key, filename, content_type, size_bytes, sample_rate, " +
      "device, created_at, raw_text, corrected_text, cleaned_text " +
      `FROM recordings ORDER BY created_at DESC LIMIT ${limit}`
```

- [ ] **Step 5: wrangler.toml — models + secret docs**

Replace the `[vars]` block and trailing comment with:

```toml
[vars]
DB9_DB_ID = "emwisr1axc1o"
STT_MODEL = "gpt-4o-transcribe"
FIX_MODEL = "gpt-4o-mini"

# Secrets (never committed):
#   DB9_TOKEN       - scoped db9 rw token (set once via `wrangler secret put`)
#   UPLOAD_KEY      - shared key the device sends as X-Upload-Key (ditto)
#   OPENAI_API_KEY  - synced from GitHub Secrets by the deploy workflow
```

- [ ] **Step 6: Tests still green** — `cd backend/voice-worker && npm test`

- [ ] **Step 7: Commit**

```bash
git add backend/voice-worker/src/index.js backend/voice-worker/wrangler.toml
git commit -m "feat(voice): transcription pipeline in /upload, text layers in /recordings"
```

---

### Task 6: GitHub Actions Deploy Workflow

**Files:**
- Create: `.github/workflows/deploy-voice-worker.yml`
- Modify: `backend/voice-worker/README.md`

- [ ] **Step 1: Write `.github/workflows/deploy-voice-worker.yml`**

```yaml
name: deploy-voice-worker
on:
  push:
    branches: [main]
    paths:
      - "backend/voice-worker/**"
      - ".github/workflows/deploy-voice-worker.yml"
jobs:
  deploy:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: 20
      - name: Install and test
        working-directory: backend/voice-worker
        run: |
          npm ci
          npm test
      - name: Deploy worker (syncs OPENAI_API_KEY)
        uses: cloudflare/wrangler-action@v3
        with:
          apiToken: ${{ secrets.CLOUDFLARE_API_TOKEN }}
          workingDirectory: backend/voice-worker
          secrets: |
            OPENAI_API_KEY
        env:
          OPENAI_API_KEY: ${{ secrets.OPENAI_API_KEY }}
```

- [ ] **Step 2: README — CI note**

Add to `backend/voice-worker/README.md` (deploy section):

```markdown
## CI deploys

Pushes to `main` touching `backend/voice-worker/**` run tests and deploy via
GitHub Actions (`.github/workflows/deploy-voice-worker.yml`). Required GitHub
repository secrets: `CLOUDFLARE_API_TOKEN` (Workers edit), `OPENAI_API_KEY`
(synced into the Worker on every deploy). `DB9_TOKEN`/`UPLOAD_KEY` remain
manually-set Worker secrets; deploys do not touch them.
```

- [ ] **Step 3: Commit**

```bash
git add .github/workflows/deploy-voice-worker.yml backend/voice-worker/README.md
git commit -m "ci: GitHub Actions deploy for cardos-voice with OPENAI_API_KEY sync"
```

---

### Task 7: Device — Show the Cleaned Transcript

**Files:**
- Modify: `src/apps/VoiceMemoApp.cpp`

- [ ] **Step 1: Includes + timeout management**

Add `#include <ArduinoJson.h>` to the includes of `src/apps/VoiceMemoApp.cpp`.

In `onEnter()` add (the pipeline takes 5–10 s after the upload itself):

```cpp
  cardos::http::setTimeout(30000);
```

In `onExit()` add:

```cpp
  cardos::http::setTimeout(10000);  // restore the global default
```

- [ ] **Step 2: Parse the upload response and show the transcript**

The upload block in `update()` currently does (around line 118):

```cpp
    auto r = cardos::http::uploadFile(std::string(kWorkerUrl) + "/upload", ...);
    if (r.ok()) {
      status_ = "Uploaded OK";
      result_.setText("HTTP " + std::to_string(r.status) + "\n" + ...);
```

Replace the success branch body with:

```cpp
    if (r.ok()) {
      JsonDocument doc;
      std::string text, err;
      if (deserializeJson(doc, r.body) == DeserializationError::Ok) {
        auto pick = [&](const char* k) -> std::string {
          const char* v = doc[k].as<const char*>();
          return v ? std::string(v) : std::string();
        };
        text = pick("cleaned");
        if (text.empty()) text = pick("corrected");
        if (text.empty()) text = pick("raw");
        err = pick("error");
      }
      if (!text.empty()) {
        status_ = "Transcribed";
        result_.setText(text + (err.empty() ? "" : "\n[warn: " + err + "]"));
      } else {
        status_ = "Uploaded (no transcript)";
        result_.setText(err.empty() ? r.body.substr(0, 200)
                                    : "Pipeline error:\n" + err);
      }
    }
```

(keep the existing failure branch — wrong key / network errors — unchanged;
keep the existing post-upload local-WAV delete behavior exactly as is).

- [ ] **Step 3: Label the longer wait**

In `render()`, change the Uploading label string from `"Uploading..."` to
`"Uploading + transcribing..."`.

- [ ] **Step 4: Build + native tests**

`pio run -e m5stack-cardputer` → SUCCESS; `pio test -e native` → all pass
(71+ — count unchanged by this task).

- [ ] **Step 5: Commit**

```bash
git add src/apps/VoiceMemoApp.cpp
git commit -m "feat(voice): show cleaned transcript on device after upload"
```

---

### Task 8: Push, CI Deploy, End-to-End Verify

- [ ] **Step 1: Push and watch the workflow**

```bash
git push origin main
gh run watch --repo c4pt0r/cardos --exit-status || gh run list --repo c4pt0r/cardos --limit 3
```

Expected: `deploy-voice-worker` run succeeds (tests + deploy + secret sync).

- [ ] **Step 2: curl end-to-end with a real WAV**

Reuse any small WAV (e.g. generate 1 s of silence is NOT useful — record a short
real utterance via the device first, or fetch an existing one):

```bash
curl -s https://cardos-voice.db9.workers.dev/recordings | head -c 400
# pick the newest id, fetch it back as test input:
curl -s -o /tmp/sample.wav https://cardos-voice.db9.workers.dev/audio/<id>
curl -s -X POST https://cardos-voice.db9.workers.dev/upload \
  -H "X-Upload-Key: $UPLOAD_KEY" -F "file=@/tmp/sample.wav" | python3 -m json.tool
```

Expected JSON: `id`, `raw` (non-empty transcript), `corrected`, `cleaned`, no
`error`. Then confirm persistence:

```bash
db9 sql cardos_voice -q "SELECT id, raw_text IS NOT NULL, corrected_text IS NOT NULL, cleaned_text IS NOT NULL FROM recordings ORDER BY created_at DESC LIMIT 3"
```

(`$UPLOAD_KEY` comes from wherever it was stored when the Worker was set up —
ask the user if it isn't in the shell env; do NOT print it.)

- [ ] **Step 3: Flash the device**

`pio run -e m5stack-cardputer -t upload` (device must be on USB).

- [ ] **Step 4: On-device acceptance (user participates)**

1. Open Voice Memo; hold a key and say, with fillers, something like
   “这个，用配森写一个杰森解析器，那个，要快”;
2. Release → progress bar → "Uploading + transcribing..." → screen shows the
   cleaned text (≈“用Python写一个JSON解析器，要快”);
3. `raw` kept fillers / `corrected` fixed terms / `cleaned` dropped fillers —
   verify the three layers in db9;
4. Pull WiFi (or use a bad key temporarily) → recording still saved locally /
   error shown without crash.

- [ ] **Step 5: Update PROGRESS.md** — add a section for the transcription
pipeline (status, endpoints, models, CI deploy) following the existing style.

- [ ] **Step 6: Final commit + push**

```bash
git add PROGRESS.md
git commit -m "docs: PROGRESS — voice transcription pipeline"
git push origin main
```

---

## Plan Self-Review Notes

- **Spec coverage:** schema (T1), prompt + parsing (T2), SQL (T3), OpenAI
  calls (T4), /upload wiring + failure isolation + /recordings fields + model
  vars (T5), GitHub Secrets deploy chain (T6), device display + 30s timeout
  (T7), curl E2E + device acceptance + docs (T8). Spec §8 out-of-scope items
  absent.
- **Type consistency:** `transcribe(fetchFn, env, file)` / `fixTranscript(fetchFn,
  env, raw)` (T4) match T5 call sites (`fetch` passed as fetchFn);
  `buildUpdateTextSql(id, raw, corrected, cleaned)` (T3) matches T5;
  `parseFixResult` contract (T2) matches T4's use; device `pick()` fields
  match T5's response keys (`raw`/`corrected`/`cleaned`/`error`).
- **Failure isolation traced:** STT throw → outer catch → `raw` null, error
  set, INSERT already done; fix throw → corrected/cleaned null but raw still
  UPDATEd; db9 UPDATE throw → response still carries text. Device falls back
  cleaned→corrected→raw→error.
- **Note:** Task 8 Step 2 reuses a device-recorded WAV as E2E input — Whisper
  needs real speech for a meaningful check; silence would "pass" trivially.
