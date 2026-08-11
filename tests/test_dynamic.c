#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#include <cmocka.h>

#include <skyuv/dynamic.h>

static void test_invalid_arguments(void **state) {
	skyuv_dl library = SKYUV_DL_INITIALIZER;
	void *symbol;

	(void)state;
	assert_int_equal(skyuv_dlopen(NULL, SKYUV_TEST_MODULE_PATH), SKYUV_ERROR_INVALID_ARGUMENT);
	assert_int_equal(skyuv_dlopen(&library, NULL), SKYUV_ERROR_INVALID_ARGUMENT);
	assert_int_equal(skyuv_dlsym(NULL, "symbol", &symbol), SKYUV_ERROR_INVALID_ARGUMENT);
	assert_int_equal(skyuv_dlsym(&library, NULL, &symbol), SKYUV_ERROR_INVALID_ARGUMENT);
	assert_int_equal(skyuv_dlsym(&library, "symbol", NULL), SKYUV_ERROR_INVALID_ARGUMENT);
	assert_int_equal(skyuv_dlsym(&library, "symbol", &symbol), SKYUV_ERROR_INVALID_STATE);
	assert_null(skyuv_dlerror(NULL));
	assert_null(skyuv_dlerror(&library));
	skyuv_dlclose(NULL);
	skyuv_dlclose(&library);
}

static void test_open_failure(void **state) {
	skyuv_dl library = SKYUV_DL_INITIALIZER;
	const char *error;

	(void)state;
	assert_int_equal(skyuv_dlopen(&library, "skyuv-module-that-does-not-exist"),
					 SKYUV_ERROR_SYSTEM);
	error = skyuv_dlerror(&library);
	assert_non_null(error);
	assert_true(strlen(error) > 0);
	assert_int_equal(skyuv_dlopen(&library, SKYUV_TEST_MODULE_PATH), SKYUV_ERROR_INVALID_STATE);
	skyuv_dlclose(&library);
	assert_null(skyuv_dlerror(&library));
}

static void test_load_and_find_symbol(void **state) {
	skyuv_dl library = SKYUV_DL_INITIALIZER;
	void *symbol;

	(void)state;
	assert_int_equal(skyuv_dlopen(&library, SKYUV_TEST_MODULE_PATH), SKYUV_OK);
	assert_int_equal(skyuv_dlsym(&library, "skyuv_test_module_value", &symbol), SKYUV_OK);
	assert_non_null(symbol);
	assert_int_equal(*(const int *)symbol, 42);
	assert_int_equal(skyuv_dlsym(&library, "skyuv_missing_symbol", &symbol), SKYUV_ERROR_SYSTEM);
	assert_null(symbol);
	assert_non_null(skyuv_dlerror(&library));
	skyuv_dlclose(&library);
	skyuv_dlclose(&library);
}

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_invalid_arguments),
		cmocka_unit_test(test_open_failure),
		cmocka_unit_test(test_load_and_find_symbol),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
