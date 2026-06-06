#include <unity.h>
#include <cstring>
#include <cstdio>
#include "../../src/core/KeyEvent.h"
#include "../../src/core/KeyTracker.h"
#include "../../src/ui/scroll.h"
#include "../../src/core/IdlePolicy.h"
#include "../../src/services/WiFiStore.h"
#include "../../src/sdk/FsPath.h"
#include "../../src/sdk/WavWriter.h"
#include "../../src/sdk/Multipart.h"
#include "../../src/ui/textwrap.h"

void test_enter_key() {
  KeyEvent ev = mapKey(0, false, true, false, false);
  TEST_ASSERT_EQUAL((int)KeyCode::Enter, (int)ev.code);
}

void test_backspace_key() {
  KeyEvent ev = mapKey(0, false, false, true, false);
  TEST_ASSERT_EQUAL((int)KeyCode::Backspace, (int)ev.code);
}

void test_esc_is_backtick() {
  KeyEvent ev = mapKey('`', false, false, false, false);
  TEST_ASSERT_EQUAL((int)KeyCode::Esc, (int)ev.code);
}

void test_nav_keys_carry_char() {
  KeyEvent up = mapKey(';', false, false, false, false);
  TEST_ASSERT_EQUAL((int)KeyCode::Up, (int)up.code);
  TEST_ASSERT_EQUAL(';', up.ch);  // TextInput can still type ';'
  KeyEvent down = mapKey('.', false, false, false, false);
  TEST_ASSERT_EQUAL((int)KeyCode::Down, (int)down.code);
}

void test_printable_char() {
  KeyEvent ev = mapKey('a', false, false, false, false);
  TEST_ASSERT_EQUAL((int)KeyCode::Char, (int)ev.code);
  TEST_ASSERT_EQUAL('a', ev.ch);
}

void test_fn_flag_passthrough() {
  KeyEvent ev = mapKey('a', true, false, false, false);
  TEST_ASSERT_TRUE(ev.fn);
}

void test_nav_keys_left_right() {
  KeyEvent left = mapKey(',', false, false, false, false);
  TEST_ASSERT_EQUAL((int)KeyCode::Left, (int)left.code);
  TEST_ASSERT_EQUAL(',', left.ch);
  KeyEvent right = mapKey('/', false, false, false, false);
  TEST_ASSERT_EQUAL((int)KeyCode::Right, (int)right.code);
  TEST_ASSERT_EQUAL('/', right.ch);
}

void test_null_char_maps_to_none() {
  KeyEvent ev = mapKey(0, false, false, false, false);
  TEST_ASSERT_EQUAL((int)KeyCode::None, (int)ev.code);
}

void test_scroll_fits_on_one_page() {
  TEST_ASSERT_EQUAL(0, scrollFirstVisible(3, 4, 6));  // 4 items, 6 visible
}

void test_scroll_centers_selection() {
  // 20 items, 6 visible, selected 10 -> window starts at 10 - 3 = 7
  TEST_ASSERT_EQUAL(7, scrollFirstVisible(10, 20, 6));
}

void test_scroll_clamps_top() {
  TEST_ASSERT_EQUAL(0, scrollFirstVisible(1, 20, 6));
}

void test_scroll_clamps_bottom() {
  TEST_ASSERT_EQUAL(14, scrollFirstVisible(19, 20, 6));  // 20 - 6 = 14
}

void test_idle_starts_active() {
  IdlePolicy p(60000, 300000);
  TEST_ASSERT_EQUAL((int)IdleState::Active, (int)p.state(0));
}

void test_idle_dims_after_dim_timeout() {
  IdlePolicy p(60000, 300000);
  p.onInput(1000);
  TEST_ASSERT_EQUAL((int)IdleState::Active, (int)p.state(60999));
  TEST_ASSERT_EQUAL((int)IdleState::Dimmed, (int)p.state(61000));
}

void test_idle_sleeps_after_sleep_timeout() {
  IdlePolicy p(60000, 300000);
  p.onInput(1000);
  TEST_ASSERT_EQUAL((int)IdleState::SleepPending, (int)p.state(301000));
}

void test_input_resets_timer() {
  IdlePolicy p(60000, 300000);
  p.onInput(1000);
  p.onInput(200000);
  TEST_ASSERT_EQUAL((int)IdleState::Active, (int)p.state(259999));
}

void test_wake_input_is_swallowed_when_dimmed() {
  IdlePolicy p(60000, 300000);
  p.onInput(0);
  TEST_ASSERT_TRUE(p.onInput(61000));    // dimmed -> swallow this key
  TEST_ASSERT_FALSE(p.onInput(62000));   // active again -> forward
}

