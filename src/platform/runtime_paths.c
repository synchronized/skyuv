#include "../runtime/runtime_paths.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <uv.h>

static void normalize_separators(char *path) {
#ifdef _WIN32
	for (; *path != '\0'; ++path) {
		if (*path == '\\') {
			*path = '/';
		}
	}
#else
	(void)path;
#endif
}

int skyuv_path_exists(const char *path) {
	uv_fs_t request;
	int result = uv_fs_stat(NULL, &request, path, NULL);
	uv_fs_req_cleanup(&request);
	return result == 0;
}

int skyuv_path_real(const char *path, char *output, size_t capacity) {
	uv_fs_t request;
	int result = uv_fs_realpath(NULL, &request, path, NULL);
	if (result == 0) {
		const char *resolved = request.ptr;
		if (strlen(resolved) >= capacity) {
			result = UV_ENAMETOOLONG;
		} else {
			memcpy(output, resolved, strlen(resolved) + 1);
			normalize_separators(output);
		}
	}
	uv_fs_req_cleanup(&request);
	return result;
}

int skyuv_path_parent(char *path) {
	char *separator;
	normalize_separators(path);
	separator = strrchr(path, '/');
	if (separator == NULL) {
		return UV_EINVAL;
	}
	if (separator == path) {
		separator[1] = '\0';
	} else {
#ifdef _WIN32
		if (separator == path + 2 && path[1] == ':') {
			separator[1] = '\0';
			return 0;
		}
#endif
		*separator = '\0';
	}
	return 0;
}

int skyuv_find_install_root(char *path, size_t capacity) {
	size_t size = capacity;
	char *separator;
	if (uv_exepath(path, &size) != 0 || size == 0 || size >= capacity) {
		return 0;
	}
	path[size] = '\0';
	if (skyuv_path_parent(path) != 0) {
		return 0;
	}
	separator = strrchr(path, '/');
	if (separator == NULL || strcmp(separator + 1, "bin") != 0) {
		return 0;
	}
	return skyuv_path_parent(path) == 0;
}

int skyuv_path_is_absolute(const char *path) {
	int absolute = path[0] == '/';
#ifdef _WIN32
	absolute = absolute || path[0] == '\\' ||
			   ((path[0] != '\0' && path[1] == ':') && (path[2] == '/' || path[2] == '\\'));
	return absolute;
#else
	return absolute;
#endif
}

int skyuv_path_absolute(const char *base, const char *path, char *output, size_t capacity) {
	int absolute = skyuv_path_is_absolute(path);
	int length;
#ifdef _WIN32
	/* 驱动器相对路径依赖进程的隐藏工作目录，不能用于稳定目录契约。 */
	if (path[0] != '\0' && path[1] == ':' && !absolute) {
		return UV_EINVAL;
	}
	/* 根分隔符路径使用配置根的驱动器，不依赖切换安装目录后的当前盘符。 */
	if ((path[0] == '/' || path[0] == '\\') && path[1] != '/' && path[1] != '\\') {
		if (base[0] == '\0' || base[1] != ':') {
			return UV_EINVAL;
		}
		length = snprintf(output, capacity, "%c:%s", base[0], path);
		if (length < 0 || (size_t)length >= capacity) {
			return UV_ENAMETOOLONG;
		}
		normalize_separators(output);
		return 0;
	}
#endif
	if (path[0] == '\0') {
		return UV_EINVAL;
	}
	length = absolute ? snprintf(output, capacity, "%s", path)
					  : snprintf(output, capacity, "%s/%s", base, path);
	if (length < 0 || (size_t)length >= capacity) {
		return UV_ENAMETOOLONG;
	}
	normalize_separators(output);
	return 0;
}

int skyuv_path_mkdir(const char *path) {
	uv_fs_t request;
	char parent[SKYUV_RUNTIME_PATH_CAPACITY];
	int result = uv_fs_mkdir(NULL, &request, path, 0755, NULL);
	uv_fs_req_cleanup(&request);
	if (result == UV_EEXIST) {
		result = uv_fs_stat(NULL, &request, path, NULL);
		if (result == 0 &&
#ifdef _WIN32
			(request.statbuf.st_mode & _S_IFMT) != _S_IFDIR
#else
			!S_ISDIR(request.statbuf.st_mode)
#endif
		) {
			result = UV_ENOTDIR;
		}
		uv_fs_req_cleanup(&request);
		return result;
	}
	if (result != UV_ENOENT) {
		return result;
	}
	if (strlen(path) >= sizeof(parent)) {
		return UV_ENAMETOOLONG;
	}
	memcpy(parent, path, strlen(path) + 1);
	if (skyuv_path_parent(parent) != 0 || strcmp(parent, path) == 0) {
		return UV_ENOENT;
	}
	result = skyuv_path_mkdir(parent);
	if (result != 0) {
		return result;
	}
	/* 允许另一个实例并发创建目录，但必须复核它确实是目录。 */
	return skyuv_path_mkdir(path);
}

int skyuv_daemon_supported(void) {
#ifdef _WIN32
	return 0;
#else
	return 1;
#endif
}
