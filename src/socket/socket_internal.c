#include "socket_internal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <signal.h>
#endif

#include <uv.h>

struct skyuv_socket_entry {
	uv_tcp_t tcp;
	uv_udp_t udp;
	uv_connect_t connect;
	struct skyuv_socket_runtime *runtime;
	enum skyuv_socket_state state;
	int id;
	int external_fd;
	uintptr_t opaque;
	bool initialized;
	bool is_udp;
	struct sockaddr_storage udp_target;
	bool has_udp_target;
	struct skyuv_socket_write *high_head;
	struct skyuv_socket_write *high_tail;
	struct skyuv_socket_write *low_head;
	struct skyuv_socket_write *low_tail;
	bool writing;
	size_t queued_bytes;
	size_t warn_size;
	bool pump_pending;
	struct skyuv_socket_entry *pump_next;
	uint64_t read_bytes;
	uint64_t write_bytes;
	uint64_t read_time;
	uint64_t write_time;
	uint64_t accept_count;
	bool reading;
	bool close_requested;
	char name[128];
};

static uv_handle_t *entry_handle(struct skyuv_socket_entry *entry) {
	return entry->is_udp ? (uv_handle_t *)&entry->udp : (uv_handle_t *)&entry->tcp;
}

struct skyuv_socket_write {
	struct skyuv_socket_write *next;
	uv_write_t request;
	struct skyuv_socket_entry *entry;
	void *data;
	size_t size;
	void (*release)(void *data);
};

struct skyuv_udp_write {
	uv_udp_send_t request;
	struct skyuv_socket_entry *entry;
	void *data;
	size_t size;
	void (*release)(void *data);
};

static void pump_write(struct skyuv_socket_entry *entry);
static void release_write(struct skyuv_socket_write *write);
static void process_udp_send(struct skyuv_socket_runtime *runtime,
							 struct skyuv_socket_command *command);
static void close_after_writes(struct skyuv_socket_entry *entry);
static void push_event(struct skyuv_socket_runtime *runtime,
					   enum skyuv_socket_event_type type, int id, uintptr_t opaque,
					   int value);

struct skyuv_socket_runtime {
	uv_loop_t loop;
	uv_async_t async;
#ifndef _WIN32
	uv_signal_t hangup_signal;
	bool hangup_signal_initialized;
#endif
	struct skyuv_socket_command_queue commands;
	skyuv_mutex slots_mutex;
	struct skyuv_socket_entry **slots;
	uint16_t *generations;
	uint32_t next_slot;
	struct skyuv_socket_event *event_head;
	struct skyuv_socket_event *event_tail;
	struct skyuv_socket_entry *pump_head;
	struct skyuv_socket_entry *pump_tail;
	uint32_t entry_count;
	bool exiting;
	bool exit_ready;
	uint64_t current_time;
};

#ifndef _WIN32
static void on_hangup_signal(uv_signal_t *handle, int signal_number) {
	struct skyuv_socket_runtime *runtime = handle->data;

	push_event(runtime, SKYUV_SOCKET_EVENT_PROCESS_SIGNAL, 0, 0, signal_number);
}
#endif

static void cache_tcp_name(struct skyuv_socket_entry *entry, bool peer) {
	struct sockaddr_storage address;
	char ip[INET6_ADDRSTRLEN];
	char name[128];
	int length = sizeof(address);
	int result;
	int port;

	result = peer ? uv_tcp_getpeername(&entry->tcp, (struct sockaddr *)&address, &length)
				  : uv_tcp_getsockname(&entry->tcp, (struct sockaddr *)&address, &length);
	if (result != 0) {
		return;
	}
	if (address.ss_family == AF_INET) {
		struct sockaddr_in *ipv4 = (struct sockaddr_in *)&address;

		if (uv_ip4_name(ipv4, ip, sizeof(ip)) != 0) {
			return;
		}
		port = ntohs(ipv4->sin_port);
		(void)snprintf(name, sizeof(name), "%s:%d", ip, port);
	} else if (address.ss_family == AF_INET6) {
		struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&address;

		if (uv_ip6_name(ipv6, ip, sizeof(ip)) != 0) {
			return;
		}
		port = ntohs(ipv6->sin6_port);
		(void)snprintf(name, sizeof(name), "[%s]:%d", ip, port);
	} else {
		return;
	}
	skyuv_mutex_lock(&entry->runtime->slots_mutex);
	(void)memcpy(entry->name, name, strlen(name) + 1);
	skyuv_mutex_unlock(&entry->runtime->slots_mutex);
}

static void update_exit_ready(struct skyuv_socket_runtime *runtime) {
	bool ready;

	skyuv_mutex_lock(&runtime->slots_mutex);
	ready = runtime->exiting && runtime->entry_count == 0;
	skyuv_mutex_unlock(&runtime->slots_mutex);
	if (ready) {
		runtime->exit_ready = true;
	}
}

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
	skyuv_mutex_lock(&runtime->slots_mutex);
	entry->read_bytes += size;
	entry->read_time = runtime->current_time;
	skyuv_mutex_unlock(&runtime->slots_mutex);
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
		entry->external_fd = -1;
		entry->opaque = opaque;
		runtime->slots[slot] = entry;
		++runtime->entry_count;
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
	struct skyuv_socket_write *write;

	while ((write = entry->high_head) != NULL) {
		entry->high_head = write->next;
		release_write(write);
	}
	while ((write = entry->low_head) != NULL) {
		entry->low_head = write->next;
		release_write(write);
	}

	skyuv_mutex_lock(&runtime->slots_mutex);
	if (runtime->slots[slot] == entry) {
		runtime->slots[slot] = NULL;
		--runtime->entry_count;
	}
	skyuv_mutex_unlock(&runtime->slots_mutex);
	free(entry);
	update_exit_ready(runtime);
}

static void close_after_writes(struct skyuv_socket_entry *entry) {
	if (!entry->close_requested || entry->writing || entry->high_head != NULL ||
		entry->low_head != NULL) {
		return;
	}
	set_entry_state(entry, SKYUV_SOCKET_STATE_CLOSING);
	uv_close((uv_handle_t *)&entry->tcp, close_entry);
}

