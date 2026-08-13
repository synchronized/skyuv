#ifndef SKYUV_COMPAT_SKYNET_START_PTHREAD_H
#define SKYUV_COMPAT_SKYNET_START_PTHREAD_H

#include <stddef.h>
#include <stdlib.h>

#ifndef _WIN32
#include <signal.h>
#endif

#include <skyuv/thread.h>

#define pthread_t skyuv_thread
#define pthread_mutex_t skyuv_mutex
#define pthread_cond_t skyuv_cond

struct skyuv_compat_thread_start {
	void *(*routine)(void *);
	void *argument;
};

static void
skyuv_compat_thread_entry(void *argument) {
	struct skyuv_compat_thread_start *start = argument;
	void *(*routine)(void *) = start->routine;
	void *routine_argument = start->argument;

	free(start);
	(void)routine(routine_argument);
}

static int
skyuv_compat_pthread_create(pthread_t *thread, const void *attributes,
	void *(*routine)(void *), void *argument) {
	struct skyuv_compat_thread_start *start;
	int result;

	(void)attributes;
	*thread = (skyuv_thread)SKYUV_THREAD_INITIALIZER;
	start = malloc(sizeof(*start));
	if (start == NULL) {
		return SKYUV_ERROR_OUT_OF_MEMORY;
	}
	start->routine = routine;
	start->argument = argument;
	result = skyuv_thread_create(thread, skyuv_compat_thread_entry, start);
	if (result != SKYUV_OK) {
		free(start);
	}
	return result;
}

static int
skyuv_compat_pthread_join(pthread_t thread, void **result) {
	(void)result;
	return skyuv_thread_join(&thread);
}

static int
skyuv_compat_pthread_mutex_init(pthread_mutex_t *mutex, const void *attributes) {
	(void)attributes;
	*mutex = (skyuv_mutex)SKYUV_MUTEX_INITIALIZER;
	return skyuv_mutex_init(mutex);
}

static int
skyuv_compat_pthread_mutex_destroy(pthread_mutex_t *mutex) {
	skyuv_mutex_destroy(mutex);
	return 0;
}

static int
skyuv_compat_pthread_mutex_lock(pthread_mutex_t *mutex) {
	skyuv_mutex_lock(mutex);
	return 0;
}

static int
skyuv_compat_pthread_mutex_unlock(pthread_mutex_t *mutex) {
	skyuv_mutex_unlock(mutex);
	return 0;
}

static int
skyuv_compat_pthread_cond_init(pthread_cond_t *condition, const void *attributes) {
	(void)attributes;
	*condition = (skyuv_cond)SKYUV_COND_INITIALIZER;
	return skyuv_cond_init(condition);
}

static int
skyuv_compat_pthread_cond_destroy(pthread_cond_t *condition) {
	skyuv_cond_destroy(condition);
	return 0;
}

static int
skyuv_compat_pthread_cond_signal(pthread_cond_t *condition) {
	skyuv_cond_signal(condition);
	return 0;
}

static int
skyuv_compat_pthread_cond_broadcast(pthread_cond_t *condition) {
	skyuv_cond_broadcast(condition);
	return 0;
}

static int
skyuv_compat_pthread_cond_wait(pthread_cond_t *condition, pthread_mutex_t *mutex) {
	skyuv_cond_wait(condition, mutex);
	return 0;
}

#define pthread_create skyuv_compat_pthread_create
#define pthread_join skyuv_compat_pthread_join
#define pthread_mutex_init skyuv_compat_pthread_mutex_init
#define pthread_mutex_destroy skyuv_compat_pthread_mutex_destroy
#define pthread_mutex_lock skyuv_compat_pthread_mutex_lock
#define pthread_mutex_unlock skyuv_compat_pthread_mutex_unlock
#define pthread_cond_init skyuv_compat_pthread_cond_init
#define pthread_cond_destroy skyuv_compat_pthread_cond_destroy
#define pthread_cond_signal skyuv_compat_pthread_cond_signal
#define pthread_cond_broadcast skyuv_compat_pthread_cond_broadcast
#define pthread_cond_wait skyuv_compat_pthread_cond_wait

#endif
