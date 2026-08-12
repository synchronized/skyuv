#include "socket_internal.h"

#include <stdlib.h>

#include <uv.h>

struct skyuv_socket_runtime {
	uv_loop_t loop;
	uv_async_t async;
	struct skyuv_socket_command_queue commands;
	bool exit_ready;
};

static void consume_commands(uv_async_t *async) {
	struct skyuv_socket_runtime *runtime = async->data;
	struct skyuv_socket_command *command;

	while ((command = skyuv_socket_command_queue_pop(&runtime->commands)) != NULL) {
		if (command->type == SKYUV_SOCKET_COMMAND_EXIT) {
			runtime->exit_ready = true;
			uv_close((uv_handle_t *)&runtime->async, NULL);
		}
		skyuv_socket_command_destroy(command);
	}
}

int skyuv_socket_command_queue_init(struct skyuv_socket_command_queue *queue) {
	int result;

	if (queue == NULL) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	queue->mutex = (skyuv_mutex)SKYUV_MUTEX_INITIALIZER;
	queue->head = NULL;
	queue->tail = NULL;
	queue->accepting = false;
	result = skyuv_mutex_init(&queue->mutex);
	if (result != SKYUV_OK) {
		return result;
	}
	queue->accepting = true;
	return SKYUV_OK;
}

void skyuv_socket_command_destroy(struct skyuv_socket_command *command) {
	if (command == NULL) {
		return;
	}
	if (command->type == SKYUV_SOCKET_COMMAND_LISTEN) {
		free(command->payload.listen.host);
	} else if (command->type == SKYUV_SOCKET_COMMAND_CONNECT) {
		free(command->payload.connect.host);
	} else if (command->type == SKYUV_SOCKET_COMMAND_SEND &&
			   command->payload.send.ownership == SKYUV_SOCKET_BUFFER_OWNED) {
		if (command->payload.send.release != NULL) {
			command->payload.send.release(command->payload.send.data);
		} else {
			free(command->payload.send.data);
		}
	}
	free(command);
}

void skyuv_socket_command_queue_destroy(struct skyuv_socket_command_queue *queue) {
	struct skyuv_socket_command *command;

	if (queue == NULL || queue->mutex.implementation == NULL) {
		return;
	}
	skyuv_mutex_lock(&queue->mutex);
	queue->accepting = false;
	command = queue->head;
	queue->head = NULL;
	queue->tail = NULL;
	skyuv_mutex_unlock(&queue->mutex);
	while (command != NULL) {
		struct skyuv_socket_command *next = command->next;
		skyuv_socket_command_destroy(command);
		command = next;
	}
	skyuv_mutex_destroy(&queue->mutex);
}

static int queue_push_locked(struct skyuv_socket_command_queue *queue,
							 struct skyuv_socket_command *command) {
	command->next = NULL;
	if (queue->tail == NULL) {
		queue->head = command;
	} else {
		queue->tail->next = command;
	}
	queue->tail = command;
	return SKYUV_OK;
}

int skyuv_socket_command_queue_push(struct skyuv_socket_command_queue *queue,
									struct skyuv_socket_command *command) {
	int result = SKYUV_OK;

	if (queue == NULL || command == NULL || queue->mutex.implementation == NULL) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	skyuv_mutex_lock(&queue->mutex);
	if (!queue->accepting || command->type == SKYUV_SOCKET_COMMAND_EXIT) {
		result = SKYUV_ERROR_INVALID_STATE;
	} else {
		queue_push_locked(queue, command);
	}
	skyuv_mutex_unlock(&queue->mutex);
	return result;
}

int skyuv_socket_command_queue_stop(struct skyuv_socket_command_queue *queue,
									struct skyuv_socket_command *exit_command) {
	int result = SKYUV_OK;

	if (queue == NULL || exit_command == NULL || queue->mutex.implementation == NULL ||
		exit_command->type != SKYUV_SOCKET_COMMAND_EXIT) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	skyuv_mutex_lock(&queue->mutex);
	if (!queue->accepting) {
		result = SKYUV_ERROR_INVALID_STATE;
	} else {
		queue->accepting = false;
		queue_push_locked(queue, exit_command);
	}
	skyuv_mutex_unlock(&queue->mutex);
	return result;
}

struct skyuv_socket_command *
skyuv_socket_command_queue_pop(struct skyuv_socket_command_queue *queue) {
	struct skyuv_socket_command *command;

	if (queue == NULL || queue->mutex.implementation == NULL) {
		return NULL;
	}
	skyuv_mutex_lock(&queue->mutex);
	command = queue->head;
	if (command != NULL) {
		queue->head = command->next;
		if (queue->head == NULL) {
			queue->tail = NULL;
		}
		command->next = NULL;
	}
	skyuv_mutex_unlock(&queue->mutex);
	return command;
}

