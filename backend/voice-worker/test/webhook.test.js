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
