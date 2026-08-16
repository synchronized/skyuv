#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <cmocka.h>

#include <skyuv/memory.h>

static void test_zero_size_and_null(void **state) {
	void *memory;

	(void)state;
	memory = skyuv_malloc(0);
	assert_non_null(memory);
	skyuv_free(memory);
	skyuv_free(NULL);

	memory = skyuv_calloc(0, 32);
	assert_non_null(memory);
	assert_int_equal(*(unsigned char *)memory, 0);
	skyuv_free(memory);

	memory = skyuv_realloc(NULL, 32);
	assert_non_null(memory);
	assert_null(skyuv_realloc(memory, 0));
}

static void test_calloc_and_overflow(void **state) {
	unsigned char *memory;
	size_t index;

	(void)state;
	memory = skyuv_calloc(16, 8);
	assert_non_null(memory);
	for (index = 0; index < 128; ++index) {
		assert_int_equal(memory[index], 0);
	}
	skyuv_free(memory);
	assert_null(skyuv_calloc(SIZE_MAX, 2));
}

static void test_reallocation_preserves_content(void **state) {
	unsigned char *memory;
	size_t index;

	(void)state;
	memory = skyuv_malloc(64);
	assert_non_null(memory);
	memset(memory, 0x5a, 64);
	memory = skyuv_realloc(memory, 256);
	assert_non_null(memory);
	for (index = 0; index < 64; ++index) {
		assert_int_equal(memory[index], 0x5a);
	}
	skyuv_free(memory);
}

static void test_aligned_allocation(void **state) {
	void *memory;

	(void)state;
	memory = skyuv_aligned_alloc(64, 257);
	assert_non_null(memory);
	assert_int_equal((uintptr_t)memory % 64, 0);
	memset(memory, 0xa5, 257);
	skyuv_free(memory);

	assert_null(skyuv_aligned_alloc(3, 64));
	assert_null(skyuv_aligned_alloc(sizeof(void *) / 2, 64));
}

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_zero_size_and_null),
		cmocka_unit_test(test_calloc_and_overflow),
		cmocka_unit_test(test_reallocation_preserves_content),
		cmocka_unit_test(test_aligned_allocation),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
