#include <skyuv/atomic.h>

#include <limits.h>

#if defined(_MSC_VER)
#include <intrin.h>

_Static_assert(sizeof(int32_t) == sizeof(long), "MSVC long 必须为 32 位");

void skyuv_atomic_i32_init(skyuv_atomic_i32 *atomic, int32_t value) {
	atomic->value = value;
}

int32_t skyuv_atomic_i32_load(const skyuv_atomic_i32 *atomic, skyuv_memory_order order) {
	(void)order;
	return (int32_t)_InterlockedCompareExchange((volatile long *)&atomic->value, 0, 0);
}

void skyuv_atomic_i32_store(skyuv_atomic_i32 *atomic, int32_t value, skyuv_memory_order order) {
	(void)order;
	(void)_InterlockedExchange((volatile long *)&atomic->value, (long)value);
}

int32_t skyuv_atomic_i32_exchange(skyuv_atomic_i32 *atomic, int32_t value,
								  skyuv_memory_order order) {
	(void)order;
	return (int32_t)_InterlockedExchange((volatile long *)&atomic->value, (long)value);
}

bool skyuv_atomic_i32_compare_exchange(skyuv_atomic_i32 *atomic, int32_t *expected, int32_t desired,
									   skyuv_memory_order success, skyuv_memory_order failure) {
	long previous;

	(void)success;
	(void)failure;
	previous = _InterlockedCompareExchange((volatile long *)&atomic->value, (long)desired,
										   (long)*expected);
	if ((int32_t)previous == *expected) {
		return true;
	}
	*expected = (int32_t)previous;
	return false;
}

int32_t skyuv_atomic_i32_fetch_add(skyuv_atomic_i32 *atomic, int32_t value,
								   skyuv_memory_order order) {
	(void)order;
	return (int32_t)_InterlockedExchangeAdd((volatile long *)&atomic->value, (long)value);
}

int32_t skyuv_atomic_i32_fetch_sub(skyuv_atomic_i32 *atomic, int32_t value,
								   skyuv_memory_order order) {
	long delta = (long)(int32_t)(UINT32_C(0) - (uint32_t)value);

	(void)order;
	return (int32_t)_InterlockedExchangeAdd((volatile long *)&atomic->value, delta);
}

int32_t skyuv_atomic_i32_fetch_and(skyuv_atomic_i32 *atomic, int32_t value,
								   skyuv_memory_order order) {
	(void)order;
	return (int32_t)_InterlockedAnd((volatile long *)&atomic->value, (long)value);
}

#if UINTPTR_MAX == UINT64_MAX
#define SKYUV_INTERLOCKED_COMPARE_EXCHANGE_UINTPTR _InterlockedCompareExchange64
#define SKYUV_INTERLOCKED_EXCHANGE_UINTPTR _InterlockedExchange64
#define SKYUV_INTERLOCKED_EXCHANGE_ADD_UINTPTR _InterlockedExchangeAdd64
typedef __int64 skyuv_interlocked_uintptr;
#else
#define SKYUV_INTERLOCKED_COMPARE_EXCHANGE_UINTPTR _InterlockedCompareExchange
#define SKYUV_INTERLOCKED_EXCHANGE_UINTPTR _InterlockedExchange
#define SKYUV_INTERLOCKED_EXCHANGE_ADD_UINTPTR _InterlockedExchangeAdd
typedef long skyuv_interlocked_uintptr;
#endif

void skyuv_atomic_uintptr_init(skyuv_atomic_uintptr *atomic, uintptr_t value) {
	atomic->value = value;
}

uintptr_t skyuv_atomic_uintptr_load(const skyuv_atomic_uintptr *atomic, skyuv_memory_order order) {
	(void)order;
	return (uintptr_t)SKYUV_INTERLOCKED_COMPARE_EXCHANGE_UINTPTR(
		(volatile skyuv_interlocked_uintptr *)&atomic->value, 0, 0);
}

