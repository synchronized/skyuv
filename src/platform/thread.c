#include <skyuv/thread.h>

#include <stdlib.h>

#include <uv.h>

struct skyuv_thread_start {
	skyuv_thread_entry entry;
	void *argument;
};

static void
thread_start(void *argument) {
	struct skyuv_thread_start *start = argument;
	skyuv_thread_entry entry = start->entry;
	void *entry_argument = start->argument;

	free(start);
	entry(entry_argument);
}

int
skyuv_thread_create(skyuv_thread *thread, skyuv_thread_entry entry, void *argument) {
	uv_thread_t *implementation;
	struct skyuv_thread_start *start;
	int result;

	if (thread == NULL || entry == NULL) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	if (thread->implementation != NULL) {
		return SKYUV_ERROR_INVALID_STATE;
	}

	implementation = malloc(sizeof(*implementation));
	start = malloc(sizeof(*start));
	if (implementation == NULL || start == NULL) {
		free(implementation);
		free(start);
		return SKYUV_ERROR_OUT_OF_MEMORY;
	}

	start->entry = entry;
	start->argument = argument;
	result = uv_thread_create(implementation, thread_start, start);
	if (result != 0) {
		free(start);
		free(implementation);
		return SKYUV_ERROR_SYSTEM;
	}

	thread->implementation = implementation;
	return SKYUV_OK;
}

int
skyuv_thread_join(skyuv_thread *thread) {
	uv_thread_t *implementation;
	int result;

	if (thread == NULL) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	if (thread->implementation == NULL) {
		return SKYUV_ERROR_INVALID_STATE;
	}

	implementation = thread->implementation;
	result = uv_thread_join(implementation);
	if (result != 0) {
		return SKYUV_ERROR_SYSTEM;
	}

	free(implementation);
	thread->implementation = NULL;
	return SKYUV_OK;
}

int
skyuv_mutex_init(skyuv_mutex *mutex) {
	uv_mutex_t *implementation;
	int result;

	if (mutex == NULL) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	if (mutex->implementation != NULL) {
		return SKYUV_ERROR_INVALID_STATE;
	}

	implementation = malloc(sizeof(*implementation));
	if (implementation == NULL) {
		return SKYUV_ERROR_OUT_OF_MEMORY;
	}

	result = uv_mutex_init(implementation);
	if (result != 0) {
		free(implementation);
		return SKYUV_ERROR_SYSTEM;
	}

	mutex->implementation = implementation;
	return SKYUV_OK;
}

void
skyuv_mutex_destroy(skyuv_mutex *mutex) {
	uv_mutex_t *implementation;

	if (mutex == NULL || mutex->implementation == NULL) {
		return;
	}

	implementation = mutex->implementation;
	uv_mutex_destroy(implementation);
	free(implementation);
	mutex->implementation = NULL;
}

void
skyuv_mutex_lock(skyuv_mutex *mutex) {
	uv_mutex_lock(mutex->implementation);
}

bool
skyuv_mutex_trylock(skyuv_mutex *mutex) {
	return uv_mutex_trylock(mutex->implementation) == 0;
}

void
skyuv_mutex_unlock(skyuv_mutex *mutex) {
	uv_mutex_unlock(mutex->implementation);
}
