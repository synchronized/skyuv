#ifndef SKYUV_COMPAT_SKYNET_TIME_H
#define SKYUV_COMPAT_SKYNET_TIME_H

#include <stdint.h>
#include <time.h>

#include <skyuv/time.h>

static inline int skyuv_compat_clock_gettime(int clock_id, struct timespec *time) {
	uint64_t time_ns;
	int result;

	if (clock_id == CLOCK_REALTIME) {
		result = skyuv_time_realtime(&time_ns);
	} else if (clock_id == CLOCK_MONOTONIC) {
		result = skyuv_time_monotonic(&time_ns);
	} else if (clock_id == CLOCK_THREAD_CPUTIME_ID) {
		result = skyuv_time_thread_cpu(&time_ns);
	} else {
		return -1;
	}
	if (result != SKYUV_OK) {
		return -1;
	}
	time->tv_sec = (time_t)(time_ns / UINT64_C(1000000000));
	time->tv_nsec = (long)(time_ns % UINT64_C(1000000000));
	return 0;
}

#define clock_gettime skyuv_compat_clock_gettime

#endif
