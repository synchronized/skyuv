#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <cmocka.h>

#include <skyuv/dynamic.h>
#include <skyuv/memory.h>

typedef void *(*skyuv_test_allocate_fn)(size_t size, int value);
typedef void *(*skyuv_test_reallocate_fn)(void *pointer, size_t size);
typedef void (*skyuv_test_release_fn)(void *pointer);

static const char *module_path;

static void load_function(skyuv_dl *library, const char *name, void *function, size_t size) {
	void *symbol = NULL;

	assert_int_equal(size, sizeof(symbol));
	assert_int_equal(skyuv_dlsym(library, name, &symbol), SKYUV_OK);
	memcpy(function, &symbol, size);
}

static void test_invalid_arguments(void **state) {
	skyuv_dl library = SKYUV_DL_INITIALIZER;
	void *symbol;

	(void)state;
	assert_int_equal(skyuv_dlopen(NULL, module_path), SKYUV_ERROR_INVALID_ARGUMENT);
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
	assert_int_equal(skyuv_dlopen(&library, module_path), SKYUV_ERROR_INVALID_STATE);
	skyuv_dlclose(&library);
	assert_null(skyuv_dlerror(&library));
}

static void test_load_and_find_symbol(void **state) {
	skyuv_dl library = SKYUV_DL_INITIALIZER;
	void *symbol;

	(void)state;
	assert_int_equal(skyuv_dlopen(&library, module_path), SKYUV_OK);
	assert_int_equal(skyuv_dlsym(&library, "skyuv_test_module_value", &symbol), SKYUV_OK);
	assert_non_null(symbol);
	assert_int_equal(*(const int *)symbol, 42);
	assert_int_equal(skyuv_dlsym(&library, "skyuv_missing_symbol", &symbol), SKYUV_ERROR_SYSTEM);
	assert_null(symbol);
	assert_non_null(skyuv_dlerror(&library));
	skyuv_dlclose(&library);
	skyuv_dlclose(&library);
}

static void test_cross_module_memory_ownership(void **state) {
	unsigned char *module_memory;
	unsigned char *core_memory;
	size_t cycle;
	size_t index;

	(void)state;
	for (cycle = 0; cycle < 64; ++cycle) {
		skyuv_dl library = SKYUV_DL_INITIALIZER;
		skyuv_test_allocate_fn module_allocate = NULL;
		skyuv_test_reallocate_fn module_reallocate = NULL;
		skyuv_test_release_fn module_release = NULL;

		assert_int_equal(skyuv_dlopen(&library, module_path), SKYUV_OK);
		load_function(&library, "skyuv_test_module_allocate", &module_allocate,
					  sizeof(module_allocate));
		load_function(&library, "skyuv_test_module_reallocate", &module_reallocate,
					  sizeof(module_reallocate));
		load_function(&library, "skyuv_test_module_release", &module_release,
					  sizeof(module_release));

		module_memory = module_allocate(128, 0x5a);
		assert_non_null(module_memory);
		for (index = 0; index < 128; ++index) {
			assert_int_equal(module_memory[index], 0x5a);
		}

		core_memory = skyuv_malloc(64);
		assert_non_null(core_memory);
		memset(core_memory, 0xa5, 64);
		core_memory = module_reallocate(core_memory, 256);
		assert_non_null(core_memory);
		for (index = 0; index < 64; ++index) {
			assert_int_equal(core_memory[index], 0xa5);
		}
		module_release(core_memory);

		skyuv_dlclose(&library);
		/* 分配发生在模块调用期间，但所有权属于主程序，卸载后仍可由核心释放。 */
		skyuv_free(module_memory);
	}
}

int main(int argc, char **argv) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_invalid_arguments),
		cmocka_unit_test(test_open_failure),
		cmocka_unit_test(test_load_and_find_symbol),
		cmocka_unit_test(test_cross_module_memory_ownership),
	};

	if (argc != 2) {
		return EXIT_FAILURE;
	}
	module_path = argv[1];

	return cmocka_run_group_tests(tests, NULL, NULL);
}
