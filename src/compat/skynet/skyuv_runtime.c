#include <stddef.h>
#include <stdlib.h>
#include <time.h>

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
