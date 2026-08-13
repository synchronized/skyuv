#include "skynet_daemon.h"

#include <stdlib.h>
#include <time.h>

int daemon_init(const char *pidfile) {
	/* Windows 和首期跨平台目标不支持 daemon 模式。 */
	(void)pidfile;
	return 1;
}

int daemon_exit(const char *pidfile) {
	(void)pidfile;
	return 0;
}

void *skynet_lalloc(void *ptr, size_t old_size, size_t new_size) {
	(void)old_size;
	if (new_size == 0) {
		free(ptr);
		return NULL;
	}
	return realloc(ptr, new_size);
}

#ifdef _WIN32
struct tm *localtime_r(const time_t *time_value, struct tm *result) {
	return localtime_s(result, time_value) == 0 ? result : NULL;
}
#endif
