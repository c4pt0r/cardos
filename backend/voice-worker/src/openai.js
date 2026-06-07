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
