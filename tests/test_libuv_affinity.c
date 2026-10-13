#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <uv.h>
#include <windows.h>
#include <string.h>
#include <assert.h>

static DWORD_PTR fixture_process_mask;
static DWORD_PTR fixture_thread_mask;

static BOOL fixture_get_process_mask(HANDLE process, DWORD_PTR *mask, DWORD_PTR *system) {
	(void)process;
	*mask = fixture_process_mask;
	*system = fixture_process_mask;
	return TRUE;
}

static DWORD_PTR fixture_set_thread_mask(HANDLE thread, DWORD_PTR mask) {
	(void)thread;
	DWORD_PTR previous = fixture_thread_mask;
	fixture_thread_mask = mask;
	return previous;
}

/* 使用生产补丁副本中的原函数，仅替换系统查询，避免依赖 64 CPU 测试机。 */
#define GetProcessAffinityMask fixture_get_process_mask
#define SetThreadAffinityMask fixture_set_thread_mask
#define uv_thread_setaffinity fixture_thread_setaffinity
#include "libuv_affinity_fixture.h"
#undef uv_thread_setaffinity
#undef SetThreadAffinityMask
#undef GetProcessAffinityMask

static void affinity_high_bits(void **unused) {
	(void)unused;
	uv_thread_t thread = uv_thread_self();
	char mask[sizeof(DWORD_PTR) * 8];
	char oldmask[sizeof(mask)];
	for (size_t i = 0; i < sizeof(mask); ++i) {
		memset(mask, 0, sizeof(mask));
		mask[i] = 1;
		fixture_process_mask = (DWORD_PTR)1 << i;
		fixture_thread_mask = fixture_process_mask;
		assert_int_equal(fixture_thread_setaffinity(&thread, mask, oldmask, sizeof(mask)), 0);
		assert_true(fixture_thread_mask == fixture_process_mask);
		assert_memory_equal(oldmask, mask, sizeof(mask));
		fixture_process_mask = 0;
		assert_int_equal(fixture_thread_setaffinity(&thread, mask, NULL, sizeof(mask)), UV_EINVAL);
	}
}

static void affinity_mask_width(void **unused) {
	(void)unused;
	DWORD_PTR process_mask;
	DWORD_PTR system_mask;
	assert_true(GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask));
	uv_thread_t thread = uv_thread_self();
	char original[sizeof(DWORD_PTR) * 8];
	char mask[sizeof(DWORD_PTR) * 8];
	assert_int_equal(uv_cpumask_size(), sizeof(mask));
	assert_int_equal(uv_thread_getaffinity(&thread, original, sizeof(original)), 0);
	assert_int_equal(uv_thread_setaffinity(&thread, original, NULL, sizeof(mask) - 1), UV_EINVAL);
	/* 逐位测试所有 CPU，包括未获授权的高位；每次先恢复线程归属，再断言结果。 */
	for (size_t i = 0; i < sizeof(mask); ++i) {
		memset(mask, 0, sizeof(mask));
		mask[i] = 1;
		int result = uv_thread_setaffinity(&thread, mask, NULL, sizeof(mask));
		int observed = -1;
		char current[sizeof(mask)];
		if (result == 0)
			observed = uv_thread_getaffinity(&thread, current, sizeof(current));
		assert_int_equal(uv_thread_setaffinity(&thread, original, NULL, sizeof(original)), 0);
		if (process_mask & ((DWORD_PTR)1 << i)) {
			assert_int_equal(result, 0);
			assert_int_equal(observed, 0);
			assert_memory_equal(current, mask, sizeof(mask));
		} else {
			assert_int_equal(result, UV_EINVAL);
		}
	}
}

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(affinity_mask_width), cmocka_unit_test(affinity_high_bits)};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
