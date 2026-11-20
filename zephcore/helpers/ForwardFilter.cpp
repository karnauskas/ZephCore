/* SPDX-License-Identifier: MIT */
#include "ForwardFilter.h"
#include <mesh/Utils.h>

bool allowFilteredForward(const mesh::Packet *packet, const ForwardFilters &filters)
{
	if (filters.one_byte && packet->isRouteFlood() && packet->getPathHashSize() == 1 &&
	    packet->getPathHashCount() > 0) return false;
	uint8_t type = packet->getPayloadType();
	if (filters.data && (type == PAYLOAD_TYPE_GRP_DATA || type >= 0x0C)) return false;
	if (!filters.public_channel || (type != PAYLOAD_TYPE_GRP_TXT && type != PAYLOAD_TYPE_GRP_DATA) ||
	    packet->payload_len < 1 + CIPHER_MAC_SIZE + CIPHER_BLOCK_SIZE || packet->payload[0] != 0x11) return true;
	/* Public's published 16-byte key, zero-padded for MeshCore's 32-byte HMAC key. */
	static const uint8_t public_key[PUB_KEY_SIZE] = {
		0x8b, 0x33, 0x87, 0xe9, 0xc5, 0xcd, 0xea, 0x6a,
		0xc9, 0xe5, 0xed, 0xba, 0xa1, 0x15, 0xcd, 0x72,
	};
	return !mesh::Utils::verifyMAC(public_key, packet->payload + 1, packet->payload_len - 1);
}
