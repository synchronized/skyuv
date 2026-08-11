#ifndef SKYUV_ATOMIC_H
#define SKYUV_ATOMIC_H

#include <stdbool.h>
#include <stdint.h>

typedef enum skyuv_memory_order {
	SKYUV_MEMORY_RELAXED,
	SKYUV_MEMORY_ACQUIRE,
	SKYUV_MEMORY_RELEASE,
	SKYUV_MEMORY_ACQ_REL,
	SKYUV_MEMORY_SEQ_CST,
} skyuv_memory_order;

typedef struct skyuv_atomic_i32 {
	int32_t value;
} skyuv_atomic_i32;

typedef struct skyuv_atomic_uintptr {
	uintptr_t value;
} skyuv_atomic_uintptr;

typedef skyuv_atomic_uintptr skyuv_atomic_pointer;

#define SKYUV_ATOMIC_I32_INITIALIZER(value) {(value)}
#define SKYUV_ATOMIC_UINTPTR_INITIALIZER(value) {(value)}
#define SKYUV_ATOMIC_POINTER_INITIALIZER(value) {(uintptr_t)(value)}

void skyuv_atomic_i32_init(skyuv_atomic_i32 *atomic, int32_t value);
int32_t skyuv_atomic_i32_load(const skyuv_atomic_i32 *atomic, skyuv_memory_order order);
void skyuv_atomic_i32_store(skyuv_atomic_i32 *atomic, int32_t value, skyuv_memory_order order);
int32_t skyuv_atomic_i32_exchange(skyuv_atomic_i32 *atomic, int32_t value,
								  skyuv_memory_order order);
bool skyuv_atomic_i32_compare_exchange(skyuv_atomic_i32 *atomic, int32_t *expected, int32_t desired,
									   skyuv_memory_order success, skyuv_memory_order failure);
int32_t skyuv_atomic_i32_fetch_add(skyuv_atomic_i32 *atomic, int32_t value,
								   skyuv_memory_order order);
int32_t skyuv_atomic_i32_fetch_sub(skyuv_atomic_i32 *atomic, int32_t value,
								   skyuv_memory_order order);
int32_t skyuv_atomic_i32_fetch_and(skyuv_atomic_i32 *atomic, int32_t value,
								   skyuv_memory_order order);

void skyuv_atomic_uintptr_init(skyuv_atomic_uintptr *atomic, uintptr_t value);
uintptr_t skyuv_atomic_uintptr_load(const skyuv_atomic_uintptr *atomic, skyuv_memory_order order);
void skyuv_atomic_uintptr_store(skyuv_atomic_uintptr *atomic, uintptr_t value,
								skyuv_memory_order order);
uintptr_t skyuv_atomic_uintptr_exchange(skyuv_atomic_uintptr *atomic, uintptr_t value,
										skyuv_memory_order order);
bool skyuv_atomic_uintptr_compare_exchange(skyuv_atomic_uintptr *atomic, uintptr_t *expected,
										   uintptr_t desired, skyuv_memory_order success,
										   skyuv_memory_order failure);
uintptr_t skyuv_atomic_uintptr_fetch_add(skyuv_atomic_uintptr *atomic, uintptr_t value,
										 skyuv_memory_order order);
uintptr_t skyuv_atomic_uintptr_fetch_sub(skyuv_atomic_uintptr *atomic, uintptr_t value,
										 skyuv_memory_order order);

void skyuv_atomic_pointer_init(skyuv_atomic_pointer *atomic, void *value);
void *skyuv_atomic_pointer_load(const skyuv_atomic_pointer *atomic, skyuv_memory_order order);
void skyuv_atomic_pointer_store(skyuv_atomic_pointer *atomic, void *value,
								skyuv_memory_order order);
void *skyuv_atomic_pointer_exchange(skyuv_atomic_pointer *atomic, void *value,
									skyuv_memory_order order);
bool skyuv_atomic_pointer_compare_exchange(skyuv_atomic_pointer *atomic, void **expected,
										   void *desired, skyuv_memory_order success,
										   skyuv_memory_order failure);

#endif
