#include "client_module.h"
#include <uv.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

/* stdin 阻塞线程可超过 Lua 状态寿命。为代码与静态队列持有进程级模块引用，
 * 只在首次使用 stdin 时取得，不随 Lua GC 释放；进程退出统一回收。
 * 地址必须指向调用模块内的静态对象。 */
int skyuv_client_module_retain(void *address) {
#ifdef _WIN32
	HMODULE module;
	if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)address, &module))
		return uv_translate_sys_error((int)GetLastError());
#else
	Dl_info info;
	if (dladdr(address, &info) == 0)
		return UV_ENOENT;
	if (dlopen(info.dli_fname, RTLD_NOW | RTLD_LOCAL) == NULL)
		return UV_EIO;
#endif
	return 0;
}
