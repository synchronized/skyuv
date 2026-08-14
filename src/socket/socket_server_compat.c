#include "socket_internal.h"
#include "../compat/skynet/skyuv_control.h"

#include <stdlib.h>
#include <string.h>

#include <uv.h>

#include "socket_server.h"

struct socket_server {
	struct skyuv_socket_runtime *runtime;
	struct socket_object_interface object_interface;
};

struct socket_server *socket_server_create(uint64_t time) {
	struct socket_server *server = calloc(1, sizeof(*server));

	(void)time;
	if (server == NULL) {
		return NULL;
	}
	if (skyuv_socket_runtime_create(&server->runtime) != SKYUV_OK) {
		free(server);
		return NULL;
	}
	return server;
}

void socket_server_release(struct socket_server *server) {
	if (server == NULL) {
		return;
	}
	skyuv_socket_runtime_release(&server->runtime);
	free(server);
}

void socket_server_updatetime(struct socket_server *server, uint64_t time) {
	if (server != NULL) {
		skyuv_socket_runtime_updatetime(server->runtime, time);
	}
}

int socket_server_poll(struct socket_server *server, struct socket_message *result, int *more) {
	struct skyuv_socket_event event;

	if (server == NULL || result == NULL) {
		return SOCKET_ERR;
	}
	do {
		if (skyuv_socket_runtime_poll(server->runtime, &event) != SKYUV_OK) {
			return SOCKET_ERR;
		}
		if (event.type == SKYUV_SOCKET_EVENT_REOPEN_LOG) {
			(void)skyuv_skynet_reopen_log();
		} else if (event.type == SKYUV_SOCKET_EVENT_PROCESS_SHUTDOWN) {
			skyuv_skynet_shutdown();
		}
	} while (event.type == SKYUV_SOCKET_EVENT_REOPEN_LOG ||
			 event.type == SKYUV_SOCKET_EVENT_PROCESS_SHUTDOWN);
	result->id = event.id;
	result->opaque = event.opaque;
	result->ud = event.type == SKYUV_SOCKET_EVENT_DATA || event.type == SKYUV_SOCKET_EVENT_UDP
					 ? (int)event.size
					 : event.value;
	result->data = event.type == SKYUV_SOCKET_EVENT_ERROR ? (char *)uv_strerror(event.value)
												  : event.data;
	if (more != NULL) {
		*more = 0;
	}
	return (int)event.type;
}

void socket_server_exit(struct socket_server *server) {
	if (server != NULL) {
		(void)skyuv_socket_runtime_exit(server->runtime);
	}
}

void socket_server_close(struct socket_server *server, uintptr_t opaque, int id) {
	if (server != NULL) {
		(void)skyuv_socket_runtime_close(server->runtime, id, opaque);
	}
}

void socket_server_shutdown(struct socket_server *server, uintptr_t opaque, int id) {
	if (server != NULL) {
		(void)skyuv_socket_runtime_shutdown(server->runtime, id, opaque);
	}
}

void socket_server_start(struct socket_server *server, uintptr_t opaque, int id) {
	if (server != NULL) {
		(void)skyuv_socket_runtime_start(server->runtime, id, opaque);
	}
}

void socket_server_pause(struct socket_server *server, uintptr_t opaque, int id) {
	if (server != NULL) {
		(void)skyuv_socket_runtime_pause(server->runtime, id, opaque);
	}
}

static void release_sendbuffer(struct socket_server *server, struct socket_sendbuffer *buffer) {
	if (buffer->type == SOCKET_BUFFER_MEMORY) {
		free((void *)buffer->buffer);
	} else if (buffer->type == SOCKET_BUFFER_OBJECT && server->object_interface.free != NULL) {
		server->object_interface.free((void *)buffer->buffer);
	}
}

