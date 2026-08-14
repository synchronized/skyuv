#include "skyuv_control.h"

#include <stddef.h>
#include <stdint.h>

struct skynet_message {
	uint32_t source;
	int session;
	void *data;
	size_t sz;
};

uint32_t skynet_handle_findname(const char *name);
int skynet_context_push(uint32_t handle, struct skynet_message *message);

enum {
	SKYUV_SKYNET_PTYPE_SYSTEM = 4,
	SKYUV_SKYNET_MESSAGE_TYPE_SHIFT = (sizeof(size_t) - 1) * 8,
};

int skyuv_skynet_reopen_log(void) {
	struct skynet_message message;
	uint32_t logger = skynet_handle_findname("logger");

	if (logger == 0) {
		return 0;
	}
	message.source = 0;
	message.session = 0;
	message.data = NULL;
	message.sz = (size_t)SKYUV_SKYNET_PTYPE_SYSTEM << SKYUV_SKYNET_MESSAGE_TYPE_SHIFT;
	return skynet_context_push(logger, &message) == 0;
}
