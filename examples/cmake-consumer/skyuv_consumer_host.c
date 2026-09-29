#include <lauxlib.h>
#include <lua.h>
#include <string.h>

int luaopen_skyuv_consumer(lua_State *L);

int main(void) {
	lua_State *L = luaL_newstate();
	if (L == NULL) {
		return 1;
	}

	luaL_requiref(L, "skyuv_consumer", luaopen_skyuv_consumer, 1);
	lua_pop(L, 1);

	lua_getglobal(L, "skyuv_consumer");
	lua_getfield(L, -1, "version");
	if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
		lua_close(L);
		return 1;
	}

	const char *version = lua_tostring(L, -1);
	int success = version != NULL && strcmp(version, "skyuv-cmake-consumer") == 0;
	lua_pop(L, 2);
	lua_close(L);
	return success ? 0 : 1;
}
