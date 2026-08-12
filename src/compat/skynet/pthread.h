#ifndef SKYUV_COMPAT_SKYNET_PTHREAD_H
#define SKYUV_COMPAT_SKYNET_PTHREAD_H

#include <skyuv/thread.h>

typedef skyuv_thread pthread_t;
typedef skyuv_mutex pthread_mutex_t;
typedef skyuv_cond pthread_cond_t;
typedef skyuv_tls pthread_key_t;

int skyuv_compat_pthread_create(pthread_t *thread, const void *attributes, void *(*entry)(void *),
								void *argument);

#define pthread_create skyuv_compat_pthread_create

static inline int pthread_join(pthread_t thread, void **result) {
	(void)result;
	return skyuv_thread_join(&thread) == SKYUV_OK ? 0 : -1;
}

static inline int pthread_mutex_init(pthread_mutex_t *mutex, const void *attributes) {
	(void)attributes;
	mutex->implementation = NULL;
	return skyuv_mutex_init(mutex) == SKYUV_OK ? 0 : -1;
}

static inline int pthread_mutex_destroy(pthread_mutex_t *mutex) {
	skyuv_mutex_destroy(mutex);
	return 0;
}

static inline int pthread_mutex_lock(pthread_mutex_t *mutex) {
	skyuv_mutex_lock(mutex);
	return 0;
}

static inline int pthread_mutex_unlock(pthread_mutex_t *mutex) {
	skyuv_mutex_unlock(mutex);
	return 0;
}

static inline int pthread_cond_init(pthread_cond_t *condition, const void *attributes) {
	(void)attributes;
	condition->implementation = NULL;
	return skyuv_cond_init(condition) == SKYUV_OK ? 0 : -1;
}

static inline int pthread_cond_destroy(pthread_cond_t *condition) {
	skyuv_cond_destroy(condition);
	return 0;
}

static inline int pthread_cond_signal(pthread_cond_t *condition) {
	skyuv_cond_signal(condition);
	return 0;
}

static inline int pthread_cond_broadcast(pthread_cond_t *condition) {
	skyuv_cond_broadcast(condition);
	return 0;
}

static inline int pthread_cond_wait(pthread_cond_t *condition, pthread_mutex_t *mutex) {
	skyuv_cond_wait(condition, mutex);
	return 0;
}

static inline int pthread_key_create(pthread_key_t *key, void (*destructor)(void *)) {
	if (destructor != NULL) {
		return -1;
	}
	key->implementation = NULL;
	return skyuv_tls_init(key) == SKYUV_OK ? 0 : -1;
}

static inline int pthread_key_delete(pthread_key_t key) {
	skyuv_tls_destroy(&key);
	return 0;
}

static inline void *pthread_getspecific(pthread_key_t key) {
	return skyuv_tls_get(&key);
}

static inline int pthread_setspecific(pthread_key_t key, const void *value) {
	skyuv_tls_set(&key, (void *)value);
	return 0;
}

#endif
