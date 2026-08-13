#include "skynet_daemon.h"

int daemon_init(const char *pidfile) {
	/* Windows 和首期跨平台目标不支持 daemon 模式。 */
	(void)pidfile;
	return 1;
}

int daemon_exit(const char *pidfile) {
	(void)pidfile;
	return 0;
}
