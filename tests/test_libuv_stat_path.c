#include <uv.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <winioctl.h>
#include "../3rd/libuv/src/win/winapi.h"
#include <string.h>
#include <wchar.h>

static int fail_stage;
static int closed;
static int assigned;

static HANDLE fixture_open(LPCWSTR path, DWORD access, DWORD share,
		LPSECURITY_ATTRIBUTES security, DWORD disposition, DWORD flags, HANDLE template_file) {
	(void)path; (void)access; (void)share; (void)security;
	(void)disposition; (void)flags; (void)template_file;
	if (fail_stage == 1) {
		SetLastError(ERROR_ACCESS_DENIED);
		return INVALID_HANDLE_VALUE;
	}
	return (HANDLE)(uintptr_t)1;
}

static BOOL fixture_close(HANDLE handle) {
	assert_true(handle == (HANDLE)(uintptr_t)1);
	++closed;
	return TRUE;
}

static NTSTATUS fixture_unicode(PUNICODE_STRING destination, PWSTR source, size_t length) {
	(void)destination; (void)source; (void)length;
	return fail_stage == 2 ? STATUS_INVALID_PARAMETER : STATUS_SUCCESS;
}

static NTSTATUS fixture_directory(HANDLE handle, HANDLE event, void *apc, void *context,
		PIO_STATUS_BLOCK status, void *buffer, ULONG length, FILE_INFORMATION_CLASS type,
		BOOLEAN single, PUNICODE_STRING mask, BOOLEAN restart) {
	(void)handle; (void)event; (void)apc; (void)context; (void)status;
	(void)type; (void)single; (void)mask; (void)restart;
	memset(buffer, 0, length);
	return fail_stage == 3 ? STATUS_NO_MORE_FILES : STATUS_SUCCESS;
}

static NTSTATUS fixture_volume(HANDLE handle, PIO_STATUS_BLOCK status, void *buffer,
		ULONG length, FS_INFORMATION_CLASS type) {
	(void)handle;
	memset(buffer, 0, length);
	status->Status = STATUS_SUCCESS;
	if ((fail_stage == 4 && type == FileFsVolumeInformation) ||
		(fail_stage == 5 && type == FileFsDeviceInformation))
		return STATUS_INVALID_PARAMETER;
	return STATUS_SUCCESS;
}

static ULONG fixture_error(NTSTATUS status) {
	(void)status;
	return ERROR_INVALID_PARAMETER;
}

static void fixture_assign(uv_stat_t *statbuf, FILE_STAT_BASIC_INFORMATION info, int lstat) {
	(void)statbuf; (void)info; (void)lstat;
	++assigned;
}

/* 使用完整原函数，只替换外部系统边界，验证路径所有权及关闭行为。 */
#define CreateFileW fixture_open
#define CloseHandle fixture_close
#define uv__RtlUnicodeStringInit fixture_unicode
#define pNtQueryDirectoryFile fixture_directory
#define pNtQueryVolumeInformationFile fixture_volume
#define pRtlNtStatusToDosError fixture_error
#define fs__stat_assign_statbuf fixture_assign
#include "libuv_stat_fixture.h"
#undef fs__stat_assign_statbuf
#undef pRtlNtStatusToDosError
#undef pNtQueryVolumeInformationFile
#undef pNtQueryDirectoryFile
#undef uv__RtlUnicodeStringInit
#undef CloseHandle
#undef CreateFileW

static void path_restoration(void **unused) {
	(void)unused;
	const WCHAR *paths[] = {L"C:\\", L"C:", L".", L"..", L"", L"dir\\",
		L"dir/", L"file", L"dir\\file", L"dir/file", L"dir\\bad?"};
	for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
		for (fail_stage = 0; fail_stage <= 5; ++fail_stage) {
			WCHAR storage[128];
			/* 前后哨兵便于检测错误恢复写入，路径缓冲区每次重新初始化。 */
			for (size_t j = 0; j < 128; ++j)
				storage[j] = L'!';
			WCHAR *path = &storage[1];
			memcpy(path, paths[i], (wcslen(paths[i]) + 1) * sizeof(WCHAR));
			WCHAR expected[128];
			memcpy(expected, storage, sizeof(storage));
			uv_stat_t statbuf;
			closed = assigned = 0;
			DWORD result = fs__stat_directory(path, &statbuf, 0, ERROR_ACCESS_DENIED);
			assert_memory_equal(storage, expected, sizeof(storage));
			if (wcschr(paths[i], L'?') != NULL) {
				assert_int_equal(result, ERROR_INVALID_NAME);
				assert_int_equal(closed, 0);
			} else {
				assert_int_equal(result, fail_stage == 0 ? 0 :
					fail_stage == 1 ? ERROR_ACCESS_DENIED :
					fail_stage == 3 ? ERROR_PATH_NOT_FOUND : ERROR_INVALID_PARAMETER);
				assert_int_equal(closed, fail_stage == 1 ? 0 : 1);
			}
			assert_int_equal(assigned, result == 0 ? 1 : 0);
		}
	}
}

int main(void) {
	const struct CMUnitTest tests[] = {cmocka_unit_test(path_restoration)};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
