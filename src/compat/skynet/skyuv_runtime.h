#ifndef SKYUV_COMPAT_SKYNET_RUNTIME_H
#define SKYUV_COMPAT_SKYNET_RUNTIME_H

#include <time.h>

#ifdef _WIN32
/* 上游 logger 使用 POSIX localtime_r，实现在 skyuv_runtime.c 中。 */
struct tm *localtime_r(const time_t *time_value, struct tm *result);
#endif

#endif
