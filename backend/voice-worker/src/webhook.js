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
