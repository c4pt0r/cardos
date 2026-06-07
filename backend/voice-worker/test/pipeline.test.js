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
