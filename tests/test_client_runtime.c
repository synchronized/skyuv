#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdlib.h>
#include <string.h>

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
	assert_int_equal(uv_tcp_getsockname(&listener, (struct sockaddr *)&address, &address_length),
					 0);
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
	int readable = 0;
	int writable = 0;
	assert_int_equal(skyuv_client_runtime_ready(runtime, call.id, &readable, &writable), 0);
	assert_int_equal(writable, 1);
	assert_int_equal(skyuv_client_runtime_shutdown(runtime, call.id, 1, 0), 0);
	assert_int_equal(skyuv_client_runtime_shutdown(runtime, call.id, 1, 0), 0);
	assert_int_equal(skyuv_client_runtime_ready(runtime, call.id, &readable, &writable), 0);
	assert_int_equal(readable, 1);
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
	assert_int_equal(skyuv_client_runtime_connect_timeout(runtime, "127.0.0.1", port, 0, &id),
					 UV_ETIMEDOUT);
	assert_int_equal(id, 0);
	assert_int_equal(skyuv_client_runtime_connect(runtime, "invalid", port, &id), UV_EINVAL);
	assert_int_equal(id, 0);
	assert_int_equal(skyuv_client_runtime_connect(runtime, "127.0.0.1", -1, &id), UV_EINVAL);
	assert_int_equal(skyuv_client_runtime_close(runtime, previous + 100000), UV_EBADF);
	/* 绑定但不监听的端口在测试期间保留，避免固定端口被其他进程占用。 */
	assert_int_equal(uv_tcp_init(&loop, &refused), 0);
	assert_int_equal(uv_ip4_addr("127.0.0.1", 0, &address), 0);
	assert_int_equal(uv_tcp_bind(&refused, (const struct sockaddr *)&address, 0), 0);
	address_length = (int)sizeof(address);
	assert_int_equal(uv_tcp_getsockname(&refused, (struct sockaddr *)&address, &address_length), 0);
	/* 未监听的绑定端口在 macOS 可返回超时，其他平台通常拒绝连接。 */
	int failure =
		skyuv_client_runtime_connect(runtime, "127.0.0.1", (int)ntohs(address.sin_port), &id);
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

struct echo_peer {
	uv_tcp_t tcp;
	size_t total;
	int pending;
};

struct echo_write {
	uv_write_t write;
	struct echo_peer *peer;
	char *data;
};

static void echo_written(uv_write_t *write, int status) {
	struct echo_write *request = write->data;
	struct echo_peer *peer = request->peer;
	assert_int_equal(status, 0);
	free(request->data);
	free(request);
	if (--peer->pending == 0 && peer->total == 131072U)
		uv_close((uv_handle_t *)&peer->tcp, accepted_closed);
}

static void echo_allocate(uv_handle_t *handle, size_t suggested, uv_buf_t *buffer) {
	(void)handle;
	(void)suggested;
	*buffer = uv_buf_init(malloc(4096), 4096);
	assert_non_null(buffer->base);
}

static void echo_read(uv_stream_t *stream, ssize_t count, const uv_buf_t *buffer) {
	struct echo_peer *peer = stream->data;
	if (count <= 0) {
		free(buffer->base);
		assert_true(count == 0);
		return;
	}
	struct echo_write *request = calloc(1, sizeof(*request));
	assert_non_null(request);
	request->peer = peer;
	request->data = buffer->base;
	request->write.data = request;
	peer->total += (size_t)count;
	++peer->pending;
	uv_buf_t data = uv_buf_init(buffer->base, (unsigned int)count);
	assert_int_equal(uv_write(&request->write, stream, &data, 1, echo_written), 0);
	if (peer->total == 131072U)
		assert_int_equal(uv_read_stop(stream), 0);
}

static void echo_accept(uv_stream_t *listener, int status) {
	assert_int_equal(status, 0);
	struct echo_peer *peer = calloc(1, sizeof(*peer));
	assert_non_null(peer);
	assert_int_equal(uv_tcp_init(listener->loop, &peer->tcp), 0);
	peer->tcp.data = peer;
	assert_int_equal(uv_accept(listener, (uv_stream_t *)&peer->tcp), 0);
	assert_int_equal(uv_read_start((uv_stream_t *)&peer->tcp, echo_allocate, echo_read), 0);
}

struct send_call {
	struct skyuv_client_runtime *runtime;
	int64_t id;
	char *data;
	int result;
	uv_sem_t *done;
};