void test_keep_awake_suppresses_sleep_not_dim() {
  IdlePolicy p(60000, 300000);
  p.onInput(0);
  p.keepAwake(true);
  TEST_ASSERT_EQUAL((int)IdleState::Dimmed, (int)p.state(301000));
  p.keepAwake(false);
  TEST_ASSERT_EQUAL((int)IdleState::SleepPending, (int)p.state(301000));
}

struct InMemoryStorage : StorageBackend {
  std::string data;
  std::string load() override { return data; }
  void save(const std::string& s) override { data = s; }
};

void test_store_starts_empty() {
  InMemoryStorage mem;
  WiFiStore store(mem);
  store.load();
  TEST_ASSERT_EQUAL(0, (int)store.networks().size());
}

void test_store_upsert_and_find() {
  InMemoryStorage mem;
  WiFiStore store(mem);
  store.upsert("home", "pw123");
  const WifiNetwork* n = store.find("home");
  TEST_ASSERT_NOT_NULL(n);
  TEST_ASSERT_EQUAL_STRING("pw123", n->password.c_str());
}

void test_store_upsert_updates_password() {
  InMemoryStorage mem;
  WiFiStore store(mem);
  store.upsert("home", "old");
  store.upsert("home", "new");
  TEST_ASSERT_EQUAL(1, (int)store.networks().size());
  TEST_ASSERT_EQUAL_STRING("new", store.find("home")->password.c_str());
}

void test_store_persists_roundtrip() {
  InMemoryStorage mem;
  {
    WiFiStore store(mem);
    store.upsert("home", "pw123");
    store.touch("home", 42);
  }
  WiFiStore store2(mem);
  store2.load();
  TEST_ASSERT_EQUAL(1, (int)store2.networks().size());
  TEST_ASSERT_EQUAL_STRING("pw123", store2.find("home")->password.c_str());
  TEST_ASSERT_EQUAL(42, (int)store2.find("home")->lastOkTs);
}

void test_store_evicts_oldest_when_full() {
  InMemoryStorage mem;
  WiFiStore store(mem);
  for (int i = 0; i < 8; i++) {
    std::string ssid = "net" + std::to_string(i);
    store.upsert(ssid, "pw");
    store.touch(ssid, 100 + i);  // net0 oldest
  }
  store.upsert("net8", "pw");    // 9th entry -> evict net0
  TEST_ASSERT_EQUAL(8, (int)store.networks().size());
  TEST_ASSERT_NULL(store.find("net0"));
  TEST_ASSERT_NOT_NULL(store.find("net8"));
}

void test_store_remove() {
  InMemoryStorage mem;
  WiFiStore store(mem);
  store.upsert("home", "pw");
  store.remove("home");
  TEST_ASSERT_NULL(store.find("home"));
  TEST_ASSERT_EQUAL(0, (int)store.networks().size());
}

void test_store_load_garbage_is_empty() {
  InMemoryStorage mem;
  mem.data = "not json {{{";
  WiFiStore store(mem);
  store.load();
  TEST_ASSERT_EQUAL(0, (int)store.networks().size());
}

static std::vector<KeyTracker::Out> upd(KeyTracker& t,
                                        std::initializer_list<uint16_t> keys,
                                        uint32_t now) {
  std::vector<uint16_t> v(keys);
  return t.update(v.data(), v.size(), now);
}

void test_tracker_press_and_release() {
  KeyTracker t;
  auto e1 = upd(t, {'a'}, 0);
  TEST_ASSERT_EQUAL(1, (int)e1.size());
  TEST_ASSERT_EQUAL('a', e1[0].id);
  TEST_ASSERT_EQUAL((int)KeyAction::Press, (int)e1[0].action);
  auto e2 = upd(t, {'a'}, 100);   // still held, below threshold
  TEST_ASSERT_EQUAL(0, (int)e2.size());
  auto e3 = upd(t, {}, 200);      // released
  TEST_ASSERT_EQUAL(1, (int)e3.size());
  TEST_ASSERT_EQUAL((int)KeyAction::Release, (int)e3[0].action);
}

void test_tracker_long_press_fires_once() {
  KeyTracker t;
  upd(t, {'a'}, 0);
  auto e1 = upd(t, {'a'}, 599);
  TEST_ASSERT_EQUAL(0, (int)e1.size());
  auto e2 = upd(t, {'a'}, 600);   // threshold
  TEST_ASSERT_EQUAL(1, (int)e2.size());
  TEST_ASSERT_EQUAL((int)KeyAction::LongPress, (int)e2[0].action);
  auto e3 = upd(t, {'a'}, 1200);  // no repeat
  TEST_ASSERT_EQUAL(0, (int)e3.size());
  auto e4 = upd(t, {}, 1300);
  TEST_ASSERT_EQUAL((int)KeyAction::Release, (int)e4[0].action);
}

