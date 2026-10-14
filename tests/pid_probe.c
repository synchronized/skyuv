#include "daemon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
	int count = 1;
	int index;
	if (argc != 3) {
		return 2;
	}
	if (strcmp(argv[2], "cycle") == 0) {
		count = 100;
	}
	for (index = 0; index < count; ++index) {
		if (skyuv_pid_acquire(argv[1]) != 0 || skyuv_pid_publish() != 0) {
			return 1;
		}
		if (strcmp(argv[2], "exit") == 0) {
			/* 验证启动后错误退出也经过 atexit 释放，而非依赖正常关闭调用。 */
			return 3;
		}
		if (count == 1) {
			puts("READY");
			fflush(stdout);
			(void)getchar();
		}
		skyuv_pid_release();
		skyuv_pid_release();
	}
	return 0;
}
