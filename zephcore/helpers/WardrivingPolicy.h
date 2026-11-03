#pragma once

#include <RoutingPolicy.h>
#include <mesh/Mesh.h>
#include <string.h>

inline const mesh::GroupChannel& wardrivingChannel()
{
	// Public hashtag key: SHA256("#wardriving")[0:16], hash SHA256(key)[0].
	static const mesh::GroupChannel channel = { { 0x81 }, { 0xE3, 0xC2, 0x64, 0x91, 0xE9, 0xCD, 0x32, 0x1E, 0x3A, 0x6B, 0xE5, 0x0D, 0x57, 0xD5, 0x4A, 0xCF } };
	return channel;
}

// Called only after Mesh has authenticated and decrypted the channel packet.
inline bool blockWardriving(uint8_t mode, const mesh::Packet& packet,
	const mesh::GroupChannel& channel)
{
	return packet.isRouteFlood() && mode != 0 &&
		memcmp(channel.secret, wardrivingChannel().secret, sizeof(channel.secret)) == 0 &&
		(mode == 2 || mesh::isUnscopedFlood(&packet));
}
