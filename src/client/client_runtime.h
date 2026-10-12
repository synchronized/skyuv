#ifndef SKYUV_CLIENT_RUNTIME_H
#define SKYUV_CLIENT_RUNTIME_H

#include <uv.h>
#include <stdint.h>

struct skyuv_client_runtime;
typedef int (*skyuv_client_command)(uv_loop_t *loop, void *argument);

/* 同一实例的调用与销毁须串行；命令及其参数在返回前保持有效。
 * 回调在专属线程执行，不得操作 Lua、递归调用或销毁运行时。
 * 回调可创建 handle，但必须安排关闭，销毁会等待全部关闭回调。 */
int skyuv_client_runtime_create(struct skyuv_client_runtime **runtime);
int skyuv_client_runtime_call(struct skyuv_client_runtime *runtime, skyuv_client_command command,
							  void *argument);
/* 接受 NULL；返回后指针置空，允许重复销毁。 */
void skyuv_client_runtime_destroy(struct skyuv_client_runtime **runtime);

/* 仅接受 IPv4 数字地址；返回正整数 ID，错误路径将输出置零。
 * ID 在实例内不复用，不能作为原生 socket 使用。调用与销毁仍须串行。 */
int skyuv_client_runtime_connect(struct skyuv_client_runtime *runtime, const char *address,
								 int port, int64_t *id);
int skyuv_client_runtime_connect_timeout(struct skyuv_client_runtime *runtime, const char *address,
										 int port, int64_t timeout_ms, int64_t *id);
int skyuv_client_runtime_shutdown(struct skyuv_client_runtime *runtime, int64_t id, int read,
								  int write);
int skyuv_client_runtime_ready(struct skyuv_client_runtime *runtime, int64_t id, int *read,
							   int *write);
uint64_t skyuv_client_runtime_revision(struct skyuv_client_runtime *runtime);
void skyuv_client_runtime_wait(struct skyuv_client_runtime *runtime, uint64_t revision,
							   int64_t timeout_ns);
/* 已关闭的正整数 ID 可重复关闭；不存在的 ID 返回 UV_EBADF。 */
int skyuv_client_runtime_close(struct skyuv_client_runtime *runtime, int64_t id);
/* send 等待全部写完成；调用返回前 buffer 必须有效。
 * recv 非阻塞：无数据返回 UV_EAGAIN，缓存读完后 EOF 返回 UV_EOF；实际长度写入 size。 */
int skyuv_client_runtime_send(struct skyuv_client_runtime *runtime, int64_t id, const void *buffer,
							  size_t size);
int skyuv_client_runtime_recv(struct skyuv_client_runtime *runtime, int64_t id, void *buffer,
							  size_t capacity, size_t *size);

#endif
