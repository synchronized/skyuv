#include "runtime_paths.h"

#include <stdio.h>
#include <string.h>

#include <uv.h>

int skyuv_skynet_main(int argc, char *argv[]);

struct skyuv_runtime_paths skyuv_runtime_paths;

static int prepare_paths(int argc, char *argv[]) {
	const char *install = NULL;
	const char *config = NULL;
	char working[SKYUV_RUNTIME_PATH_CAPACITY];
	char candidate[SKYUV_RUNTIME_PATH_CAPACITY];
	size_t capacity = sizeof(working);
	int index;
	int result;

	for (index = 2; index < argc; ++index) {
		if (strcmp(argv[index], "--") == 0) {
			if (++index == argc - 1) {
				config = argv[index];
				break;
			}
			return UV_EINVAL;
		}
		if (argv[index][0] != '-') {
			if (index != argc - 1) {
				return UV_EINVAL;
			}
			config = argv[index];
			break;
		}
		if (index + 1 >= argc || argv[index + 1][0] == '\0') {
			return UV_EINVAL;
		}
		if (strcmp(argv[index], "--install-root") == 0 && install == NULL) {
			install = argv[++index];
		} else if (strcmp(argv[index], "--log-root") == 0 && skyuv_runtime_paths.log_root == NULL) {
			skyuv_runtime_paths.log_root = argv[++index];
		} else if (strcmp(argv[index], "--run-root") == 0 && skyuv_runtime_paths.run_root == NULL) {
			skyuv_runtime_paths.run_root = argv[++index];
		} else {
			return UV_EINVAL;
		}
	}
	if (config == NULL || config[0] == '\0') {
		return UV_EINVAL;
	}
	result = uv_cwd(working, &capacity);
	if (result != 0) {
		return result;
	}
	if (install != NULL) {
		result = skyuv_path_real(install, skyuv_runtime_paths.install_root,
								 sizeof(skyuv_runtime_paths.install_root));
		if (result != 0) {
			fprintf(stderr, "skyuv: 无法解析安装根目录：%s\n", install);
			return result;
		}
	} else if (!skyuv_find_install_root(skyuv_runtime_paths.install_root,
										sizeof(skyuv_runtime_paths.install_root))) {
		fprintf(stderr, "skyuv: 源码构建入口需要显式指定 --install-root\n");
		return UV_EINVAL;
	}
	result = skyuv_path_absolute(working, config, candidate, sizeof(candidate));
	if (result != 0) {
		return result;
	}
	if (!skyuv_path_exists(candidate)) {
		result = skyuv_path_absolute(skyuv_runtime_paths.install_root, config, candidate,
									 sizeof(candidate));
		if (result != 0) {
			return result;
		}
	}
	result = skyuv_path_real(candidate, skyuv_runtime_paths.config_file,
							 sizeof(skyuv_runtime_paths.config_file));
	if (result != 0) {
		fprintf(stderr, "skyuv: 无法解析主配置：%s\n", candidate);
		return result;
	}
	memcpy(skyuv_runtime_paths.config_root, skyuv_runtime_paths.config_file,
		   strlen(skyuv_runtime_paths.config_file) + 1);
	result = skyuv_path_parent(skyuv_runtime_paths.config_root);
	if (result != 0) {
		return result;
	}
	result = uv_chdir(skyuv_runtime_paths.install_root);
	if (result != 0) {
		fprintf(stderr, "skyuv: 无法切换到安装根目录：%s\n", skyuv_runtime_paths.install_root);
		return result;
	}
	skyuv_runtime_paths.enabled = 1;
	return 0;
}

int main(int argc, char *argv[]) {
	char install_root[SKYUV_RUNTIME_PATH_CAPACITY];
	char config_path[SKYUV_RUNTIME_PATH_CAPACITY];
	int length;

	if (argc > 1 && strcmp(argv[1], "--runtime-paths") == 0) {
		char *arguments[3] = {NULL, NULL, NULL};
		int result = prepare_paths(argc, argv);
		if (result != 0) {
			fprintf(stderr,
					"skyuv: 运行路径参数或目录无效：%s\n"
					"用法：skyuv --runtime-paths [--install-root DIR] [--log-root DIR] "
					"[--run-root DIR] CONFIG\n",
					uv_strerror(result));
			return 1;
		}
		arguments[0] = argv[0];
		arguments[1] = skyuv_runtime_paths.config_file;
		return skyuv_skynet_main(2, arguments);
	}
	if (argc > 1 && !skyuv_path_exists(argv[1]) &&
		skyuv_find_install_root(install_root, sizeof(install_root))) {
		length = snprintf(config_path, sizeof(config_path), "%s/%s", install_root, argv[1]);
		if (length > 0 && (size_t)length < sizeof(config_path) && skyuv_path_exists(config_path) &&
			uv_chdir(install_root) != 0) {
			fprintf(stderr, "无法切换到 skyuv 安装根目录：%s\n", install_root);
			return 1;
		}
	}
	return skyuv_skynet_main(argc, argv);
}
