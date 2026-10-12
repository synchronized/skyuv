#ifndef SKYUV_CLIENT_RUNTIME_H
#define SKYUV_CLIENT_RUNTIME_H

#include <uv.h>

struct skyuv_client_runtime;
typedef int (*skyuv_client_command)(uv_loop_t *loop, void *argument);

/* 同一实例的调用与销毁须串行；命令及其参数在返回前保持有效。
 * 回调在专属线程执行，不得操作 Lua、递归调用或销毁运行时。
 * 回调可创建 handle，但必须安排关闭，销毁会等待全部关闭回调。 */
int skyuv_client_runtime_create(struct skyuv_client_runtime **runtime);
int skyuv_client_runtime_call(struct skyuv_client_runtime *runtime,
	skyuv_client_command command, void *argument);
/* 接受 NULL；返回后指针置空，允许重复销毁。 */
void skyuv_client_runtime_destroy(struct skyuv_client_runtime **runtime);

#endif
