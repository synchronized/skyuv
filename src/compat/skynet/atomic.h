#ifndef SKYNET_ATOMIC_H
#define SKYNET_ATOMIC_H

#include <skyuv/atomic.h>

#define ATOM_INT skyuv_atomic_i32
#define ATOM_POINTER skyuv_atomic_uintptr
#define ATOM_SIZET skyuv_atomic_size
#define ATOM_ULONG skyuv_atomic_ulong
#define ATOM_INIT(pointer, value)                                                                  \
	_Generic((pointer),                                                                            \
		skyuv_atomic_i32 *: skyuv_atomic_i32_init,                                                 \
		skyuv_atomic_ulong *: skyuv_atomic_ulong_init,                                             \
		default: skyuv_atomic_uintptr_init)((pointer), (value))
#define ATOM_LOAD(pointer)                                                                         \
	_Generic((pointer),                                                                            \
		skyuv_atomic_i32 *: skyuv_atomic_i32_load,                                                 \
		const skyuv_atomic_i32 *: skyuv_atomic_i32_load,                                           \
		skyuv_atomic_ulong *: skyuv_atomic_ulong_load,                                             \
		const skyuv_atomic_ulong *: skyuv_atomic_ulong_load,                                       \
		default: skyuv_atomic_uintptr_load)((pointer), SKYUV_MEMORY_SEQ_CST)
#define ATOM_STORE(pointer, value)                                                                 \
	_Generic((pointer),                                                                            \
		skyuv_atomic_i32 *: skyuv_atomic_i32_store,                                                \
		skyuv_atomic_ulong *: skyuv_atomic_ulong_store,                                            \
		default: skyuv_atomic_uintptr_store)((pointer), (value), SKYUV_MEMORY_SEQ_CST)
#define ATOM_CAS(pointer, old_value, new_value)                                                    \
	skyuv_compat_atomic_cas_i32((pointer), (old_value), (new_value))
#define ATOM_CAS_ULONG(pointer, old_value, new_value)                                              \
	skyuv_compat_atomic_cas_ulong((pointer), (old_value), (new_value))
#define ATOM_CAS_SIZET(pointer, old_value, new_value)                                              \
	skyuv_compat_atomic_cas_size((pointer), (old_value), (new_value))
#define ATOM_CAS_POINTER(pointer, old_value, new_value)                                            \
	skyuv_compat_atomic_cas_uintptr((pointer), (old_value), (new_value))
#define ATOM_FINC(pointer) ATOM_FADD((pointer), 1)
#define ATOM_FDEC(pointer) ATOM_FSUB((pointer), 1)
#define ATOM_FADD(pointer, value)                                                                  \
	_Generic((pointer),                                                                            \
		skyuv_atomic_i32 *: skyuv_atomic_i32_fetch_add,                                            \
		skyuv_atomic_ulong *: skyuv_atomic_ulong_fetch_add,                                        \
		default: skyuv_atomic_uintptr_fetch_add)((pointer), (value), SKYUV_MEMORY_SEQ_CST)
#define ATOM_FSUB(pointer, value)                                                                  \
	_Generic((pointer),                                                                            \
		skyuv_atomic_i32 *: skyuv_atomic_i32_fetch_sub,                                            \
		skyuv_atomic_ulong *: skyuv_atomic_ulong_fetch_sub,                                        \
		default: skyuv_atomic_uintptr_fetch_sub)((pointer), (value), SKYUV_MEMORY_SEQ_CST)
#define ATOM_FAND(pointer, value)                                                                  \
	_Generic((pointer),                                                                            \
		skyuv_atomic_i32 *: skyuv_atomic_i32_fetch_and,                                            \
		skyuv_atomic_ulong *: skyuv_atomic_ulong_fetch_and,                                        \
		default: skyuv_atomic_uintptr_fetch_and)((pointer), (value), SKYUV_MEMORY_SEQ_CST)

static inline int skyuv_compat_atomic_cas_i32(skyuv_atomic_i32 *atomic, int32_t expected,
											  int32_t desired) {
	return skyuv_atomic_i32_compare_exchange(atomic, &expected, desired, SKYUV_MEMORY_SEQ_CST,
											 SKYUV_MEMORY_SEQ_CST);
}

static inline int skyuv_compat_atomic_cas_ulong(skyuv_atomic_ulong *atomic, unsigned long expected,
												unsigned long desired) {
	return skyuv_atomic_ulong_compare_exchange(atomic, &expected, desired, SKYUV_MEMORY_SEQ_CST,
											   SKYUV_MEMORY_SEQ_CST);
}

static inline int skyuv_compat_atomic_cas_size(skyuv_atomic_size *atomic, size_t expected,
											   size_t desired) {
	return skyuv_atomic_size_compare_exchange(atomic, &expected, desired, SKYUV_MEMORY_SEQ_CST,
											  SKYUV_MEMORY_SEQ_CST);
}

static inline int skyuv_compat_atomic_cas_uintptr(skyuv_atomic_uintptr *atomic, uintptr_t expected,
												  uintptr_t desired) {
	return skyuv_atomic_uintptr_compare_exchange(atomic, &expected, desired, SKYUV_MEMORY_SEQ_CST,
												 SKYUV_MEMORY_SEQ_CST);
}

#endif
