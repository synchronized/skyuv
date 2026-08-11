#include <skyuv/time.h>

#include <limits.h>

#include <uv.h>

#define NANOSECONDS_PER_SECOND UINT64_C(1000000000)
#define NANOSECONDS_PER_MICROSECOND UINT64_C(1000)

static int timeval_to_nanoseconds(const uv_timeval_t *time, uint64_t *time_ns) {
	uint64_t seconds;
	uint64_t microseconds;

	if (time->tv_sec < 0 || time->tv_usec < 0) {
		return SKYUV_ERROR_SYSTEM;
	}
	seconds = (uint64_t)time->tv_sec;
	microseconds = (uint64_t)time->tv_usec;
	if (seconds > UINT64_MAX / NANOSECONDS_PER_SECOND) {
		return SKYUV_ERROR_SYSTEM;
	}
	*time_ns = seconds * NANOSECONDS_PER_SECOND + microseconds * NANOSECONDS_PER_MICROSECOND;
	return SKYUV_OK;
}

int skyuv_time_monotonic(uint64_t *time_ns) {
	if (time_ns == NULL) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	*time_ns = uv_hrtime();
	return SKYUV_OK;
}

int skyuv_time_realtime(uint64_t *time_ns) {
	uv_timespec64_t time;
	int result;

	if (time_ns == NULL) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	result = uv_clock_gettime(UV_CLOCK_REALTIME, &time);
	if (result != 0 || time.tv_sec < 0 || time.tv_nsec < 0 || time.tv_nsec >= 1000000000) {
		return SKYUV_ERROR_SYSTEM;
	}
	if ((uint64_t)time.tv_sec > UINT64_MAX / NANOSECONDS_PER_SECOND) {
		return SKYUV_ERROR_SYSTEM;
	}
	*time_ns = (uint64_t)time.tv_sec * NANOSECONDS_PER_SECOND + (uint64_t)time.tv_nsec;
	return SKYUV_OK;
}

int skyuv_time_thread_cpu(uint64_t *time_ns) {
	uv_rusage_t usage;
	uint64_t user_ns;
	uint64_t system_ns;
	int result;

	if (time_ns == NULL) {
		return SKYUV_ERROR_INVALID_ARGUMENT;
	}
	result = uv_getrusage_thread(&usage);
	if (result == UV_ENOTSUP) {
		return SKYUV_ERROR_NOT_SUPPORTED;
	}
	if (result != 0 || timeval_to_nanoseconds(&usage.ru_utime, &user_ns) != SKYUV_OK ||
		timeval_to_nanoseconds(&usage.ru_stime, &system_ns) != SKYUV_OK ||
		user_ns > UINT64_MAX - system_ns) {
		return SKYUV_ERROR_SYSTEM;
	}
	*time_ns = user_ns + system_ns;
	return SKYUV_OK;
}
