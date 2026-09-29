#include <lua.h>
#include <lauxlib.h>

static int skyuv_consumer_version(lua_State *L) {
	lua_pushliteral(L, "skyuv-cmake-consumer");
	return 1;
}

int luaopen_skyuv_consumer(lua_State *L) {
	static const luaL_Reg functions[] = {
		{"version", skyuv_consumer_version},
		{NULL, NULL},
	};
	luaL_newlib(L, functions);
	return 1;
}
