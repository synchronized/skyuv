#ifndef SKYUV_TIME_H
#define SKYUV_TIME_H

#include <stdint.h>

#include <skyuv/error.h>

int skyuv_time_monotonic(uint64_t *time_ns);
int skyuv_time_realtime(uint64_t *time_ns);
int skyuv_time_thread_cpu(uint64_t *time_ns);

#endif
