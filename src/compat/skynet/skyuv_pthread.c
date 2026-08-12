#define SKYUV_COMPAT_PTHREAD_IMPLEMENTATION
#include "skyuv_pthread.h"

#include <stdlib.h>

struct skyuv_compat_thread_start {
	void *(*entry)(void *);
	void *argument;
};

static void skyuv_compat_thread_entry(void *argument) {
	struct skyuv_compat_thread_start *start = argument;
	void *(*entry)(void *) = start->entry;
	void *entry_argument = start->argument;

	free(start);
	(void)entry(entry_argument);
}

int skyuv_compat_pthread_create(pthread_t *thread, const void *attributes, void *(*entry)(void *),
								void *argument) {
	struct skyuv_compat_thread_start *start;
	int result;

	if (attributes != NULL || thread == NULL || entry == NULL) {
		return -1;
	}
	thread->implementation = NULL;
	start = malloc(sizeof(*start));
	if (start == NULL) {
		return -1;
	}
	start->entry = entry;
	start->argument = argument;
	result = skyuv_thread_create(thread, skyuv_compat_thread_entry, start);
	if (result != SKYUV_OK) {
		free(start);
		return -1;
	}
	return 0;
}
