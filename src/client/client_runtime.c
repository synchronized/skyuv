#include "client_runtime.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define CLIENT_RECEIVE_LIMIT 65536U

struct client_request {
	uv_mutex_t mutex;
	uv_cond_t condition;
	struct sockaddr_in address;
	struct skyuv_client_runtime *runtime;
	int64_t id;
	int done;
	int result;
	uv_write_t write;
	const void *source;
	void *destination;
	size_t size;
};

struct client_connection {
	uv_tcp_t tcp;
	uv_connect_t connect;
	struct skyuv_client_runtime *runtime;
	struct client_request *request;
	struct client_connection *next;
	int64_t id;
	/* 此缓存及状态仅由 I/O 线程访问；满时停止读，不丢弃已收到的数据。 */
	char received[CLIENT_RECEIVE_LIMIT];
	size_t received_size;
	int read_status;
	int paused;
};

struct skyuv_client_runtime {
	uv_thread_t thread;
	uv_mutex_t mutex;
	uv_cond_t condition;
	uv_loop_t loop;
	uv_async_t wakeup;
	skyuv_client_command command;
	void *argument;
	int result;
	int ready;
	int pending;
	int stopping;
	struct client_connection *connections;
	int64_t next_id;
};

static void complete(struct client_request *request, int result) {
	uv_mutex_lock(&request->mutex);
	request->result = result;
	request->done = 1;
	uv_cond_signal(&request->condition);
	uv_mutex_unlock(&request->mutex);
}

static void connection_closed(uv_handle_t *handle) {
	struct client_connection *connection = handle->data;
	struct client_connection **entry = &connection->runtime->connections;
	while (*entry != connection)
		entry = &(*entry)->next;
	*entry = connection->next;
	struct client_request *request = connection->request;
	free(connection);
	if (request != NULL)
		complete(request, request->result);
}

static void allocate_read(uv_handle_t *handle, size_t suggested, uv_buf_t *buffer) {
	(void)suggested;
	struct client_connection *connection = handle->data;
	*buffer = uv_buf_init(connection->received + connection->received_size,
		(unsigned int)(CLIENT_RECEIVE_LIMIT - connection->received_size));
}

static void received(uv_stream_t *stream, ssize_t count, const uv_buf_t *buffer) {
	(void)buffer;
	struct client_connection *connection = stream->data;
	if (count > 0) {
		connection->received_size += (size_t)count;
		if (connection->received_size == CLIENT_RECEIVE_LIMIT) {
			(void)uv_read_stop(stream);
			connection->paused = 1;
		}
	} else if (count < 0) {
		connection->read_status = (int)count;
		(void)uv_read_stop(stream);
	}
}

static void connected(uv_connect_t *connect, int result) {
	struct client_connection *connection = connect->data;
	struct client_request *request = connection->request;
	if (result == 0)
		result = uv_read_start((uv_stream_t *)&connection->tcp, allocate_read, received);
	if (result != 0) {
		request->result = result;
		uv_close((uv_handle_t *)&connection->tcp, connection_closed);
		return;
	}
	request->id = connection->id;
	connection->request = NULL;
	complete(request, 0);
}

static int start_connect(uv_loop_t *loop, void *argument) {
	struct client_request *request = argument;
	struct skyuv_client_runtime *runtime = request->runtime;
	if (runtime->next_id == INT64_MAX)
		return UV_ENOSPC;
	struct client_connection *connection = calloc(1, sizeof(*connection));
	if (connection == NULL)
		return UV_ENOMEM;
	int result = uv_tcp_init(loop, &connection->tcp);
	if (result != 0) {
		free(connection);
		return result;
	}
	connection->runtime = runtime;
	connection->request = request;
	connection->id = ++runtime->next_id;
	connection->tcp.data = connection;
	connection->connect.data = connection;
	connection->next = runtime->connections;
	runtime->connections = connection;
	result = uv_tcp_connect(&connection->connect, &connection->tcp,
		(const struct sockaddr *)&request->address, connected);
	if (result != 0) {
		request->result = result;
		uv_close((uv_handle_t *)&connection->tcp, connection_closed);
	}
	/* 即时失败也经关闭回调确认资源释放，调用者等 complete。 */
	return 0;
}

static int start_close(uv_loop_t *loop, void *argument) {
	(void)loop;
	struct client_request *request = argument;
	struct client_connection *connection = request->runtime->connections;
	while (connection != NULL && connection->id != request->id)
		connection = connection->next;
	if (connection == NULL) {
		if (request->id > 0 && request->id <= request->runtime->next_id) {
			complete(request, 0);
			return 0;
		}
		return UV_EBADF;
	}
	connection->request = request;
	uv_close((uv_handle_t *)&connection->tcp, connection_closed);
	return 0;
}

