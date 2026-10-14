#ifndef SKYUV_COMPAT_SKYNET_RUNTIME_H
#define SKYUV_COMPAT_SKYNET_RUNTIME_H

#include <time.h>
#include <stdio.h>

/* logger Actor 独占句柄，重开失败保留旧文件。 */
FILE *skyuv_logger_reopen(FILE *current, const char *filename);

#ifdef _WIN32
/* 上游 logger 使用 POSIX localtime_r，实现在 skyuv_runtime.c 中。 */
struct tm *localtime_r(const time_t *time_value, struct tm *result);
#endif

#endif
