#ifndef SKYUV_THREAD_H
#define SKYUV_THREAD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <skyuv/error.h>

typedef void (*skyuv_thread_entry)(void *argument);

typedef struct skyuv_thread {
	void *implementation;
} skyuv_thread;

typedef struct skyuv_mutex {
	void *implementation;
} skyuv_mutex;

typedef struct skyuv_cond {
	void *implementation;
} skyuv_cond;

typedef struct skyuv_rwlock {
	void *implementation;
} skyuv_rwlock;

#define SKYUV_TLS_STORAGE_SIZE 16

typedef union skyuv_tls_storage {
	void *pointer_alignment;
	uint64_t integer_alignment;
	unsigned char data[SKYUV_TLS_STORAGE_SIZE];
} skyuv_tls_storage;

typedef struct skyuv_tls {
	skyuv_tls_storage storage;
	bool initialized;
} skyuv_tls;

#define SKYUV_THREAD_INITIALIZER {NULL}
#define SKYUV_MUTEX_INITIALIZER {NULL}
#define SKYUV_COND_INITIALIZER {NULL}
#define SKYUV_RWLOCK_INITIALIZER {NULL}
#define SKYUV_TLS_INITIALIZER {0}

int skyuv_thread_create(skyuv_thread *thread, skyuv_thread_entry entry, void *argument);
int skyuv_thread_join(skyuv_thread *thread);

int skyuv_mutex_init(skyuv_mutex *mutex);
void skyuv_mutex_destroy(skyuv_mutex *mutex);
void skyuv_mutex_lock(skyuv_mutex *mutex);
bool skyuv_mutex_trylock(skyuv_mutex *mutex);
void skyuv_mutex_unlock(skyuv_mutex *mutex);

int skyuv_cond_init(skyuv_cond *condition);
void skyuv_cond_destroy(skyuv_cond *condition);
void skyuv_cond_signal(skyuv_cond *condition);
void skyuv_cond_broadcast(skyuv_cond *condition);
void skyuv_cond_wait(skyuv_cond *condition, skyuv_mutex *mutex);
int skyuv_cond_timedwait(skyuv_cond *condition, skyuv_mutex *mutex, uint64_t timeout_ns);

int skyuv_rwlock_init(skyuv_rwlock *rwlock);
void skyuv_rwlock_destroy(skyuv_rwlock *rwlock);
void skyuv_rwlock_rdlock(skyuv_rwlock *rwlock);
bool skyuv_rwlock_tryrdlock(skyuv_rwlock *rwlock);
void skyuv_rwlock_rdunlock(skyuv_rwlock *rwlock);
void skyuv_rwlock_wrlock(skyuv_rwlock *rwlock);
bool skyuv_rwlock_trywrlock(skyuv_rwlock *rwlock);
void skyuv_rwlock_wrunlock(skyuv_rwlock *rwlock);

int skyuv_tls_init(skyuv_tls *tls);
void skyuv_tls_destroy(skyuv_tls *tls);
void *skyuv_tls_get(skyuv_tls *tls);
void skyuv_tls_set(skyuv_tls *tls, void *value);

#endif