static int send_buffer(struct socket_server *server, struct socket_sendbuffer *buffer,
					   bool low_priority) {
	const void *data;
	size_t size;
	void *copy;
	int result;

	if (server == NULL || buffer == NULL || buffer->buffer == NULL) {
		return -1;
	}
	if (buffer->type == SOCKET_BUFFER_OBJECT) {
		if (server->object_interface.buffer == NULL || server->object_interface.size == NULL ||
			server->object_interface.free == NULL) {
			return -1;
		}
		data = server->object_interface.buffer(buffer->buffer);
		size = server->object_interface.size(buffer->buffer);
		copy = malloc(size);
		if (copy == NULL) {
			release_sendbuffer(server, buffer);
			return -1;
		}
		memcpy(copy, data, size);
		release_sendbuffer(server, buffer);
		result = low_priority
					 ? skyuv_socket_runtime_send_low(server->runtime, buffer->id, copy, size,
												 SKYUV_SOCKET_BUFFER_OWNED, NULL)
					 : skyuv_socket_runtime_send(server->runtime, buffer->id, copy, size,
											 SKYUV_SOCKET_BUFFER_OWNED, NULL);
		if (result != SKYUV_OK) {
			free(copy);
		}
		return result == SKYUV_OK ? 0 : -1;
	}
	result = low_priority
				 ? skyuv_socket_runtime_send_low(
					   server->runtime, buffer->id, (void *)buffer->buffer, buffer->sz,
					   buffer->type == SOCKET_BUFFER_MEMORY ? SKYUV_SOCKET_BUFFER_OWNED
													 : SKYUV_SOCKET_BUFFER_BORROWED,
					   NULL)
				 : skyuv_socket_runtime_send(
					   server->runtime, buffer->id, (void *)buffer->buffer, buffer->sz,
					   buffer->type == SOCKET_BUFFER_MEMORY ? SKYUV_SOCKET_BUFFER_OWNED
													 : SKYUV_SOCKET_BUFFER_BORROWED,
					   NULL);
	if (result != SKYUV_OK && buffer->type == SOCKET_BUFFER_MEMORY) {
		release_sendbuffer(server, buffer);
	}
	return result == SKYUV_OK ? 0 : -1;
}

int socket_server_send(struct socket_server *server, struct socket_sendbuffer *buffer) {
	return send_buffer(server, buffer, false);
}

int socket_server_send_lowpriority(struct socket_server *server, struct socket_sendbuffer *buffer) {
	return send_buffer(server, buffer, true);
}

int socket_server_listen(struct socket_server *server, uintptr_t opaque, const char *address,
						 int port, int backlog) {
	int id;

	if (server == NULL || skyuv_socket_runtime_listen(server->runtime, address, port, backlog,
													  opaque, &id) != SKYUV_OK) {
		return -1;
	}
	return id;
}

int socket_server_connect(struct socket_server *server, uintptr_t opaque, const char *address,
						  int port) {
	int id;

	if (server == NULL ||
		skyuv_socket_runtime_connect(server->runtime, address, port, opaque, &id) != SKYUV_OK) {
		return -1;
	}
	return id;
}

int socket_server_bind(struct socket_server *server, uintptr_t opaque, int fd) {
	int id;

	if (server == NULL ||
		skyuv_socket_runtime_bind(server->runtime, fd, opaque, &id) != SKYUV_OK) {
		return -1;
	}
	return id;
}

void socket_server_nodelay(struct socket_server *server, int id) {
	if (server != NULL) {
		(void)skyuv_socket_runtime_nodelay(server->runtime, id);
	}
}

void socket_server_userobject(struct socket_server *server,
							  struct socket_object_interface *object_interface) {
	if (server != NULL && object_interface != NULL) {
		server->object_interface = *object_interface;
	}
}

int socket_server_udp(struct socket_server *server, uintptr_t opaque, const char *address,
					  int port) {
	int id;

	if (server == NULL ||
		skyuv_socket_runtime_udp(server->runtime, address, port, opaque, &id) != SKYUV_OK) {
		return -1;
	}
	return id;
}

int socket_server_udp_connect(struct socket_server *server, int id, const char *address, int port) {
	return server != NULL &&
			   skyuv_socket_runtime_udp_connect(server->runtime, id, address, port) == SKYUV_OK
			   ? 0
			   : -1;
}

int socket_server_udp_dial(struct socket_server *server, uintptr_t opaque, const char *address,
						   int port) {
	int id = socket_server_udp(server, opaque,
							   address != NULL && strchr(address, ':') != NULL ? "::" : NULL, 0);

	if (id < 0 || socket_server_udp_connect(server, id, address, port) != 0) {
		return -1;
	}
	return id;
}

int socket_server_udp_listen(struct socket_server *server, uintptr_t opaque, const char *address,
							 int port) {
	return socket_server_udp(server, opaque, address, port);
}

