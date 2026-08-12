#ifndef SKYUV_ATOMIC_H
#define SKYUV_ATOMIC_H

#include <stdbool.h>
#include <stddef.h>
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
typedef skyuv_atomic_uintptr skyuv_atomic_size;

typedef struct skyuv_atomic_ulong {
	unsigned long value;
} skyuv_atomic_ulong;

#define SKYUV_ATOMIC_I32_INITIALIZER(value) {(value)}
#define SKYUV_ATOMIC_UINTPTR_INITIALIZER(value) {(value)}
#define SKYUV_ATOMIC_POINTER_INITIALIZER(value) {(uintptr_t)(value)}
#define SKYUV_ATOMIC_SIZE_INITIALIZER(value) {(uintptr_t)(value)}
#define SKYUV_ATOMIC_ULONG_INITIALIZER(value) {(value)}

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
uintptr_t skyuv_atomic_uintptr_fetch_and(skyuv_atomic_uintptr *atomic, uintptr_t value,
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

void skyuv_atomic_size_init(skyuv_atomic_size *atomic, size_t value);
size_t skyuv_atomic_size_load(const skyuv_atomic_size *atomic, skyuv_memory_order order);
void skyuv_atomic_size_store(skyuv_atomic_size *atomic, size_t value, skyuv_memory_order order);
size_t skyuv_atomic_size_exchange(skyuv_atomic_size *atomic, size_t value,
								  skyuv_memory_order order);
bool skyuv_atomic_size_compare_exchange(skyuv_atomic_size *atomic, size_t *expected, size_t desired,
										skyuv_memory_order success, skyuv_memory_order failure);
size_t skyuv_atomic_size_fetch_add(skyuv_atomic_size *atomic, size_t value,
								   skyuv_memory_order order);
size_t skyuv_atomic_size_fetch_sub(skyuv_atomic_size *atomic, size_t value,
								   skyuv_memory_order order);
size_t skyuv_atomic_size_fetch_and(skyuv_atomic_size *atomic, size_t value,
								   skyuv_memory_order order);

void skyuv_atomic_ulong_init(skyuv_atomic_ulong *atomic, unsigned long value);
unsigned long skyuv_atomic_ulong_load(const skyuv_atomic_ulong *atomic, skyuv_memory_order order);
void skyuv_atomic_ulong_store(skyuv_atomic_ulong *atomic, unsigned long value,
							  skyuv_memory_order order);
unsigned long skyuv_atomic_ulong_exchange(skyuv_atomic_ulong *atomic, unsigned long value,
										  skyuv_memory_order order);
bool skyuv_atomic_ulong_compare_exchange(skyuv_atomic_ulong *atomic, unsigned long *expected,
										 unsigned long desired, skyuv_memory_order success,
										 skyuv_memory_order failure);
unsigned long skyuv_atomic_ulong_fetch_add(skyuv_atomic_ulong *atomic, unsigned long value,
										   skyuv_memory_order order);
unsigned long skyuv_atomic_ulong_fetch_sub(skyuv_atomic_ulong *atomic, unsigned long value,
										   skyuv_memory_order order);
unsigned long skyuv_atomic_ulong_fetch_and(skyuv_atomic_ulong *atomic, unsigned long value,
										   skyuv_memory_order order);

#endif