void test_tracker_multiple_keys() {
  KeyTracker t;
  auto e1 = upd(t, {'a', 'b'}, 0);
  TEST_ASSERT_EQUAL(2, (int)e1.size());
  auto e2 = upd(t, {'b'}, 100);   // 'a' released, 'b' held
  TEST_ASSERT_EQUAL(1, (int)e2.size());
  TEST_ASSERT_EQUAL('a', e2[0].id);
  TEST_ASSERT_EQUAL((int)KeyAction::Release, (int)e2[0].action);
}

void test_tracker_special_ids() {
  KeyTracker t;
  auto e1 = upd(t, {KeyTracker::kEnterId}, 0);
  TEST_ASSERT_EQUAL((int)KeyTracker::kEnterId, (int)e1[0].id);
  TEST_ASSERT_EQUAL((int)KeyAction::Press, (int)e1[0].action);
}

void test_tracker_overflow_ignored() {
  KeyTracker t;
  // 9 keys; capacity is 8 — the 9th must be silently ignored.
  auto e1 = upd(t, {'a','b','c','d','e','f','g','h','i'}, 0);
  TEST_ASSERT_EQUAL(8, (int)e1.size());
}

void test_fspath_flash() {
  std::string mount, rel;
  TEST_ASSERT_TRUE(cardos::fs::splitPath("/flash/rec/a.wav", mount, rel));
  TEST_ASSERT_EQUAL_STRING("/flash", mount.c_str());
  TEST_ASSERT_EQUAL_STRING("/rec/a.wav", rel.c_str());
}

void test_fspath_sd_root() {
  std::string mount, rel;
  TEST_ASSERT_TRUE(cardos::fs::splitPath("/sd", mount, rel));
  TEST_ASSERT_EQUAL_STRING("/sd", mount.c_str());
  TEST_ASSERT_EQUAL_STRING("/", rel.c_str());
}

void test_fspath_invalid() {
  std::string mount, rel;
  TEST_ASSERT_FALSE(cardos::fs::splitPath("/nvs/x", mount, rel));
  TEST_ASSERT_FALSE(cardos::fs::splitPath("flash/x", mount, rel));
  TEST_ASSERT_FALSE(cardos::fs::splitPath("/flashy/x", mount, rel));
}

void test_wav_header_fields() {
  uint8_t h[44];
  cardos::audio::writeWavHeader(h, 16000, 1, 3200);  // 0.1s of 16k mono
  TEST_ASSERT_EQUAL_MEMORY("RIFF", h, 4);
  TEST_ASSERT_EQUAL_MEMORY("WAVE", h + 8, 4);
  TEST_ASSERT_EQUAL_MEMORY("data", h + 36, 4);
  uint32_t riffSize;  memcpy(&riffSize, h + 4, 4);
  TEST_ASSERT_EQUAL(36 + 3200, (int)riffSize);
  uint32_t rate;      memcpy(&rate, h + 24, 4);
  TEST_ASSERT_EQUAL(16000, (int)rate);
  uint32_t byteRate;  memcpy(&byteRate, h + 28, 4);
  TEST_ASSERT_EQUAL(32000, (int)byteRate);   // 16k * 1ch * 2B
  uint16_t bits;      memcpy(&bits, h + 34, 2);
  TEST_ASSERT_EQUAL(16, (int)bits);
  uint32_t dataSize;  memcpy(&dataSize, h + 40, 4);
  TEST_ASSERT_EQUAL(3200, (int)dataSize);
}

void test_wav_writer_roundtrip() {
  const char* path = "/tmp/cardos_test.wav";
  cardos::audio::WavWriter w;
  TEST_ASSERT_TRUE(w.open(path, 16000));
  int16_t samples[256];
  for (int i = 0; i < 256; i++) samples[i] = (int16_t)(i * 100);
  w.write(samples, 256);
  w.write(samples, 256);
  TEST_ASSERT_EQUAL(1024, (int)w.dataBytes());  // 512 samples * 2B
  w.close();
  FILE* f = fopen(path, "rb");
  TEST_ASSERT_NOT_NULL(f);
  fseek(f, 0, SEEK_END);
  TEST_ASSERT_EQUAL(44 + 1024, (int)ftell(f));
  uint8_t h[44];
  fseek(f, 0, SEEK_SET);
  fread(h, 1, 44, f);
  uint32_t dataSize;  memcpy(&dataSize, h + 40, 4);
  TEST_ASSERT_EQUAL(1024, (int)dataSize);       // header patched on close
  fclose(f);
  ::remove(path);
}

