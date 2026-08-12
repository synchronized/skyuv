#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#include <cmocka.h>

#include <skyuv/atomic.h>
#include <skyuv/thread.h>

#define THREAD_COUNT 4
#define INCREMENT_COUNT 10000

struct counter_context {
	skyuv_atomic_i32 *counter;
};

struct publish_context {
	skyuv_atomic_pointer pointer;
	int value;
};

static void increment_counter(void *argument) {
	struct counter_context *context = argument;
	int index;

	for (index = 0; index < INCREMENT_COUNT; ++index) {
		(void)skyuv_atomic_i32_fetch_add(context->counter, 1, SKYUV_MEMORY_RELAXED);
	}
}

static void publish_pointer(void *argument) {
	struct publish_context *context = argument;

	context->value = 42;
	skyuv_atomic_pointer_store(&context->pointer, &context->value, SKYUV_MEMORY_RELEASE);
}

static void test_i32_operations(void **state) {
	skyuv_atomic_i32 atomic = SKYUV_ATOMIC_I32_INITIALIZER(1);
	int32_t expected;

	(void)state;
	assert_int_equal(skyuv_atomic_i32_load(&atomic, SKYUV_MEMORY_RELAXED), 1);
	skyuv_atomic_i32_store(&atomic, 2, SKYUV_MEMORY_RELEASE);
	assert_int_equal(skyuv_atomic_i32_exchange(&atomic, 3, SKYUV_MEMORY_ACQ_REL), 2);
	expected = 2;
	assert_false(skyuv_atomic_i32_compare_exchange(&atomic, &expected, 4, SKYUV_MEMORY_ACQ_REL,
												   SKYUV_MEMORY_ACQUIRE));
	assert_int_equal(expected, 3);
	assert_true(skyuv_atomic_i32_compare_exchange(&atomic, &expected, 7, SKYUV_MEMORY_ACQ_REL,
												  SKYUV_MEMORY_ACQUIRE));
	assert_int_equal(skyuv_atomic_i32_fetch_add(&atomic, 5, SKYUV_MEMORY_SEQ_CST), 7);
	assert_int_equal(skyuv_atomic_i32_fetch_sub(&atomic, 2, SKYUV_MEMORY_SEQ_CST), 12);
	assert_int_equal(skyuv_atomic_i32_fetch_and(&atomic, 6, SKYUV_MEMORY_SEQ_CST), 10);
	assert_int_equal(skyuv_atomic_i32_load(&atomic, SKYUV_MEMORY_ACQUIRE), 2);
	skyuv_atomic_i32_init(&atomic, -1);
	assert_int_equal(skyuv_atomic_i32_load(&atomic, SKYUV_MEMORY_RELAXED), -1);
	skyuv_atomic_i32_init(&atomic, INT32_MIN);
	assert_int_equal(skyuv_atomic_i32_fetch_sub(&atomic, INT32_MIN, SKYUV_MEMORY_SEQ_CST),
					 INT32_MIN);
	assert_int_equal(skyuv_atomic_i32_load(&atomic, SKYUV_MEMORY_RELAXED), 0);
}

static void test_uintptr_operations(void **state) {
	skyuv_atomic_uintptr atomic = SKYUV_ATOMIC_UINTPTR_INITIALIZER(1);
	uintptr_t expected;

	(void)state;
	assert_int_equal(skyuv_atomic_uintptr_load(&atomic, SKYUV_MEMORY_RELAXED), 1);
	assert_int_equal(skyuv_atomic_uintptr_fetch_add(&atomic, 2, SKYUV_MEMORY_SEQ_CST), 1);
	assert_int_equal(skyuv_atomic_uintptr_fetch_sub(&atomic, 1, SKYUV_MEMORY_SEQ_CST), 3);
	assert_int_equal(skyuv_atomic_uintptr_exchange(&atomic, 8, SKYUV_MEMORY_SEQ_CST), 2);
	expected = 7;
	assert_false(skyuv_atomic_uintptr_compare_exchange(&atomic, &expected, 9, SKYUV_MEMORY_ACQ_REL,
													   SKYUV_MEMORY_ACQUIRE));
	assert_int_equal(expected, 8);
	assert_true(skyuv_atomic_uintptr_compare_exchange(&atomic, &expected, 9, SKYUV_MEMORY_ACQ_REL,
													  SKYUV_MEMORY_ACQUIRE));
	assert_int_equal(skyuv_atomic_uintptr_load(&atomic, SKYUV_MEMORY_ACQUIRE), 9);
	skyuv_atomic_uintptr_init(&atomic, 0);
	assert_int_equal(skyuv_atomic_uintptr_load(&atomic, SKYUV_MEMORY_RELAXED), 0);
}

static void test_concurrent_counter(void **state) {
	skyuv_atomic_i32 counter = SKYUV_ATOMIC_I32_INITIALIZER(0);
	skyuv_thread threads[THREAD_COUNT] = {
		SKYUV_THREAD_INITIALIZER,
		SKYUV_THREAD_INITIALIZER,
		SKYUV_THREAD_INITIALIZER,
		SKYUV_THREAD_INITIALIZER,
	};
	struct counter_context context = {&counter};
	int index;

	(void)state;
	for (index = 0; index < THREAD_COUNT; ++index) {
		assert_int_equal(skyuv_thread_create(&threads[index], increment_counter, &context),
						 SKYUV_OK);
	}
	for (index = 0; index < THREAD_COUNT; ++index) {
		assert_int_equal(skyuv_thread_join(&threads[index]), SKYUV_OK);
	}
	assert_int_equal(skyuv_atomic_i32_load(&counter, SKYUV_MEMORY_RELAXED),
					 THREAD_COUNT * INCREMENT_COUNT);
}