void skyuv_atomic_uintptr_store(skyuv_atomic_uintptr *atomic, uintptr_t value,
								skyuv_memory_order order) {
	(void)order;
	(void)SKYUV_INTERLOCKED_EXCHANGE_UINTPTR((volatile skyuv_interlocked_uintptr *)&atomic->value,
											 (skyuv_interlocked_uintptr)value);
}

uintptr_t skyuv_atomic_uintptr_exchange(skyuv_atomic_uintptr *atomic, uintptr_t value,
										skyuv_memory_order order) {
	(void)order;
	return (uintptr_t)SKYUV_INTERLOCKED_EXCHANGE_UINTPTR(
		(volatile skyuv_interlocked_uintptr *)&atomic->value, (skyuv_interlocked_uintptr)value);
}

bool skyuv_atomic_uintptr_compare_exchange(skyuv_atomic_uintptr *atomic, uintptr_t *expected,
										   uintptr_t desired, skyuv_memory_order success,
										   skyuv_memory_order failure) {
	skyuv_interlocked_uintptr previous;

	(void)success;
	(void)failure;
	previous = SKYUV_INTERLOCKED_COMPARE_EXCHANGE_UINTPTR(
		(volatile skyuv_interlocked_uintptr *)&atomic->value, (skyuv_interlocked_uintptr)desired,
		(skyuv_interlocked_uintptr)*expected);
	if ((uintptr_t)previous == *expected) {
		return true;
	}
	*expected = (uintptr_t)previous;
	return false;
}

uintptr_t skyuv_atomic_uintptr_fetch_add(skyuv_atomic_uintptr *atomic, uintptr_t value,
										 skyuv_memory_order order) {
	(void)order;
	return (uintptr_t)SKYUV_INTERLOCKED_EXCHANGE_ADD_UINTPTR(
		(volatile skyuv_interlocked_uintptr *)&atomic->value, (skyuv_interlocked_uintptr)value);
}

uintptr_t skyuv_atomic_uintptr_fetch_sub(skyuv_atomic_uintptr *atomic, uintptr_t value,
										 skyuv_memory_order order) {
	return skyuv_atomic_uintptr_fetch_add(atomic, (uintptr_t)(0 - value), order);
}

uintptr_t skyuv_atomic_uintptr_fetch_and(skyuv_atomic_uintptr *atomic, uintptr_t value,
										 skyuv_memory_order order) {
	uintptr_t expected = skyuv_atomic_uintptr_load(atomic, SKYUV_MEMORY_RELAXED);

	while (!skyuv_atomic_uintptr_compare_exchange(atomic, &expected, expected & value, order,
												  SKYUV_MEMORY_RELAXED)) {
	}
	return expected;
}

void skyuv_atomic_ulong_init(skyuv_atomic_ulong *atomic, unsigned long value) {
	atomic->value = value;
}

unsigned long skyuv_atomic_ulong_load(const skyuv_atomic_ulong *atomic, skyuv_memory_order order) {
	(void)order;
	return (unsigned long)_InterlockedCompareExchange((volatile long *)&atomic->value, 0, 0);
}

void skyuv_atomic_ulong_store(skyuv_atomic_ulong *atomic, unsigned long value,
							  skyuv_memory_order order) {
	(void)order;
	(void)_InterlockedExchange((volatile long *)&atomic->value, (long)value);
}

unsigned long skyuv_atomic_ulong_exchange(skyuv_atomic_ulong *atomic, unsigned long value,
										  skyuv_memory_order order) {
	(void)order;
	return (unsigned long)_InterlockedExchange((volatile long *)&atomic->value, (long)value);
}

bool skyuv_atomic_ulong_compare_exchange(skyuv_atomic_ulong *atomic, unsigned long *expected,
										 unsigned long desired, skyuv_memory_order success,
										 skyuv_memory_order failure) {
	unsigned long previous;

	(void)success;
	(void)failure;
	previous = (unsigned long)_InterlockedCompareExchange((volatile long *)&atomic->value,
														  (long)desired, (long)*expected);
	if (previous == *expected) {
		return true;
	}
	*expected = previous;
	return false;
}

