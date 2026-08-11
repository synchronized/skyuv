#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <skyuv/time.h>

#define UNIX_TIME_2020_NS UINT64_C(1577836800000000000)

static void test_invalid_arguments(void **state) {
	(void)state;
	assert_int_equal(skyuv_time_monotonic(NULL), SKYUV_ERROR_INVALID_ARGUMENT);
	assert_int_equal(skyuv_time_realtime(NULL), SKYUV_ERROR_INVALID_ARGUMENT);
	assert_int_equal(skyuv_time_thread_cpu(NULL), SKYUV_ERROR_INVALID_ARGUMENT);
}

static void test_monotonic_time(void **state) {
	uint64_t before;
	uint64_t after;

	(void)state;
	assert_int_equal(skyuv_time_monotonic(&before), SKYUV_OK);
	assert_int_equal(skyuv_time_monotonic(&after), SKYUV_OK);
	assert_true(after >= before);
}

static void test_realtime(void **state) {
	uint64_t time_ns;

	(void)state;
	assert_int_equal(skyuv_time_realtime(&time_ns), SKYUV_OK);
	assert_true(time_ns >= UNIX_TIME_2020_NS);
}

static void test_thread_cpu_time(void **state) {
	uint64_t before;
	uint64_t after;
	int result;

	(void)state;
	result = skyuv_time_thread_cpu(&before);
	if (result == SKYUV_ERROR_NOT_SUPPORTED) {
		skip();
	}
	assert_int_equal(result, SKYUV_OK);
	assert_int_equal(skyuv_time_thread_cpu(&after), SKYUV_OK);
	assert_true(after >= before);
}

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_invalid_arguments),
		cmocka_unit_test(test_monotonic_time),
		cmocka_unit_test(test_realtime),
		cmocka_unit_test(test_thread_cpu_time),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
