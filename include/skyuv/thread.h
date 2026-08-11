#ifndef SKYUV_THREAD_H
#define SKYUV_THREAD_H

#include <stdbool.h>
#include <stddef.h>

#include <skyuv/error.h>

typedef void (*skyuv_thread_entry)(void *argument);

typedef struct skyuv_thread {
	void *implementation;
} skyuv_thread;

typedef struct skyuv_mutex {
	void *implementation;
} skyuv_mutex;

#define SKYUV_THREAD_INITIALIZER { NULL }
#define SKYUV_MUTEX_INITIALIZER { NULL }

int skyuv_thread_create(skyuv_thread *thread, skyuv_thread_entry entry, void *argument);
int skyuv_thread_join(skyuv_thread *thread);

int skyuv_mutex_init(skyuv_mutex *mutex);
void skyuv_mutex_destroy(skyuv_mutex *mutex);
void skyuv_mutex_lock(skyuv_mutex *mutex);
bool skyuv_mutex_trylock(skyuv_mutex *mutex);
void skyuv_mutex_unlock(skyuv_mutex *mutex);

#endif
