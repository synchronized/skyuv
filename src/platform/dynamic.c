#include <skyuv/dynamic.h>

#include <stdlib.h>

#include <uv.h>

int skyuv_dlopen(skyuv_dl *library, const char *path) {
	uv_lib_t *implementation;
	int result;

	if (library == NULL || path == NULL) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	if (library->implementation != NULL) {
		return SKYUV_ERROR_INVALID_STATE;
	}
	implementation = malloc(sizeof(*implementation));
	if (implementation == NULL) {
		return SKYUV_ERROR_OUT_OF_MEMORY;
	}
	result = uv_dlopen(path, implementation);
	library->implementation = implementation;
	return result == 0 ? SKYUV_OK : SKYUV_ERROR_SYSTEM;
}

int skyuv_dlsym(skyuv_dl *library, const char *name, void **symbol) {
	if (library == NULL || name == NULL || symbol == NULL) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	if (library->implementation == NULL) {
		return SKYUV_ERROR_INVALID_STATE;
	}
	*symbol = NULL;
	return uv_dlsym(library->implementation, name, symbol) == 0 ? SKYUV_OK : SKYUV_ERROR_SYSTEM;
}

void skyuv_dlclose(skyuv_dl *library) {
	uv_lib_t *implementation;

	if (library == NULL || library->implementation == NULL) {
		return;
	}
	implementation = library->implementation;
	uv_dlclose(implementation);
	free(implementation);
	library->implementation = NULL;
}

const char *skyuv_dlerror(const skyuv_dl *library) {
	if (library == NULL || library->implementation == NULL) {
		return NULL;
	}
	return uv_dlerror(library->implementation);
}
