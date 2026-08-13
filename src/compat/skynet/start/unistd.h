#ifndef SKYUV_COMPAT_SKYNET_START_UNISTD_H
#define SKYUV_COMPAT_SKYNET_START_UNISTD_H

#ifndef _WIN32

#include_next <unistd.h>

#else

#include <process.h>
#include <stdlib.h>
#include <uv.h>

typedef int pid_t;

static pid_t skyuv_compat_getpid(void) {
	return _getpid();
}

static unsigned int skyuv_compat_sleep(unsigned int seconds) {
	uv_sleep(seconds * 1000U);
	return 0;
}

static int skyuv_compat_usleep(unsigned int microseconds) {
	uv_sleep((microseconds + 999U) / 1000U);
	return 0;
}

static long skyuv_compat_random(void) {
	return (long)rand();
}

static void skyuv_compat_srandom(unsigned int seed) {
	srand(seed);
}

#define sleep skyuv_compat_sleep
#define usleep skyuv_compat_usleep
#define getpid skyuv_compat_getpid
#define random skyuv_compat_random
#define srandom skyuv_compat_srandom

#endif

#endif
