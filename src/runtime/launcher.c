#include <stdio.h>
#include <string.h>

#include <uv.h>

int skyuv_skynet_main(int argc, char *argv[]);

static int skyuv_path_exists(const char *path) {
	uv_fs_t request;
	int result = uv_fs_stat(NULL, &request, path, NULL);

	uv_fs_req_cleanup(&request);
	return result == 0;
}

static int skyuv_find_install_root(char *path, size_t capacity) {
	size_t size = capacity;
	char *separator;
	char *bin;

	if (uv_exepath(path, &size) != 0 || size == 0 || size >= capacity) {
		return 0;
	}
	path[size] = '\0';
	separator = strrchr(path, '/');
#ifdef _WIN32
	{
		char *backslash = strrchr(path, '\\');
		if (backslash != NULL && (separator == NULL || backslash > separator)) {
			separator = backslash;
		}
	}
#endif
	if (separator == NULL) {
		return 0;
	}
	*separator = '\0';
	bin = strrchr(path, '/');
#ifdef _WIN32
	{
		char *backslash = strrchr(path, '\\');
		if (backslash != NULL && (bin == NULL || backslash > bin)) {
			bin = backslash;
		}
	}
#endif
	if (bin == NULL || strcmp(bin + 1, "bin") != 0) {
		return 0;
	}
	*bin = '\0';
	return 1;
}

int main(int argc, char *argv[]) {
	char install_root[4096];
	char config_path[4096];
	int length;

	if (argc > 1 && !skyuv_path_exists(argv[1]) &&
		skyuv_find_install_root(install_root, sizeof(install_root))) {
		length = snprintf(config_path, sizeof(config_path), "%s/%s", install_root, argv[1]);
		if (length > 0 && (size_t)length < sizeof(config_path) &&
			skyuv_path_exists(config_path) && uv_chdir(install_root) != 0) {
			fprintf(stderr, "无法切换到 skyuv 安装根目录：%s\n", install_root);
			return 1;
		}
	}
	return skyuv_skynet_main(argc, argv);
}
