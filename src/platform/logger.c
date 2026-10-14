#include "../compat/skynet/skyuv_runtime.h"

#include <errno.h>
#include <stdio.h>

FILE *skyuv_logger_reopen(FILE *current, const char *filename) {
	FILE *replacement = NULL;
	int error = 0;
#ifdef _WIN32
	error = fopen_s(&replacement, filename, "a");
#else
	replacement = fopen(filename, "a");
	if (replacement == NULL) {
		error = errno;
	}
#endif
	/* logger Actor 串行调用；失败时旧句柄的所有权及可用性保持不变。 */
	if (replacement == NULL) {
		fprintf(stderr, "skyuv: 日志重开失败（错误码 %d），继续使用旧文件：%s\n", error, filename);
		return current;
	}
	if (current != NULL && fclose(current) != 0) {
		fprintf(stderr, "skyuv: 关闭旧日志句柄失败：%s\n", filename);
	}
	return replacement;
}
