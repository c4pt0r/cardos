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
