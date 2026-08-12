#include "socket_internal.h"

uint16_t skyuv_socket_id_slot(int id) {
	return (uint16_t)((uint32_t)id & UINT32_C(0xffff));
}

uint16_t skyuv_socket_id_generation(int id) {
	return (uint16_t)(((uint32_t)id >> SKYUV_SOCKET_SLOT_BITS) & SKYUV_SOCKET_GENERATION_MAX);
}

uint16_t skyuv_socket_id_next_generation(uint16_t generation) {
	generation = (uint16_t)(generation + 1U);
	if (generation == 0U || generation > SKYUV_SOCKET_GENERATION_MAX) {
		generation = 1U;
	}
	return generation;
}

int skyuv_socket_id_make(uint16_t slot, uint16_t generation) {
	uint32_t id;

	if (generation == 0U || generation > SKYUV_SOCKET_GENERATION_MAX) {
		return -1;
	}
	id = ((uint32_t)generation << SKYUV_SOCKET_SLOT_BITS) | slot;
	return (int)id;
}

bool skyuv_socket_state_can_receive(enum skyuv_socket_state state) {
	return state == SKYUV_SOCKET_STATE_LISTENING || state == SKYUV_SOCKET_STATE_CONNECTED;
}

bool skyuv_socket_state_is_live(enum skyuv_socket_state state) {
	return state != SKYUV_SOCKET_STATE_INVALID && state != SKYUV_SOCKET_STATE_CLOSING;
}
