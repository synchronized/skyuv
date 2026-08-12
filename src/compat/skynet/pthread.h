#ifndef SKYUV_COMPAT_SKYNET_PTHREAD_H
#define SKYUV_COMPAT_SKYNET_PTHREAD_H

#include <skyuv/thread.h>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include_next <pthread.h>
#pragma GCC diagnostic pop
#define pthread_t skyuv_thread
#define pthread_mutex_t skyuv_mutex
#define pthread_cond_t skyuv_cond
#define pthread_key_t skyuv_tls
#else
typedef skyuv_thread pthread_t;
typedef skyuv_mutex pthread_mutex_t;
typedef skyuv_cond pthread_cond_t;
typedef skyuv_tls pthread_key_t;
#endif

int skyuv_compat_pthread_create(pthread_t *thread, const void *attributes, void *(*entry)(void *),
								void *argument);

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
#define pthread_key_create skyuv_compat_pthread_key_create
#define pthread_key_delete skyuv_compat_pthread_key_delete
#define pthread_getspecific skyuv_compat_pthread_getspecific
#define pthread_setspecific skyuv_compat_pthread_setspecific

static inline int skyuv_compat_pthread_join(pthread_t thread, void **result) {
	(void)result;
	return skyuv_thread_join(&thread) == SKYUV_OK ? 0 : -1;
}

static inline int skyuv_compat_pthread_mutex_init(pthread_mutex_t *mutex, const void *attributes) {
	(void)attributes;
	mutex->implementation = NULL;
	return skyuv_mutex_init(mutex) == SKYUV_OK ? 0 : -1;
}

static inline int skyuv_compat_pthread_mutex_destroy(pthread_mutex_t *mutex) {
	skyuv_mutex_destroy(mutex);
	return 0;
}

static inline int skyuv_compat_pthread_mutex_lock(pthread_mutex_t *mutex) {
	skyuv_mutex_lock(mutex);
	return 0;
}

static inline int skyuv_compat_pthread_mutex_unlock(pthread_mutex_t *mutex) {
	skyuv_mutex_unlock(mutex);
	return 0;
}

static inline int skyuv_compat_pthread_cond_init(pthread_cond_t *condition,
												 const void *attributes) {
	(void)attributes;
	condition->implementation = NULL;
	return skyuv_cond_init(condition) == SKYUV_OK ? 0 : -1;
}

static inline int skyuv_compat_pthread_cond_destroy(pthread_cond_t *condition) {
	skyuv_cond_destroy(condition);
	return 0;
}

static inline int skyuv_compat_pthread_cond_signal(pthread_cond_t *condition) {
	skyuv_cond_signal(condition);
	return 0;
}

static inline int skyuv_compat_pthread_cond_broadcast(pthread_cond_t *condition) {
	skyuv_cond_broadcast(condition);
	return 0;
}

static inline int skyuv_compat_pthread_cond_wait(pthread_cond_t *condition,
												 pthread_mutex_t *mutex) {
	skyuv_cond_wait(condition, mutex);
	return 0;
}

static inline int skyuv_compat_pthread_key_create(pthread_key_t *key, void (*destructor)(void *)) {
	if (destructor != NULL) {
		return -1;
	}
	key->implementation = NULL;
	return skyuv_tls_init(key) == SKYUV_OK ? 0 : -1;
}

static inline int skyuv_compat_pthread_key_delete(pthread_key_t key) {
	skyuv_tls_destroy(&key);
	return 0;
}

static inline void *skyuv_compat_pthread_getspecific(pthread_key_t key) {
	return skyuv_tls_get(&key);
}

static inline int skyuv_compat_pthread_setspecific(pthread_key_t key, const void *value) {
	skyuv_tls_set(&key, (void *)value);
	return 0;
}

#endif