static void discard_uninitialized_entry(struct skyuv_socket_entry *entry) {
	struct skyuv_socket_runtime *runtime = entry->runtime;
	uint16_t slot = skyuv_socket_id_slot(entry->id);

	skyuv_mutex_lock(&runtime->slots_mutex);
	if (runtime->slots[slot] == entry) {
		runtime->slots[slot] = NULL;
		--runtime->entry_count;
	}
	skyuv_mutex_unlock(&runtime->slots_mutex);
	free(entry);
	update_exit_ready(runtime);
}

static void reject_entry(struct skyuv_socket_entry *entry, int error) {
	push_event(entry->runtime, SKYUV_SOCKET_EVENT_ERROR, entry->id, entry->opaque, error);
	set_entry_state(entry, SKYUV_SOCKET_STATE_CLOSING);
	uv_close(entry_handle(entry), close_entry);
}

static void finish_entry(struct skyuv_socket_entry *entry, enum skyuv_socket_event_type type,
						 int value) {
	if (entry_state(entry) == SKYUV_SOCKET_STATE_CLOSING) {
		return;
	}
	push_event(entry->runtime, type, entry->id, entry->opaque, value);
	set_entry_state(entry, SKYUV_SOCKET_STATE_CLOSING);
	uv_close(entry_handle(entry), close_entry);
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
		if (entry_state(entry) != SKYUV_SOCKET_STATE_HALFCLOSE_READ) {
			skyuv_mutex_lock(&entry->runtime->slots_mutex);
			entry->reading = false;
			skyuv_mutex_unlock(&entry->runtime->slots_mutex);
			push_event(entry->runtime, SKYUV_SOCKET_EVENT_CLOSE, entry->id, entry->opaque, 0);
			set_entry_state(entry, SKYUV_SOCKET_STATE_HALFCLOSE_READ);
		}
	} else {
		finish_entry(entry, SKYUV_SOCKET_EVENT_ERROR, (int)nread);
	}
}

static void udp_received(uv_udp_t *handle, ssize_t nread, const uv_buf_t *buffer,
						 const struct sockaddr *address, unsigned flags) {
	struct skyuv_socket_entry *entry = handle->data;
	struct skyuv_socket_event *event;
	size_t address_size;
	uint8_t *encoded;

	(void)flags;
	if (nread < 0) {
		free(buffer->base);
		finish_entry(entry, SKYUV_SOCKET_EVENT_ERROR, (int)nread);
		return;
	}
	if (address == NULL) {
		free(buffer->base);
		return;
	}
	address_size = address->sa_family == AF_INET ? 7U : 19U;
	encoded = malloc((size_t)nread + address_size);
	if (encoded == NULL) {
		free(buffer->base);
		return;
	}
	if (nread > 0) {
		(void)memcpy(encoded, buffer->base, (size_t)nread);
	}
	free(buffer->base);
	encoded[nread] = address->sa_family == AF_INET ? 1U : 2U;
	if (address->sa_family == AF_INET) {
		const struct sockaddr_in *ipv4 = (const struct sockaddr_in *)address;

		(void)memcpy(encoded + nread + 1, &ipv4->sin_port, 2);
		(void)memcpy(encoded + nread + 3, &ipv4->sin_addr, 4);
	} else {
		const struct sockaddr_in6 *ipv6 = (const struct sockaddr_in6 *)address;

		(void)memcpy(encoded + nread + 1, &ipv6->sin6_port, 2);
		(void)memcpy(encoded + nread + 3, &ipv6->sin6_addr, 16);
	}
	event = calloc(1, sizeof(*event));
	if (event == NULL) {
		free(encoded);
		return;
	}
	event->type = SKYUV_SOCKET_EVENT_UDP;
	event->id = entry->id;
	event->opaque = entry->opaque;
	event->data = encoded;
	event->size = (size_t)nread;
	event->value = (int)address_size;
	skyuv_mutex_lock(&entry->runtime->slots_mutex);
	entry->read_bytes += (size_t)nread;
	entry->read_time = entry->runtime->current_time;
	skyuv_mutex_unlock(&entry->runtime->slots_mutex);
	if (entry->runtime->event_tail == NULL) {
		entry->runtime->event_head = event;
	} else {
		entry->runtime->event_tail->next = event;
	}
	entry->runtime->event_tail = event;
}

static int decode_udp_address(const uint8_t *encoded, size_t size,
							  struct sockaddr_storage *address) {
	if (encoded == NULL || address == NULL || (size != 7U && size != 19U)) {
		return UV_EINVAL;
	}
	(void)memset(address, 0, sizeof(*address));
	if (size == 7U && encoded[0] == 1U) {
		struct sockaddr_in *ipv4 = (struct sockaddr_in *)address;

		ipv4->sin_family = AF_INET;
		(void)memcpy(&ipv4->sin_port, encoded + 1, 2);
		(void)memcpy(&ipv4->sin_addr, encoded + 3, 4);
		return 0;
	}
	if (size == 19U && encoded[0] == 2U) {
		struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)address;

		ipv6->sin6_family = AF_INET6;
		(void)memcpy(&ipv6->sin6_port, encoded + 1, 2);
		(void)memcpy(&ipv6->sin6_addr, encoded + 3, 16);
		return 0;
	}
	return UV_EINVAL;
}

static void udp_write_completed(uv_udp_send_t *request, int status) {
	struct skyuv_udp_write *write = request->data;
	struct skyuv_socket_entry *entry = write->entry;

	if (status != 0 && entry_state(entry) != SKYUV_SOCKET_STATE_CLOSING) {
		push_event(entry->runtime, SKYUV_SOCKET_EVENT_ERROR, entry->id, entry->opaque, status);
	}
	skyuv_mutex_lock(&entry->runtime->slots_mutex);
	entry->write_bytes += write->size;
	entry->write_time = entry->runtime->current_time;
	skyuv_mutex_unlock(&entry->runtime->slots_mutex);
	if (write->release != NULL) {
		write->release(write->data);
	} else {
		free(write->data);
	}
	free(write);
}

