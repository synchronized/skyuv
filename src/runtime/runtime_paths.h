#ifndef SKYUV_RUNTIME_PATHS_H
#define SKYUV_RUNTIME_PATHS_H

#include <stddef.h>

struct lua_State;

#define SKYUV_RUNTIME_PATH_CAPACITY 4096

/* 入口线程独占，生命周期覆盖配置加载；不暴露到公共 API。 */
struct skyuv_runtime_paths {
	int enabled;
	char install_root[SKYUV_RUNTIME_PATH_CAPACITY];
	char config_file[SKYUV_RUNTIME_PATH_CAPACITY];
	char config_root[SKYUV_RUNTIME_PATH_CAPACITY];
	const char *log_root;
	const char *run_root;
};

extern struct skyuv_runtime_paths skyuv_runtime_paths;

int skyuv_path_exists(const char *path);
int skyuv_find_install_root(char *path, size_t capacity);
int skyuv_path_is_absolute(const char *path);
int skyuv_path_absolute(const char *base, const char *path, char *output, size_t capacity);
int skyuv_path_real(const char *path, char *output, size_t capacity);
int skyuv_path_parent(char *path);
int skyuv_path_mkdir(const char *path);
int skyuv_daemon_supported(void);
int skyuv_config_load(struct lua_State *L, const char *legacy_loader, const char *config_file);

#endif
