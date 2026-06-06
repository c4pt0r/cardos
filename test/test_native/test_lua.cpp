#include <unity.h>

#include <string>

#include "../../src/lua/Lua.h"
#include "../../src/core/crc32.h"
#include "../../src/core/SerialProto.h"

using namespace minlua;

// Run a chunk; return the captured print() output (trimmed of trailing \n).
static std::string evalOut(const std::string& src) {
  Lua lua;
  Lua::Result r = lua.run(src);
  TEST_ASSERT_TRUE_MESSAGE(r.ok, r.error.c_str());
  std::string o = lua.output();
  if (!o.empty() && o.back() == '\n') o.pop_back();
  return o;
}

void test_lua_arithmetic() {
  TEST_ASSERT_EQUAL_STRING("7", evalOut("print(1+2*3)").c_str());
  TEST_ASSERT_EQUAL_STRING("8", evalOut("print(2^3)").c_str());
  TEST_ASSERT_EQUAL_STRING("1", evalOut("print(7 % 3)").c_str());
  TEST_ASSERT_EQUAL_STRING("2", evalOut("print(7 // 3)").c_str());
  TEST_ASSERT_EQUAL_STRING("-9", evalOut("print(-3^2)").c_str());  // -(3^2)
}

void test_lua_strings() {
  TEST_ASSERT_EQUAL_STRING("hello world", evalOut("print('hello'..' '..'world')").c_str());
  TEST_ASSERT_EQUAL_STRING("5", evalOut("print(#'hello')").c_str());
  TEST_ASSERT_EQUAL_STRING("ELL", evalOut("print(string.upper(string.sub('hello',2,4)))").c_str());
  TEST_ASSERT_EQUAL_STRING("x=42", evalOut("print(string.format('x=%d', 42))").c_str());
}

void test_lua_locals_and_globals() {
  TEST_ASSERT_EQUAL_STRING("3", evalOut("local a=1 local b=2 print(a+b)").c_str());
  TEST_ASSERT_EQUAL_STRING("10", evalOut("x=10 print(x)").c_str());
  TEST_ASSERT_EQUAL_STRING("1\t2", evalOut("local a,b=1,2 print(a,b)").c_str());
}

void test_lua_if_while() {
  TEST_ASSERT_EQUAL_STRING("big", evalOut("if 5>3 then print('big') else print('small') end").c_str());
  TEST_ASSERT_EQUAL_STRING("15", evalOut(
      "local s=0 local i=1 while i<=5 do s=s+i i=i+1 end print(s)").c_str());
}

void test_lua_numeric_for() {
  TEST_ASSERT_EQUAL_STRING("55", evalOut(
      "local s=0 for i=1,10 do s=s+i end print(s)").c_str());
  TEST_ASSERT_EQUAL_STRING("9\t6\t3", evalOut(
      "local t={} for i=9,1,-3 do t[#t+1]=i end print(t[1],t[2],t[3])").c_str());
}

void test_lua_functions_closures() {
  TEST_ASSERT_EQUAL_STRING("120", evalOut(
      "local function fact(n) if n<=1 then return 1 end return n*fact(n-1) end print(fact(5))").c_str());
  TEST_ASSERT_EQUAL_STRING("1\t2\t3", evalOut(
      "local function counter() local n=0 return function() n=n+1 return n end end "
      "local c=counter() print(c(),c(),c())").c_str());
}

void test_lua_tables_and_pairs() {
  TEST_ASSERT_EQUAL_STRING("3", evalOut("local t={10,20,30} print(#t)").c_str());
  TEST_ASSERT_EQUAL_STRING("60", evalOut(
      "local t={10,20,30} local s=0 for _,v in ipairs(t) do s=s+v end print(s)").c_str());
  TEST_ASSERT_EQUAL_STRING("100", evalOut(
      "local t={} t.a=40 t.b=60 local s=0 for k,v in pairs(t) do s=s+v end print(s)").c_str());
  TEST_ASSERT_EQUAL_STRING("7", evalOut(
      "local t={x=3} t.x=t.x+4 print(t.x)").c_str());
}

