#include <stddef.h>
#include <string.h>

#include <skyuv/memory.h>

int skyuv_test_module_value = 42;

void *skyuv_test_module_allocate(size_t size, int value) {
	void *pointer = skyuv_malloc(size);

	if (pointer != NULL) {
		memset(pointer, value, size);
	}
	return pointer;
}

void *skyuv_test_module_reallocate(void *pointer, size_t size) {
	return skyuv_realloc(pointer, size);
}

void skyuv_test_module_release(void *pointer) {
	skyuv_free(pointer);
}
