#include "daemon.h"
#include "skynet_daemon.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* .lock 不删除：保持协调锁 inode 稳定，避免 PID 删除与并发 open 形成两把锁。 */
static int guard_fd = -1;
static int pid_fd = -1;
static char *pid_path;
static int owns_pid;
static int exit_registered;

void skyuv_pid_release(void) {
	if (pid_fd >= 0) {
		struct stat held;
		struct stat current;
		if (owns_pid && fstat(pid_fd, &held) == 0 && lstat(pid_path, &current) == 0 &&
			held.st_dev == current.st_dev && held.st_ino == current.st_ino) {
			if (unlink(pid_path) != 0 && errno != ENOENT) {
				fprintf(stderr, "skyuv: 无法删除 PID 文件：%s（%s）\n", pid_path, strerror(errno));
			}
		}
		close(pid_fd);
		pid_fd = -1;
	}
	free(pid_path);
	pid_path = NULL;
	owns_pid = 0;
	if (guard_fd >= 0) {
		close(guard_fd);
		guard_fd = -1;
	}
}

static int lock_file(const char *path) {
	struct stat info;
	int fd = open(path, O_RDWR | O_CREAT | O_NOFOLLOW, 0644);
	if (fd < 0) {
		return -1;
	}
	if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)) {
		close(fd);
		errno = EINVAL;
		return -1;
	}
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0 || flock(fd, LOCK_EX | LOCK_NB) < 0) {
		int error = errno;
		close(fd);
		errno = error;
		return -1;
	}
	return fd;
}

int skyuv_pid_acquire(const char *pidfile) {
	char *guard;
	char buffer[64];
	ssize_t size;
	long previous;
	char *end;
	size_t length;
	if (pid_fd >= 0 || guard_fd >= 0 || pidfile == NULL || pidfile[0] == '\0') {
		fprintf(stderr, "skyuv: PID 状态或路径无效\n");
		return 1;
	}
	length = strlen(pidfile);
	guard = malloc(length + sizeof(".lock"));
	pid_path = malloc(length + 1);
	if (guard == NULL || pid_path == NULL) {
		free(guard);
		skyuv_pid_release();
		return 1;
	}
	memcpy(pid_path, pidfile, length + 1);
	memcpy(guard, pidfile, length);
	memcpy(guard + length, ".lock", sizeof(".lock"));
	guard_fd = lock_file(guard);
	free(guard);
	if (guard_fd < 0) {
		fprintf(stderr, "skyuv: 无法锁定 PID 协调文件：%s（%s）\n", pid_path, strerror(errno));
		skyuv_pid_release();
		return 1;
	}
	pid_fd = lock_file(pid_path);
	if (pid_fd < 0) {
		fprintf(stderr, "skyuv: 无法锁定 PID 文件：%s（%s）\n", pid_path, strerror(errno));
		skyuv_pid_release();
		return 1;
	}
	do {
		size = read(pid_fd, buffer, sizeof(buffer) - 1);
	} while (size < 0 && errno == EINTR);
	if (size < 0) {
		fprintf(stderr, "skyuv: 无法读取 PID 文件：%s（%s）\n", pid_path, strerror(errno));
		skyuv_pid_release();
		return 1;
	}
	buffer[(size_t)size] = '\0';
	errno = 0;
	previous = strtol(buffer, &end, 10);
	if (errno == 0 && end != buffer && previous > 0 && (long)(pid_t)previous == previous &&
		(kill((pid_t)previous, 0) == 0 || errno != ESRCH)) {
		fprintf(stderr, "skyuv: PID 文件对应的进程仍存在：%s（PID %ld）\n", pid_path, previous);
		skyuv_pid_release();
		return 1;
	}
	owns_pid = 1;
	if (!exit_registered) {
		if (atexit(skyuv_pid_release) != 0) {
			skyuv_pid_release();
			return 1;
		}
		exit_registered = 1;
	}
	return 0;
}

int skyuv_pid_publish(void) {
	char buffer[64];
	int length;
	size_t offset = 0;
	if (pid_fd < 0 || !owns_pid) {
		return 1;
	}
	length = snprintf(buffer, sizeof(buffer), "%ld\n", (long)getpid());
	if (length <= 0 || (size_t)length >= sizeof(buffer) || ftruncate(pid_fd, 0) != 0 ||
		lseek(pid_fd, 0, SEEK_SET) < 0) {
		goto failure;
	}
	while (offset < (size_t)length) {
		ssize_t written = write(pid_fd, buffer + offset, (size_t)length - offset);
		if (written < 0 && errno == EINTR) {
			continue;
		}
		if (written <= 0) {
			goto failure;
		}
		offset += (size_t)written;
	}
	if (fsync(pid_fd) != 0) {
		goto failure;
	}
	return 0;

failure:
	fprintf(stderr, "skyuv: 无法写入 PID 文件：%s（%s）\n", pid_path, strerror(errno));
	skyuv_pid_release();
	return 1;
}

static int redirect_fds(void) {
	int fd = open("/dev/null", O_RDWR);
	int result = 0;
	int target;
	if (fd < 0) {
		return 1;
	}
	for (target = 0; target <= 2; ++target) {
		if (dup2(fd, target) < 0) {
			result = 1;
			break;
		}
	}
	if (fd > 2) {
		close(fd);
	}
	return result;
}

int daemon_init(const char *pidfile) {
	if (skyuv_pid_acquire(pidfile) != 0) {
		return 1;
	}
#ifndef __APPLE__
	/* 锁在 fork 前取得，父进程由 daemon 内部 _exit，子进程继承锁直到退出。 */
	if (daemon(1, 1) != 0) {
		fprintf(stderr, "skyuv: daemon 启动失败：%s\n", strerror(errno));
		skyuv_pid_release();
		return 1;
	}
#else
	fprintf(stderr, "skyuv: macOS daemon 不 fork，推荐使用 launchd\n");
#endif
	if (skyuv_pid_publish() != 0 || redirect_fds() != 0) {
		skyuv_pid_release();
		return 1;
	}
	return 0;
}

int daemon_exit(const char *pidfile) {
	(void)pidfile;
	skyuv_pid_release();
	return 0;
}
