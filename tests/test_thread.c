#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <skyuv/thread.h>

#define THREAD_COUNT 4
#define INCREMENT_COUNT 10000

struct counter_context {
	skyuv_mutex *mutex;
	int value;
};

static void
increment_counter(void *argument) {
	struct counter_context *context = argument;
	int index;

	for (index = 0; index < INCREMENT_COUNT; ++index) {
		skyuv_mutex_lock(context->mutex);
		++context->value;
		skyuv_mutex_unlock(context->mutex);
	}
}

static void
test_invalid_arguments(void **state) {
	skyuv_thread thread = SKYUV_THREAD_INITIALIZER;
	skyuv_mutex mutex = SKYUV_MUTEX_INITIALIZER;

	(void)state;
	assert_int_equal(
		skyuv_thread_create(NULL, increment_counter, NULL),
		SKYUV_ERROR_INVALID_ARGUMENT
	);
	assert_int_equal(
		skyuv_thread_create(&thread, NULL, NULL),
		SKYUV_ERROR_INVALID_ARGUMENT
	);
	assert_int_equal(skyuv_thread_join(&thread), SKYUV_ERROR_INVALID_STATE);
	assert_int_equal(skyuv_mutex_init(NULL), SKYUV_ERROR_INVALID_ARGUMENT);
	assert_int_equal(skyuv_mutex_init(&mutex), SKYUV_OK);
	assert_int_equal(skyuv_mutex_init(&mutex), SKYUV_ERROR_INVALID_STATE);
	skyuv_mutex_destroy(&mutex);
	skyuv_mutex_destroy(&mutex);
}

static void
test_threaded_counter(void **state) {
	skyuv_mutex mutex = SKYUV_MUTEX_INITIALIZER;
	skyuv_thread threads[THREAD_COUNT] = {
		SKYUV_THREAD_INITIALIZER,
		SKYUV_THREAD_INITIALIZER,
		SKYUV_THREAD_INITIALIZER,
		SKYUV_THREAD_INITIALIZER,
	};
	struct counter_context context;
	int index;

	(void)state;
	assert_int_equal(skyuv_mutex_init(&mutex), SKYUV_OK);
	context.mutex = &mutex;
	context.value = 0;

	for (index = 0; index < THREAD_COUNT; ++index) {
		assert_int_equal(
			skyuv_thread_create(&threads[index], increment_counter, &context),
			SKYUV_OK
		);
	}
	for (index = 0; index < THREAD_COUNT; ++index) {
		assert_int_equal(skyuv_thread_join(&threads[index]), SKYUV_OK);
		assert_int_equal(skyuv_thread_join(&threads[index]), SKYUV_ERROR_INVALID_STATE);
	}

	assert_int_equal(context.value, THREAD_COUNT * INCREMENT_COUNT);
	assert_true(skyuv_mutex_trylock(&mutex));
	skyuv_mutex_unlock(&mutex);
	skyuv_mutex_destroy(&mutex);
}

int
main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_invalid_arguments),
		cmocka_unit_test(test_threaded_counter),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
