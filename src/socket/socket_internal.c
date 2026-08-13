#include "socket_internal.h"

#include <stdlib.h>
#include <string.h>

#include <uv.h>

struct skyuv_socket_entry {
	uv_tcp_t tcp;
	uv_connect_t connect;
	struct skyuv_socket_runtime *runtime;
	enum skyuv_socket_state state;
	int id;
	uintptr_t opaque;
	bool initialized;
};

struct skyuv_socket_runtime {
	uv_loop_t loop;
	uv_async_t async;
	struct skyuv_socket_command_queue commands;
	skyuv_mutex slots_mutex;
	struct skyuv_socket_entry **slots;
	uint16_t *generations;
	uint32_t next_slot;
	struct skyuv_socket_event *event_head;
	struct skyuv_socket_event *event_tail;
	bool exit_ready;
};

static void push_event(struct skyuv_socket_runtime *runtime, enum skyuv_socket_event_type type,
					   int id, uintptr_t opaque, int value) {
	struct skyuv_socket_event *event = calloc(1, sizeof(*event));

	if (event == NULL) {
		return;
	}
	event->type = type;
	event->id = id;
	event->opaque = opaque;
	event->value = value;
	if (runtime->event_tail == NULL) {
		runtime->event_head = event;
	} else {
		runtime->event_tail->next = event;
	}
	runtime->event_tail = event;
}

static void push_data_event(struct skyuv_socket_entry *entry, void *data, size_t size) {
	struct skyuv_socket_runtime *runtime = entry->runtime;
	struct skyuv_socket_event *event = calloc(1, sizeof(*event));

	if (event == NULL) {
		free(data);
		return;
	}
	event->type = SKYUV_SOCKET_EVENT_DATA;
	event->id = entry->id;
	event->opaque = entry->opaque;
	event->data = data;
	event->size = size;
	if (runtime->event_tail == NULL) {
		runtime->event_head = event;
	} else {
		runtime->event_tail->next = event;
	}
	runtime->event_tail = event;
}

static struct skyuv_socket_entry *find_entry(struct skyuv_socket_runtime *runtime, int id) {
	uint16_t slot = skyuv_socket_id_slot(id);
	struct skyuv_socket_entry *entry = runtime->slots[slot];

	if (entry == NULL || entry->id != id) {
		return NULL;
	}
	return entry;
}

static enum skyuv_socket_state entry_state(struct skyuv_socket_entry *entry) {
	enum skyuv_socket_state state;

	skyuv_mutex_lock(&entry->runtime->slots_mutex);
	state = entry->state;
	skyuv_mutex_unlock(&entry->runtime->slots_mutex);
	return state;
}

static void set_entry_state(struct skyuv_socket_entry *entry, enum skyuv_socket_state state) {
	skyuv_mutex_lock(&entry->runtime->slots_mutex);
	entry->state = state;
	skyuv_mutex_unlock(&entry->runtime->slots_mutex);
}

static int reserve_entry(struct skyuv_socket_runtime *runtime, uintptr_t opaque,
						 struct skyuv_socket_entry **result) {
	uint32_t checked;

	skyuv_mutex_lock(&runtime->slots_mutex);
	for (checked = 0; checked < SKYUV_SOCKET_SLOT_COUNT; ++checked) {
		uint16_t slot = (uint16_t)runtime->next_slot++;
		struct skyuv_socket_entry *entry;

		if (runtime->slots[slot] != NULL) {
			continue;
		}
		entry = calloc(1, sizeof(*entry));
		if (entry == NULL) {
			skyuv_mutex_unlock(&runtime->slots_mutex);
			return SKYUV_ERROR_OUT_OF_MEMORY;
		}
		runtime->generations[slot] = skyuv_socket_id_next_generation(runtime->generations[slot]);
		entry->runtime = runtime;
		entry->state = SKYUV_SOCKET_STATE_RESERVED;
		entry->id = skyuv_socket_id_make(slot, runtime->generations[slot]);
		entry->opaque = opaque;
		runtime->slots[slot] = entry;
		*result = entry;
		skyuv_mutex_unlock(&runtime->slots_mutex);
		return SKYUV_OK;
	}
	skyuv_mutex_unlock(&runtime->slots_mutex);
	return SKYUV_ERROR_OUT_OF_MEMORY;
}

