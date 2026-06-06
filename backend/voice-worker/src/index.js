// cardos-voice: receives WAV uploads from the CardOS Voice Memo app,
// stores audio in R2, and writes metadata to db9 (serverless Postgres).
import { buildInsertSql } from "./sql.js";

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    try {
      if (request.method === "POST" && url.pathname === "/upload") {
        return await handleUpload(request, env);
      }
      if (request.method === "GET" && url.pathname === "/recordings") {
        return await handleList(url, env);
      }
      if (request.method === "GET" && url.pathname.startsWith("/audio/")) {
        return await handleAudio(url, env);
      }
      if (url.pathname === "/") {
        return new Response("cardos-voice worker ok\n", {
          headers: { "Content-Type": "text/plain" },
        });
      }
      return json({ error: "not found" }, 404);
    } catch (e) {
      return json({ error: String((e && e.message) || e) }, 500);
    }
  },
};

function json(obj, status = 200) {
  return new Response(JSON.stringify(obj), {
    status,
    headers: { "Content-Type": "application/json" },
  });
}

// Run one SQL statement against db9's HTTP API with the scoped token.
async function db9(env, query) {
  const resp = await fetch(
    `https://api.db9.ai/customer/databases/${env.DB9_DB_ID}/sql`,
    {
      method: "POST",
      headers: {
        Authorization: `Bearer ${env.DB9_TOKEN}`,
        "Content-Type": "application/json",
      },
      body: JSON.stringify({ query }),
    }
  );
  const text = await resp.text();
  if (!resp.ok) throw new Error(`db9 ${resp.status}: ${text}`);
  return JSON.parse(text);
}

async function handleUpload(request, env) {
  if ((request.headers.get("X-Upload-Key") || "") !== env.UPLOAD_KEY) {
    return json({ error: "unauthorized" }, 401);
  }
  const form = await request.formData();
  const file = form.get("file");
  if (!file || typeof file === "string") {
    return json({ error: "missing file field" }, 400);
  }

  const id = crypto.randomUUID();
  const key = `recordings/${id}.wav`;
  const contentType = file.type || "audio/wav";
  const device = request.headers.get("X-Device") || "unknown";
  const sampleRate =
    parseInt(request.headers.get("X-Sample-Rate") || "", 10) || null;
  const filename = file.name || `${id}.wav`;

  // Store the audio. A File is a Blob, so R2 knows the size.
  await env.BUCKET.put(key, file, { httpMetadata: { contentType } });
  const size = file.size || 0;

  const meta = {
    id,
    r2_key: key,
    filename,
    content_type: contentType,
    size_bytes: size,
    sample_rate: sampleRate,
    device,
  };
  try {
    await db9(env, buildInsertSql(meta));
  } catch (e) {
    // R2 object is kept; metadata can be reconciled later.
    return json(
      { error: "metadata write failed", detail: String((e && e.message) || e), id, key, size },
      502
    );
  }
  return json({ id, key, size }, 200);
}

async function handleList(url, env) {
  let limit = parseInt(url.searchParams.get("limit") || "20", 10);
  if (!Number.isFinite(limit) || limit < 1) limit = 20;
  if (limit > 100) limit = 100;
  const r = await db9(
    env,
    "SELECT id, r2_key, filename, content_type, size_bytes, sample_rate, " +
      `device, created_at FROM recordings ORDER BY created_at DESC LIMIT ${limit}`
  );
  const cols = (r.columns || []).map((c) => c.name);
  const rows = (r.rows || []).map((row) =>
    Object.fromEntries(row.map((v, i) => [cols[i], v]))
  );
  return json({ recordings: rows }, 200);
}

async function handleAudio(url, env) {
  const id = url.pathname.slice("/audio/".length);
  if (!/^[0-9a-fA-F-]{36}$/.test(id)) return json({ error: "bad id" }, 400);
  const r = await db9(
    env,
    `SELECT r2_key FROM recordings WHERE id = '${id}'`
  );
  if (!r.rows || r.rows.length === 0) return json({ error: "not found" }, 404);
  const obj = await env.BUCKET.get(r.rows[0][0]);
  if (!obj) return json({ error: "object missing" }, 404);
  return new Response(obj.body, {
    headers: {
      "Content-Type": "audio/wav",
      "Content-Length": String(obj.size),
    },
  });
}