static int submit_request(struct client_request *request, skyuv_client_command command) {
	int result = uv_mutex_init(&request->mutex);
	if (result != 0)
		return result;
	result = uv_cond_init(&request->condition);
	if (result == 0) {
		result = skyuv_client_runtime_call(request->runtime, command, request);
		if (result == 0) {
			uv_mutex_lock(&request->mutex);
			while (!request->done)
				uv_cond_wait(&request->condition, &request->mutex);
			result = request->result;
			uv_mutex_unlock(&request->mutex);
		}
		uv_cond_destroy(&request->condition);
	}
	uv_mutex_destroy(&request->mutex);
	return result;
}

static struct client_connection *find_connection(struct client_request *request) {
	struct client_connection *connection = request->runtime->connections;
	while (connection != NULL && connection->id != request->id)
		connection = connection->next;
	return connection;
}

static void written(uv_write_t *write, int status) {
	struct client_request *request = write->data;
	complete(request, status);
}

static int start_send(uv_loop_t *loop, void *argument) {
	(void)loop;
	struct client_request *request = argument;
	struct client_connection *connection = find_connection(request);
	if (connection == NULL)
		return UV_EBADF;
	if (request->size == 0) {
		complete(request, 0);
		return 0;
	}
	request->write.data = request;
	uv_buf_t buffer = uv_buf_init((char *)request->source, (unsigned int)request->size);
	return uv_write(&request->write, (uv_stream_t *)&connection->tcp, &buffer, 1, written);
}

static int receive_data(uv_loop_t *loop, void *argument) {
	(void)loop;
	struct client_request *request = argument;
	struct client_connection *connection = find_connection(request);
	if (connection == NULL)
		return UV_EBADF;
	if (connection->received_size == 0) {
		request->size = 0;
		return connection->read_status != 0 ? connection->read_status : UV_EAGAIN;
	}
	if (request->size > connection->received_size)
		request->size = connection->received_size;
	memcpy(request->destination, connection->received, request->size);
	connection->received_size -= request->size;
	memmove(connection->received, connection->received + request->size, connection->received_size);
	if (connection->paused && connection->read_status == 0) {
		int result = uv_read_start((uv_stream_t *)&connection->tcp, allocate_read, received);
		if (result != 0)
			connection->read_status = result;
		connection->paused = 0;
	}
	return 0;
}

int skyuv_client_runtime_send(struct skyuv_client_runtime *runtime, int64_t id,
	const void *buffer, size_t size) {
	if (runtime == NULL || (buffer == NULL && size != 0))
		return UV_EINVAL;
	/* 分段提交避免 uv_buf_init 的 unsigned int 长度截断，保持完整发送。 */
	const char *source = buffer;
	do {
		size_t part = size > UINT_MAX ? UINT_MAX : size;
		struct client_request request = {0};
		request.runtime = runtime;
		request.id = id;
		request.source = source;
		request.size = part;
		int result = submit_request(&request, start_send);
		if (result != 0)
			return result;
		if (size == 0)
			break;
		source += part;
		size -= part;
	} while (size != 0);
	return 0;
}

int skyuv_client_runtime_recv(struct skyuv_client_runtime *runtime, int64_t id,
	void *buffer, size_t capacity, size_t *size) {
	if (size == NULL)
		return UV_EINVAL;
	*size = 0;
	if (runtime == NULL || buffer == NULL || capacity == 0)
		return UV_EINVAL;
	struct client_request request = {0};
	request.runtime = runtime;
	request.id = id;
	request.destination = buffer;
	request.size = capacity;
	int result = skyuv_client_runtime_call(runtime, receive_data, &request);
	if (result == 0)
		*size = request.size;
	return result;
}

int skyuv_client_runtime_connect(struct skyuv_client_runtime *runtime,
	const char *address, int port, int64_t *id) {
	if (id == NULL)
		return UV_EINVAL;
	*id = 0;
	if (runtime == NULL || address == NULL || port < 0 || port > 65535)
		return UV_EINVAL;
	struct client_request request = {0};
	request.runtime = runtime;
	int result = uv_ip4_addr(address, port, &request.address);
	if (result == 0)
		result = submit_request(&request, start_connect);
	if (result == 0)
		*id = request.id;
	return result;
}

