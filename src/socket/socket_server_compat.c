#include "socket_internal.h"

#include <stdlib.h>
#include <string.h>

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
	(void)server;
	(void)time;
}

int socket_server_poll(struct socket_server *server, struct socket_message *result, int *more) {
	struct skyuv_socket_event event;

	if (server == NULL || result == NULL ||
		skyuv_socket_runtime_poll(server->runtime, &event) != SKYUV_OK) {
		return SOCKET_ERR;
	}
	result->id = event.id;
	result->opaque = event.opaque;
	result->ud = event.type == SKYUV_SOCKET_EVENT_DATA ? (int)event.size : event.value;
	result->data = event.data;
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
	/* 首期没有 half-close，以完整关闭明确降级。 */
	socket_server_close(server, opaque, id);
}

void socket_server_start(struct socket_server *server, uintptr_t opaque, int id) {
	if (server != NULL) {
		(void)skyuv_socket_runtime_start(server->runtime, id, opaque);
	}
}

void socket_server_pause(struct socket_server *server, uintptr_t opaque, int id) {
	(void)server;
	(void)opaque;
	(void)id;
}

static void release_sendbuffer(struct socket_server *server, struct socket_sendbuffer *buffer) {
	if (buffer->type == SOCKET_BUFFER_MEMORY) {
		free((void *)buffer->buffer);
	} else if (buffer->type == SOCKET_BUFFER_OBJECT && server->object_interface.free != NULL) {
		server->object_interface.free((void *)buffer->buffer);
	}
}

int socket_server_send(struct socket_server *server, struct socket_sendbuffer *buffer) {
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
		result = skyuv_socket_runtime_send(server->runtime, buffer->id, copy, size,
										   SKYUV_SOCKET_BUFFER_OWNED, NULL);
		if (result != SKYUV_OK) {
			free(copy);
		}
		return result == SKYUV_OK ? 0 : -1;
	}
	result = skyuv_socket_runtime_send(
		server->runtime, buffer->id, (void *)buffer->buffer, buffer->sz,
		buffer->type == SOCKET_BUFFER_MEMORY ? SKYUV_SOCKET_BUFFER_OWNED
											 : SKYUV_SOCKET_BUFFER_BORROWED,
		NULL);
	if (result != SKYUV_OK && buffer->type == SOCKET_BUFFER_MEMORY) {
		release_sendbuffer(server, buffer);
	}
	return result == SKYUV_OK ? 0 : -1;
}

int socket_server_send_lowpriority(struct socket_server *server, struct socket_sendbuffer *buffer) {
	return socket_server_send(server, buffer);
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
	(void)server;
	(void)opaque;
	(void)fd;
	return -1;
}

void socket_server_nodelay(struct socket_server *server, int id) {
	(void)server;
	(void)id;
}

void socket_server_userobject(struct socket_server *server,
							  struct socket_object_interface *object_interface) {
	if (server != NULL && object_interface != NULL) {
		server->object_interface = *object_interface;
	}
}

int socket_server_udp(struct socket_server *server, uintptr_t opaque, const char *address,
					  int port) {
	(void)server;
	(void)opaque;
	(void)address;
	(void)port;
	return -1;
}

int socket_server_udp_connect(struct socket_server *server, int id, const char *address, int port) {
	(void)server;
	(void)id;
	(void)address;
	(void)port;
	return -1;
}

int socket_server_udp_dial(struct socket_server *server, uintptr_t opaque, const char *address,
						   int port) {
	return socket_server_udp(server, opaque, address, port);
}

int socket_server_udp_listen(struct socket_server *server, uintptr_t opaque, const char *address,
							 int port) {
	return socket_server_udp(server, opaque, address, port);
}

int socket_server_udp_send(struct socket_server *server, const struct socket_udp_address *address,
						   struct socket_sendbuffer *buffer) {
	(void)address;
	release_sendbuffer(server, buffer);
	return -1;
}

const struct socket_udp_address *socket_server_udp_address(struct socket_server *server,
														   struct socket_message *message,
														   int *address_size) {
	(void)server;
	(void)message;
	if (address_size != NULL) {
		*address_size = 0;
	}
	return NULL;
}

struct socket_info *socket_server_info(struct socket_server *server) {
	(void)server;
	return NULL;
}