static void close_entry(uv_handle_t *handle) {
	struct skyuv_socket_entry *entry = handle->data;
	struct skyuv_socket_runtime *runtime = entry->runtime;
	uint16_t slot = skyuv_socket_id_slot(entry->id);

	skyuv_mutex_lock(&runtime->slots_mutex);
	if (runtime->slots[slot] == entry) {
		runtime->slots[slot] = NULL;
	}
	skyuv_mutex_unlock(&runtime->slots_mutex);
	free(entry);
}

static void discard_uninitialized_entry(struct skyuv_socket_entry *entry) {
	struct skyuv_socket_runtime *runtime = entry->runtime;
	uint16_t slot = skyuv_socket_id_slot(entry->id);

	skyuv_mutex_lock(&runtime->slots_mutex);
	if (runtime->slots[slot] == entry) {
		runtime->slots[slot] = NULL;
	}
	skyuv_mutex_unlock(&runtime->slots_mutex);
	free(entry);
}

static void reject_entry(struct skyuv_socket_entry *entry, int error) {
	push_event(entry->runtime, SKYUV_SOCKET_EVENT_ERROR, entry->id, entry->opaque, error);
	set_entry_state(entry, SKYUV_SOCKET_STATE_CLOSING);
	uv_close((uv_handle_t *)&entry->tcp, close_entry);
}

static void finish_entry(struct skyuv_socket_entry *entry, enum skyuv_socket_event_type type,
						 int value) {
	if (entry_state(entry) == SKYUV_SOCKET_STATE_CLOSING) {
		return;
	}
	push_event(entry->runtime, type, entry->id, entry->opaque, value);
	set_entry_state(entry, SKYUV_SOCKET_STATE_CLOSING);
	uv_close((uv_handle_t *)&entry->tcp, close_entry);
}

static void allocate_read_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buffer) {
	(void)handle;
	(void)suggested_size;
	buffer->base = malloc(65536);
	buffer->len = buffer->base == NULL ? 0 : 65536;
}

static void read_completed(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buffer) {
	struct skyuv_socket_entry *entry = stream->data;

	if (nread > 0) {
		push_data_event(entry, buffer->base, (size_t)nread);
		return;
	}
	free(buffer->base);
	if (nread == 0) {
		return;
	}
	if (nread == UV_EOF) {
		finish_entry(entry, SKYUV_SOCKET_EVENT_CLOSE, 0);
	} else {
		finish_entry(entry, SKYUV_SOCKET_EVENT_ERROR, (int)nread);
	}
}

static int start_reading(struct skyuv_socket_entry *entry) {
	return uv_read_start((uv_stream_t *)&entry->tcp, allocate_read_buffer, read_completed);
}

static void connect_completed(uv_connect_t *request, int status) {
	struct skyuv_socket_entry *entry = request->data;
	enum skyuv_socket_state state = entry_state(entry);

	if (state == SKYUV_SOCKET_STATE_CLOSING) {
		return;
	}
	if (status != 0) {
		reject_entry(entry, status);
		return;
	}
	status = start_reading(entry);
	if (status != 0) {
		reject_entry(entry, status);
		return;
	}
	set_entry_state(entry, SKYUV_SOCKET_STATE_CONNECTED);
	push_event(entry->runtime, SKYUV_SOCKET_EVENT_OPEN, entry->id, entry->opaque, 0);
}

