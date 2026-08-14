#include "skyuv_control.h"

#include <lauxlib.h>
#include <lua.h>

static int reopen_log(lua_State *state) {
	lua_pushboolean(state, skyuv_skynet_reopen_log());
	return 1;
}

LUAMOD_API int luaopen_skyuv_control(lua_State *state) {
	const luaL_Reg functions[] = {
		{"reopen_log", reopen_log},
		{NULL, NULL},
	};

	luaL_checkversion(state);
	luaL_newlib(state, functions);
	return 1;
}