static int start_reading(struct skyuv_socket_entry *entry) {
	int result = uv_read_start((uv_stream_t *)&entry->tcp, allocate_read_buffer, read_completed);

	if (result == 0) {
		skyuv_mutex_lock(&entry->runtime->slots_mutex);
		entry->reading = true;
		skyuv_mutex_unlock(&entry->runtime->slots_mutex);
	}
	return result;
}

static void release_write(struct skyuv_socket_write *write) {
	if (write->release != NULL) {
		write->release(write->data);
	} else {
		free(write->data);
	}
	free(write);
}

static void write_completed(uv_write_t *request, int status) {
	struct skyuv_socket_write *write = request->data;
	struct skyuv_socket_entry *entry = write->entry;
	enum skyuv_socket_state state = entry_state(entry);

	if (status != 0 && state != SKYUV_SOCKET_STATE_CLOSING) {
		if (state == SKYUV_SOCKET_STATE_HALFCLOSE_READ) {
			set_entry_state(entry, SKYUV_SOCKET_STATE_CLOSING);
			uv_close((uv_handle_t *)&entry->tcp, close_entry);
		} else {
			finish_entry(entry, SKYUV_SOCKET_EVENT_ERROR, status);
		}
	}
	skyuv_mutex_lock(&entry->runtime->slots_mutex);
	entry->queued_bytes -= write->size;
	entry->write_bytes += write->size;
	entry->write_time = entry->runtime->current_time;
	entry->writing = false;
	skyuv_mutex_unlock(&entry->runtime->slots_mutex);
	release_write(write);
	if (entry->queued_bytes == 0 && entry->warn_size > 0 &&
		entry_state(entry) != SKYUV_SOCKET_STATE_CLOSING) {
		skyuv_mutex_lock(&entry->runtime->slots_mutex);
		entry->warn_size = 0;
		skyuv_mutex_unlock(&entry->runtime->slots_mutex);
		push_event(entry->runtime, SKYUV_SOCKET_EVENT_WARNING, entry->id, entry->opaque, 0);
	}
	if (entry->close_requested) {
		close_after_writes(entry);
	} else if (entry_state(entry) != SKYUV_SOCKET_STATE_CLOSING) {
		pump_write(entry);
	}
}

static void pump_write(struct skyuv_socket_entry *entry) {
	struct skyuv_socket_write *write;
	uv_buf_t buffer;
	int result;

	if (entry->writing) {
		return;
	}
	write = entry->high_head;
	if (write != NULL) {
		entry->high_head = write->next;
		if (entry->high_head == NULL) {
			entry->high_tail = NULL;
		}
	} else {
		write = entry->low_head;
		if (write == NULL) {
			return;
		}
		entry->low_head = write->next;
		if (entry->low_head == NULL) {
			entry->low_tail = NULL;
		}
	}
	write->next = NULL;
	write->request.data = write;
	buffer = uv_buf_init(write->data, (unsigned int)write->size);
	result = uv_write(&write->request, (uv_stream_t *)&entry->tcp, &buffer, 1, write_completed);
	if (result == 0) {
		skyuv_mutex_lock(&entry->runtime->slots_mutex);
		entry->writing = true;
		skyuv_mutex_unlock(&entry->runtime->slots_mutex);
		return;
	}
	skyuv_mutex_lock(&entry->runtime->slots_mutex);
	entry->queued_bytes -= write->size;
	skyuv_mutex_unlock(&entry->runtime->slots_mutex);
	release_write(write);
	if (entry_state(entry) == SKYUV_SOCKET_STATE_HALFCLOSE_READ) {
		set_entry_state(entry, SKYUV_SOCKET_STATE_CLOSING);
		uv_close((uv_handle_t *)&entry->tcp, close_entry);
	} else {
		finish_entry(entry, SKYUV_SOCKET_EVENT_ERROR, result);
	}
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
	cache_tcp_name(entry, true);
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
	cache_tcp_name(accepted, true);
	skyuv_mutex_lock(&runtime->slots_mutex);
	++listener->accept_count;
	listener->read_time = runtime->current_time;
	skyuv_mutex_unlock(&runtime->slots_mutex);
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
	cache_tcp_name(entry, false);
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
	enum skyuv_socket_state state;
	int result;

	if (entry == NULL) {
		push_event(runtime, SKYUV_SOCKET_EVENT_ERROR, command->id, command->opaque, UV_EINVAL);
		return;
	}
	state = entry_state(entry);
	if (state == SKYUV_SOCKET_STATE_LISTENING) {
		skyuv_mutex_lock(&runtime->slots_mutex);
		entry->opaque = command->opaque;
		skyuv_mutex_unlock(&runtime->slots_mutex);
		push_event(runtime, SKYUV_SOCKET_EVENT_OPEN, entry->id, entry->opaque, 0);
		return;
	}
	if (state == SKYUV_SOCKET_STATE_CONNECTED) {
		/* start 也承担 Skynet socket 在服务间转移所有权的职责。 */
		skyuv_mutex_lock(&runtime->slots_mutex);
		entry->opaque = command->opaque;
		skyuv_mutex_unlock(&runtime->slots_mutex);
		push_event(runtime, SKYUV_SOCKET_EVENT_OPEN, entry->id, entry->opaque, 0);
		return;
	}
	if (state != SKYUV_SOCKET_STATE_ACCEPTED_PAUSED &&
		state != SKYUV_SOCKET_STATE_CONNECTED_PAUSED) {
		push_event(runtime, SKYUV_SOCKET_EVENT_ERROR, command->id, command->opaque, UV_EINVAL);
		return;
	}
	skyuv_mutex_lock(&runtime->slots_mutex);
	entry->opaque = command->opaque;
	skyuv_mutex_unlock(&runtime->slots_mutex);
	result = start_reading(entry);
	if (result != 0) {
		reject_entry(entry, result);
		return;
	}
	set_entry_state(entry, SKYUV_SOCKET_STATE_CONNECTED);
	push_event(runtime, SKYUV_SOCKET_EVENT_OPEN, entry->id, entry->opaque, 0);
}

