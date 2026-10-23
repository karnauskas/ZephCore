#include "PingBot.h"
#include <stdio.h>
#include <string.h>

const mesh::GroupChannel& pingbotChannel()
{
	// Public hashtag key: first 16 bytes of SHA256("#test"), then SHA256(key)[0].
	static const mesh::GroupChannel channel = { { 0xD9 }, {
		0x9C, 0xD8, 0xFC, 0xF2, 0x2A, 0x47, 0x33, 0x3B,
		0x59, 0x1D, 0x96, 0xA2, 0xB8, 0x48, 0xB7, 0x3F } };
	return channel;
}

int pingbotSearch(bool enabled, const uint8_t *hash, mesh::GroupChannel *channels,
	int count, int capacity)
{
	const auto& test = pingbotChannel();
	if (!enabled || hash[0] != test.hash[0] || count >= capacity) return count;
	for (int i = 0; i < count; ++i) {
		if (memcmp(channels[i].secret, test.secret, sizeof(test.secret)) == 0) return count;
	}
	channels[count++] = test;
	return count;
}

size_t pingbotReply(bool enabled, const mesh::GroupChannel& channel, const mesh::Packet& packet,
	const uint8_t *data, size_t len, int rssi, const char *name, uint32_t timestamp,
	uint8_t payload[PINGBOT_PAYLOAD_SIZE])
{
	if (!enabled || packet.getPayloadType() != PAYLOAD_TYPE_GRP_TXT || !packet.isRouteFlood() ||
	    memcmp(channel.secret, pingbotChannel().secret, sizeof(channel.secret)) != 0 ||
	    len < 10 || len > MAX_PACKET_PAYLOAD || (data[4] >> 2) != 0 ||
	    !mesh::Packet::isValidPathLen(packet.path_len)) return 0;
	const char *text = reinterpret_cast<const char *>(data + 5);
	size_t text_len = len - 5;
	const char *end = static_cast<const char *>(memchr(text, 0, text_len));
	if (end) text_len = end - text;
	const char *sender = text;
	int sender_len = 0;
	for (size_t i = 0; i + 1 < text_len; ++i) {
		if (text[i] == ':' && text[i + 1] == ' ') {
			sender_len = i > 31 ? 31 : static_cast<int>(i);
			text += i + 2;
			text_len -= i + 2;
			break;
		}
	}
	if (text_len != 5 || memcmp(text, "!ping", 5) != 0) return 0;

	memcpy(payload, &timestamp, 4);
	payload[4] = 0;
	char *reply = reinterpret_cast<char *>(payload + 5);
	constexpr size_t max_text = PINGBOT_PAYLOAD_SIZE - 6;
	int snr = static_cast<int>(packet.getSNR() * 4);
	unsigned magnitude = snr < 0 ? -snr : snr;
	size_t used = snprintf(reply, max_text + 1,
		"%.31s: Pong%s%.*s RSSI=%ddBm SNR=%s%u.%02udB hops=%u ids=",
		name, sender_len ? " @" : "", sender_len, sender, rssi, snr < 0 ? "-" : "", magnitude / 4, (magnitude % 4) * 25,
		packet.getPathHashCount());
	const unsigned count = packet.getPathHashCount(), width = packet.getPathHashSize();
	if (count == 0) {
		strcpy(reply + used, "none");
		used += 4;
	}
	for (unsigned i = 0; i < count; ++i) {
		const size_t required = (i ? 1 : 0) + width * 2;
		if (used + required + (i + 1 < count ? 4 : 0) > max_text) {
			strcpy(reply + used, i ? ",..." : "...");
			used += i ? 4 : 3;
			break;
		}
		if (i) reply[used++] = ',';
		for (unsigned j = 0; j < width; ++j) {
			used += snprintf(reply + used, max_text + 1 - used, "%02X", packet.path[i * width + j]);
		}
	}
	return 5 + used;
}
