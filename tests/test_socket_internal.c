#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <cmocka.h>

#include "socket_internal.h"

#define PRODUCER_COUNT 4
#define COMMANDS_PER_PRODUCER 1000

struct producer_context {
	struct skyuv_socket_command_queue *queue;
	int producer;
	bool succeeded;
};

static int released_buffers;

static void count_release(void *data) {
	++released_buffers;
	free(data);
}

static struct skyuv_socket_command *new_command(enum skyuv_socket_command_type type, int id) {
	struct skyuv_socket_command *command = calloc(1, sizeof(*command));

	assert_non_null(command);
	command->type = type;
	command->id = id;
	return command;
}

static void produce_commands(void *argument) {
	struct producer_context *context = argument;
	int index;

	context->succeeded = true;
	for (index = 0; index < COMMANDS_PER_PRODUCER; ++index) {
		struct skyuv_socket_command *command = new_command(
			SKYUV_SOCKET_COMMAND_CLOSE, context->producer * COMMANDS_PER_PRODUCER + index);
		if (skyuv_socket_command_queue_push(context->queue, command) != SKYUV_OK) {
			skyuv_socket_command_destroy(command);
			context->succeeded = false;
			return;
		}
	}
}

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

static void test_command_queue_fifo(void **state) {
	struct skyuv_socket_command_queue queue;
	struct skyuv_socket_command *command;

	(void)state;
	assert_int_equal(skyuv_socket_command_queue_init(&queue), SKYUV_OK);
	assert_int_equal(
		skyuv_socket_command_queue_push(&queue, new_command(SKYUV_SOCKET_COMMAND_CLOSE, 1)),
		SKYUV_OK);
	assert_int_equal(
		skyuv_socket_command_queue_push(&queue, new_command(SKYUV_SOCKET_COMMAND_CLOSE, 2)),
		SKYUV_OK);
	command = skyuv_socket_command_queue_pop(&queue);
	assert_int_equal(command->id, 1);
	skyuv_socket_command_destroy(command);
	command = skyuv_socket_command_queue_pop(&queue);
	assert_int_equal(command->id, 2);
	skyuv_socket_command_destroy(command);
	assert_null(skyuv_socket_command_queue_pop(&queue));
	skyuv_socket_command_queue_destroy(&queue);
}

static void test_command_queue_cleanup(void **state) {
	struct skyuv_socket_command_queue queue;
	struct skyuv_socket_command *command;

	(void)state;
	released_buffers = 0;
	assert_int_equal(skyuv_socket_command_queue_init(&queue), SKYUV_OK);
	command = new_command(SKYUV_SOCKET_COMMAND_SEND, 1);
	command->payload.send.data = malloc(8);
	assert_non_null(command->payload.send.data);
	command->payload.send.size = 8;
	command->payload.send.ownership = SKYUV_SOCKET_BUFFER_OWNED;
	command->payload.send.release = count_release;
	assert_int_equal(skyuv_socket_command_queue_push(&queue, command), SKYUV_OK);
	skyuv_socket_command_queue_destroy(&queue);
	assert_int_equal(released_buffers, 1);
	skyuv_socket_command_queue_destroy(&queue);
}

static void test_command_queue_multi_producer(void **state) {
	struct skyuv_socket_command_queue queue;
	skyuv_thread threads[PRODUCER_COUNT] = {
		SKYUV_THREAD_INITIALIZER,
		SKYUV_THREAD_INITIALIZER,
		SKYUV_THREAD_INITIALIZER,
		SKYUV_THREAD_INITIALIZER,
	};
	struct producer_context contexts[PRODUCER_COUNT];
	int last[PRODUCER_COUNT] = {-1, -1, -1, -1};
	int count = 0;
	int index;

	(void)state;
	assert_int_equal(skyuv_socket_command_queue_init(&queue), SKYUV_OK);
	for (index = 0; index < PRODUCER_COUNT; ++index) {
		contexts[index].queue = &queue;
		contexts[index].producer = index;
		contexts[index].succeeded = false;
		assert_int_equal(skyuv_thread_create(&threads[index], produce_commands, &contexts[index]),
						 SKYUV_OK);
	}
	for (index = 0; index < PRODUCER_COUNT; ++index) {
		assert_int_equal(skyuv_thread_join(&threads[index]), SKYUV_OK);
		assert_true(contexts[index].succeeded);
	}
	for (;;) {
		struct skyuv_socket_command *command = skyuv_socket_command_queue_pop(&queue);
		int producer;
		int sequence;

		if (command == NULL) {
			break;
		}
		producer = command->id / COMMANDS_PER_PRODUCER;
		sequence = command->id % COMMANDS_PER_PRODUCER;
		assert_int_equal(sequence, last[producer] + 1);
		last[producer] = sequence;
		++count;
		skyuv_socket_command_destroy(command);
	}
	assert_int_equal(count, PRODUCER_COUNT * COMMANDS_PER_PRODUCER);
	skyuv_socket_command_queue_destroy(&queue);
}

static void test_runtime_exit(void **state) {
	struct skyuv_socket_runtime *runtime = NULL;
	struct skyuv_socket_command *late;
	struct skyuv_socket_event event;
	int index;

	(void)state;
	assert_int_equal(skyuv_socket_runtime_create(&runtime), SKYUV_OK);
	for (index = 0; index < 100; ++index) {
		assert_int_equal(
			skyuv_socket_runtime_submit(runtime, new_command(SKYUV_SOCKET_COMMAND_CLOSE, index)),
			SKYUV_OK);
	}
	assert_int_equal(skyuv_socket_runtime_exit(runtime), SKYUV_OK);
	assert_int_equal(skyuv_socket_runtime_exit(runtime), SKYUV_ERROR_INVALID_STATE);
	late = new_command(SKYUV_SOCKET_COMMAND_CLOSE, 101);
	assert_int_equal(skyuv_socket_runtime_submit(runtime, late), SKYUV_ERROR_INVALID_STATE);
	skyuv_socket_command_destroy(late);
	assert_int_equal(skyuv_socket_runtime_poll(runtime, &event), SKYUV_OK);
	assert_int_equal(event.type, SKYUV_SOCKET_EVENT_EXIT);
	skyuv_socket_runtime_release(&runtime);
	assert_null(runtime);
	skyuv_socket_runtime_release(&runtime);
}

static void test_runtime_repeated_lifecycle(void **state) {
	int index;

	(void)state;
	for (index = 0; index < 100; ++index) {
		struct skyuv_socket_runtime *runtime = NULL;
		struct skyuv_socket_event event;

		assert_int_equal(skyuv_socket_runtime_create(&runtime), SKYUV_OK);
		assert_int_equal(skyuv_socket_runtime_exit(runtime), SKYUV_OK);
		assert_int_equal(skyuv_socket_runtime_poll(runtime, &event), SKYUV_OK);
		assert_int_equal(event.type, SKYUV_SOCKET_EVENT_EXIT);
		skyuv_socket_runtime_release(&runtime);
	}
}

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_id_roundtrip),
		cmocka_unit_test(test_generation_wrap),
		cmocka_unit_test(test_same_slot_different_generation),
		cmocka_unit_test(test_state_classification),
		cmocka_unit_test(test_command_queue_fifo),
		cmocka_unit_test(test_command_queue_cleanup),
		cmocka_unit_test(test_command_queue_multi_producer),
		cmocka_unit_test(test_runtime_exit),
		cmocka_unit_test(test_runtime_repeated_lifecycle),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
