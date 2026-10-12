#include "client_runtime.h"
#include <lua.h>
#include <lauxlib.h>
#include <math.h>
#include <stdint.h>

int skyuv_client_readstdin(lua_State *L);

struct client_state {
	struct skyuv_client_runtime *runtime;
};

static struct client_state *state(lua_State *L) {
	return lua_touserdata(L, lua_upvalueindex(1));
}

static int report(lua_State *L, int result) {
	return luaL_error(L, "client.socket: %s (%s)", uv_strerror(result), uv_err_name(result));
}

static int dispose(lua_State *L) {
	struct client_state *context = lua_touserdata(L, 1);
	skyuv_client_runtime_destroy(&context->runtime);
	return 0;
}

static int connect_socket(lua_State *L) {
	const char *address = luaL_checkstring(L, 1);
	lua_Integer port = luaL_checkinteger(L, 2);
	double seconds = luaL_optnumber(L, 3, -1);
	if (port < 0 || port > 65535 || !isfinite(seconds) || seconds > 1000000)
		return luaL_error(L, "invalid port or connect timeout");
	struct client_state *context = state(L);
	int result = 0;
	if (context->runtime == NULL)
		result = skyuv_client_runtime_create(&context->runtime);
	int64_t id = 0;
	if (result == 0)
		result = skyuv_client_runtime_connect_timeout(
			context->runtime, address, (int)port, seconds < 0 ? -1 : (int64_t)ceil(seconds * 1000),
			&id);
	if (result != 0)
		return luaL_error(L, "Connect %s %d failed: %s (%s)", address, (int)port,
						  uv_strerror(result), uv_err_name(result));
	lua_pushinteger(L, (lua_Integer)id);
	return 1;
}

static int close_socket(lua_State *L) {
	int64_t id = (int64_t)luaL_checkinteger(L, 1);
	int result = skyuv_client_runtime_close(state(L)->runtime, id);
	return result == 0 ? 0 : report(L, result);
}

static int send_socket(lua_State *L) {
	int64_t id = (int64_t)luaL_checkinteger(L, 1);
	size_t size;
	const char *data = luaL_checklstring(L, 2, &size);
	int result = skyuv_client_runtime_send(state(L)->runtime, id, data, size);
	return result == 0 ? 0 : report(L, result);
}

static int recv_socket(lua_State *L) {
	int64_t id = (int64_t)luaL_checkinteger(L, 1);
	char buffer[4096];
	size_t size = 0;
	int result = skyuv_client_runtime_recv(state(L)->runtime, id, buffer, sizeof(buffer), &size);
	if (result == UV_EAGAIN)
		return 0;
	if (result != 0 && result != UV_EOF)
		return report(L, result);
	lua_pushlstring(L, buffer, size);
	return 1;
}

static int shutdown_socket(lua_State *L) {
	int64_t id = (int64_t)luaL_checkinteger(L, 1);
	const char *mode = luaL_checkstring(L, 2);
	int read = 0;
	int write = 0;
	for (const char *p = mode; *p != 0; ++p) {
		if (*p == 'r')
			read = 1;
		else if (*p == 'w')
			write = 1;
		else
			return luaL_error(L, "Invalid mode %c", *p);
	}
	int result = skyuv_client_runtime_shutdown(state(L)->runtime, id, read, write);
	return result == 0 ? 0 : report(L, result);
}

static int sleep_socket(lua_State *L) {
	double microseconds = luaL_checknumber(L, 1);
	if (!isfinite(microseconds) || microseconds < 0 || microseconds > 1000000000)
		return luaL_error(L, "invalid sleep duration");
	uv_sleep((unsigned int)ceil(microseconds / 1000));
	return 0;
}

static int ready_table(lua_State *L, int index, int writing, size_t *sockets) {
	size_t count = lua_rawlen(L, index);
	int ready = 0;
	lua_newtable(L);
	for (size_t i = 1; i <= count; ++i) {
		lua_rawgeti(L, index, (lua_Integer)i);
		int64_t id = (int64_t)luaL_checkinteger(L, -1);
		lua_pop(L, 1);
		if (id < 0)
			continue;
		++*sockets;
		int read = 0;
		int write = 0;
		int result = skyuv_client_runtime_ready(state(L)->runtime, id, &read, &write);
		if (result != 0)
			return report(L, result);
		if (writing ? write : read) {
			lua_pushinteger(L, (lua_Integer)id);
			lua_rawseti(L, -2, ++ready);
		}
	}
	return ready;
}

static int select_socket(lua_State *L) {
	int has_read = lua_istable(L, 1);
	int has_write = lua_istable(L, 2);
	int timeout_index = has_write ? 3 : has_read ? 2 : 1;
	if (has_read && !has_write && lua_isnil(L, 2) && lua_isnumber(L, 3))
		timeout_index = 3;
	if (lua_gettop(L) > timeout_index)
		return luaL_error(L, "invalid select options");
	double seconds = luaL_optnumber(L, timeout_index, -1);
	if (!isfinite(seconds) || seconds > 1000000)
		return luaL_error(L, "invalid select timeout");
	uint64_t start = uv_hrtime();
	int arguments = lua_gettop(L);
	for (;;) {
		uint64_t revision = skyuv_client_runtime_revision(state(L)->runtime);
		size_t sockets = 0;
		int ready = 0;
		if (has_read)
			ready += ready_table(L, 1, 0, &sockets);
		if (has_write) {
			if (!has_read)
				lua_newtable(L);
			ready += ready_table(L, 2, 1, &sockets);
		}
		if (ready != 0)
			return has_write ? 2 : 1;
		lua_settop(L, arguments);
		if (sockets == 0 && seconds < 0)
			return luaL_error(L, "no open sockets to check and no timeout set");
		if (seconds >= 0 && (double)(uv_hrtime() - start) / 1000000000.0 >= seconds) {
			lua_pushboolean(L, 0);
			return 1;
		}
		int64_t remaining =
			seconds < 0 ? -1 : (int64_t)(seconds * 1000000000.0 - (double)(uv_hrtime() - start));
		if (seconds >= 0 && remaining < 0)
			remaining = 0;
		skyuv_client_runtime_wait(state(L)->runtime, revision, remaining);
	}
}

int luaopen_client_socket(lua_State *L) {
	luaL_checkversion(L);
	struct client_state *context = lua_newuserdatauv(L, sizeof(*context), 0);
	context->runtime = NULL;
	if (luaL_newmetatable(L, "skyuv.client.runtime")) {
		lua_pushcfunction(L, dispose);
		lua_setfield(L, -2, "__gc");
	}
	lua_setmetatable(L, -2);
	const luaL_Reg functions[] = {{"connect", connect_socket},
								  {"send", send_socket},
								  {"recv", recv_socket},
								  {"close", close_socket},
								  {"shutdown", shutdown_socket},
								  {"select", select_socket},
								  {"poll", select_socket},
								  {"usleep", sleep_socket},
								  {"readstdin", skyuv_client_readstdin},
								  {NULL, NULL}};
	lua_newtable(L);
	lua_pushvalue(L, -2);
	luaL_setfuncs(L, functions, 1);
	lua_remove(L, -2);
	return 1;
}
