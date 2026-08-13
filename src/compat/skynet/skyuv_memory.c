#include "skynet.h"

#include <lauxlib.h>

#include <stdbool.h>
#include <stddef.h>

size_t malloc_used_memory(void) {
	return 0;
}

size_t malloc_memory_block(void) {
	return 0;
}

void memory_info_dump(const char *options) {
	(void)options;
	skynet_error(NULL, "skyuv 系统分配器不提供 jemalloc 统计");
}

size_t mallctl_int64(const char *name, size_t *new_value) {
	(void)name;
	(void)new_value;
	return 0;
}

int mallctl_opt(const char *name, int *new_value) {
	(void)name;
	(void)new_value;
	return 0;
}

bool mallctl_bool(const char *name, bool *new_value) {
	(void)name;
	(void)new_value;
	return false;
}

int mallctl_cmd(const char *name) {
	(void)name;
	return 0;
}

void dump_c_mem(void) {
	skynet_error(NULL, "skyuv 系统分配器不提供服务级内存统计");
}

int dump_mem_lua(lua_State *state) {
	lua_newtable(state);
	return 1;
}

size_t malloc_current_memory(void) {
	return 0;
}
