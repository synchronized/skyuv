#ifndef SKYUV_COMPAT_SKYNET_ALLOCA_H
#define SKYUV_COMPAT_SKYNET_ALLOCA_H

#if defined(_MSC_VER)
#include <malloc.h>
#define alloca _alloca
#else
#include <alloca.h>
#endif

#endif