unsigned long skyuv_atomic_ulong_fetch_add(skyuv_atomic_ulong *atomic, unsigned long value,
										   skyuv_memory_order order) {
	(void)order;
	return (unsigned long)_InterlockedExchangeAdd((volatile long *)&atomic->value, (long)value);
}

unsigned long skyuv_atomic_ulong_fetch_sub(skyuv_atomic_ulong *atomic, unsigned long value,
										   skyuv_memory_order order) {
	return skyuv_atomic_ulong_fetch_add(atomic, ULONG_MAX - value + 1, order);
}

unsigned long skyuv_atomic_ulong_fetch_and(skyuv_atomic_ulong *atomic, unsigned long value,
										   skyuv_memory_order order) {
	(void)order;
	return (unsigned long)_InterlockedAnd((volatile long *)&atomic->value, (long)value);
}

#else

static int native_memory_order(skyuv_memory_order order) {
	switch (order) {
	case SKYUV_MEMORY_RELAXED:
		return __ATOMIC_RELAXED;
	case SKYUV_MEMORY_ACQUIRE:
		return __ATOMIC_ACQUIRE;
	case SKYUV_MEMORY_RELEASE:
		return __ATOMIC_RELEASE;
	case SKYUV_MEMORY_ACQ_REL:
		return __ATOMIC_ACQ_REL;
	case SKYUV_MEMORY_SEQ_CST:
	default:
		return __ATOMIC_SEQ_CST;
	}
}

#define DEFINE_ATOMIC_FUNCTIONS(suffix, atomic_type, value_type)                                   \
	void skyuv_atomic_##suffix##_init(atomic_type *atomic, value_type value) {                     \
		__atomic_store_n(&atomic->value, value, __ATOMIC_RELAXED);                                 \
	}                                                                                              \
	value_type skyuv_atomic_##suffix##_load(const atomic_type *atomic, skyuv_memory_order order) { \
		return __atomic_load_n(&atomic->value, native_memory_order(order));                        \
	}                                                                                              \
	void skyuv_atomic_##suffix##_store(atomic_type *atomic, value_type value,                      \
									   skyuv_memory_order order) {                                 \
		__atomic_store_n(&atomic->value, value, native_memory_order(order));                       \
	}                                                                                              \
	value_type skyuv_atomic_##suffix##_exchange(atomic_type *atomic, value_type value,             \
												skyuv_memory_order order) {                        \
		return __atomic_exchange_n(&atomic->value, value, native_memory_order(order));             \
	}                                                                                              \
	bool skyuv_atomic_##suffix##_compare_exchange(atomic_type *atomic, value_type *expected,       \
												  value_type desired, skyuv_memory_order success,  \
												  skyuv_memory_order failure) {                    \
		return __atomic_compare_exchange_n(&atomic->value, expected, desired, false,               \
										   native_memory_order(success),                           \
										   native_memory_order(failure));                          \
	}                                                                                              \
	value_type skyuv_atomic_##suffix##_fetch_add(atomic_type *atomic, value_type value,            \
												 skyuv_memory_order order) {                       \
		return __atomic_fetch_add(&atomic->value, value, native_memory_order(order));              \
	}                                                                                              \
	value_type skyuv_atomic_##suffix##_fetch_sub(atomic_type *atomic, value_type value,            \
												 skyuv_memory_order order) {                       \
		return __atomic_fetch_sub(&atomic->value, value, native_memory_order(order));              \
	}

DEFINE_ATOMIC_FUNCTIONS(i32, skyuv_atomic_i32, int32_t)
DEFINE_ATOMIC_FUNCTIONS(uintptr, skyuv_atomic_uintptr, uintptr_t)
DEFINE_ATOMIC_FUNCTIONS(ulong, skyuv_atomic_ulong, unsigned long)

int32_t skyuv_atomic_i32_fetch_and(skyuv_atomic_i32 *atomic, int32_t value,
								   skyuv_memory_order order) {
	return __atomic_fetch_and(&atomic->value, value, native_memory_order(order));
}