void test_multipart_prefix() {
  std::string p = cardos::http::multipartPrefix("BNDRY", "file", "a.wav");
  TEST_ASSERT_EQUAL_STRING(
      "--BNDRY\r\n"
      "Content-Disposition: form-data; name=\"file\"; filename=\"a.wav\"\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n",
      p.c_str());
}

void test_multipart_suffix() {
  std::string s = cardos::http::multipartSuffix("BNDRY");
  TEST_ASSERT_EQUAL_STRING("\r\n--BNDRY--\r\n", s.c_str());
}

static int fakeMeasure(const std::string& s) { return (int)s.size() * 6; }

void test_wrap_simple() {
  auto lines = wrapText("hello world", 36, fakeMeasure);  // 6 chars/line
  TEST_ASSERT_EQUAL(2, (int)lines.size());
  TEST_ASSERT_EQUAL_STRING("hello ", lines[0].c_str());
  TEST_ASSERT_EQUAL_STRING("world", lines[1].c_str());
}

void test_wrap_honors_newlines() {
  auto lines = wrapText("ab\ncd", 600, fakeMeasure);
  TEST_ASSERT_EQUAL(2, (int)lines.size());
  TEST_ASSERT_EQUAL_STRING("ab", lines[0].c_str());
  TEST_ASSERT_EQUAL_STRING("cd", lines[1].c_str());
}

void test_wrap_utf8_not_split() {
  // "中" is 3 bytes; width 18px per glyph under fakeMeasure. Width 20
  // fits exactly one glyph per line — multibyte sequences must not be cut.
  auto lines = wrapText("中文", 20, fakeMeasure);
  TEST_ASSERT_EQUAL(2, (int)lines.size());
  TEST_ASSERT_EQUAL_STRING("中", lines[0].c_str());
  TEST_ASSERT_EQUAL_STRING("文", lines[1].c_str());
}

void test_wrap_empty() {
  auto lines = wrapText("", 100, fakeMeasure);
  TEST_ASSERT_EQUAL(0, (int)lines.size());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_enter_key);
  RUN_TEST(test_backspace_key);
  RUN_TEST(test_esc_is_backtick);
  RUN_TEST(test_nav_keys_carry_char);
  RUN_TEST(test_printable_char);
  RUN_TEST(test_fn_flag_passthrough);
  RUN_TEST(test_nav_keys_left_right);
  RUN_TEST(test_null_char_maps_to_none);
  RUN_TEST(test_scroll_fits_on_one_page);
  RUN_TEST(test_scroll_centers_selection);
  RUN_TEST(test_scroll_clamps_top);
  RUN_TEST(test_scroll_clamps_bottom);
  RUN_TEST(test_idle_starts_active);
  RUN_TEST(test_idle_dims_after_dim_timeout);
  RUN_TEST(test_idle_sleeps_after_sleep_timeout);
  RUN_TEST(test_input_resets_timer);
  RUN_TEST(test_wake_input_is_swallowed_when_dimmed);
  RUN_TEST(test_keep_awake_suppresses_sleep_not_dim);
  RUN_TEST(test_store_starts_empty);
  RUN_TEST(test_store_upsert_and_find);
  RUN_TEST(test_store_upsert_updates_password);
  RUN_TEST(test_store_persists_roundtrip);
  RUN_TEST(test_store_evicts_oldest_when_full);
  RUN_TEST(test_store_remove);
  RUN_TEST(test_store_load_garbage_is_empty);
  RUN_TEST(test_tracker_press_and_release);
  RUN_TEST(test_tracker_long_press_fires_once);
  RUN_TEST(test_tracker_multiple_keys);
  RUN_TEST(test_tracker_special_ids);
  RUN_TEST(test_tracker_overflow_ignored);
  RUN_TEST(test_fspath_flash);
  RUN_TEST(test_fspath_sd_root);
  RUN_TEST(test_fspath_invalid);
  RUN_TEST(test_wav_header_fields);
  RUN_TEST(test_wav_writer_roundtrip);
  RUN_TEST(test_multipart_prefix);
  RUN_TEST(test_multipart_suffix);
  RUN_TEST(test_wrap_simple);
  RUN_TEST(test_wrap_honors_newlines);
  RUN_TEST(test_wrap_utf8_not_split);
  RUN_TEST(test_wrap_empty);
  return UNITY_END();
}
