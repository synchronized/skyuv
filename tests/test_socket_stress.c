#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <cmocka.h>
#include <uv.h>

#include "socket_internal.h"

#define STRESS_PRODUCER_COUNT 4
#define STRESS_SENDS_PER_PRODUCER 500

struct send_context {
	struct skyuv_socket_runtime *runtime;
	uv_barrier_t *ready;
	uv_barrier_t *exit_done;
	int id;
	int accepted;
	int rejected;
	bool allocation_failed;
};

struct close_context {
	struct skyuv_socket_runtime *runtime;
	uv_barrier_t *ready;
	int id;
	uintptr_t opaque;
	int result;
};

static int released_buffers;

static void count_release(void *data) {
	++released_buffers;
	free(data);
}

static void create_connected_pair(struct skyuv_socket_runtime *runtime, int *connection,
								  int *accepted) {
	struct skyuv_socket_event event;
	int listener;
	int port;
	uint32_t index;

	assert_int_equal(skyuv_socket_runtime_listen(runtime, "127.0.0.1", 0, 16, 0, &listener),
					 SKYUV_OK);
	assert_int_equal(skyuv_socket_runtime_poll(runtime, &event), SKYUV_OK);
	assert_int_equal(event.type, SKYUV_SOCKET_EVENT_OPEN);
	port = event.value;
	assert_int_equal(skyuv_socket_runtime_connect(runtime, "127.0.0.1", port, 0, connection),
					 SKYUV_OK);
	*accepted = -1;
	for (index = 0; index < 2; ++index) {
		assert_int_equal(skyuv_socket_runtime_poll(runtime, &event), SKYUV_OK);
		if (event.type == SKYUV_SOCKET_EVENT_ACCEPT) {
			*accepted = event.value;
		} else {
			assert_int_equal(event.type, SKYUV_SOCKET_EVENT_OPEN);
			assert_int_equal(event.id, *connection);
		}
	}
	assert_true(*accepted > 0);
	assert_int_equal(skyuv_socket_runtime_start(runtime, *accepted, 0), SKYUV_OK);
	assert_int_equal(skyuv_socket_runtime_poll(runtime, &event), SKYUV_OK);
	assert_int_equal(event.type, SKYUV_SOCKET_EVENT_OPEN);
}

static void produce_after_exit(void *argument) {
	struct send_context *context = argument;
	int index;

	context->accepted = 0;
	context->rejected = 0;
	context->allocation_failed = false;
	for (index = 0; index < STRESS_SENDS_PER_PRODUCER; ++index) {
		char *data;
		int result;

		if (index == 1) {
			uv_barrier_wait(context->ready);
			uv_barrier_wait(context->exit_done);
		}
		data = malloc(1);
		if (data == NULL) {
			context->allocation_failed = true;
			return;
		}
		*data = 's';
		result = skyuv_socket_runtime_send(context->runtime, context->id, data, 1,
										 SKYUV_SOCKET_BUFFER_OWNED, count_release);
		if (result == SKYUV_OK) {
			++context->accepted;
		} else {
			free(data);
			++context->rejected;
		}
	}
}

static void close_at_once(void *argument) {
	struct close_context *context = argument;

	uv_barrier_wait(context->ready);
	context->result = skyuv_socket_runtime_close(context->runtime, context->id, context->opaque);
}

static void test_generation_rejects_stale_id_after_slot_reuse(void **state) {
	struct skyuv_socket_runtime *runtime = NULL;
	struct skyuv_socket_event event;
	int first_id = -1;
	int current_id;
	uint32_t index;

	(void)state;
	assert_int_equal(SKYUV_SOCKET_SLOT_COUNT, 256);
	assert_int_equal(skyuv_socket_runtime_create(&runtime), SKYUV_OK);
	for (index = 0; index < SKYUV_SOCKET_SLOT_COUNT; ++index) {
		int id;

		assert_int_equal(skyuv_socket_runtime_udp(runtime, "invalid-address", 0, 0, &id),
						 SKYUV_OK);
		assert_int_equal(skyuv_socket_runtime_poll(runtime, &event), SKYUV_OK);
		assert_int_equal(event.type, SKYUV_SOCKET_EVENT_ERROR);
		assert_int_equal(event.id, id);
		assert_int_equal(skyuv_socket_runtime_state(runtime, id), SKYUV_SOCKET_STATE_INVALID);
		if (index == 0) {
			first_id = id;
		}
	}
	assert_int_equal(skyuv_socket_runtime_listen(runtime, "127.0.0.1", 0, 16, 0, &current_id),
					 SKYUV_OK);
	assert_int_equal(skyuv_socket_runtime_poll(runtime, &event), SKYUV_OK);
	assert_int_equal(event.type, SKYUV_SOCKET_EVENT_OPEN);
	assert_int_equal(skyuv_socket_id_slot(first_id), skyuv_socket_id_slot(current_id));
	assert_true(skyuv_socket_id_generation(first_id) != skyuv_socket_id_generation(current_id));

	assert_int_equal(skyuv_socket_runtime_close(runtime, first_id, 1), SKYUV_OK);
	assert_int_equal(skyuv_socket_runtime_close(runtime, current_id, 2), SKYUV_OK);
	assert_int_equal(skyuv_socket_runtime_poll(runtime, &event), SKYUV_OK);
	assert_int_equal(event.type, SKYUV_SOCKET_EVENT_CLOSE);
	assert_int_equal(event.id, current_id);
	assert_int_equal(event.opaque, 2);
	skyuv_socket_runtime_release(&runtime);
}

