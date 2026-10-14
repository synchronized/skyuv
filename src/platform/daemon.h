#ifndef SKYUV_PLATFORM_DAEMON_H
#define SKYUV_PLATFORM_DAEMON_H

/* 仅用于 Unix 入口和生命周期测试；入口线程独占，重复释放安全。 */
int skyuv_pid_acquire(const char *pidfile);
int skyuv_pid_publish(void);
void skyuv_pid_release(void);

#endif
