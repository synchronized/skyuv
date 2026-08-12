#ifndef SKYUV_COMPAT_SKYNET_SPINLOCK_H
#define SKYUV_COMPAT_SKYNET_SPINLOCK_H

#include "atomic.h"

#define SPIN_INIT(object) spinlock_init(&(object)->lock);
#define SPIN_LOCK(object) spinlock_lock(&(object)->lock);
#define SPIN_UNLOCK(object) spinlock_unlock(&(object)->lock);
#define SPIN_DESTROY(object) spinlock_destroy(&(object)->lock);

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#define atomic_pause_() _mm_pause()
#elif (defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__))
#define atomic_pause_() __builtin_ia32_pause()
#else
#define atomic_pause_() ((void)0)
#endif

struct spinlock {
	skyuv_atomic_i32 lock;
};

static inline void spinlock_init(struct spinlock *lock) {
	skyuv_atomic_i32_init(&lock->lock, 0);
}

static inline void spinlock_lock(struct spinlock *lock) {
	for (;;) {
		if (skyuv_atomic_i32_exchange(&lock->lock, 1, SKYUV_MEMORY_ACQUIRE) == 0) {
			return;
		}
		while (skyuv_atomic_i32_load(&lock->lock, SKYUV_MEMORY_RELAXED) != 0) {
			atomic_pause_();
		}
	}
}

static inline int spinlock_trylock(struct spinlock *lock) {
	return skyuv_atomic_i32_load(&lock->lock, SKYUV_MEMORY_RELAXED) == 0 &&
		   skyuv_atomic_i32_exchange(&lock->lock, 1, SKYUV_MEMORY_ACQUIRE) == 0;
}

static inline void spinlock_unlock(struct spinlock *lock) {
	skyuv_atomic_i32_store(&lock->lock, 0, SKYUV_MEMORY_RELEASE);
}

static inline void spinlock_destroy(struct spinlock *lock) {
	(void)lock;
}

#endif
