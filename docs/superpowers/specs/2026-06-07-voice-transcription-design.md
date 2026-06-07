# Voice Transcription Pipeline Design

Date: 2026-06-07
Builds on: Voice Memo app + cardos-voice Worker (`2026-06-06-voice-memo-design.md`)

## 1. Goal

Record on the Cardputer → upload → **OpenAI speech-to-text** → **conservative
LLM correction** tuned for mixed Chinese/English (tech vocabulary) → result
shown on the device screen and stored in db9.

Two text outputs per recording, produced by ONE chat-completion call:

- `corrected` — the transcript with only obvious ASR errors fixed (Chinese
  homophones; English tech terms mis-heard as Chinese, e.g. 配森→Python,
  杰森→JSON). No summarizing, rephrasing, or translating.
- `cleaned` — `corrected` minus filler words, false starts, and repeated
  verbal tics (这个/那个/就是说/嗯/啊…), for intent recognition. Deletion
  only; still no rewriting.

## 2. Architecture: Synchronous Pipeline in the Existing Worker

`POST /upload` (existing R2 + db9 insert logic unchanged) gains steps:

```
WAV → R2 + db9 INSERT                    (existing)
    → POST /v1/audio/transcriptions      (STT_MODEL, default gpt-4o-transcribe) → raw
    → POST /v1/chat/completions          (FIX_MODEL, default gpt-4o-mini,
                                          response_format json_object)         → {corrected, cleaned}
    → db9 UPDATE recordings SET raw_text, corrected_text, cleaned_text
    → 200 {id, raw, corrected, cleaned}
```

Waiting on upstream APIs costs wall time, not Worker CPU time — fine on the
free plan. Rejected alternatives: async + device polling (double the device
logic; revisit only if hardware timeouts bite — see Risks), queues/Durable
Objects (overkill).

**Failure isolation:** the recording is already persisted before any OpenAI
call. If STT fails → `200 {id, error}` with text fields NULL. If only the
LLM fails → `200 {id, raw, error}` and `raw_text` is still written. The
device shows whatever text came back, or the error.

## 3. LLM System Prompt (stored in code, verbatim)

```
You fix speech-recognition transcripts. The speaker mixes Chinese and
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

If you are unsure whether something is an error, leave it unchanged.
```

User message = the raw transcript. Defensive parsing: strip markdown fences,
tolerate missing fields (fall back to `corrected`→`raw`), non-JSON → treat as
LLM failure.

## 4. Data Model

`recordings` gains three nullable columns (idempotent migration executed by
the Worker on demand or via a one-time db9 statement):

```sql
ALTER TABLE recordings ADD COLUMN IF NOT EXISTS raw_text TEXT;
ALTER TABLE recordings ADD COLUMN IF NOT EXISTS corrected_text TEXT;
ALTER TABLE recordings ADD COLUMN IF NOT EXISTS cleaned_text TEXT;
```

`GET /recordings` returns the three fields.

## 5. Configuration & Secrets

- `wrangler.toml [vars]`: `STT_MODEL = "gpt-4o-transcribe"`,
  `FIX_MODEL = "gpt-4o-mini"` — switchable without code changes.
- `OPENAI_API_KEY` lives in **GitHub repository secrets** (user requirement)
  and reaches the Worker via the deploy workflow.
- New `.github/workflows/deploy-voice-worker.yml`: on push to `main`
  touching `backend/voice-worker/**` → `cloudflare/wrangler-action` runs
  tests, deploys, and syncs the `OPENAI_API_KEY` Worker secret.
- Required GitHub secrets: `OPENAI_API_KEY`, `CLOUDFLARE_API_TOKEN`
  (Workers edit permission). Existing `DB9_TOKEN`/`UPLOAD_KEY` remain
  already-set Worker secrets; deploys do not disturb them.

## 6. Device (VoiceMemoApp delta)

- `cardos::http::setTimeout(30000)` while in the app (restore 10s on exit) —
  the synchronous pipeline takes ~5–10 s after upload.
- Parse the `/upload` JSON (ArduinoJson); show **cleaned** in the existing
  TextView, falling back corrected → raw → error message. CJK font already
  renders Chinese.
- Screen flow: release key → upload progress bar → "Transcribing..." →
  text on screen; Esc returns.

## 7. Testing

- **vitest (pure logic)**: LLM response parsing (fences / missing fields /
  garbage), UPDATE SQL building + escaping, OpenAI request construction;
  mocked-fetch pipeline branches (STT fails → 200+error, text NULL; LLM
  fails → raw persisted and returned).
- **curl end-to-end** with a real key + WAV: three text layers land in db9.
- **Device acceptance**: speak mixed zh/en with fillers
  (e.g. “这个，用配森写一个杰森解析器，那个，要快”) → screen shows the
  cleaned text (≈“用Python写一个JSON解析器，要快”); db9 row has all three
  columns; STT-failure path shows the error without losing the recording.

## 8. Out of Scope

TTS reply / voice assistant loop, streaming transcription, speaker
diarization, on-device display of raw-vs-corrected diff, async polling
endpoint (only if 30 s proves too tight), key rotation automation.

## 9. Risks

- Long memos: `/flash` now fits ~2.4 min of audio; pipeline total may
  approach the device's 30 s HTTP timeout for multi-minute clips. Keep
  memos short; revisit with the async/polling design if it bites.
- `gpt-4o-transcribe` request size limit (~25 MB) is far above our WAVs.
- Cost: ≈$0.006/min audio + negligible mini-model tokens.
- LLM may over-correct despite the conservative prompt; the faithful
  `corrected` and untouched `raw` layers are kept precisely so nothing is
  ever lost.