void test_lua_multiple_return() {
  TEST_ASSERT_EQUAL_STRING("1\t2", evalOut(
      "local function two() return 1,2 end local a,b=two() print(a,b)").c_str());
}

void test_lua_pcall_error() {
  TEST_ASSERT_EQUAL_STRING("false\tboom", evalOut(
      "local ok,err=pcall(function() error('boom') end) print(ok,err)").c_str());
  TEST_ASSERT_EQUAL_STRING("true", evalOut(
      "local ok=pcall(function() return 1 end) print(ok)").c_str());
}

void test_lua_native_binding() {
  Lua lua;
  int sum = 0;
  lua.registerFn("add", [&](Interp& in, const ValueList& a) -> ValueList {
    sum = (int)a[0].num + (int)a[1].num;
    return {Value::number(sum)};
  });
  Lua::Result r = lua.run("result = add(3, 4)");
  TEST_ASSERT_TRUE_MESSAGE(r.ok, r.error.c_str());
  TEST_ASSERT_EQUAL(7, sum);
  TEST_ASSERT_EQUAL(7, (int)lua.getGlobal("result").num);
}

void test_lua_callglobal_app_callback() {
  // Mirrors how ScriptApp invokes on_key.
  Lua lua;
  lua.run("function on_key(code) if code=='enter' then return true end return false end");
  ValueList out;
  Lua::Result r = lua.callGlobal("on_key", {Value::string("enter")}, &out);
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL(1, (int)out.size());
  TEST_ASSERT_TRUE(out[0].truthy());
}

void test_lua_syntax_error_reported() {
  Lua lua;
  Lua::Result r = lua.run("local = ");
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_TRUE(r.error.size() > 0);
}

void test_crc32_vectors() {
  TEST_ASSERT_EQUAL_HEX32(0x00000000u, crc32::of(""));
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, crc32::of("123456789"));
  TEST_ASSERT_EQUAL_HEX32(0x414FA339u, crc32::of("The quick brown fox jumps over the lazy dog"));
}

void test_serialproto_parse() {
  auto c = serialproto::parse("put hello.lua 128 cbf43926");
  TEST_ASSERT_EQUAL_STRING("PUT", c.verb.c_str());
  TEST_ASSERT_EQUAL(3, (int)c.args.size());
  TEST_ASSERT_EQUAL_STRING("hello.lua", c.args[0].c_str());
  TEST_ASSERT_EQUAL_STRING("128", c.args[1].c_str());

  auto e = serialproto::parse("   \t  ");
  TEST_ASSERT_TRUE(e.empty());

  auto p = serialproto::parse("PING");
  TEST_ASSERT_EQUAL_STRING("PING", p.verb.c_str());
  TEST_ASSERT_EQUAL(0, (int)p.args.size());
}

void test_serialproto_valid_name() {
  TEST_ASSERT_TRUE(serialproto::validAppName("hello.lua"));
  TEST_ASSERT_FALSE(serialproto::validAppName("hello.txt"));
  TEST_ASSERT_FALSE(serialproto::validAppName("../etc/x.lua"));
  TEST_ASSERT_FALSE(serialproto::validAppName("a/b.lua"));
  TEST_ASSERT_FALSE(serialproto::validAppName(".lua"));
}

void run_lua_tests() {
  RUN_TEST(test_crc32_vectors);
  RUN_TEST(test_serialproto_parse);
  RUN_TEST(test_serialproto_valid_name);
  RUN_TEST(test_lua_arithmetic);
  RUN_TEST(test_lua_strings);
  RUN_TEST(test_lua_locals_and_globals);
  RUN_TEST(test_lua_if_while);
  RUN_TEST(test_lua_numeric_for);
  RUN_TEST(test_lua_functions_closures);
  RUN_TEST(test_lua_tables_and_pairs);
  RUN_TEST(test_lua_multiple_return);
  RUN_TEST(test_lua_pcall_error);
  RUN_TEST(test_lua_native_binding);
  RUN_TEST(test_lua_callglobal_app_callback);
  RUN_TEST(test_lua_syntax_error_reported);
}
