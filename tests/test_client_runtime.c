#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdlib.h>

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

static void accepted_closed(uv_handle_t *handle) {
	free(handle);
}

static void accept_client(uv_stream_t *listener, int status) {
	assert_int_equal(status, 0);
	uv_tcp_t *client = malloc(sizeof(*client));
	assert_non_null(client);
	assert_int_equal(uv_tcp_init(listener->loop, client), 0);
	assert_int_equal(uv_accept(listener, (uv_stream_t *)client), 0);
	uv_close((uv_handle_t *)client, accepted_closed);
}

struct migrated_call {
	struct skyuv_client_runtime *runtime;
	int port;
	int result;
	int64_t id;
};

static void connect_from_worker(void *argument) {
	struct migrated_call *call = argument;
	call->result = skyuv_client_runtime_connect(call->runtime, "127.0.0.1", call->port, &call->id);
}

static void tcp_connections(void **unused) {
	(void)unused;
	uv_loop_t loop;
	uv_tcp_t listener;
	uv_tcp_t refused;
	struct sockaddr_in address;
	int address_length = (int)sizeof(address);
	assert_int_equal(uv_loop_init(&loop), 0);
	assert_int_equal(uv_tcp_init(&loop, &listener), 0);
	assert_int_equal(uv_ip4_addr("127.0.0.1", 0, &address), 0);
	assert_int_equal(uv_tcp_bind(&listener, (const struct sockaddr *)&address, 0), 0);
	assert_int_equal(uv_listen((uv_stream_t *)&listener, 128, accept_client), 0);
	assert_int_equal(uv_tcp_getsockname(&listener, (struct sockaddr *)&address, &address_length), 0);
	int port = (int)ntohs(address.sin_port);
	struct skyuv_client_runtime *runtime = NULL;
	assert_int_equal(skyuv_client_runtime_create(&runtime), 0);
	/* 调用线程可迁移；连接创建与后续关闭仍由同一 I/O 线程执行。 */
	struct migrated_call call = {runtime, port, 0, 0};
	uv_thread_t worker;
	assert_int_equal(uv_thread_create(&worker, connect_from_worker, &call), 0);
	assert_int_equal(uv_thread_join(&worker), 0);
	assert_int_equal(call.result, 0);
	assert_true(call.id > 0);
	(void)uv_run(&loop, UV_RUN_NOWAIT);
	assert_int_equal(skyuv_client_runtime_close(runtime, call.id), 0);
	int64_t previous = 0;
	for (int iteration = 0; iteration < 100; ++iteration) {
		int64_t id = 0;
		assert_int_equal(skyuv_client_runtime_connect(runtime, "127.0.0.1", port, &id), 0);
		assert_true(id > previous);
		previous = id;
		(void)uv_run(&loop, UV_RUN_NOWAIT);
		assert_int_equal(skyuv_client_runtime_close(runtime, id), 0);
		assert_int_equal(skyuv_client_runtime_close(runtime, id), 0);
	}
	int64_t id = 42;
	assert_int_equal(skyuv_client_runtime_connect(runtime, "invalid", port, &id), UV_EINVAL);
	assert_int_equal(id, 0);
	assert_int_equal(skyuv_client_runtime_connect(runtime, "127.0.0.1", -1, &id), UV_EINVAL);
	assert_int_equal(skyuv_client_runtime_close(runtime, previous + 1), UV_EBADF);
	/* 绑定但不监听的端口在测试期间保留，避免固定端口被其他进程占用。 */
	assert_int_equal(uv_tcp_init(&loop, &refused), 0);
	assert_int_equal(uv_ip4_addr("127.0.0.1", 0, &address), 0);
	assert_int_equal(uv_tcp_bind(&refused, (const struct sockaddr *)&address, 0), 0);
	address_length = (int)sizeof(address);
	assert_int_equal(uv_tcp_getsockname(&refused, (struct sockaddr *)&address, &address_length), 0);
	/* 未监听的绑定端口在 macOS 可返回超时，其他平台通常拒绝连接。 */
	int failure = skyuv_client_runtime_connect(runtime, "127.0.0.1",
		(int)ntohs(address.sin_port), &id);
	assert_true(failure == UV_ECONNREFUSED || failure == UV_ETIMEDOUT);
	assert_int_equal(id, 0);
	assert_int_equal(skyuv_client_runtime_connect(runtime, "127.0.0.1", port, &id), 0);
	/* 销毁时仍有已连接 socket，必须完成关闭回调后释放运行时。 */
	skyuv_client_runtime_destroy(&runtime);
	uv_close((uv_handle_t *)&listener, NULL);
	uv_close((uv_handle_t *)&refused, NULL);
	(void)uv_run(&loop, UV_RUN_DEFAULT);
	assert_int_equal(uv_loop_close(&loop), 0);
}

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(lifecycle), cmocka_unit_test(tcp_connections)
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
