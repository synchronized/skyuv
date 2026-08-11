#ifndef SKYUV_DYNAMIC_H
#define SKYUV_DYNAMIC_H

#include <stddef.h>

#include <skyuv/error.h>

typedef struct skyuv_dl {
	void *implementation;
} skyuv_dl;

#define SKYUV_DL_INITIALIZER {NULL}

/* 打开失败时仍会保留错误信息，调用方读取后必须调用 skyuv_dlclose。 */
int skyuv_dlopen(skyuv_dl *library, const char *path);
int skyuv_dlsym(skyuv_dl *library, const char *name, void **symbol);
void skyuv_dlclose(skyuv_dl *library);
/* 返回的文本由 library 持有，下次符号查询或关闭后失效。 */
const char *skyuv_dlerror(const skyuv_dl *library);

#endif
