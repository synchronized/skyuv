#ifndef SKYUV_COMPAT_SKYNET_TLS_PTHREAD_H
#define SKYUV_COMPAT_SKYNET_TLS_PTHREAD_H

#include <stddef.h>

#include <skyuv/thread.h>

#define pthread_key_t skyuv_tls

static int
skyuv_compat_pthread_key_create(pthread_key_t *key, void (*destructor)(void *)) {
	if (destructor != NULL) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	*key = (skyuv_tls)SKYUV_TLS_INITIALIZER;
	return skyuv_tls_init(key);
}

static int
skyuv_compat_pthread_key_delete(pthread_key_t key) {
	skyuv_tls_destroy(&key);
	return 0;
}

static void *
skyuv_compat_pthread_getspecific(pthread_key_t key) {
	return skyuv_tls_get(&key);
}

static int
skyuv_compat_pthread_setspecific(pthread_key_t key, const void *value) {
	skyuv_tls_set(&key, (void *)value);
	return 0;
}

#define pthread_key_create skyuv_compat_pthread_key_create
#define pthread_key_delete skyuv_compat_pthread_key_delete
#define pthread_getspecific skyuv_compat_pthread_getspecific
#define pthread_setspecific skyuv_compat_pthread_setspecific

#endif