int skyuv_socket_runtime_create(struct skyuv_socket_runtime **runtime) {
	struct skyuv_socket_runtime *created;
	int result;

	if (runtime == NULL || *runtime != NULL) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	created = calloc(1, sizeof(*created));
	if (created == NULL) {
		return SKYUV_ERROR_OUT_OF_MEMORY;
	}
	result = uv_loop_init(&created->loop);
	if (result != 0) {
		free(created);
		return SKYUV_ERROR_SYSTEM;
	}
	result = skyuv_socket_command_queue_init(&created->commands);
	if (result != SKYUV_OK) {
		uv_loop_close(&created->loop);
		free(created);
		return result;
	}
	result = uv_async_init(&created->loop, &created->async, consume_commands);
	if (result != 0) {
		skyuv_socket_command_queue_destroy(&created->commands);
		uv_loop_close(&created->loop);
		free(created);
		return SKYUV_ERROR_SYSTEM;
	}
	created->async.data = created;
	*runtime = created;
	return SKYUV_OK;
}

int skyuv_socket_runtime_submit(struct skyuv_socket_runtime *runtime,
								struct skyuv_socket_command *command) {
	int result;

	if (runtime == NULL || command == NULL) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	result = skyuv_socket_command_queue_push(&runtime->commands, command);
	if (result != SKYUV_OK) {
		return result;
	}
	/* 入队成功即已转移所有权；已初始化的 async 只能在这里被安全唤醒。 */
	(void)uv_async_send(&runtime->async);
	return SKYUV_OK;
}

int skyuv_socket_runtime_exit(struct skyuv_socket_runtime *runtime) {
	struct skyuv_socket_command *command;
	int result;

	if (runtime == NULL) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	command = calloc(1, sizeof(*command));
	if (command == NULL) {
		return SKYUV_ERROR_OUT_OF_MEMORY;
	}
	command->type = SKYUV_SOCKET_COMMAND_EXIT;
	result = skyuv_socket_command_queue_stop(&runtime->commands, command);
	if (result != SKYUV_OK) {
		free(command);
		return result;
	}
	(void)uv_async_send(&runtime->async);
	return SKYUV_OK;
}

int skyuv_socket_runtime_poll(struct skyuv_socket_runtime *runtime,
							  struct skyuv_socket_event *event) {
	if (runtime == NULL || event == NULL) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	while (!runtime->exit_ready) {
		if (uv_run(&runtime->loop, UV_RUN_ONCE) == 0 && !runtime->exit_ready) {
			return SKYUV_ERROR_INVALID_STATE;
		}
	}
	event->next = NULL;
	event->type = SKYUV_SOCKET_EVENT_EXIT;
	event->id = 0;
	event->opaque = 0;
	event->value = 0;
	event->data = NULL;
	event->size = 0;
	runtime->exit_ready = false;
	return SKYUV_OK;
}

void skyuv_socket_runtime_release(struct skyuv_socket_runtime **runtime) {
	struct skyuv_socket_runtime *released;

	if (runtime == NULL || *runtime == NULL) {
		return;
	}
	released = *runtime;
	if (!uv_is_closing((uv_handle_t *)&released->async)) {
		uv_close((uv_handle_t *)&released->async, NULL);
	}
	while (uv_run(&released->loop, UV_RUN_DEFAULT) != 0) {
	}
	(void)uv_loop_close(&released->loop);
	skyuv_socket_command_queue_destroy(&released->commands);
	free(released);
	*runtime = NULL;
}

uint16_t skyuv_socket_id_slot(int id) {
	return (uint16_t)((uint32_t)id & UINT32_C(0xffff));
}

uint16_t skyuv_socket_id_generation(int id) {
	return (uint16_t)(((uint32_t)id >> SKYUV_SOCKET_SLOT_BITS) & SKYUV_SOCKET_GENERATION_MAX);
}

uint16_t skyuv_socket_id_next_generation(uint16_t generation) {
	generation = (uint16_t)(generation + 1U);
	if (generation == 0U || generation > SKYUV_SOCKET_GENERATION_MAX) {
		generation = 1U;
	}
	return generation;
}

int skyuv_socket_id_make(uint16_t slot, uint16_t generation) {
	uint32_t id;

	if (generation == 0U || generation > SKYUV_SOCKET_GENERATION_MAX) {
		return -1;
	}
	id = ((uint32_t)generation << SKYUV_SOCKET_SLOT_BITS) | slot;
	return (int)id;
}

bool skyuv_socket_state_can_receive(enum skyuv_socket_state state) {
	return state == SKYUV_SOCKET_STATE_LISTENING || state == SKYUV_SOCKET_STATE_CONNECTED;
}

bool skyuv_socket_state_is_live(enum skyuv_socket_state state) {
	return state != SKYUV_SOCKET_STATE_INVALID && state != SKYUV_SOCKET_STATE_CLOSING;
}
