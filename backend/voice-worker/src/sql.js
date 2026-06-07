// Pure SQL string builders for the recordings table. No I/O — unit-tested.
// db9 has no parameterized HTTP SQL endpoint, so we escape values ourselves.
// Safety rests on Postgres `standard_conforming_strings = on` (the default,
// verified on the db9 instance): doubling single quotes fully contains any
// string value, so backslashes are literal and cannot start an escape.

// Escape a value as a SQL string literal (single quotes doubled).
// null/undefined -> bare NULL.
export function sqlStr(v) {
  if (v === null || v === undefined) return "NULL";
  return "'" + String(v).replace(/'/g, "''") + "'";
}

// Render an integer literal, or NULL for empty/non-finite input.
export function sqlNum(v) {
  if (v === null || v === undefined || v === "") return "NULL";
  const n = Number(v);
  return Number.isFinite(n) ? String(Math.trunc(n)) : "NULL";
}

// Build the INSERT for one recording's metadata. `id` is a uuid string;
// Postgres casts the quoted literal into the uuid column.
export function buildInsertSql(m) {
  return (
    "INSERT INTO recordings " +
    "(id, r2_key, filename, content_type, size_bytes, sample_rate, device) VALUES (" +
    `${sqlStr(m.id)}, ${sqlStr(m.r2_key)}, ${sqlStr(m.filename)}, ` +
    `${sqlStr(m.content_type)}, ${sqlNum(m.size_bytes)}, ` +
    `${sqlNum(m.sample_rate)}, ${sqlStr(m.device)})`
  );
}

// Build the UPDATE that attaches transcription text layers to a recording.
export function buildUpdateTextSql(id, rawText, correctedText, cleanedText) {
  return (
    `UPDATE recordings SET raw_text = ${sqlStr(rawText)}, ` +
    `corrected_text = ${sqlStr(correctedText)}, ` +
    `cleaned_text = ${sqlStr(cleanedText)} ` +
    `WHERE id = ${sqlStr(id)}`
  );
}
