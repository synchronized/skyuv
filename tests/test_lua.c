#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

static void
test_lua_expression(void **state) {
	lua_State *lua;
	int result;

	(void)state;
	lua = luaL_newstate();
	assert_non_null(lua);

	luaL_openlibs(lua);
	result = luaL_dostring(lua, "return 6 * 7");
	assert_int_equal(result, LUA_OK);
	assert_true(lua_isinteger(lua, -1));
	assert_int_equal(lua_tointeger(lua, -1), 42);

	lua_close(lua);
}

int
main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_lua_expression),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