static void accept_connection(uv_stream_t *server, int status) {
	struct skyuv_socket_entry *listener = server->data;
	struct skyuv_socket_runtime *runtime = listener->runtime;
	struct skyuv_socket_entry *accepted;
	int result;

	if (status < 0) {
		push_event(runtime, SKYUV_SOCKET_EVENT_ERROR, listener->id, listener->opaque, status);
		return;
	}
	result = reserve_entry(runtime, listener->opaque, &accepted);
	if (result != SKYUV_OK) {
		return;
	}
	result = uv_tcp_init(&runtime->loop, &accepted->tcp);
	if (result != 0) {
		discard_uninitialized_entry(accepted);
		return;
	}
	accepted->initialized = true;
	accepted->tcp.data = accepted;
	result = uv_accept(server, (uv_stream_t *)&accepted->tcp);
	if (result != 0) {
		reject_entry(accepted, result);
		return;
	}
	set_entry_state(accepted, SKYUV_SOCKET_STATE_ACCEPTED_PAUSED);
	push_event(runtime, SKYUV_SOCKET_EVENT_ACCEPT, listener->id, listener->opaque, accepted->id);
}

static void process_listen(struct skyuv_socket_runtime *runtime,
						   struct skyuv_socket_command *command) {
	struct skyuv_socket_entry *entry = find_entry(runtime, command->id);
	struct sockaddr_storage address;
	int length = sizeof(address);
	int result;

	if (entry == NULL || entry_state(entry) != SKYUV_SOCKET_STATE_RESERVED) {
		return;
	}
	set_entry_state(entry, SKYUV_SOCKET_STATE_LISTEN_PENDING);
	result = uv_ip4_addr(command->payload.listen.host, command->payload.listen.port,
						 (struct sockaddr_in *)&address);
	if (result != 0) {
		result = uv_ip6_addr(command->payload.listen.host, command->payload.listen.port,
							 (struct sockaddr_in6 *)&address);
	}
	if (result != 0) {
		push_event(runtime, SKYUV_SOCKET_EVENT_ERROR, entry->id, entry->opaque, result);
		discard_uninitialized_entry(entry);
		return;
	}
	result = uv_tcp_init(&runtime->loop, &entry->tcp);
	if (result != 0) {
		push_event(runtime, SKYUV_SOCKET_EVENT_ERROR, entry->id, entry->opaque, result);
		discard_uninitialized_entry(entry);
		return;
	}
	entry->initialized = true;
	entry->tcp.data = entry;
	result = uv_tcp_bind(&entry->tcp, (const struct sockaddr *)&address, 0);
	if (result == 0) {
		result = uv_listen((uv_stream_t *)&entry->tcp, command->payload.listen.backlog,
						   accept_connection);
	}
	if (result != 0) {
		reject_entry(entry, result);
		return;
	}
	set_entry_state(entry, SKYUV_SOCKET_STATE_LISTENING);
	result = uv_tcp_getsockname(&entry->tcp, (struct sockaddr *)&address, &length);
	if (result == 0 && address.ss_family == AF_INET) {
		command->payload.listen.port = ntohs(((struct sockaddr_in *)&address)->sin_port);
	} else if (result == 0 && address.ss_family == AF_INET6) {
		command->payload.listen.port = ntohs(((struct sockaddr_in6 *)&address)->sin6_port);
	}
	push_event(runtime, SKYUV_SOCKET_EVENT_OPEN, entry->id, entry->opaque,
			   command->payload.listen.port);
}

static void process_start(struct skyuv_socket_runtime *runtime,
						  struct skyuv_socket_command *command) {
	struct skyuv_socket_entry *entry = find_entry(runtime, command->id);
	int result;

	if (entry == NULL || entry_state(entry) != SKYUV_SOCKET_STATE_ACCEPTED_PAUSED) {
		push_event(runtime, SKYUV_SOCKET_EVENT_ERROR, command->id, command->opaque, UV_EINVAL);
		return;
	}
	entry->opaque = command->opaque;
	result = start_reading(entry);
	if (result != 0) {
		reject_entry(entry, result);
		return;
	}
	set_entry_state(entry, SKYUV_SOCKET_STATE_CONNECTED);
	push_event(runtime, SKYUV_SOCKET_EVENT_OPEN, entry->id, entry->opaque, 0);
}

