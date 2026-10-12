#include "client_runtime.h"

#include <stdlib.h>

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
};

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
