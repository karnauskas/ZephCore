#pragma once

#include <mesh/Mesh.h>

constexpr size_t PINGBOT_PAYLOAD_SIZE = 5 + 10 * CIPHER_BLOCK_SIZE + 1;

const mesh::GroupChannel& pingbotChannel();
int pingbotSearch(bool enabled, const uint8_t *hash, mesh::GroupChannel *channels,
	int count, int capacity);
size_t pingbotReply(bool enabled, const mesh::GroupChannel& channel, const mesh::Packet& packet,
	const uint8_t *data, size_t len, int rssi, const char *name, uint32_t timestamp,
	uint8_t payload[PINGBOT_PAYLOAD_SIZE]);
