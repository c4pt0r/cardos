#include <unity.h>

#include "../../src/apps/ReplEval.h"

using cardos::replEval;

void test_repl_expression_echoes_value() {
  minlua::Lua lua;
  auto r = replEval(lua, "1 + 2");
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL_STRING("3", r.text.c_str());
}

void test_repl_statement_no_echo_state_persists() {
  minlua::Lua lua;
  auto r1 = replEval(lua, "x = 10");
  TEST_ASSERT_TRUE(r1.ok);
  TEST_ASSERT_EQUAL_STRING("", r1.text.c_str());
  auto r2 = replEval(lua, "x * 2");
  TEST_ASSERT_TRUE(r2.ok);
  TEST_ASSERT_EQUAL_STRING("20", r2.text.c_str());
}

void test_repl_nil_not_echoed() {
  minlua::Lua lua;
  auto r = replEval(lua, "nil");
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL_STRING("", r.text.c_str());
}

void test_repl_print_goes_to_output() {
  minlua::Lua lua;
  lua.clearOutput();
  auto r = replEval(lua, "print(\"hi\")");
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL_STRING("hi\n", lua.output().c_str());
}

void test_repl_syntax_error_reported() {
  minlua::Lua lua;
  auto r = replEval(lua, "x ==");
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_TRUE(r.text.size() > 0);
}

void test_repl_runtime_error_reported() {
  minlua::Lua lua;
  auto r = replEval(lua, "1 + nil");
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_TRUE(r.text.size() > 0);
}

void test_repl_statement_runs_exactly_once() {
  // An assignment must take the raw-statement path without a trial
  // execution — side effects may not happen twice.
  minlua::Lua lua;
  replEval(lua, "cnt = (cnt or 0) + 1");
  auto r = replEval(lua, "cnt");
  TEST_ASSERT_EQUAL_STRING("1", r.text.c_str());
}

void test_repl_string_value_echoed() {
  minlua::Lua lua;
  auto r = replEval(lua, "\"a\" .. \"b\"");
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL_STRING("ab", r.text.c_str());
}

void run_repl_tests() {
  RUN_TEST(test_repl_expression_echoes_value);
  RUN_TEST(test_repl_statement_no_echo_state_persists);
  RUN_TEST(test_repl_nil_not_echoed);
  RUN_TEST(test_repl_print_goes_to_output);
  RUN_TEST(test_repl_syntax_error_reported);
  RUN_TEST(test_repl_runtime_error_reported);
  RUN_TEST(test_repl_statement_runs_exactly_once);
  RUN_TEST(test_repl_string_value_echoed);
}
