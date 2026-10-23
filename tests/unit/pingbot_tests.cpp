#include "test.h"
#include <helpers/PingBot.h>
#include <cstring>

namespace {
struct Ping {
	mesh::Packet packet{};
	uint8_t data[MAX_PACKET_PAYLOAD]{};
	uint8_t reply[PINGBOT_PAYLOAD_SIZE + 1]{};
	Ping() {
		packet.header = ROUTE_TYPE_FLOOD | (PAYLOAD_TYPE_GRP_TXT << PH_TYPE_SHIFT);
		packet.setPathHashSizeAndCount(2, 2);
		packet.path[0] = 0xAB; packet.path[1] = 0xCD;
		packet.path[2] = 0x12; packet.path[3] = 0x34;
		packet._snr = -1;
		memcpy(data + 5, "Alice: !ping", 12);
		reply[PINGBOT_PAYLOAD_SIZE] = 0xA5;
	}
	size_t run(bool enabled = true, size_t len = 32) {
		return pingbotReply(enabled, pingbotChannel(), packet, data, len, -97, "Node", 1234, reply);
	}
	std::string text() { return reinterpret_cast<const char *>(reply + 5); }
};
}

TEST(pingbot_match, "UNIT-PINGBOT-001", "Pingbot matches only enabled test channel pings and preserves wire payload") {
	Ping p;
	CHECK(p.run(false) == 0);
	CHECK(p.run() == 5 + p.text().size());
	CHECK(p.text() == "Node: Pong @Alice RSSI=-97dBm SNR=-0.25dB hops=2 ids=ABCD,1234");
	uint32_t timestamp;
	memcpy(&timestamp, p.reply, 4);
	CHECK(timestamp == 1234 && p.reply[4] == 0);
	CHECK(p.reply[PINGBOT_PAYLOAD_SIZE] == 0xA5);
	for (const char *text : { "Alice: !ping extra", "Alice: !pingbot", "Alice: Pong", "Alice: say !ping" }) {
		memset(p.data + 5, 0, sizeof(p.data) - 5);
		strcpy(reinterpret_cast<char *>(p.data + 5), text);
		CHECK(p.run() == 0);
	}
	p = Ping();
	auto foreign = pingbotChannel(); foreign.secret[31] = 1;
	CHECK(pingbotReply(true, foreign, p.packet, p.data, 32, -97, "Node", 1, p.reply) == 0);
	CHECK(p.run(true, 4) == 0 && p.run(true, 15) == 0);
	p.data[4] = 4; CHECK(p.run() == 0);
	p.data[4] = 0; p.packet.header = ROUTE_TYPE_DIRECT | (PAYLOAD_TYPE_GRP_TXT << PH_TYPE_SHIFT);
	CHECK(p.run() == 0);
}

TEST(pingbot_paths, "UNIT-PINGBOT-002", "Pingbot formats hop widths zero hops and bounded truncated paths") {
	Ping p;
	p.packet.setPathHashSizeAndCount(1, 0);
	CHECK(p.run() > 0 && p.text().find("hops=0 ids=none") != std::string::npos);
	for (unsigned width = 1; width <= 3; ++width) {
		p.packet.setPathHashSizeAndCount(width, 2);
		CHECK(p.run() > 0);
		CHECK(p.text().find(width == 1 ? "ids=AB,CD" : width == 2 ? "ids=ABCD,1234" : "ids=ABCD12,340000") != std::string::npos);
		p.packet.setPathHashSizeAndCount(width, 63 / width);
		CHECK(p.run() <= PINGBOT_PAYLOAD_SIZE - 1);
		CHECK(p.text().find(",...") != std::string::npos);
		CHECK(p.reply[PINGBOT_PAYLOAD_SIZE] == 0xA5);
	}
	p.packet.setPathHashSizeAndCount(3, 63);
	CHECK(p.run() == 0);
}

TEST(pingbot_search, "UNIT-PINGBOT-003", "Pingbot adds public test key without consuming or duplicating configured channels") {
	mesh::GroupChannel channels[4]{};
	uint8_t hash = 0xD9;
	CHECK(pingbotSearch(false, &hash, channels, 0, 4) == 0);
	CHECK(pingbotSearch(true, &hash, channels, 0, 4) == 1);
	const uint8_t key[] = { 0x9C, 0xD8, 0xFC, 0xF2, 0x2A, 0x47, 0x33, 0x3B, 0x59, 0x1D, 0x96, 0xA2, 0xB8, 0x48, 0xB7, 0x3F };
	CHECK(memcmp(channels[0].secret, key, sizeof(key)) == 0);
	CHECK(channels[0].secret[16] == 0);
	CHECK(pingbotSearch(true, &hash, channels, 1, 4) == 1);
	CHECK(pingbotSearch(true, &hash, channels, 4, 4) == 4);
	hash = 0;
	CHECK(pingbotSearch(true, &hash, channels, 0, 4) == 0);
}

TEST(pingbot_addressee, "UNIT-PINGBOT-005", "Pingbot addresses bounded sender names and handles unprefixed pings") {
	Ping p;
	memset(p.data + 5, 0, sizeof(p.data) - 5);
	memcpy(p.data + 5, "Bob: !ping", 10);
	CHECK(p.run() > 0 && p.text().find("Pong @Bob RSSI=") != std::string::npos);
	memset(p.data + 5, 'A', 100);
	memcpy(p.data + 105, ": !ping", 7);
	memset(p.data + 112, 0, sizeof(p.data) - 112);
	p.packet.setPathHashSizeAndCount(1, 63);
	CHECK(p.run(true, sizeof(p.data)) <= PINGBOT_PAYLOAD_SIZE - 1);
	CHECK(p.text().find("Pong @" + std::string(31, 'A') + " RSSI=") != std::string::npos);
	CHECK(p.text().find(",...") != std::string::npos);
	CHECK(p.reply[PINGBOT_PAYLOAD_SIZE] == 0xA5);
	memset(p.data + 5, 0, sizeof(p.data) - 5);
	memcpy(p.data + 5, "!ping", 5);
	CHECK(p.run() > 0 && p.text().find("Pong RSSI=") != std::string::npos);
}
