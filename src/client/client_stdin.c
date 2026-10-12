#include <lua.h>
#include <lauxlib.h>
#include <uv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* stdin 属于进程。阻塞读取线程只访问静态队列，不持有 Lua 对象；
 * 不因任一 Lua 状态 GC 关闭共享 stdin，进程退出负责终止尚在等待输入的线程。
 * EOF 后队列消费完成时回收线程，所有队列字段均由 mutex 保护。 */
static struct {
	uv_mutex_t mutex;
	uv_thread_t thread;
	char *lines[1024];
	unsigned int head;
	unsigned int tail;
	int started;
	int joined;
	int eof;
	int failed;
	int overflow;
	int init_error;
	int finished;
} input;
static uv_once_t once = UV_ONCE_INIT;

static void initialize(void) {
	input.init_error = uv_mutex_init(&input.mutex);
}

static void read_input(void *argument) {
	(void)argument;
	char buffer[1024];
	while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
		size_t length = strlen(buffer);
		while (length > 0 && (buffer[length - 1] == '\n' || buffer[length - 1] == '\r'))
			--length;
		char *line = malloc(length + 1);
		if (line != NULL) {
			memcpy(line, buffer, length);
			line[length] = 0;
		}
		uv_mutex_lock(&input.mutex);
		unsigned int next = (input.tail + 1U) % 1024U;
		if (line == NULL)
			input.failed = 1;
		else if (next == input.head) {
			input.overflow = 1;
			free(line);
		} else {
			input.lines[input.tail] = line;
			input.tail = next;
		}
		uv_mutex_unlock(&input.mutex);
	}
	uv_mutex_lock(&input.mutex);
	input.eof = feof(stdin) != 0;
	input.failed |= ferror(stdin) != 0;
	input.finished = 1;
	uv_mutex_unlock(&input.mutex);
}

int skyuv_client_readstdin(lua_State *L) {
	uv_once(&once, initialize);
	if (input.init_error != 0)
		return luaL_error(L, "stdin initialization failed: %s", uv_strerror(input.init_error));
	uv_mutex_lock(&input.mutex);
	if (!input.started) {
		input.started = 1;
		if (uv_thread_create(&input.thread, read_input, NULL) != 0) {
			input.failed = 1;
			input.joined = 1;
		}
	}
	char *line = NULL;
	if (input.head != input.tail) {
		line = input.lines[input.head];
		input.lines[input.head] = NULL;
		input.head = (input.head + 1U) % 1024U;
	}
	int eof = input.eof;
	int failed = input.failed;
	int overflow = input.overflow;
	int join = !input.joined && input.finished;
	if (join)
		input.joined = 1;
	uv_mutex_unlock(&input.mutex);
	if (join)
		(void)uv_thread_join(&input.thread);
	if (line != NULL) {
		lua_pushstring(L, line);
		free(line);
		return 1;
	}
	if (overflow || failed || eof) {
		lua_pushnil(L);
		lua_pushstring(L, overflow ? "overflow" : failed ? "error" : "eof");
		return 2;
	}
	return 0;
}
