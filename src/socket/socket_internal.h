#ifndef SKYUV_SOCKET_INTERNAL_H
#define SKYUV_SOCKET_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <skyuv/error.h>
#include <skyuv/thread.h>

#define SKYUV_SOCKET_SLOT_BITS 16U
#define SKYUV_SOCKET_SLOT_COUNT (UINT32_C(1) << SKYUV_SOCKET_SLOT_BITS)
#define SKYUV_SOCKET_GENERATION_MAX UINT16_C(0x7fff)

enum skyuv_socket_state {
	SKYUV_SOCKET_STATE_INVALID = 0,
	SKYUV_SOCKET_STATE_RESERVED,
	SKYUV_SOCKET_STATE_LISTEN_PENDING,
	SKYUV_SOCKET_STATE_LISTENING,
	SKYUV_SOCKET_STATE_CONNECTING,
	SKYUV_SOCKET_STATE_ACCEPTED_PAUSED,
	SKYUV_SOCKET_STATE_CONNECTED,
	SKYUV_SOCKET_STATE_CONNECTED_PAUSED,
	SKYUV_SOCKET_STATE_HALFCLOSE_READ,
	SKYUV_SOCKET_STATE_CLOSING,
};

enum skyuv_socket_command_type {
	SKYUV_SOCKET_COMMAND_LISTEN = 0,
	SKYUV_SOCKET_COMMAND_CONNECT,
	SKYUV_SOCKET_COMMAND_START,
	SKYUV_SOCKET_COMMAND_PAUSE,
	SKYUV_SOCKET_COMMAND_NODELAY,
	SKYUV_SOCKET_COMMAND_SEND,
	SKYUV_SOCKET_COMMAND_CLOSE,
	SKYUV_SOCKET_COMMAND_SHUTDOWN,
	SKYUV_SOCKET_COMMAND_EXIT,
};

enum skyuv_socket_event_type {
	SKYUV_SOCKET_EVENT_DATA = 0,
	SKYUV_SOCKET_EVENT_CLOSE,
	SKYUV_SOCKET_EVENT_OPEN,
	SKYUV_SOCKET_EVENT_ACCEPT,
	SKYUV_SOCKET_EVENT_ERROR,
	SKYUV_SOCKET_EVENT_EXIT,
};

enum skyuv_socket_buffer_ownership {
	SKYUV_SOCKET_BUFFER_BORROWED = 0,
	SKYUV_SOCKET_BUFFER_OWNED,
};

struct skyuv_socket_command {
	struct skyuv_socket_command *next;
	enum skyuv_socket_command_type type;
	int id;
	uintptr_t opaque;
	union {
		struct {
			/* 命令拥有 host，消费或丢弃命令时释放。 */
			char *host;
			int port;
			int backlog;
		} listen;
		struct {
			/* 命令拥有 host，消费或丢弃命令时释放。 */
			char *host;
			int port;
		} connect;
		struct {
			/* OWNED 由命令转移给 write 请求；BORROWED 必须在入队前复制。 */
			void *data;
			size_t size;
			enum skyuv_socket_buffer_ownership ownership;
			void (*release)(void *data);
		} send;
	} payload;
};

struct skyuv_socket_event {
	struct skyuv_socket_event *next;
	enum skyuv_socket_event_type type;
	int id;
	uintptr_t opaque;
	int value;
	/* DATA 的 data 在事件出队后转移给 Skynet；其他类型由事件节点释放。 */
	void *data;
	size_t size;
};

struct skyuv_socket_command_queue {
	skyuv_mutex mutex;
	struct skyuv_socket_command *head;
	struct skyuv_socket_command *tail;
	bool accepting;
};

struct skyuv_socket_runtime;

int skyuv_socket_command_queue_init(struct skyuv_socket_command_queue *queue);
void skyuv_socket_command_queue_destroy(struct skyuv_socket_command_queue *queue);
int skyuv_socket_command_queue_push(struct skyuv_socket_command_queue *queue,
									struct skyuv_socket_command *command);
int skyuv_socket_command_queue_stop(struct skyuv_socket_command_queue *queue,
									struct skyuv_socket_command *exit_command);
struct skyuv_socket_command *
skyuv_socket_command_queue_pop(struct skyuv_socket_command_queue *queue);
void skyuv_socket_command_destroy(struct skyuv_socket_command *command);

int skyuv_socket_runtime_create(struct skyuv_socket_runtime **runtime);
int skyuv_socket_runtime_submit(struct skyuv_socket_runtime *runtime,
								struct skyuv_socket_command *command);
int skyuv_socket_runtime_listen(struct skyuv_socket_runtime *runtime, const char *host, int port,
								int backlog, uintptr_t opaque, int *id);
int skyuv_socket_runtime_connect(struct skyuv_socket_runtime *runtime, const char *host, int port,
								 uintptr_t opaque, int *id);
int skyuv_socket_runtime_start(struct skyuv_socket_runtime *runtime, int id, uintptr_t opaque);
int skyuv_socket_runtime_pause(struct skyuv_socket_runtime *runtime, int id, uintptr_t opaque);
int skyuv_socket_runtime_nodelay(struct skyuv_socket_runtime *runtime, int id);
int skyuv_socket_runtime_send(struct skyuv_socket_runtime *runtime, int id, void *data, size_t size,
							  enum skyuv_socket_buffer_ownership ownership,
							  void (*release)(void *data));
int skyuv_socket_runtime_close(struct skyuv_socket_runtime *runtime, int id, uintptr_t opaque);
int skyuv_socket_runtime_shutdown(struct skyuv_socket_runtime *runtime, int id, uintptr_t opaque);
enum skyuv_socket_state skyuv_socket_runtime_state(struct skyuv_socket_runtime *runtime, int id);
int skyuv_socket_runtime_exit(struct skyuv_socket_runtime *runtime);
int skyuv_socket_runtime_poll(struct skyuv_socket_runtime *runtime,
							  struct skyuv_socket_event *event);
void skyuv_socket_runtime_release(struct skyuv_socket_runtime **runtime);

uint16_t skyuv_socket_id_slot(int id);
uint16_t skyuv_socket_id_generation(int id);
uint16_t skyuv_socket_id_next_generation(uint16_t generation);
int skyuv_socket_id_make(uint16_t slot, uint16_t generation);
bool skyuv_socket_state_can_receive(enum skyuv_socket_state state);
bool skyuv_socket_state_is_live(enum skyuv_socket_state state);

#endif