static void process_connect(struct skyuv_socket_runtime *runtime,
							struct skyuv_socket_command *command) {
	struct skyuv_socket_entry *entry = find_entry(runtime, command->id);
	struct sockaddr_storage address;
	int result;

	if (entry == NULL || entry_state(entry) != SKYUV_SOCKET_STATE_RESERVED) {
		return;
	}
	result = uv_ip4_addr(command->payload.connect.host, command->payload.connect.port,
						 (struct sockaddr_in *)&address);
	if (result != 0) {
		result = uv_ip6_addr(command->payload.connect.host, command->payload.connect.port,
							 (struct sockaddr_in6 *)&address);
	}
	if (result != 0) {
		push_event(runtime, SKYUV_SOCKET_EVENT_ERROR, entry->id, entry->opaque, result);
		discard_uninitialized_entry(entry);
		return;
	}
	result = uv_tcp_init(&runtime->loop, &entry->tcp);
	if (result != 0) {
		push_event(runtime, SKYUV_SOCKET_EVENT_ERROR, entry->id, entry->opaque, result);
		discard_uninitialized_entry(entry);
		return;
	}
	entry->initialized = true;
	entry->tcp.data = entry;
	entry->connect.data = entry;
	set_entry_state(entry, SKYUV_SOCKET_STATE_CONNECTING);
	result = uv_tcp_connect(&entry->connect, &entry->tcp, (const struct sockaddr *)&address,
							connect_completed);
	if (result != 0) {
		reject_entry(entry, result);
		return;
	}
}

static void process_close(struct skyuv_socket_runtime *runtime,
						  struct skyuv_socket_command *command) {
	struct skyuv_socket_entry *entry = find_entry(runtime, command->id);

	if (entry == NULL || entry_state(entry) == SKYUV_SOCKET_STATE_CLOSING) {
		return;
	}
	entry->opaque = command->opaque;
	push_event(runtime, SKYUV_SOCKET_EVENT_CLOSE, entry->id, entry->opaque, 0);
	if (!entry->initialized) {
		discard_uninitialized_entry(entry);
		return;
	}
	set_entry_state(entry, SKYUV_SOCKET_STATE_CLOSING);
	uv_close((uv_handle_t *)&entry->tcp, close_entry);
}

