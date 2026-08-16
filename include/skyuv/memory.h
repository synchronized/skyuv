#ifndef SKYUV_MEMORY_H
#define SKYUV_MEMORY_H

#include <stddef.h>

/*
 * 这是进程级内部内存接口，不属于首版公共 SDK。
 * 所有可能跨主程序与动态模块边界的内存都必须使用同一实现分配和释放。
 */
void *skyuv_malloc(size_t size);
void *skyuv_calloc(size_t count, size_t size);
void *skyuv_realloc(void *pointer, size_t size);
void skyuv_free(void *pointer);
void *skyuv_aligned_alloc(size_t alignment, size_t size);

#endif
