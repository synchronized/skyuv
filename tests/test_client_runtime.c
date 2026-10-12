#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "client_runtime.h"

struct command_state {
	uv_thread_t caller;
	uv_thread_t owner;
	uv_timer_t timer;
	int count;
};

static int increment(uv_loop_t *loop, void *argument) {
	struct command_state *state = argument;
	(void)loop;
	state->owner = uv_thread_self();
	++state->count;
	return UV_EACCES;
}

static int create_handle(uv_loop_t *loop, void *argument) {
	struct command_state *state = argument;
	return uv_timer_init(loop, &state->timer);
}

static void lifecycle(void **unused) {
	(void)unused;
	assert_int_equal(skyuv_client_runtime_create(NULL), UV_EINVAL);
	assert_int_equal(skyuv_client_runtime_call(NULL, increment, NULL), UV_EINVAL);
	skyuv_client_runtime_destroy(NULL);
	for (int iteration = 0; iteration < 100; ++iteration) {
		struct skyuv_client_runtime *runtime = NULL;
		struct command_state state = {0};
		state.caller = uv_thread_self();
		assert_int_equal(skyuv_client_runtime_create(&runtime), 0);
		assert_non_null(runtime);
		assert_int_equal(skyuv_client_runtime_call(runtime, NULL, NULL), UV_EINVAL);
		for (int command = 0; command < 20; ++command)
			assert_int_equal(skyuv_client_runtime_call(runtime, increment, &state), UV_EACCES);
		assert_int_equal(state.count, 20);
		assert_false(uv_thread_equal(&state.caller, &state.owner));
		assert_int_equal(skyuv_client_runtime_call(runtime, create_handle, &state), 0);
		skyuv_client_runtime_destroy(&runtime);
		assert_null(runtime);
		skyuv_client_runtime_destroy(&runtime);
	}
}

int main(void) {
	const struct CMUnitTest tests[] = {cmocka_unit_test(lifecycle)};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
