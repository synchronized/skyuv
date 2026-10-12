#include "client_shutdown.h"
#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <errno.h>
#endif

int skyuv_client_shutdown_read(uv_tcp_t *tcp) {
	uv_os_fd_t fd;
	int result = uv_fileno((uv_handle_t *)tcp, &fd);
	if (result != 0)
		return result;
#ifdef _WIN32
	if (shutdown((SOCKET)fd, SD_RECEIVE) != 0)
		return uv_translate_sys_error(WSAGetLastError());
#else
	if (shutdown(fd, SHUT_RD) != 0)
		return uv_translate_sys_error(errno);
#endif
	return 0;
}
