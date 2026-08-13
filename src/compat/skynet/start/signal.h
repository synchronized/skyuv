#ifndef SKYUV_COMPAT_SKYNET_START_SIGNAL_H
#define SKYUV_COMPAT_SKYNET_START_SIGNAL_H

#ifndef _WIN32

#include_next <signal.h>

#else

#define SIG_IGN ((void (*)(int))1)
#define SIGHUP 1
#define SIGPIPE 13
#define SA_RESTART 0

typedef int sigset_t;
typedef int sig_atomic_t;

struct sigaction {
	void (*sa_handler)(int);
	int sa_flags;
	sigset_t sa_mask;
};

static int sigfillset(sigset_t *set) {
	*set = 0;
	return 0;
}

static int sigemptyset(sigset_t *set) {
	*set = 0;
	return 0;
}

static int sigaction(int signal_number, const struct sigaction *action,
					 struct sigaction *previous) {
	(void)signal_number;
	(void)action;
	(void)previous;
	return 0;
}

#endif

#endif
