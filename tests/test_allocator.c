#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <cmocka.h>

#if defined(SKYUV_USE_JEMALLOC)
#include <jemalloc.h>
#define allocator_malloc je_malloc
#define allocator_realloc je_realloc
#define allocator_free je_free
#else
#define allocator_malloc malloc
#define allocator_realloc realloc
#define allocator_free free
#endif

static void
test_allocator_reallocation(void **state) {
	unsigned char *memory;
	size_t index;

	(void)state;
	memory = allocator_malloc(256);
	assert_non_null(memory);

	memset(memory, 0x5a, 256);
	memory = allocator_realloc(memory, 512);
	assert_non_null(memory);

	for (index = 0; index < 256; ++index) {
		assert_int_equal(memory[index], 0x5a);
	}

	allocator_free(memory);
}

int
main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_allocator_reallocation),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
