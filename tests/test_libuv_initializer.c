#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <uv.h>
#include <string.h>

/* 此内部函数仅用于验证固定版本补丁；测试链接静态 libuv，不扩展 skyuv 公共 API。 */
int uv__convert_to_localhost_if_unspecified(const struct sockaddr *address,
											struct sockaddr_storage *storage);

static void localhost_initializer(void **unused) {
	(void)unused;
	struct sockaddr_in6 address;
	struct sockaddr_storage storage;
	assert_int_equal(uv_ip6_addr("::", 12345, &address), 0);
	assert_int_equal(
		uv__convert_to_localhost_if_unspecified((const struct sockaddr *)&address, &storage), 0);
	struct sockaddr_in6 *result = (struct sockaddr_in6 *)&storage;
	assert_int_equal(result->sin6_port, address.sin6_port);
	for (size_t i = 0; i < 15; ++i)
		assert_int_equal(result->sin6_addr.s6_addr[i], 0);
	assert_int_equal(result->sin6_addr.s6_addr[15], 1);
	assert_int_equal(uv_ip6_addr("2001:db8::1234", 12345, &address), 0);
	assert_int_equal(
		uv__convert_to_localhost_if_unspecified((const struct sockaddr *)&address, &storage), 0);
	assert_memory_equal(&result->sin6_addr, &address.sin6_addr, sizeof(address.sin6_addr));
	assert_int_equal(uv__convert_to_localhost_if_unspecified(NULL, &storage), UV_EINVAL);
}

int main(void) {
	const struct CMUnitTest tests[] = {cmocka_unit_test(localhost_initializer)};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
