#include "runtime_paths.h"
#include "runtime_config_script.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <uv.h>

static const char *config_string(lua_State *L, const char *key, int *valid) {
	const char *value = NULL;
	lua_getfield(L, -1, key);
	if (!lua_isnil(L, -1)) {
		if (lua_type(L, -1) != LUA_TSTRING) {
			*valid = 0;
		} else {
			value = lua_tostring(L, -1);
			if (strlen(value) != lua_rawlen(L, -1)) {
				*valid = 0;
			}
		}
	}
	lua_pop(L, 1);
	return value;
}

static void set_string(lua_State *L, const char *key, const char *value) {
	lua_pushstring(L, value);
	lua_setfield(L, -2, key);
}

static int resolve_root(lua_State *L, const char *option, const char *environment, const char *key,
						const char *fallback, char *output, size_t capacity) {
	int valid = 1;
	const char *value = option;
	if (value == NULL) {
		value = getenv(environment);
	}
	if (value == NULL) {
		value = config_string(L, key, &valid);
	}
	if (!valid) {
		return UV_EINVAL;
	}
	return skyuv_path_absolute(skyuv_runtime_paths.config_root, value == NULL ? fallback : value,
							   output, capacity);
}

static int resolve_output(lua_State *L, const char *key, const char *root, int create_root) {
	char path[SKYUV_RUNTIME_PATH_CAPACITY];
	int valid = 1;
	const char *value = config_string(L, key, &valid);
	int result;
	if (!valid) {
		return UV_EINVAL;
	}
	if (value == NULL) {
		return 0;
	}
	result = skyuv_path_absolute(root, value, path, sizeof(path));
	if (result != 0) {
		return result;
	}
	if (create_root && !skyuv_path_is_absolute(value)) {
		result = skyuv_path_mkdir(root);
		if (result != 0) {
			fprintf(stderr, "skyuv: 无法准备 %s 根目录：%s（目标：%s）\n", key, root, path);
			return result;
		}
	}
	set_string(L, key, path);
	fprintf(stderr, "skyuv: %s=%s\n", key, path);
	return 0;
}

int skyuv_config_load(lua_State *L, const char *legacy_loader, const char *config_file) {
	const char *script = skyuv_runtime_paths.enabled ? skyuv_config_script : legacy_loader;
	char log_root[SKYUV_RUNTIME_PATH_CAPACITY];
	char run_root[SKYUV_RUNTIME_PATH_CAPACITY];
	int result = luaL_loadbufferx(L, script, strlen(script), "=[skyuv config]", "t");
	int arguments = 1;
	int valid = 1;
	const char *daemon;
	const char *service;
	if (result != LUA_OK) {
		return result;
	}
	lua_pushstring(L, config_file);
	if (skyuv_runtime_paths.enabled) {
		lua_newtable(L);
		set_string(L, "skyuv_install_root", skyuv_runtime_paths.install_root);
		set_string(L, "skyuv_config_root", skyuv_runtime_paths.config_root);
		arguments = 2;
	}
	result = lua_pcall(L, arguments, 1, 0);
	if (result != LUA_OK || !skyuv_runtime_paths.enabled) {
		return result;
	}
	result = resolve_root(L, skyuv_runtime_paths.log_root, "SKYUV_LOG_ROOT", "skyuv_log_root",
						  "log", log_root, sizeof(log_root));
	if (result == 0) {
		result = resolve_root(L, skyuv_runtime_paths.run_root, "SKYUV_RUN_ROOT", "skyuv_run_root",
							  "run", run_root, sizeof(run_root));
	}
	if (result != 0) {
		lua_pushfstring(L, "skyuv: 根目录解析失败：%s", uv_strerror(result));
		return LUA_ERRRUN;
	}
	set_string(L, "skyuv_install_root", skyuv_runtime_paths.install_root);
	set_string(L, "skyuv_config_root", skyuv_runtime_paths.config_root);
	set_string(L, "skyuv_log_root", log_root);
	set_string(L, "skyuv_run_root", run_root);
	fprintf(stderr,
			"skyuv: install_root=%s\nskyuv: config_root=%s\n"
			"skyuv: log_root=%s\nskyuv: run_root=%s\n",
			skyuv_runtime_paths.install_root, skyuv_runtime_paths.config_root, log_root, run_root);
	daemon = config_string(L, "daemon", &valid);
	service = config_string(L, "logservice", &valid);
	if (!valid) {
		lua_pushliteral(L, "skyuv: daemon 与 logservice 必须为字符串或 nil");
		return LUA_ERRRUN;
	}
	if (daemon != NULL && !skyuv_daemon_supported()) {
		lua_pushliteral(L, "skyuv: daemon mode is not supported on Windows");
		return LUA_ERRRUN;
	}
	if (service == NULL || strcmp(service, "logger") == 0) {
		result = resolve_output(L, "logger", log_root, 1);
	}
	if (result == 0 && daemon != NULL) {
		result = resolve_output(L, "daemon", run_root, 1);
	}
	if (result != 0) {
		lua_pushfstring(L, "skyuv: 输出路径准备失败：%s", uv_strerror(result));
		return LUA_ERRRUN;
	}
	return LUA_OK;
}
