#include <skyuv/thread.h>

#include <stdlib.h>

#include <uv.h>

struct skyuv_thread_start {
	skyuv_thread_entry entry;
	void *argument;
};

static void thread_start(void *argument) {
	struct skyuv_thread_start *start = argument;
	skyuv_thread_entry entry = start->entry;
	void *entry_argument = start->argument;

	free(start);
	entry(entry_argument);
}

int skyuv_thread_create(skyuv_thread *thread, skyuv_thread_entry entry, void *argument) {
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

int skyuv_thread_join(skyuv_thread *thread) {
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

int skyuv_mutex_init(skyuv_mutex *mutex) {
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

void skyuv_mutex_destroy(skyuv_mutex *mutex) {
	uv_mutex_t *implementation;

	if (mutex == NULL || mutex->implementation == NULL) {
		return;
	}

	implementation = mutex->implementation;
	uv_mutex_destroy(implementation);
	free(implementation);
	mutex->implementation = NULL;
}

void skyuv_mutex_lock(skyuv_mutex *mutex) {
	uv_mutex_lock(mutex->implementation);
}

bool skyuv_mutex_trylock(skyuv_mutex *mutex) {
	return uv_mutex_trylock(mutex->implementation) == 0;
}

void skyuv_mutex_unlock(skyuv_mutex *mutex) {
	uv_mutex_unlock(mutex->implementation);
}

#define DEFINE_PLATFORM_INIT(function_name, public_type, native_type, native_init)                 \
	int function_name(public_type *object) {                                                       \
		native_type *implementation;                                                               \
		int result;                                                                                \
		if (object == NULL) {                                                                      \
			return SKYUV_ERROR_INVALID_ARGUMENT;                                                   \
		}                                                                                          \
		if (object->implementation != NULL) {                                                      \
			return SKYUV_ERROR_INVALID_STATE;                                                      \
		}                                                                                          \
		implementation = malloc(sizeof(*implementation));                                          \
		if (implementation == NULL) {                                                              \
			return SKYUV_ERROR_OUT_OF_MEMORY;                                                      \
		}                                                                                          \
		result = native_init(implementation);                                                      \
		if (result != 0) {                                                                         \
			free(implementation);                                                                  \
			return SKYUV_ERROR_SYSTEM;                                                             \
		}                                                                                          \
		object->implementation = implementation;                                                   \
		return SKYUV_OK;                                                                           \
	}

#define DEFINE_PLATFORM_DESTROY(function_name, public_type, native_type, native_destroy)           \
	void function_name(public_type *object) {                                                      \
		native_type *implementation;                                                               \
		if (object == NULL || object->implementation == NULL) {                                    \
			return;                                                                                \
		}                                                                                          \
		implementation = object->implementation;                                                   \
		native_destroy(implementation);                                                            \
		free(implementation);                                                                      \
		object->implementation = NULL;                                                             \
	}

DEFINE_PLATFORM_INIT(skyuv_cond_init, skyuv_cond, uv_cond_t, uv_cond_init)
DEFINE_PLATFORM_DESTROY(skyuv_cond_destroy, skyuv_cond, uv_cond_t, uv_cond_destroy)

void skyuv_cond_signal(skyuv_cond *condition) {
	uv_cond_signal(condition->implementation);
}

void skyuv_cond_broadcast(skyuv_cond *condition) {
	uv_cond_broadcast(condition->implementation);
}

void skyuv_cond_wait(skyuv_cond *condition, skyuv_mutex *mutex) {
	uv_cond_wait(condition->implementation, mutex->implementation);
}

int skyuv_cond_timedwait(skyuv_cond *condition, skyuv_mutex *mutex, uint64_t timeout_ns) {
	int result = uv_cond_timedwait(condition->implementation, mutex->implementation, timeout_ns);

	if (result == 0) {
		return SKYUV_OK;
	}
	if (result == UV_ETIMEDOUT) {
		return SKYUV_ERROR_TIMEOUT;
	}
	return SKYUV_ERROR_SYSTEM;
}

DEFINE_PLATFORM_INIT(skyuv_rwlock_init, skyuv_rwlock, uv_rwlock_t, uv_rwlock_init)
DEFINE_PLATFORM_DESTROY(skyuv_rwlock_destroy, skyuv_rwlock, uv_rwlock_t, uv_rwlock_destroy)

void skyuv_rwlock_rdlock(skyuv_rwlock *rwlock) {
	uv_rwlock_rdlock(rwlock->implementation);
}

bool skyuv_rwlock_tryrdlock(skyuv_rwlock *rwlock) {
	return uv_rwlock_tryrdlock(rwlock->implementation) == 0;
}

void skyuv_rwlock_rdunlock(skyuv_rwlock *rwlock) {
	uv_rwlock_rdunlock(rwlock->implementation);
}

void skyuv_rwlock_wrlock(skyuv_rwlock *rwlock) {
	uv_rwlock_wrlock(rwlock->implementation);
}

bool skyuv_rwlock_trywrlock(skyuv_rwlock *rwlock) {
	return uv_rwlock_trywrlock(rwlock->implementation) == 0;
}

void skyuv_rwlock_wrunlock(skyuv_rwlock *rwlock) {
	uv_rwlock_wrunlock(rwlock->implementation);
}

DEFINE_PLATFORM_INIT(skyuv_tls_init, skyuv_tls, uv_key_t, uv_key_create)
DEFINE_PLATFORM_DESTROY(skyuv_tls_destroy, skyuv_tls, uv_key_t, uv_key_delete)

void *skyuv_tls_get(skyuv_tls *tls) {
	return uv_key_get(tls->implementation);
}

void skyuv_tls_set(skyuv_tls *tls, void *value) {
	uv_key_set(tls->implementation, value);
}
