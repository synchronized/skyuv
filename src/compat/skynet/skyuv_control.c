#include "skyuv_control.h"

#include "skynet_handle.h"
#include "skynet_mq.h"
#include "skynet_server.h"

enum {
	SKYUV_SKYNET_PTYPE_SYSTEM = 4,
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
	message.sz = (size_t)SKYUV_SKYNET_PTYPE_SYSTEM << MESSAGE_TYPE_SHIFT;
	return skynet_context_push(logger, &message) == 0;
}