static void test_concurrent_close_has_single_event(void **state) {
	struct skyuv_socket_runtime *runtime = NULL;
	struct skyuv_socket_event event;
	skyuv_thread threads[STRESS_PRODUCER_COUNT] = {
		SKYUV_THREAD_INITIALIZER,
		SKYUV_THREAD_INITIALIZER,
		SKYUV_THREAD_INITIALIZER,
		SKYUV_THREAD_INITIALIZER,
	};
	struct close_context contexts[STRESS_PRODUCER_COUNT];
	uv_barrier_t ready;
	int connection;
	int accepted;
	int close_events = 0;
	int index;
	bool saw_exit = false;

	(void)state;
	assert_int_equal(skyuv_socket_runtime_create(&runtime), SKYUV_OK);
	create_connected_pair(runtime, &connection, &accepted);
	assert_int_equal(uv_barrier_init(&ready, STRESS_PRODUCER_COUNT + 1), 0);
	for (index = 0; index < STRESS_PRODUCER_COUNT; ++index) {
		contexts[index].runtime = runtime;
		contexts[index].ready = &ready;
		contexts[index].id = connection;
		contexts[index].opaque = (uintptr_t)(100 + index);
		contexts[index].result = SKYUV_ERROR_INVALID_STATE;
		assert_int_equal(skyuv_thread_create(&threads[index], close_at_once, &contexts[index]),
						 SKYUV_OK);
	}
	uv_barrier_wait(&ready);
	for (index = 0; index < STRESS_PRODUCER_COUNT; ++index) {
		assert_int_equal(skyuv_thread_join(&threads[index]), SKYUV_OK);
		assert_int_equal(contexts[index].result, SKYUV_OK);
	}
	uv_barrier_destroy(&ready);
	assert_int_equal(skyuv_socket_runtime_exit(runtime), SKYUV_OK);
	while (!saw_exit) {
		assert_int_equal(skyuv_socket_runtime_poll(runtime, &event), SKYUV_OK);
		if (event.type == SKYUV_SOCKET_EVENT_CLOSE && event.id == connection) {
			++close_events;
		} else if (event.type == SKYUV_SOCKET_EVENT_DATA) {
			free(event.data);
		} else if (event.type == SKYUV_SOCKET_EVENT_EXIT) {
			saw_exit = true;
		}
	}
	assert_int_equal(close_events, 1);
	skyuv_socket_runtime_release(&runtime);
}

static void test_exit_rejects_concurrent_owned_sends(void **state) {
	struct skyuv_socket_runtime *runtime = NULL;
	struct skyuv_socket_event event;
	skyuv_thread threads[STRESS_PRODUCER_COUNT] = {
		SKYUV_THREAD_INITIALIZER,
		SKYUV_THREAD_INITIALIZER,
		SKYUV_THREAD_INITIALIZER,
		SKYUV_THREAD_INITIALIZER,
	};
	struct send_context contexts[STRESS_PRODUCER_COUNT];
	uv_barrier_t ready;
	uv_barrier_t exit_done;
	int connection;
	int accepted;
	int accepted_sends = 0;
	int rejected_sends = 0;
	int index;
	bool saw_exit = false;

	(void)state;
	released_buffers = 0;
	assert_int_equal(skyuv_socket_runtime_create(&runtime), SKYUV_OK);
	create_connected_pair(runtime, &connection, &accepted);
	assert_int_equal(uv_barrier_init(&ready, STRESS_PRODUCER_COUNT + 1), 0);
	assert_int_equal(uv_barrier_init(&exit_done, STRESS_PRODUCER_COUNT + 1), 0);
	for (index = 0; index < STRESS_PRODUCER_COUNT; ++index) {
		contexts[index].runtime = runtime;
		contexts[index].ready = &ready;
		contexts[index].exit_done = &exit_done;
		contexts[index].id = connection;
		assert_int_equal(skyuv_thread_create(&threads[index], produce_after_exit, &contexts[index]),
						 SKYUV_OK);
	}
	uv_barrier_wait(&ready);
	assert_int_equal(skyuv_socket_runtime_exit(runtime), SKYUV_OK);
	uv_barrier_wait(&exit_done);
	for (index = 0; index < STRESS_PRODUCER_COUNT; ++index) {
		assert_int_equal(skyuv_thread_join(&threads[index]), SKYUV_OK);
		assert_false(contexts[index].allocation_failed);
		accepted_sends += contexts[index].accepted;
		rejected_sends += contexts[index].rejected;
	}
	uv_barrier_destroy(&exit_done);
	uv_barrier_destroy(&ready);
	while (!saw_exit) {
		assert_int_equal(skyuv_socket_runtime_poll(runtime, &event), SKYUV_OK);
		if (event.type == SKYUV_SOCKET_EVENT_DATA) {
			free(event.data);
		} else if (event.type == SKYUV_SOCKET_EVENT_EXIT) {
			saw_exit = true;
		}
	}
	assert_int_equal(accepted_sends, STRESS_PRODUCER_COUNT);
	assert_int_equal(rejected_sends,
					 STRESS_PRODUCER_COUNT * (STRESS_SENDS_PER_PRODUCER - 1));
	assert_int_equal(released_buffers, accepted_sends);
	skyuv_socket_runtime_release(&runtime);
}

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_generation_rejects_stale_id_after_slot_reuse),
		cmocka_unit_test(test_concurrent_close_has_single_event),
		cmocka_unit_test(test_exit_rejects_concurrent_owned_sends),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
