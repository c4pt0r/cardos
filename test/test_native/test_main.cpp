#include <unity.h>
#include "../../src/core/KeyEvent.h"

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
  return UNITY_END();
}
