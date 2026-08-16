#include "socket_info.h"

#include <skyuv/memory.h>

#include <stdlib.h>

struct socket_info *socket_info_create(struct socket_info *last) {
	struct socket_info *info = skyuv_calloc(1, sizeof(*info));

	if (last != NULL) {
		last->next = info;
	}
	return info;
}

void socket_info_release(struct socket_info *info) {
	while (info != NULL) {
		struct socket_info *next = info->next;

		skyuv_free(info);
		info = next;
	}
}
