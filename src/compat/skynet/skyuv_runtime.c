#include <stddef.h>
#include <time.h>

#include <skyuv/memory.h>

void *skynet_lalloc(void *ptr, size_t old_size, size_t new_size) {
	(void)old_size;
	return skyuv_realloc(ptr, new_size);
}

#ifdef _WIN32
struct tm *localtime_r(const time_t *time_value, struct tm *result) {
	return localtime_s(result, time_value) == 0 ? result : NULL;
}
#endif
