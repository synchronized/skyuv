#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include "socket_internal.h"

static void test_id_roundtrip(void **state) {
	int id;

	(void)state;
	id = skyuv_socket_id_make(UINT16_C(0xabcd), UINT16_C(0x1234));
	assert_true(id > 0);
	assert_int_equal(skyuv_socket_id_slot(id), UINT16_C(0xabcd));
	assert_int_equal(skyuv_socket_id_generation(id), UINT16_C(0x1234));
}

static void test_generation_wrap(void **state) {
	(void)state;
	assert_int_equal(skyuv_socket_id_make(0U, 0U), -1);
	assert_int_equal(skyuv_socket_id_make(0U, UINT16_MAX), -1);
	assert_int_equal(skyuv_socket_id_next_generation(0U), 1U);
	assert_int_equal(skyuv_socket_id_next_generation(1U), 2U);
	assert_int_equal(skyuv_socket_id_next_generation(SKYUV_SOCKET_GENERATION_MAX), 1U);
}

static void test_same_slot_different_generation(void **state) {
	int first;
	int second;

	(void)state;
	first = skyuv_socket_id_make(7U, 1U);
	second = skyuv_socket_id_make(7U, 2U);
	assert_int_not_equal(first, second);
	assert_int_equal(skyuv_socket_id_slot(first), skyuv_socket_id_slot(second));
}

static void test_state_classification(void **state) {
	(void)state;
	assert_false(skyuv_socket_state_is_live(SKYUV_SOCKET_STATE_INVALID));
	assert_true(skyuv_socket_state_is_live(SKYUV_SOCKET_STATE_RESERVED));
	assert_false(skyuv_socket_state_is_live(SKYUV_SOCKET_STATE_CLOSING));
	assert_true(skyuv_socket_state_can_receive(SKYUV_SOCKET_STATE_LISTENING));
	assert_true(skyuv_socket_state_can_receive(SKYUV_SOCKET_STATE_CONNECTED));
	assert_false(skyuv_socket_state_can_receive(SKYUV_SOCKET_STATE_ACCEPTED_PAUSED));
	assert_false(skyuv_socket_state_can_receive(SKYUV_SOCKET_STATE_CONNECTING));
}

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_id_roundtrip),
		cmocka_unit_test(test_generation_wrap),
		cmocka_unit_test(test_same_slot_different_generation),
		cmocka_unit_test(test_state_classification),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