static void consume_commands(uv_async_t *async) {
	struct skyuv_socket_runtime *runtime = async->data;
	struct skyuv_socket_command *command;

	while ((command = skyuv_socket_command_queue_pop(&runtime->commands)) != NULL) {
		if (command->type == SKYUV_SOCKET_COMMAND_LISTEN) {
			process_listen(runtime, command);
		} else if (command->type == SKYUV_SOCKET_COMMAND_CONNECT) {
			process_connect(runtime, command);
		} else if (command->type == SKYUV_SOCKET_COMMAND_START) {
			process_start(runtime, command);
		} else if (command->type == SKYUV_SOCKET_COMMAND_CLOSE) {
			process_close(runtime, command);
		} else if (command->type == SKYUV_SOCKET_COMMAND_EXIT) {
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
	created->slots = calloc(SKYUV_SOCKET_SLOT_COUNT, sizeof(*created->slots));
	created->generations = calloc(SKYUV_SOCKET_SLOT_COUNT, sizeof(*created->generations));
	if (created->slots == NULL || created->generations == NULL) {
		free(created->slots);
		free(created->generations);
		free(created);
		return SKYUV_ERROR_OUT_OF_MEMORY;
	}
	result = uv_loop_init(&created->loop);
	if (result != 0) {
		free(created->slots);
		free(created->generations);
		free(created);
		return SKYUV_ERROR_SYSTEM;
	}
	result = skyuv_socket_command_queue_init(&created->commands);
	if (result != SKYUV_OK) {
		uv_loop_close(&created->loop);
		free(created->slots);
		free(created->generations);
		free(created);
		return result;
	}
	created->slots_mutex = (skyuv_mutex)SKYUV_MUTEX_INITIALIZER;
	result = skyuv_mutex_init(&created->slots_mutex);
	if (result != SKYUV_OK) {
		skyuv_socket_command_queue_destroy(&created->commands);
		uv_loop_close(&created->loop);
		free(created->slots);
		free(created->generations);
		free(created);
		return result;
	}
	result = uv_async_init(&created->loop, &created->async, consume_commands);
	if (result != 0) {
		skyuv_mutex_destroy(&created->slots_mutex);
		skyuv_socket_command_queue_destroy(&created->commands);
		uv_loop_close(&created->loop);
		free(created->slots);
		free(created->generations);
		free(created);
		return SKYUV_ERROR_SYSTEM;
	}
	created->async.data = created;
	*runtime = created;
	return SKYUV_OK;
}

static char *copy_string(const char *value) {
	size_t size;
	char *copy;

	if (value == NULL) {
		return NULL;
	}
	size = strlen(value) + 1;
	copy = malloc(size);
	if (copy != NULL) {
		memcpy(copy, value, size);
	}
	return copy;
}

int skyuv_socket_runtime_listen(struct skyuv_socket_runtime *runtime, const char *host, int port,
								int backlog, uintptr_t opaque, int *id) {
	struct skyuv_socket_command *command;
	struct skyuv_socket_entry *entry;
	int result;

	if (runtime == NULL || host == NULL || port < 0 || port > 65535 || backlog <= 0 || id == NULL) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	result = reserve_entry(runtime, opaque, &entry);
	if (result != SKYUV_OK) {
		return result;
	}
	command = calloc(1, sizeof(*command));
	if (command != NULL) {
		command->payload.listen.host = copy_string(host);
	}
	if (command == NULL || command->payload.listen.host == NULL) {
		free(command);
		discard_uninitialized_entry(entry);
		return SKYUV_ERROR_OUT_OF_MEMORY;
	}
	command->type = SKYUV_SOCKET_COMMAND_LISTEN;
	command->id = entry->id;
	command->opaque = opaque;
	command->payload.listen.port = port;
	command->payload.listen.backlog = backlog;
	result = skyuv_socket_runtime_submit(runtime, command);
	if (result != SKYUV_OK) {
		skyuv_socket_command_destroy(command);
		discard_uninitialized_entry(entry);
		return result;
	}
	*id = entry->id;
	return SKYUV_OK;
}

int skyuv_socket_runtime_start(struct skyuv_socket_runtime *runtime, int id, uintptr_t opaque) {
	struct skyuv_socket_command *command;

	if (runtime == NULL || id <= 0) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	command = calloc(1, sizeof(*command));
	if (command == NULL) {
		return SKYUV_ERROR_OUT_OF_MEMORY;
	}
	command->type = SKYUV_SOCKET_COMMAND_START;
	command->id = id;
	command->opaque = opaque;
	if (skyuv_socket_runtime_submit(runtime, command) != SKYUV_OK) {
		free(command);
		return SKYUV_ERROR_INVALID_STATE;
	}
	return SKYUV_OK;
}

int skyuv_socket_runtime_connect(struct skyuv_socket_runtime *runtime, const char *host, int port,
								 uintptr_t opaque, int *id) {
	struct skyuv_socket_command *command;
	struct skyuv_socket_entry *entry;
	int result;

	if (runtime == NULL || host == NULL || port < 0 || port > 65535 || id == NULL) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	result = reserve_entry(runtime, opaque, &entry);
	if (result != SKYUV_OK) {
		return result;
	}
	command = calloc(1, sizeof(*command));
	if (command != NULL) {
		command->payload.connect.host = copy_string(host);
	}
	if (command == NULL || command->payload.connect.host == NULL) {
		free(command);
		discard_uninitialized_entry(entry);
		return SKYUV_ERROR_OUT_OF_MEMORY;
	}
	command->type = SKYUV_SOCKET_COMMAND_CONNECT;
	command->id = entry->id;
	command->opaque = opaque;
	command->payload.connect.port = port;
	result = skyuv_socket_runtime_submit(runtime, command);
	if (result != SKYUV_OK) {
		skyuv_socket_command_destroy(command);
		discard_uninitialized_entry(entry);
		return result;
	}
	*id = entry->id;
	return SKYUV_OK;
}

int skyuv_socket_runtime_close(struct skyuv_socket_runtime *runtime, int id, uintptr_t opaque) {
	struct skyuv_socket_command *command;
	int result;

	if (runtime == NULL || id <= 0) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	command = calloc(1, sizeof(*command));
	if (command == NULL) {
		return SKYUV_ERROR_OUT_OF_MEMORY;
	}
	command->type = SKYUV_SOCKET_COMMAND_CLOSE;
	command->id = id;
	command->opaque = opaque;
	result = skyuv_socket_runtime_submit(runtime, command);
	if (result != SKYUV_OK) {
		free(command);
	}
	return result;
}

enum skyuv_socket_state skyuv_socket_runtime_state(struct skyuv_socket_runtime *runtime, int id) {
	struct skyuv_socket_entry *entry;
	enum skyuv_socket_state state = SKYUV_SOCKET_STATE_INVALID;

	if (runtime == NULL || id <= 0) {
		return state;
	}
	skyuv_mutex_lock(&runtime->slots_mutex);
	entry = find_entry(runtime, id);
	if (entry != NULL) {
		state = entry->state;
	}
	skyuv_mutex_unlock(&runtime->slots_mutex);
	return state;
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
	struct skyuv_socket_event *ready;

	if (runtime == NULL || event == NULL) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	while (runtime->event_head == NULL && !runtime->exit_ready) {
		if (uv_run(&runtime->loop, UV_RUN_ONCE) == 0 && runtime->event_head == NULL &&
			!runtime->exit_ready) {
			return SKYUV_ERROR_INVALID_STATE;
		}
	}
	if (runtime->event_head != NULL) {
		ready = runtime->event_head;
		runtime->event_head = ready->next;
		if (runtime->event_head == NULL) {
			runtime->event_tail = NULL;
		}
		*event = *ready;
		event->next = NULL;
		free(ready);
		return SKYUV_OK;
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
	uint32_t slot;

	if (runtime == NULL || *runtime == NULL) {
		return;
	}
	released = *runtime;
	for (slot = 0; slot < SKYUV_SOCKET_SLOT_COUNT; ++slot) {
		struct skyuv_socket_entry *entry = released->slots[slot];

		if (entry != NULL && entry->initialized && !uv_is_closing((uv_handle_t *)&entry->tcp)) {
			set_entry_state(entry, SKYUV_SOCKET_STATE_CLOSING);
			uv_close((uv_handle_t *)&entry->tcp, close_entry);
		} else if (entry != NULL && !entry->initialized) {
			released->slots[slot] = NULL;
			free(entry);
		}
	}
	if (!uv_is_closing((uv_handle_t *)&released->async)) {
		uv_close((uv_handle_t *)&released->async, NULL);
	}
	while (uv_run(&released->loop, UV_RUN_DEFAULT) != 0) {
	}
	(void)uv_loop_close(&released->loop);
	skyuv_socket_command_queue_destroy(&released->commands);
	while (released->event_head != NULL) {
		struct skyuv_socket_event *next = released->event_head->next;
		free(released->event_head->data);
		free(released->event_head);
		released->event_head = next;
	}
	skyuv_mutex_destroy(&released->slots_mutex);
	free(released->slots);
	free(released->generations);
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
