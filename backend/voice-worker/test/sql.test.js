import { describe, it, expect } from "vitest";
import { buildInsertSql, sqlStr, sqlNum } from "../src/sql.js";

describe("sqlStr", () => {
  it("quotes and escapes single quotes", () => {
    expect(sqlStr("hello")).toBe("'hello'");
    expect(sqlStr("o'hara")).toBe("'o''hara'");
  });
  it("renders NULL for null/undefined", () => {
    expect(sqlStr(null)).toBe("NULL");
    expect(sqlStr(undefined)).toBe("NULL");
  });
});

describe("sqlNum", () => {
  it("renders integers", () => {
    expect(sqlNum(16000)).toBe("16000");
    expect(sqlNum(1.9)).toBe("1");
  });
  it("renders NULL for empty/non-finite", () => {
    expect(sqlNum(null)).toBe("NULL");
    expect(sqlNum("")).toBe("NULL");
    expect(sqlNum("abc")).toBe("NULL");
  });
});

describe("buildInsertSql", () => {
  it("builds a safe single-statement insert", () => {
    const sql = buildInsertSql({
      id: "11111111-1111-1111-1111-111111111111",
      r2_key: "recordings/x.wav",
      filename: "o'hara.wav",
      content_type: "audio/wav",
      size_bytes: 1024,
      sample_rate: 16000,
      device: "cardputer",
    });
    expect(sql.startsWith("INSERT INTO recordings")).toBe(true);
    expect(sql).toContain("'recordings/x.wav'");
    expect(sql).toContain("'o''hara.wav'"); // escaped quote
    expect(sql).toContain("1024");
    expect(sql).toContain("16000");
    expect(sql).not.toContain(";"); // no statement breakout
  });

  it("uses NULL for missing numeric fields", () => {
    const sql = buildInsertSql({
      id: "22222222-2222-2222-2222-222222222222",
      r2_key: "recordings/y.wav",
      filename: "y.wav",
      content_type: "audio/wav",
      size_bytes: 0,
      sample_rate: null,
      device: "cardputer",
    });
    expect(sql).toContain("NULL"); // sample_rate
  });
});
