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