static void process_pause(struct skyuv_socket_runtime *runtime,
						  struct skyuv_socket_command *command) {
	struct skyuv_socket_entry *entry = find_entry(runtime, command->id);
	int result;

	(void)command->opaque;
	if (entry == NULL || entry_state(entry) != SKYUV_SOCKET_STATE_CONNECTED) {
		return;
	}
	result = uv_read_stop((uv_stream_t *)&entry->tcp);
	if (result != 0) {
		push_event(runtime, SKYUV_SOCKET_EVENT_ERROR, entry->id, entry->opaque, result);
		return;
	}
	skyuv_mutex_lock(&runtime->slots_mutex);
	entry->reading = false;
	skyuv_mutex_unlock(&runtime->slots_mutex);
	set_entry_state(entry, SKYUV_SOCKET_STATE_CONNECTED_PAUSED);
}

static void process_nodelay(struct skyuv_socket_runtime *runtime,
							struct skyuv_socket_command *command) {
	struct skyuv_socket_entry *entry = find_entry(runtime, command->id);
	enum skyuv_socket_state state;

	if (entry == NULL) {
		return;
	}
	state = entry_state(entry);
	if (state != SKYUV_SOCKET_STATE_CONNECTED && state != SKYUV_SOCKET_STATE_CONNECTED_PAUSED &&
		state != SKYUV_SOCKET_STATE_HALFCLOSE_READ) {
		return;
	}
	/* 与原版一致，设置失败不产生额外 socket 事件。 */
	(void)uv_tcp_nodelay(&entry->tcp, 1);
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

static void process_bind(struct skyuv_socket_runtime *runtime,
						 struct skyuv_socket_command *command) {
#if SKYUV_SOCKET_EXTERNAL_FD
	struct skyuv_socket_entry *entry = find_entry(runtime, command->id);
	int result;

	if (entry == NULL || entry_state(entry) != SKYUV_SOCKET_STATE_RESERVED) {
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
	result = uv_tcp_open(&entry->tcp, (uv_os_sock_t)command->payload.bind.fd);
	if (result == 0) {
		result = start_reading(entry);
	}
	if (result != 0) {
		reject_entry(entry, result);
		return;
	}
	set_entry_state(entry, SKYUV_SOCKET_STATE_CONNECTED);
	cache_tcp_name(entry, true);
	push_event(runtime, SKYUV_SOCKET_EVENT_OPEN, entry->id, entry->opaque, 0);
#else
	(void)runtime;
	(void)command;
#endif
}

static void process_send(struct skyuv_socket_runtime *runtime,
						 struct skyuv_socket_command *command) {
	struct skyuv_socket_entry *entry = find_entry(runtime, command->id);
	struct skyuv_socket_write *write;
	struct skyuv_socket_write **head;
	struct skyuv_socket_write **tail;
	enum skyuv_socket_state state;

	state = entry == NULL ? SKYUV_SOCKET_STATE_INVALID : entry_state(entry);
	if (state == SKYUV_SOCKET_STATE_UDP) {
		process_udp_send(runtime, command);
		return;
	}
	if (state != SKYUV_SOCKET_STATE_CONNECTED && state != SKYUV_SOCKET_STATE_CONNECTED_PAUSED &&
		state != SKYUV_SOCKET_STATE_HALFCLOSE_READ) {
		push_event(runtime, SKYUV_SOCKET_EVENT_ERROR, command->id, command->opaque, UV_ENOTCONN);
		return;
	}
	if (entry->close_requested) {
		push_event(runtime, SKYUV_SOCKET_EVENT_ERROR, command->id, command->opaque, UV_ECANCELED);
		return;
	}
	write = calloc(1, sizeof(*write));
	if (write == NULL) {
		push_event(runtime, SKYUV_SOCKET_EVENT_ERROR, entry->id, entry->opaque, UV_ENOMEM);
		return;
	}
	write->entry = entry;
	write->data = command->payload.send.data;
	write->size = command->payload.send.size;
	write->release = command->payload.send.release;
	if (command->type == SKYUV_SOCKET_COMMAND_SEND_LOW) {
		head = &entry->low_head;
		tail = &entry->low_tail;
	} else {
		head = &entry->high_head;
		tail = &entry->high_tail;
	}
	if (*tail == NULL) {
		*head = write;
	} else {
		(*tail)->next = write;
	}
	*tail = write;
	skyuv_mutex_lock(&runtime->slots_mutex);
	entry->queued_bytes += write->size;
	if (entry->queued_bytes >= SKYUV_SOCKET_WARNING_SIZE &&
		entry->queued_bytes >= entry->warn_size) {
		size_t warning_kb = (entry->queued_bytes + 1023U) / 1024U;

		entry->warn_size =
			entry->warn_size == 0 ? SKYUV_SOCKET_WARNING_SIZE * 2U : entry->warn_size * 2U;
		push_event(runtime, SKYUV_SOCKET_EVENT_WARNING, entry->id, entry->opaque,
				   warning_kb > INT_MAX ? INT_MAX : (int)warning_kb);
	}
	skyuv_mutex_unlock(&runtime->slots_mutex);
	if (!entry->writing && !entry->pump_pending) {
		entry->pump_pending = true;
		if (runtime->pump_tail == NULL) {
			runtime->pump_head = entry;
		} else {
			runtime->pump_tail->pump_next = entry;
		}
		runtime->pump_tail = entry;
	}
	/* 入队后 write 节点成为缓冲区的唯一所有者。 */
	command->payload.send.ownership = SKYUV_SOCKET_BUFFER_BORROWED;
}

static void process_udp(struct skyuv_socket_runtime *runtime,
						struct skyuv_socket_command *command) {
	struct skyuv_socket_entry *entry = find_entry(runtime, command->id);
	struct sockaddr_storage address;
	int result;

	if (entry == NULL || entry_state(entry) != SKYUV_SOCKET_STATE_RESERVED) {
		return;
	}
	result = uv_ip4_addr(command->payload.udp.host, command->payload.udp.port,
						 (struct sockaddr_in *)&address);
	if (result != 0) {
		result = uv_ip6_addr(command->payload.udp.host, command->payload.udp.port,
							 (struct sockaddr_in6 *)&address);
	}
	if (result != 0) {
		push_event(runtime, SKYUV_SOCKET_EVENT_ERROR, entry->id, entry->opaque, result);
		discard_uninitialized_entry(entry);
		return;
	}
	entry->is_udp = true;
	result = uv_udp_init(&runtime->loop, &entry->udp);
	if (result != 0) {
		push_event(runtime, SKYUV_SOCKET_EVENT_ERROR, entry->id, entry->opaque, result);
		discard_uninitialized_entry(entry);
		return;
	}
	entry->initialized = true;
	entry->udp.data = entry;
	result = uv_udp_bind(&entry->udp, (const struct sockaddr *)&address, 0);
	if (result == 0) {
		result = uv_udp_recv_start(&entry->udp, allocate_read_buffer, udp_received);
	}
	if (result != 0) {
		reject_entry(entry, result);
		return;
	}
	set_entry_state(entry, SKYUV_SOCKET_STATE_UDP);
}

static void process_udp_connect(struct skyuv_socket_runtime *runtime,
								struct skyuv_socket_command *command) {
	struct skyuv_socket_entry *entry = find_entry(runtime, command->id);
	struct sockaddr_storage address;
	int result;

	if (entry == NULL || entry_state(entry) != SKYUV_SOCKET_STATE_UDP) {
		return;
	}
	result = uv_ip4_addr(command->payload.udp.host, command->payload.udp.port,
						 (struct sockaddr_in *)&address);
	if (result != 0) {
		result = uv_ip6_addr(command->payload.udp.host, command->payload.udp.port,
							 (struct sockaddr_in6 *)&address);
	}
	if (result == 0) {
		entry->udp_target = address;
		entry->has_udp_target = true;
	}
}

static void process_udp_send(struct skyuv_socket_runtime *runtime,
							 struct skyuv_socket_command *command) {
	struct skyuv_socket_entry *entry = find_entry(runtime, command->id);
	struct skyuv_udp_write *write;
	struct sockaddr_storage address;
	const struct sockaddr *target;
	uv_buf_t buffer;
	int result;

	if (entry == NULL || entry_state(entry) != SKYUV_SOCKET_STATE_UDP) {
		push_event(runtime, SKYUV_SOCKET_EVENT_ERROR, command->id, command->opaque, UV_ENOTCONN);
		return;
	}
	if (command->payload.send.address_size > 0) {
		result = decode_udp_address(command->payload.send.address,
									command->payload.send.address_size, &address);
		if (result != 0) {
			push_event(runtime, SKYUV_SOCKET_EVENT_ERROR, entry->id, entry->opaque, result);
			return;
		}
		target = (const struct sockaddr *)&address;
	} else if (entry->has_udp_target) {
		target = (const struct sockaddr *)&entry->udp_target;
	} else {
		push_event(runtime, SKYUV_SOCKET_EVENT_ERROR, entry->id, entry->opaque, UV_EDESTADDRREQ);
		return;
	}
	write = calloc(1, sizeof(*write));
	if (write == NULL) {
		push_event(runtime, SKYUV_SOCKET_EVENT_ERROR, entry->id, entry->opaque, UV_ENOMEM);
		return;
	}
	write->entry = entry;
	write->data = command->payload.send.data;
	write->size = command->payload.send.size;
	write->release = command->payload.send.release;
	write->request.data = write;
	buffer = uv_buf_init(write->data, (unsigned int)write->size);
	result = uv_udp_send(&write->request, &entry->udp, &buffer, 1, target, udp_write_completed);
	if (result != 0) {
		free(write);
		push_event(runtime, SKYUV_SOCKET_EVENT_ERROR, entry->id, entry->opaque, result);
		return;
	}
	command->payload.send.ownership = SKYUV_SOCKET_BUFFER_BORROWED;
}

static void process_close(struct skyuv_socket_runtime *runtime,
						  struct skyuv_socket_command *command) {
	struct skyuv_socket_entry *entry = find_entry(runtime, command->id);
	enum skyuv_socket_state state;

	if (entry == NULL || entry_state(entry) == SKYUV_SOCKET_STATE_CLOSING) {
		return;
	}
	state = entry_state(entry);
	skyuv_mutex_lock(&runtime->slots_mutex);
	entry->opaque = command->opaque;
	skyuv_mutex_unlock(&runtime->slots_mutex);
	if (state != SKYUV_SOCKET_STATE_HALFCLOSE_READ) {
		push_event(runtime, SKYUV_SOCKET_EVENT_CLOSE, entry->id, entry->opaque, 0);
	}
	if (!entry->initialized) {
		discard_uninitialized_entry(entry);
		return;
	}
	if (command->type == SKYUV_SOCKET_COMMAND_CLOSE && !entry->is_udp && entry->queued_bytes > 0) {
		(void)uv_read_stop((uv_stream_t *)&entry->tcp);
		skyuv_mutex_lock(&runtime->slots_mutex);
		entry->reading = false;
		entry->close_requested = true;
		skyuv_mutex_unlock(&runtime->slots_mutex);
		close_after_writes(entry);
		return;
	}
	set_entry_state(entry, SKYUV_SOCKET_STATE_CLOSING);
	uv_close(entry_handle(entry), close_entry);
}

static void process_exit(struct skyuv_socket_runtime *runtime) {
	uint32_t slot;

	runtime->exiting = true;
	for (slot = 0; slot < SKYUV_SOCKET_SLOT_COUNT; ++slot) {
		struct skyuv_socket_entry *entry = runtime->slots[slot];

		if (entry == NULL) {
			continue;
		}
		if (!entry->initialized) {
			discard_uninitialized_entry(entry);
		} else if (!uv_is_closing(entry_handle(entry))) {
			set_entry_state(entry, SKYUV_SOCKET_STATE_CLOSING);
			uv_close(entry_handle(entry), close_entry);
		}
	}
	uv_close((uv_handle_t *)&runtime->async, NULL);
	update_exit_ready(runtime);
}

static void consume_commands(uv_async_t *async) {
	struct skyuv_socket_runtime *runtime = async->data;
	struct skyuv_socket_command *command;

	while ((command = skyuv_socket_command_queue_pop(&runtime->commands)) != NULL) {
		if (command->type == SKYUV_SOCKET_COMMAND_LISTEN) {
			process_listen(runtime, command);
		} else if (command->type == SKYUV_SOCKET_COMMAND_CONNECT) {
			process_connect(runtime, command);
		} else if (command->type == SKYUV_SOCKET_COMMAND_BIND) {
			process_bind(runtime, command);
		} else if (command->type == SKYUV_SOCKET_COMMAND_UDP) {
			process_udp(runtime, command);
		} else if (command->type == SKYUV_SOCKET_COMMAND_UDP_CONNECT) {
			process_udp_connect(runtime, command);
		} else if (command->type == SKYUV_SOCKET_COMMAND_UDP_SEND) {
			process_udp_send(runtime, command);
		} else if (command->type == SKYUV_SOCKET_COMMAND_START) {
			process_start(runtime, command);
		} else if (command->type == SKYUV_SOCKET_COMMAND_PAUSE) {
			process_pause(runtime, command);
		} else if (command->type == SKYUV_SOCKET_COMMAND_NODELAY) {
			process_nodelay(runtime, command);
		} else if (command->type == SKYUV_SOCKET_COMMAND_SEND ||
				   command->type == SKYUV_SOCKET_COMMAND_SEND_LOW) {
			process_send(runtime, command);
		} else if (command->type == SKYUV_SOCKET_COMMAND_CLOSE ||
				   command->type == SKYUV_SOCKET_COMMAND_SHUTDOWN) {
			process_close(runtime, command);
		} else if (command->type == SKYUV_SOCKET_COMMAND_EXIT) {
			process_exit(runtime);
		}
		skyuv_socket_command_destroy(command);
	}
	while (runtime->pump_head != NULL) {
		struct skyuv_socket_entry *entry = runtime->pump_head;

		runtime->pump_head = entry->pump_next;
		if (runtime->pump_head == NULL) {
			runtime->pump_tail = NULL;
		}
		entry->pump_next = NULL;
		entry->pump_pending = false;
		if (entry_state(entry) != SKYUV_SOCKET_STATE_CLOSING) {
			pump_write(entry);
		}
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
	} else if (command->type == SKYUV_SOCKET_COMMAND_UDP ||
			   command->type == SKYUV_SOCKET_COMMAND_UDP_CONNECT) {
		free(command->payload.udp.host);
	} else if ((command->type == SKYUV_SOCKET_COMMAND_SEND ||
				command->type == SKYUV_SOCKET_COMMAND_SEND_LOW ||
				command->type == SKYUV_SOCKET_COMMAND_UDP_SEND) &&
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
#ifndef _WIN32
	result = uv_signal_init(&created->loop, &created->hangup_signal);
	if (result != 0) {
		uv_close((uv_handle_t *)&created->async, NULL);
		(void)uv_run(&created->loop, UV_RUN_DEFAULT);
		skyuv_mutex_destroy(&created->slots_mutex);
		skyuv_socket_command_queue_destroy(&created->commands);
		(void)uv_loop_close(&created->loop);
		free(created->slots);
		free(created->generations);
		free(created);
		return SKYUV_ERROR_SYSTEM;
	}
	created->hangup_signal.data = created;
	created->hangup_signal_initialized = true;
	result = uv_signal_start(&created->hangup_signal, on_hangup_signal, SIGHUP);
	if (result != 0) {
		uv_close((uv_handle_t *)&created->hangup_signal, NULL);
		uv_close((uv_handle_t *)&created->async, NULL);
		(void)uv_run(&created->loop, UV_RUN_DEFAULT);
		skyuv_mutex_destroy(&created->slots_mutex);
		skyuv_socket_command_queue_destroy(&created->commands);
		(void)uv_loop_close(&created->loop);
		free(created->slots);
		free(created->generations);
		free(created);
		return SKYUV_ERROR_SYSTEM;
	}
#endif
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

int skyuv_socket_runtime_pause(struct skyuv_socket_runtime *runtime, int id, uintptr_t opaque) {
	struct skyuv_socket_command *command;
	int result;

	if (runtime == NULL || id <= 0) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	command = calloc(1, sizeof(*command));
	if (command == NULL) {
		return SKYUV_ERROR_OUT_OF_MEMORY;
	}
	command->type = SKYUV_SOCKET_COMMAND_PAUSE;
	command->id = id;
	command->opaque = opaque;
	result = skyuv_socket_runtime_submit(runtime, command);
	if (result != SKYUV_OK) {
		free(command);
	}
	return result;
}

int skyuv_socket_runtime_nodelay(struct skyuv_socket_runtime *runtime, int id) {
	struct skyuv_socket_command *command;
	int result;

	if (runtime == NULL || id <= 0) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	command = calloc(1, sizeof(*command));
	if (command == NULL) {
		return SKYUV_ERROR_OUT_OF_MEMORY;
	}
	command->type = SKYUV_SOCKET_COMMAND_NODELAY;
	command->id = id;
	result = skyuv_socket_runtime_submit(runtime, command);
	if (result != SKYUV_OK) {
		free(command);
	}
	return result;
}

static int runtime_send(struct skyuv_socket_runtime *runtime, int id, void *data, size_t size,
						enum skyuv_socket_buffer_ownership ownership, void (*release)(void *data),
						bool low_priority) {
	struct skyuv_socket_command *command;
	void *queued_data = data;
	int result;

	if (runtime == NULL || id <= 0 || data == NULL || size == 0 ||
		(ownership != SKYUV_SOCKET_BUFFER_BORROWED && ownership != SKYUV_SOCKET_BUFFER_OWNED) ||
		size > UINT_MAX) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	if (ownership == SKYUV_SOCKET_BUFFER_BORROWED) {
		queued_data = malloc(size);
		if (queued_data == NULL) {
			return SKYUV_ERROR_OUT_OF_MEMORY;
		}
		memcpy(queued_data, data, size);
		release = NULL;
	}
	command = calloc(1, sizeof(*command));
	if (command == NULL) {
		if (ownership == SKYUV_SOCKET_BUFFER_BORROWED) {
			free(queued_data);
		}
		return SKYUV_ERROR_OUT_OF_MEMORY;
	}
	command->type = low_priority ? SKYUV_SOCKET_COMMAND_SEND_LOW : SKYUV_SOCKET_COMMAND_SEND;
	command->id = id;
	command->payload.send.data = queued_data;
	command->payload.send.size = size;
	command->payload.send.ownership = SKYUV_SOCKET_BUFFER_OWNED;
	command->payload.send.release = release;
	result = skyuv_socket_runtime_submit(runtime, command);
	if (result != SKYUV_OK) {
		if (ownership == SKYUV_SOCKET_BUFFER_OWNED) {
			command->payload.send.ownership = SKYUV_SOCKET_BUFFER_BORROWED;
		}
		skyuv_socket_command_destroy(command);
	}
	return result;
}

int skyuv_socket_runtime_send(struct skyuv_socket_runtime *runtime, int id, void *data, size_t size,
							  enum skyuv_socket_buffer_ownership ownership,
							  void (*release)(void *data)) {
	return runtime_send(runtime, id, data, size, ownership, release, false);
}

int skyuv_socket_runtime_send_low(struct skyuv_socket_runtime *runtime, int id, void *data,
								  size_t size, enum skyuv_socket_buffer_ownership ownership,
								  void (*release)(void *data)) {
	return runtime_send(runtime, id, data, size, ownership, release, true);
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

int skyuv_socket_runtime_bind(struct skyuv_socket_runtime *runtime, int fd, uintptr_t opaque,
							  int *id) {
#if SKYUV_SOCKET_EXTERNAL_FD
	struct skyuv_socket_command *command;
	struct skyuv_socket_entry *entry;
	uint32_t slot;
	int result;

	if (runtime == NULL || fd < 0 || id == NULL) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	result = reserve_entry(runtime, opaque, &entry);
	if (result != SKYUV_OK) {
		return result;
	}
	skyuv_mutex_lock(&runtime->slots_mutex);
	for (slot = 0; slot < SKYUV_SOCKET_SLOT_COUNT; ++slot) {
		struct skyuv_socket_entry *current = runtime->slots[slot];

		if (current != NULL && current != entry && current->external_fd == fd) {
			skyuv_mutex_unlock(&runtime->slots_mutex);
			discard_uninitialized_entry(entry);
			return SKYUV_ERROR_INVALID_STATE;
		}
	}
	entry->external_fd = fd;
	skyuv_mutex_unlock(&runtime->slots_mutex);
	command = calloc(1, sizeof(*command));
	if (command == NULL) {
		discard_uninitialized_entry(entry);
		return SKYUV_ERROR_OUT_OF_MEMORY;
	}
	command->type = SKYUV_SOCKET_COMMAND_BIND;
	command->id = entry->id;
	command->opaque = opaque;
	command->payload.bind.fd = fd;
	result = skyuv_socket_runtime_submit(runtime, command);
	if (result != SKYUV_OK) {
		free(command);
		discard_uninitialized_entry(entry);
		return result;
	}
	*id = entry->id;
	return SKYUV_OK;
#else
	(void)runtime;
	(void)fd;
	(void)opaque;
	(void)id;
	return SKYUV_ERROR_NOT_SUPPORTED;
#endif
}

int skyuv_socket_runtime_udp(struct skyuv_socket_runtime *runtime, const char *host, int port,
							 uintptr_t opaque, int *id) {
	struct skyuv_socket_command *command;
	struct skyuv_socket_entry *entry;
	const char *bind_host = host == NULL ? "0.0.0.0" : host;
	int result;

	if (runtime == NULL || port < 0 || port > 65535 || id == NULL) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	result = reserve_entry(runtime, opaque, &entry);
	if (result != SKYUV_OK) {
		return result;
	}
	command = calloc(1, sizeof(*command));
	if (command != NULL) {
		command->payload.udp.host = copy_string(bind_host);
	}
	if (command == NULL || command->payload.udp.host == NULL) {
		free(command);
		discard_uninitialized_entry(entry);
		return SKYUV_ERROR_OUT_OF_MEMORY;
	}
	command->type = SKYUV_SOCKET_COMMAND_UDP;
	command->id = entry->id;
	command->opaque = opaque;
	command->payload.udp.port = port;
	result = skyuv_socket_runtime_submit(runtime, command);
	if (result != SKYUV_OK) {
		skyuv_socket_command_destroy(command);
		discard_uninitialized_entry(entry);
		return result;
	}
	*id = entry->id;
	return SKYUV_OK;
}

int skyuv_socket_runtime_udp_connect(struct skyuv_socket_runtime *runtime, int id, const char *host,
									 int port) {
	struct skyuv_socket_command *command;
	int result;

	if (runtime == NULL || id <= 0 || host == NULL || port < 0 || port > 65535) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	command = calloc(1, sizeof(*command));
	if (command != NULL) {
		command->payload.udp.host = copy_string(host);
	}
	if (command == NULL || command->payload.udp.host == NULL) {
		free(command);
		return SKYUV_ERROR_OUT_OF_MEMORY;
	}
	command->type = SKYUV_SOCKET_COMMAND_UDP_CONNECT;
	command->id = id;
	command->payload.udp.port = port;
	result = skyuv_socket_runtime_submit(runtime, command);
	if (result != SKYUV_OK) {
		skyuv_socket_command_destroy(command);
	}
	return result;
}

int skyuv_socket_runtime_udp_send(struct skyuv_socket_runtime *runtime, int id,
								  const uint8_t *address, size_t address_size, void *data,
								  size_t size, enum skyuv_socket_buffer_ownership ownership,
								  void (*release)(void *data)) {
	struct skyuv_socket_command *command;
	void *queued_data = data;
	int result;

	if (runtime == NULL || id <= 0 || (data == NULL && size > 0) || size > UINT_MAX ||
		(address_size != 0 && address_size != 7U && address_size != 19U) ||
		(address_size > 0 && address == NULL)) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	if (ownership == SKYUV_SOCKET_BUFFER_BORROWED) {
		queued_data = malloc(size == 0 ? 1 : size);
		if (queued_data == NULL) {
			return SKYUV_ERROR_OUT_OF_MEMORY;
		}
		if (size > 0) {
			(void)memcpy(queued_data, data, size);
		}
		release = NULL;
	} else if (ownership != SKYUV_SOCKET_BUFFER_OWNED) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	command = calloc(1, sizeof(*command));
	if (command == NULL) {
		if (ownership == SKYUV_SOCKET_BUFFER_BORROWED) {
			free(queued_data);
		}
		return SKYUV_ERROR_OUT_OF_MEMORY;
	}
	command->type = SKYUV_SOCKET_COMMAND_UDP_SEND;
	command->id = id;
	command->payload.send.data = queued_data;
	command->payload.send.size = size;
	command->payload.send.ownership = SKYUV_SOCKET_BUFFER_OWNED;
	command->payload.send.release = release;
	command->payload.send.address_size = address_size;
	if (address_size > 0) {
		(void)memcpy(command->payload.send.address, address, address_size);
	}
	result = skyuv_socket_runtime_submit(runtime, command);
	if (result != SKYUV_OK) {
		if (ownership == SKYUV_SOCKET_BUFFER_OWNED) {
			command->payload.send.ownership = SKYUV_SOCKET_BUFFER_BORROWED;
		}
		skyuv_socket_command_destroy(command);
	}
	return result;
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

int skyuv_socket_runtime_shutdown(struct skyuv_socket_runtime *runtime, int id, uintptr_t opaque) {
	struct skyuv_socket_command *command;
	int result;

	if (runtime == NULL || id <= 0) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	command = calloc(1, sizeof(*command));
	if (command == NULL) {
		return SKYUV_ERROR_OUT_OF_MEMORY;
	}
	command->type = SKYUV_SOCKET_COMMAND_SHUTDOWN;
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
		state = entry->close_requested ? SKYUV_SOCKET_STATE_CLOSING : entry->state;
	}
	skyuv_mutex_unlock(&runtime->slots_mutex);
	return state;
}

void skyuv_socket_runtime_updatetime(struct skyuv_socket_runtime *runtime, uint64_t time) {
	if (runtime == NULL) {
		return;
	}
	skyuv_mutex_lock(&runtime->slots_mutex);
	runtime->current_time = time;
	skyuv_mutex_unlock(&runtime->slots_mutex);
}

struct skyuv_socket_info *skyuv_socket_runtime_info(struct skyuv_socket_runtime *runtime) {
	struct skyuv_socket_info *head = NULL;
	struct skyuv_socket_info *tail = NULL;
	uint32_t slot;

	if (runtime == NULL) {
		return NULL;
	}
	skyuv_mutex_lock(&runtime->slots_mutex);
	for (slot = 0; slot < SKYUV_SOCKET_SLOT_COUNT; ++slot) {
		struct skyuv_socket_entry *entry = runtime->slots[slot];
		struct skyuv_socket_info *info;

		if (entry == NULL || (entry->state != SKYUV_SOCKET_STATE_LISTENING &&
							  entry->state != SKYUV_SOCKET_STATE_CONNECTED &&
							  entry->state != SKYUV_SOCKET_STATE_CONNECTED_PAUSED &&
							  entry->state != SKYUV_SOCKET_STATE_HALFCLOSE_READ &&
							  entry->state != SKYUV_SOCKET_STATE_UDP &&
							  entry->state != SKYUV_SOCKET_STATE_CLOSING)) {
			continue;
		}
		info = calloc(1, sizeof(*info));
		if (info == NULL) {
			break;
		}
		info->id = entry->id;
		info->type = entry->state == SKYUV_SOCKET_STATE_LISTENING
						 ? SKYUV_SOCKET_INFO_LISTEN
						 : (entry->state == SKYUV_SOCKET_STATE_UDP
								? SKYUV_SOCKET_INFO_UDP
								: (entry->close_requested ||
										   entry->state == SKYUV_SOCKET_STATE_HALFCLOSE_READ ||
										   entry->state == SKYUV_SOCKET_STATE_CLOSING
									   ? SKYUV_SOCKET_INFO_CLOSING
									   : SKYUV_SOCKET_INFO_TCP));
		info->opaque = entry->opaque;
		info->read =
			entry->state == SKYUV_SOCKET_STATE_LISTENING ? entry->accept_count : entry->read_bytes;
		info->write = entry->write_bytes;
		info->rtime = entry->read_time;
		info->wtime = entry->write_time;
		info->wbuffer = entry->queued_bytes;
		info->reading = entry->reading;
		info->writing = entry->writing || entry->queued_bytes > 0;
		(void)memcpy(info->name, entry->name, sizeof(info->name));
		if (tail == NULL) {
			head = info;
		} else {
			tail->next = info;
		}
		tail = info;
	}
	skyuv_mutex_unlock(&runtime->slots_mutex);
	return head;
}

void skyuv_socket_runtime_info_release(struct skyuv_socket_info *info) {
	while (info != NULL) {
		struct skyuv_socket_info *next = info->next;

		free(info);
		info = next;
	}
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

		if (entry != NULL && entry->initialized && !uv_is_closing(entry_handle(entry))) {
			set_entry_state(entry, SKYUV_SOCKET_STATE_CLOSING);
			uv_close(entry_handle(entry), close_entry);
		} else if (entry != NULL && !entry->initialized) {
			released->slots[slot] = NULL;
			free(entry);
		}
	}
	if (!uv_is_closing((uv_handle_t *)&released->async)) {
		uv_close((uv_handle_t *)&released->async, NULL);
	}
#ifndef _WIN32
	if (released->hangup_signal_initialized &&
		!uv_is_closing((uv_handle_t *)&released->hangup_signal)) {
		(void)uv_signal_stop(&released->hangup_signal);
		uv_close((uv_handle_t *)&released->hangup_signal, NULL);
	}
#endif
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
