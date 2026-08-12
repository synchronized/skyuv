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

struct condition_context {
	skyuv_mutex *mutex;
	skyuv_cond *condition;
	bool ready;
};

struct tls_context {
	skyuv_tls *tls;
	int value;
	bool isolated;
};

static void increment_counter(void *argument) {
	struct counter_context *context = argument;
	int index;

	for (index = 0; index < INCREMENT_COUNT; ++index) {
		skyuv_mutex_lock(context->mutex);
		++context->value;
		skyuv_mutex_unlock(context->mutex);
	}
}

static void set_condition(void *argument) {
	struct condition_context *context = argument;

	skyuv_mutex_lock(context->mutex);
	context->ready = true;
	skyuv_cond_signal(context->condition);
	skyuv_mutex_unlock(context->mutex);
}

static void verify_tls(void *argument) {
	struct tls_context *context = argument;

	skyuv_tls_set(context->tls, &context->value);
	context->isolated = skyuv_tls_get(context->tls) == &context->value;
}

static void test_invalid_arguments(void **state) {
	skyuv_thread thread = SKYUV_THREAD_INITIALIZER;
	skyuv_mutex mutex = SKYUV_MUTEX_INITIALIZER;
	skyuv_cond condition = SKYUV_COND_INITIALIZER;
	skyuv_rwlock rwlock = SKYUV_RWLOCK_INITIALIZER;
	skyuv_tls tls = SKYUV_TLS_INITIALIZER;

	(void)state;
	assert_int_equal(skyuv_thread_create(NULL, increment_counter, NULL),
					 SKYUV_ERROR_INVALID_ARGUMENT);
	assert_int_equal(skyuv_thread_create(&thread, NULL, NULL), SKYUV_ERROR_INVALID_ARGUMENT);
	assert_int_equal(skyuv_thread_join(&thread), SKYUV_ERROR_INVALID_STATE);
	assert_int_equal(skyuv_mutex_init(NULL), SKYUV_ERROR_INVALID_ARGUMENT);
	assert_int_equal(skyuv_mutex_init(&mutex), SKYUV_OK);
	assert_int_equal(skyuv_mutex_init(&mutex), SKYUV_ERROR_INVALID_STATE);
	skyuv_mutex_destroy(&mutex);
	skyuv_mutex_destroy(&mutex);
	assert_int_equal(skyuv_cond_init(NULL), SKYUV_ERROR_INVALID_ARGUMENT);
	assert_int_equal(skyuv_cond_init(&condition), SKYUV_OK);
	assert_int_equal(skyuv_cond_init(&condition), SKYUV_ERROR_INVALID_STATE);
	skyuv_cond_destroy(&condition);
	skyuv_cond_destroy(&condition);
	assert_int_equal(skyuv_rwlock_init(NULL), SKYUV_ERROR_INVALID_ARGUMENT);
	assert_int_equal(skyuv_rwlock_init(&rwlock), SKYUV_OK);
	assert_int_equal(skyuv_rwlock_init(&rwlock), SKYUV_ERROR_INVALID_STATE);
	skyuv_rwlock_destroy(&rwlock);
	skyuv_rwlock_destroy(&rwlock);
	assert_int_equal(skyuv_tls_init(NULL), SKYUV_ERROR_INVALID_ARGUMENT);
	assert_int_equal(skyuv_tls_init(&tls), SKYUV_OK);
	assert_int_equal(skyuv_tls_init(&tls), SKYUV_ERROR_INVALID_STATE);
	skyuv_tls_destroy(&tls);
	skyuv_tls_destroy(&tls);
}

static void test_threaded_counter(void **state) {
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
		assert_int_equal(skyuv_thread_create(&threads[index], increment_counter, &context),
						 SKYUV_OK);
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

static void test_condition(void **state) {
	skyuv_mutex mutex = SKYUV_MUTEX_INITIALIZER;
	skyuv_cond condition = SKYUV_COND_INITIALIZER;
	skyuv_thread thread = SKYUV_THREAD_INITIALIZER;
	struct condition_context context;

	(void)state;
	assert_int_equal(skyuv_mutex_init(&mutex), SKYUV_OK);
	assert_int_equal(skyuv_cond_init(&condition), SKYUV_OK);
	context.mutex = &mutex;
	context.condition = &condition;
	context.ready = false;
	assert_int_equal(skyuv_thread_create(&thread, set_condition, &context), SKYUV_OK);

	skyuv_mutex_lock(&mutex);
	while (!context.ready) {
		skyuv_cond_wait(&condition, &mutex);
	}
	assert_int_equal(skyuv_cond_timedwait(&condition, &mutex, UINT64_C(1000000)),
					 SKYUV_ERROR_TIMEOUT);
	skyuv_mutex_unlock(&mutex);
	assert_int_equal(skyuv_thread_join(&thread), SKYUV_OK);
	skyuv_cond_broadcast(&condition);
	skyuv_cond_destroy(&condition);
	skyuv_mutex_destroy(&mutex);
}

static void test_rwlock(void **state) {
	skyuv_rwlock rwlock = SKYUV_RWLOCK_INITIALIZER;

	(void)state;
	assert_int_equal(skyuv_rwlock_init(&rwlock), SKYUV_OK);
	assert_true(skyuv_rwlock_tryrdlock(&rwlock));
	assert_true(skyuv_rwlock_tryrdlock(&rwlock));
	skyuv_rwlock_rdunlock(&rwlock);
	skyuv_rwlock_rdunlock(&rwlock);
	assert_true(skyuv_rwlock_trywrlock(&rwlock));
	skyuv_rwlock_wrunlock(&rwlock);
	skyuv_rwlock_wrlock(&rwlock);
	skyuv_rwlock_wrunlock(&rwlock);
	skyuv_rwlock_destroy(&rwlock);
}

static void test_tls_isolation(void **state) {
	skyuv_tls tls = SKYUV_TLS_INITIALIZER;
	skyuv_thread threads[2] = {SKYUV_THREAD_INITIALIZER, SKYUV_THREAD_INITIALIZER};
	struct tls_context contexts[2] = {{&tls, 1, false}, {&tls, 2, false}};
	int main_value = 3;
	int index;

	(void)state;
	assert_int_equal(skyuv_tls_init(&tls), SKYUV_OK);
	skyuv_tls_set(&tls, &main_value);
	for (index = 0; index < 2; ++index) {
		assert_int_equal(skyuv_thread_create(&threads[index], verify_tls, &contexts[index]),
						 SKYUV_OK);
	}
	for (index = 0; index < 2; ++index) {
		assert_int_equal(skyuv_thread_join(&threads[index]), SKYUV_OK);
		assert_true(contexts[index].isolated);
	}
	assert_ptr_equal(skyuv_tls_get(&tls), &main_value);
	skyuv_tls_destroy(&tls);
}

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_invalid_arguments), cmocka_unit_test(test_threaded_counter),
		cmocka_unit_test(test_condition),		  cmocka_unit_test(test_rwlock),
		cmocka_unit_test(test_tls_isolation),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
