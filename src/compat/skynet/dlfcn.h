#ifndef SKYUV_COMPAT_SKYNET_DLFCN_H
#define SKYUV_COMPAT_SKYNET_DLFCN_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <skyuv/dynamic.h>

#define RTLD_NOW 0
#define RTLD_GLOBAL 0

#define SKYUV_COMPAT_DLERROR_SIZE 512

static char skyuv_compat_dlerror_text[SKYUV_COMPAT_DLERROR_SIZE];

static void
skyuv_compat_save_dlerror(const skyuv_dl *library) {
	const char *error = skyuv_dlerror(library);

	if (error == NULL) {
		skyuv_compat_dlerror_text[0] = '\0';
		return;
	}
	strncpy(skyuv_compat_dlerror_text, error, sizeof(skyuv_compat_dlerror_text) - 1);
	skyuv_compat_dlerror_text[sizeof(skyuv_compat_dlerror_text) - 1] = '\0';
}

static void *
skyuv_compat_dlopen(const char *path, int mode) {
	skyuv_dl *library;

	(void)mode;
	library = malloc(sizeof(*library));
	if (library == NULL) {
		strncpy(skyuv_compat_dlerror_text, "out of memory",
			sizeof(skyuv_compat_dlerror_text) - 1);
		skyuv_compat_dlerror_text[sizeof(skyuv_compat_dlerror_text) - 1] = '\0';
		return NULL;
	}
	*library = (skyuv_dl)SKYUV_DL_INITIALIZER;
	if (skyuv_dlopen(library, path) != SKYUV_OK) {
		skyuv_compat_save_dlerror(library);
		skyuv_dlclose(library);
		free(library);
		return NULL;
	}
	skyuv_compat_dlerror_text[0] = '\0';
	return library;
}

static void *
skyuv_compat_dlsym(void *handle, const char *name) {
	void *symbol = NULL;

	if (skyuv_dlsym(handle, name, &symbol) != SKYUV_OK) {
		skyuv_compat_save_dlerror(handle);
		return NULL;
	}
	return symbol;
}

static const char *
skyuv_compat_dlerror(void) {
	return skyuv_compat_dlerror_text[0] == '\0' ? NULL : skyuv_compat_dlerror_text;
}

#define dlopen skyuv_compat_dlopen
#define dlsym skyuv_compat_dlsym
#define dlerror skyuv_compat_dlerror

#endif