static void send_from_worker(void *argument) {
	struct send_call *call = argument;
	call->result = skyuv_client_runtime_send(call->runtime, call->id, call->data, 131072U);
	uv_sem_post(call->done);
}

static void tcp_io(void **unused) {
	(void)unused;
	uv_loop_t loop;
	uv_tcp_t listener;
	struct sockaddr_in address;
	int length = (int)sizeof(address);
	assert_int_equal(uv_loop_init(&loop), 0);
	assert_int_equal(uv_tcp_init(&loop, &listener), 0);
	assert_int_equal(uv_ip4_addr("127.0.0.1", 0, &address), 0);
	assert_int_equal(uv_tcp_bind(&listener, (const struct sockaddr *)&address, 0), 0);
	assert_int_equal(uv_listen((uv_stream_t *)&listener, 1, echo_accept), 0);
	assert_int_equal(uv_tcp_getsockname(&listener, (struct sockaddr *)&address, &length), 0);
	struct skyuv_client_runtime *runtime = NULL;
	assert_int_equal(skyuv_client_runtime_create(&runtime), 0);
	int64_t id = 0;
	assert_int_equal(
		skyuv_client_runtime_connect(runtime, "127.0.0.1", (int)ntohs(address.sin_port), &id), 0);
	char buffer[1024];
	size_t size = 1;
	assert_int_equal(skyuv_client_runtime_recv(runtime, id, buffer, sizeof(buffer), &size),
					 UV_EAGAIN);
	assert_int_equal(size, 0);
	assert_int_equal(skyuv_client_runtime_send(runtime, id, NULL, 0), 0);
	assert_int_equal(skyuv_client_runtime_send(runtime, id, NULL, 1), UV_EINVAL);
	char *data = malloc(131072U);
	assert_non_null(data);
	for (size_t i = 0; i < 131072U; ++i)
		data[i] = (char)(i % 127U);
	uv_sem_t done;
	assert_int_equal(uv_sem_init(&done, 0), 0);
	struct send_call call = {runtime, id, data, 0, &done};
	uv_thread_t worker;
	assert_int_equal(uv_thread_create(&worker, send_from_worker, &call), 0);
	/* 调用仍须串行，先让发送完成，同时推进测试服务端。 */
	uint64_t send_deadline = uv_hrtime() + 10000000000ULL;
	int waiting;
	while ((waiting = uv_sem_trywait(&done)) != 0 && uv_hrtime() < send_deadline) {
		(void)uv_run(&loop, UV_RUN_NOWAIT);
		uv_sleep(1);
	}
	assert_int_equal(waiting, 0);
	assert_int_equal(uv_thread_join(&worker), 0);
	uv_sem_destroy(&done);
	/* 暂不消费客户端缓存，服务端继续写，覆盖缓存满时停止读的路径。 */
	for (int i = 0; i < 100; ++i) {
		(void)uv_run(&loop, UV_RUN_NOWAIT);
		uv_sleep(1);
	}
	assert_int_equal(call.result, 0);
	size_t total = 0;
	int status = UV_EAGAIN;
	uint64_t deadline = uv_hrtime() + 10000000000ULL;
	while (status != UV_EOF && uv_hrtime() < deadline) {
		(void)uv_run(&loop, UV_RUN_NOWAIT);
		status = skyuv_client_runtime_recv(runtime, id, buffer, sizeof(buffer), &size);
		assert_true(status == 0 || status == UV_EAGAIN || status == UV_EOF);
		if (status == 0) {
			assert_true(total + size <= 131072U);
			assert_memory_equal(buffer, data + total, size);
			total += size;
		} else {
			uv_sleep(1);
		}
	}
	assert_int_equal(status, UV_EOF);
	assert_int_equal(total, 131072U);
	assert_int_equal(skyuv_client_runtime_close(runtime, id), 0);
	assert_int_equal(skyuv_client_runtime_send(runtime, id, data, 1), UV_EBADF);
	assert_int_equal(skyuv_client_runtime_recv(runtime, id, buffer, sizeof(buffer), &size),
					 UV_EBADF);
	free(data);
	skyuv_client_runtime_destroy(&runtime);
	uv_close((uv_handle_t *)&listener, NULL);
	(void)uv_run(&loop, UV_RUN_DEFAULT);
	assert_int_equal(uv_loop_close(&loop), 0);
}

int main(void) {
	const struct CMUnitTest tests[] = {cmocka_unit_test(lifecycle),
									   cmocka_unit_test(tcp_connections), cmocka_unit_test(tcp_io)};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