static void test_pointer_operations(void **state) {
	skyuv_atomic_pointer atomic = SKYUV_ATOMIC_POINTER_INITIALIZER(NULL);
	int first = 1;
	int second = 2;
	void *expected = NULL;

	(void)state;
	assert_null(skyuv_atomic_pointer_load(&atomic, SKYUV_MEMORY_RELAXED));
	assert_true(skyuv_atomic_pointer_compare_exchange(&atomic, &expected, &first,
													  SKYUV_MEMORY_RELEASE, SKYUV_MEMORY_RELAXED));
	assert_ptr_equal(skyuv_atomic_pointer_exchange(&atomic, &second, SKYUV_MEMORY_ACQ_REL), &first);
	expected = &first;
	assert_false(skyuv_atomic_pointer_compare_exchange(&atomic, &expected, NULL,
													   SKYUV_MEMORY_ACQ_REL, SKYUV_MEMORY_ACQUIRE));
	assert_ptr_equal(expected, &second);
	skyuv_atomic_pointer_store(&atomic, NULL, SKYUV_MEMORY_RELEASE);
	assert_null(skyuv_atomic_pointer_load(&atomic, SKYUV_MEMORY_ACQUIRE));
}

static void test_release_acquire(void **state) {
	struct publish_context context = {SKYUV_ATOMIC_UINTPTR_INITIALIZER(0), 0};
	skyuv_thread thread = SKYUV_THREAD_INITIALIZER;
	void *pointer;

	(void)state;
	assert_int_equal(skyuv_thread_create(&thread, publish_pointer, &context), SKYUV_OK);
	do {
		pointer = skyuv_atomic_pointer_load(&context.pointer, SKYUV_MEMORY_ACQUIRE);
	} while (pointer == NULL);
	assert_int_equal(*(const int *)pointer, 42);
	assert_int_equal(skyuv_thread_join(&thread), SKYUV_OK);
}

static void test_size_operations(void **state) {
	skyuv_atomic_size atomic = SKYUV_ATOMIC_SIZE_INITIALIZER(SIZE_MAX);
	size_t expected;

	(void)state;
	assert_int_equal(skyuv_atomic_size_fetch_add(&atomic, 1, SKYUV_MEMORY_SEQ_CST), SIZE_MAX);
	assert_int_equal(skyuv_atomic_size_load(&atomic, SKYUV_MEMORY_RELAXED), 0);
	skyuv_atomic_size_store(&atomic, 7, SKYUV_MEMORY_RELEASE);
	assert_int_equal(skyuv_atomic_size_fetch_sub(&atomic, 2, SKYUV_MEMORY_ACQ_REL), 7);
	assert_int_equal(skyuv_atomic_size_fetch_and(&atomic, 3, SKYUV_MEMORY_SEQ_CST), 5);
	expected = 2;
	assert_false(skyuv_atomic_size_compare_exchange(&atomic, &expected, 9, SKYUV_MEMORY_ACQ_REL,
													SKYUV_MEMORY_ACQUIRE));
	assert_int_equal(expected, 1);
	assert_int_equal(skyuv_atomic_size_exchange(&atomic, SIZE_MAX, SKYUV_MEMORY_SEQ_CST), 1);
	skyuv_atomic_size_init(&atomic, 0);
}

static void test_ulong_operations(void **state) {
	skyuv_atomic_ulong atomic = SKYUV_ATOMIC_ULONG_INITIALIZER(ULONG_MAX);
	unsigned long expected;

	(void)state;
	assert_int_equal(skyuv_atomic_ulong_fetch_add(&atomic, 1, SKYUV_MEMORY_SEQ_CST), ULONG_MAX);
	assert_int_equal(skyuv_atomic_ulong_load(&atomic, SKYUV_MEMORY_RELAXED), 0);
	skyuv_atomic_ulong_store(&atomic, 7, SKYUV_MEMORY_RELEASE);
	assert_int_equal(skyuv_atomic_ulong_fetch_sub(&atomic, 2, SKYUV_MEMORY_ACQ_REL), 7);
	assert_int_equal(skyuv_atomic_ulong_fetch_and(&atomic, 3, SKYUV_MEMORY_SEQ_CST), 5);
	expected = 2;
	assert_false(skyuv_atomic_ulong_compare_exchange(&atomic, &expected, 9, SKYUV_MEMORY_ACQ_REL,
													 SKYUV_MEMORY_ACQUIRE));
	assert_int_equal(expected, 1);
	assert_int_equal(skyuv_atomic_ulong_exchange(&atomic, ULONG_MAX, SKYUV_MEMORY_SEQ_CST), 1);
	skyuv_atomic_ulong_init(&atomic, 0);
}

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_i32_operations),	   cmocka_unit_test(test_uintptr_operations),
		cmocka_unit_test(test_concurrent_counter), cmocka_unit_test(test_pointer_operations),
		cmocka_unit_test(test_release_acquire),	   cmocka_unit_test(test_size_operations),
		cmocka_unit_test(test_ulong_operations),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