int skyuv_client_runtime_close(struct skyuv_client_runtime *runtime, int64_t id) {
	if (runtime == NULL || id <= 0)
		return UV_EBADF;
	struct client_request request = {0};
	request.runtime = runtime;
	request.id = id;
	return submit_request(&request, start_close);
}

static void close_handle(uv_handle_t *handle, void *argument) {
	(void)argument;
	if (!uv_is_closing(handle))
		uv_close(handle, NULL);
}

static void execute(uv_async_t *wakeup) {
	struct skyuv_client_runtime *runtime = wakeup->data;
	uv_mutex_lock(&runtime->mutex);
	if (runtime->stopping) {
		uv_mutex_unlock(&runtime->mutex);
		for (struct client_connection *connection = runtime->connections;
			connection != NULL; connection = connection->next)
			uv_close((uv_handle_t *)&connection->tcp, connection_closed);
		/* loop 归属线程发起关闭；保持运行直到关闭回调与取消请求完成。 */
		uv_walk(&runtime->loop, close_handle, NULL);
		return;
	}
	skyuv_client_command command = runtime->command;
	void *argument = runtime->argument;
	uv_mutex_unlock(&runtime->mutex);
	int result = command(&runtime->loop, argument);
	uv_mutex_lock(&runtime->mutex);
	runtime->result = result;
	runtime->pending = 0;
	uv_cond_signal(&runtime->condition);
	uv_mutex_unlock(&runtime->mutex);
}

static void run(void *argument) {
	struct skyuv_client_runtime *runtime = argument;
	int result = uv_loop_init(&runtime->loop);
	if (result == 0) {
		result = uv_async_init(&runtime->loop, &runtime->wakeup, execute);
		if (result != 0)
			(void)uv_loop_close(&runtime->loop);
		else
			runtime->wakeup.data = runtime;
	}
	uv_mutex_lock(&runtime->mutex);
	runtime->result = result;
	runtime->ready = 1;
	uv_cond_signal(&runtime->condition);
	uv_mutex_unlock(&runtime->mutex);
	if (result == 0) {
		(void)uv_run(&runtime->loop, UV_RUN_DEFAULT);
		(void)uv_loop_close(&runtime->loop);
	}
}

int skyuv_client_runtime_create(struct skyuv_client_runtime **output) {
	if (output == NULL)
		return UV_EINVAL;
	*output = NULL;
	struct skyuv_client_runtime *runtime = calloc(1, sizeof(*runtime));
	if (runtime == NULL)
		return UV_ENOMEM;
	int result = uv_mutex_init(&runtime->mutex);
	if (result != 0) {
		free(runtime);
		return result;
	}
	result = uv_cond_init(&runtime->condition);
	if (result != 0) {
		uv_mutex_destroy(&runtime->mutex);
		free(runtime);
		return result;
	}
	result = uv_thread_create(&runtime->thread, run, runtime);
	if (result == 0) {
		uv_mutex_lock(&runtime->mutex);
		while (!runtime->ready)
			uv_cond_wait(&runtime->condition, &runtime->mutex);
		result = runtime->result;
		uv_mutex_unlock(&runtime->mutex);
		if (result != 0)
			(void)uv_thread_join(&runtime->thread);
	}
	if (result != 0) {
		uv_cond_destroy(&runtime->condition);
		uv_mutex_destroy(&runtime->mutex);
		free(runtime);
		return result;
	}
	*output = runtime;
	return 0;
}

int skyuv_client_runtime_call(struct skyuv_client_runtime *runtime,
	skyuv_client_command command, void *argument) {
	if (runtime == NULL || command == NULL)
		return UV_EINVAL;
	uv_mutex_lock(&runtime->mutex);
	runtime->command = command;
	runtime->argument = argument;
	runtime->pending = 1;
	int result = uv_async_send(&runtime->wakeup);
	if (result == 0) {
		while (runtime->pending)
			uv_cond_wait(&runtime->condition, &runtime->mutex);
		result = runtime->result;
	} else {
		runtime->pending = 0;
	}
	uv_mutex_unlock(&runtime->mutex);
	return result;
}

void skyuv_client_runtime_destroy(struct skyuv_client_runtime **output) {
	if (output == NULL || *output == NULL)
		return;
	struct skyuv_client_runtime *runtime = *output;
	uv_mutex_lock(&runtime->mutex);
	runtime->stopping = 1;
	(void)uv_async_send(&runtime->wakeup);
	uv_mutex_unlock(&runtime->mutex);
	(void)uv_thread_join(&runtime->thread);
	uv_cond_destroy(&runtime->condition);
	uv_mutex_destroy(&runtime->mutex);
	free(runtime);
	*output = NULL;
}