int socket_server_udp_send(struct socket_server *server, const struct socket_udp_address *address,
						   struct socket_sendbuffer *buffer) {
	const uint8_t *encoded = (const uint8_t *)address;
	const void *data;
	void *copy;
	size_t size;
	size_t address_size;
	enum skyuv_socket_buffer_ownership ownership;
	int result;

	if (server == NULL || buffer == NULL || buffer->buffer == NULL || encoded == NULL) {
		return -1;
	}
	address_size = encoded[0] == 1U ? 7U : (encoded[0] == 2U ? 19U : 0U);
	if (address_size == 0) {
		release_sendbuffer(server, buffer);
		return -1;
	}
	if (buffer->type == SOCKET_BUFFER_OBJECT) {
		if (server->object_interface.buffer == NULL || server->object_interface.size == NULL ||
			server->object_interface.free == NULL) {
			return -1;
		}
		data = server->object_interface.buffer(buffer->buffer);
		size = server->object_interface.size(buffer->buffer);
		copy = malloc(size);
		if (copy == NULL) {
			release_sendbuffer(server, buffer);
			return -1;
		}
		(void)memcpy(copy, data, size);
		release_sendbuffer(server, buffer);
		result = skyuv_socket_runtime_udp_send(server->runtime, buffer->id, encoded, address_size,
											 copy, size, SKYUV_SOCKET_BUFFER_OWNED, NULL);
		if (result != SKYUV_OK) {
			free(copy);
		}
		return result == SKYUV_OK ? 0 : -1;
	}
	ownership = buffer->type == SOCKET_BUFFER_MEMORY ? SKYUV_SOCKET_BUFFER_OWNED
													 : SKYUV_SOCKET_BUFFER_BORROWED;
	result = skyuv_socket_runtime_udp_send(server->runtime, buffer->id, encoded, address_size,
										(void *)buffer->buffer, buffer->sz, ownership, NULL);
	if (result != SKYUV_OK && buffer->type == SOCKET_BUFFER_MEMORY) {
		release_sendbuffer(server, buffer);
	}
	return result == SKYUV_OK ? 0 : -1;
}

const struct socket_udp_address *socket_server_udp_address(struct socket_server *server,
														   struct socket_message *message,
														   int *address_size) {
	const uint8_t *address;
	int size;

	(void)server;
	if (message == NULL || message->data == NULL || message->ud < 0) {
		if (address_size != NULL) {
			*address_size = 0;
		}
		return NULL;
	}
	address = (const uint8_t *)message->data + message->ud;
	size = address[0] == 1U ? 7 : (address[0] == 2U ? 19 : 0);
	if (address_size != NULL) {
		*address_size = size;
	}
	return size == 0 ? NULL : (const struct socket_udp_address *)address;
}

struct socket_info *socket_server_info(struct socket_server *server) {
	struct skyuv_socket_info *snapshot;
	struct skyuv_socket_info *current;
	struct socket_info *head = NULL;
	struct socket_info *tail = NULL;

	if (server == NULL) {
		return NULL;
	}
	snapshot = skyuv_socket_runtime_info(server->runtime);
	for (current = snapshot; current != NULL; current = current->next) {
		struct socket_info *info = socket_info_create(tail);

		if (info == NULL) {
			break;
		}
		if (head == NULL) {
			head = info;
		}
		tail = info;
		info->id = current->id;
		info->type = current->type == SKYUV_SOCKET_INFO_LISTEN
						 ? SOCKET_INFO_LISTEN
						 : (current->type == SKYUV_SOCKET_INFO_UDP ? SOCKET_INFO_UDP
																	 : (current->type == SKYUV_SOCKET_INFO_CLOSING
																			? SOCKET_INFO_CLOSING
																			: SOCKET_INFO_TCP));
		info->opaque = (uint64_t)current->opaque;
		info->read = current->read;
		info->write = current->write;
		info->rtime = current->rtime;
		info->wtime = current->wtime;
		info->wbuffer = current->wbuffer > INT64_MAX ? INT64_MAX : (int64_t)current->wbuffer;
		info->reading = current->reading ? 1U : 0U;
		info->writing = current->writing ? 1U : 0U;
		(void)memcpy(info->name, current->name, sizeof(info->name));
	}
	skyuv_socket_runtime_info_release(snapshot);
	return head;
}