uintptr_t skyuv_atomic_uintptr_fetch_and(skyuv_atomic_uintptr *atomic, uintptr_t value,
										 skyuv_memory_order order) {
	return __atomic_fetch_and(&atomic->value, value, native_memory_order(order));
}

unsigned long skyuv_atomic_ulong_fetch_and(skyuv_atomic_ulong *atomic, unsigned long value,
										   skyuv_memory_order order) {
	return __atomic_fetch_and(&atomic->value, value, native_memory_order(order));
}

#endif

void skyuv_atomic_pointer_init(skyuv_atomic_pointer *atomic, void *value) {
	skyuv_atomic_uintptr_init(atomic, (uintptr_t)value);
}

void *skyuv_atomic_pointer_load(const skyuv_atomic_pointer *atomic, skyuv_memory_order order) {
	return (void *)skyuv_atomic_uintptr_load(atomic, order);
}

void skyuv_atomic_pointer_store(skyuv_atomic_pointer *atomic, void *value,
								skyuv_memory_order order) {
	skyuv_atomic_uintptr_store(atomic, (uintptr_t)value, order);
}

void *skyuv_atomic_pointer_exchange(skyuv_atomic_pointer *atomic, void *value,
									skyuv_memory_order order) {
	return (void *)skyuv_atomic_uintptr_exchange(atomic, (uintptr_t)value, order);
}

bool skyuv_atomic_pointer_compare_exchange(skyuv_atomic_pointer *atomic, void **expected,
										   void *desired, skyuv_memory_order success,
										   skyuv_memory_order failure) {
	uintptr_t integer_expected = (uintptr_t)*expected;
	bool result = skyuv_atomic_uintptr_compare_exchange(atomic, &integer_expected,
														(uintptr_t)desired, success, failure);

	if (!result) {
		*expected = (void *)integer_expected;
	}
	return result;
}

void skyuv_atomic_size_init(skyuv_atomic_size *atomic, size_t value) {
	skyuv_atomic_uintptr_init(atomic, (uintptr_t)value);
}

size_t skyuv_atomic_size_load(const skyuv_atomic_size *atomic, skyuv_memory_order order) {
	return (size_t)skyuv_atomic_uintptr_load(atomic, order);
}

void skyuv_atomic_size_store(skyuv_atomic_size *atomic, size_t value, skyuv_memory_order order) {
	skyuv_atomic_uintptr_store(atomic, (uintptr_t)value, order);
}

size_t skyuv_atomic_size_exchange(skyuv_atomic_size *atomic, size_t value,
								  skyuv_memory_order order) {
	return (size_t)skyuv_atomic_uintptr_exchange(atomic, (uintptr_t)value, order);
}

bool skyuv_atomic_size_compare_exchange(skyuv_atomic_size *atomic, size_t *expected, size_t desired,
										skyuv_memory_order success, skyuv_memory_order failure) {
	uintptr_t integer_expected = (uintptr_t)*expected;
	bool result = skyuv_atomic_uintptr_compare_exchange(atomic, &integer_expected,
														(uintptr_t)desired, success, failure);

	if (!result) {
		*expected = (size_t)integer_expected;
	}
	return result;
}

size_t skyuv_atomic_size_fetch_add(skyuv_atomic_size *atomic, size_t value,
								   skyuv_memory_order order) {
	return (size_t)skyuv_atomic_uintptr_fetch_add(atomic, (uintptr_t)value, order);
}

size_t skyuv_atomic_size_fetch_sub(skyuv_atomic_size *atomic, size_t value,
								   skyuv_memory_order order) {
	return (size_t)skyuv_atomic_uintptr_fetch_sub(atomic, (uintptr_t)value, order);
}

size_t skyuv_atomic_size_fetch_and(skyuv_atomic_size *atomic, size_t value,
								   skyuv_memory_order order) {
	return (size_t)skyuv_atomic_uintptr_fetch_and(atomic, (uintptr_t)value, order);
}
