#include "test.h"
#include "dispatcher_fixture.h"
#include <helpers/AntennaStats.h>
#include "companion_cli.inc"

namespace {
struct Fixture {
	AntennaStats stats;
	mesh::Packet packet{};
	uint8_t key[PUB_KEY_SIZE]{};
	Fixture() {
		key[0] = 0xAB;
		packet.header = ROUTE_TYPE_FLOOD | (PAYLOAD_TYPE_ADVERT << PH_TYPE_SHIFT);
		packet.setPathHashSizeAndCount(2, 0);
		packet._snr = -1;
		packet.transport_codes[0] = packet.transport_codes[1] = 0;
	}
	std::string text(unsigned index = 0, uint32_t now = 1000) {
		char reply[256];
		stats.format(index, reply, sizeof(reply), now);
		return reply;
	}
	void response(uint32_t tag = 42) {
		packet.header = ROUTE_TYPE_DIRECT | (PAYLOAD_TYPE_CONTROL << PH_TYPE_SHIFT);
		packet.setPathHashSizeAndCount(2, 0);
		packet.payload_len = 6 + PUB_KEY_SIZE;
		packet.payload[0] = 0x90 | ADV_TYPE_REPEATER;
		packet.payload[1] = static_cast<uint8_t>(-31);
		memcpy(packet.payload + 2, &tag, 4);
		memcpy(packet.payload + 6, key, PUB_KEY_SIZE);
	}
};
}

TEST(antenna_wire, "UNIT-ANTENNA-004", "Antenna probes use upstream zero-hop discovery and report pool exhaustion") {
	struct Rng : mesh::RNG {
		void random(uint8_t *dest, size_t len) override { memset(dest, 0x42, len); }
	} rng;
	struct Rtc : mesh::RTCClock {
		uint32_t getCurrentTime() override { return 0; }
		void setCurrentTime(uint32_t) override {}
	} rtc;
	struct Tables : mesh::MeshTables {
		bool wasSeen(const mesh::Packet*) override { return false; }
		void markSeen(const mesh::Packet*) override {}
		void clear(const mesh::Packet*) override {}
	} tables;
	test_support::Clock clock;
	test_support::Radio radio;
	StaticPoolPacketManager pool;
	struct TestMesh : mesh::Mesh {
		TestMesh(mesh::Radio& r, mesh::MillisecondClock& c, mesh::RNG& rng,
			mesh::RTCClock& rtc, mesh::PacketManager& p, mesh::MeshTables& t)
			: mesh::Mesh(r, c, rng, rtc, p, t) {}
	} mesh{radio, clock, rng, rtc, pool, tables};
	Fixture f;
	CHECK(f.stats.probe(mesh, 100));
	CHECK(pool.getOutboundTotal() == 1);
	auto *request = pool.getOutboundByIdx(0);
	CHECK(request && request->isRouteDirect() && request->getPathHashCount() == 0);
	CHECK(request->getPayloadType() == PAYLOAD_TYPE_CONTROL && request->payload_len == 10);
	CHECK(request->payload[0] == 0x80 && request->payload[1] == 4);
	CHECK(request->payload[6] == 0 && request->payload[7] == 0 && request->payload[8] == 0 && request->payload[9] == 0);
	uint32_t tag;
	memcpy(&tag, request->payload + 2, 4);
	f.response(tag);
	f.stats.discovery(f.packet, -97, 200);
	CHECK(f.text().find("TX=1 SNR=-7.75/-7.75/-7.75") != std::string::npos);
	std::vector<mesh::Packet*> held;
	while (auto *packet = pool.allocNew()) held.push_back(packet);
	CHECK(!f.stats.probe(mesh, 300));
	CHECK(pool.getOutboundTotal() == 1);
	for (auto *packet : held) pool.free(packet);
}

TEST(antenna_samples, "UNIT-ANTENNA-001", "Antenna aggregates signed readings and excludes relayed and shared adverts") {
	Fixture f;
	f.stats.advert(f.packet, f.key, -100, 100);
	f.packet._snr = 10;
	f.stats.advert(f.packet, f.key, -90, 200);
	CHECK(f.text(0, 2200).find("age=2s RX=2 RSSI=-100/-95.00/-90(good) SNR=-0.25/1.12/2.50(good) TX=0") != std::string::npos);
	f.packet.setPathHashSizeAndCount(2, 1);
	f.stats.advert(f.packet, f.key, -40, 300);
	CHECK(f.text().find("RX=2") != std::string::npos);
	f.packet.setPathHashSizeAndCount(2, 0);
	f.packet.header = ROUTE_TYPE_TRANSPORT_FLOOD | (PAYLOAD_TYPE_ADVERT << PH_TYPE_SHIFT);
	f.stats.advert(f.packet, f.key, -40, 300);
	CHECK(f.text().find("RX=2") != std::string::npos);
	f.packet.transport_codes[0] = 123;
	f.stats.advert(f.packet, f.key, -80, 300);
	CHECK(f.text().find("RX=3 RSSI=-100/-90.00/-80") != std::string::npos);
}

TEST(antenna_probe, "UNIT-ANTENNA-002", "Antenna accepts one full-key direct discovery response per probe within its window") {
	Fixture f;
	f.response();
	f.stats.discovery(f.packet, -97, 100);
	CHECK(f.text().find("0 stored") != std::string::npos);
	f.stats.startProbe(41, 0);
	f.stats.discovery(f.packet, -97, 100);
	CHECK(f.text().find("0 stored") != std::string::npos);
	f.stats.startProbe(42, UINT32_MAX - 100);
	f.packet.payload_len = 6 + 8;
	f.stats.discovery(f.packet, -97, 100);
	CHECK(f.text().find("0 stored") != std::string::npos);
	f.packet.payload_len = 6 + PUB_KEY_SIZE;
	f.packet.setPathHashSizeAndCount(2, 1);
	f.stats.discovery(f.packet, -97, 100);
	CHECK(f.text().find("0 stored") != std::string::npos);
	f.packet.setPathHashSizeAndCount(2, 0);
	f.stats.discovery(f.packet, -97, 100);
	CHECK(f.text().find("RX=1 RSSI=-97/-97.00/-97(good) SNR=-0.25/-0.25/-0.25(fair) TX=1 SNR=-7.75/-7.75/-7.75(weak)") != std::string::npos);
	f.stats.discovery(f.packet, -40, 200);
	CHECK(f.text().find("RX=1") != std::string::npos);
	f.response(43);
	f.stats.startProbe(43, 1000);
	f.stats.discovery(f.packet, -90, 60999);
	CHECK(f.text(0, 60999).find("RX=2") != std::string::npos);
	f.response(44);
	f.stats.startProbe(44, 1000);
	f.stats.discovery(f.packet, -40, 61000);
	CHECK(f.text().find("RX=2") != std::string::npos);
}

TEST(antenna_bounds, "UNIT-ANTENNA-003", "Antenna bounds storage by full identity and formats and resets safely") {
	Fixture f;
	for (unsigned i = 0; i < 9; ++i) {
		f.key[31] = i;
		f.stats.advert(f.packet, f.key, -100, i);
	}
	CHECK(f.text().find("08 age=") != std::string::npos);
	CHECK(f.text(7).find("07 age=") != std::string::npos);
	CHECK(f.text(8).find("8 stored") != std::string::npos);
	char reply[256];
	CHECK(f.stats.command("antenna", reply, sizeof(reply), 100));
	CHECK(std::string(reply).find("8/8 neighbours") != std::string::npos);
	for (const char *bad : { "antenna -1", "antenna 8", "antenna 1extra", "antenna " }) {
		CHECK(f.stats.command(bad, reply, sizeof(reply), 100));
		CHECK(std::string(reply).find("Usage:") == 0);
	}
	CHECK(f.stats.command("antenna 0", reply, sizeof(reply), 100));
	CHECK(std::string(reply) == f.text(0, 100));
	for (size_t size : { 0u, 1u, 20u, 161u, 256u }) {
		memset(reply, 0xA5, sizeof(reply));
		f.stats.format(0, reply, size, 100);
		if (size) CHECK(memchr(reply, 0, size) != nullptr);
		if (size < sizeof(reply)) CHECK(static_cast<unsigned char>(reply[size]) == 0xA5);
	}
	f.stats.startProbe(42, 0);
	CHECK(f.stats.command("antenna reset", reply, sizeof(reply), 100));
	f.response();
	f.stats.discovery(f.packet, -97, 100);
	CHECK(f.text().find("0 stored") != std::string::npos);
}

TEST(antenna_remote, "UNIT-ANTENNA-005", "Remote antenna commands pass the companion allowlist and fit mesh replies") {
	for (const char *command : { "antenna", "antenna 0", "antenna probe", "antenna reset" }) {
		CHECK(isRemoteCompanionCommand(command));
	}
	for (const char *command : { "antennas", "antenna.reset", "start ota", "erase", "get prv.key" }) {
		CHECK(!isRemoteCompanionCommand(command));
	}
	Fixture f;
	f.response();
	f.stats.startProbe(42, 0);
	f.stats.discovery(f.packet, -97, 100);
	char reply[162];
	memset(reply, 0xA5, sizeof(reply));
	CHECK(f.stats.command("antenna 0", reply, 161, 200));
	CHECK(static_cast<unsigned char>(reply[161]) == 0xA5);
	CHECK(std::string(reply) == "0 AB000000 RX=1 RSSI=-97/-97.00/-97(good) SNR=-0.25/-0.25/-0.25(fair) TX=1 SNR=-7.75/-7.75/-7.75(weak)");
	CHECK(f.text().find(std::string(62, '0')) != std::string::npos);
	CHECK(f.stats.command("antenna reset", reply, 161, 200));
	CHECK(f.text().find("0 stored") != std::string::npos);
}

TEST(antenna_quality, "UNIT-ANTENNA-006", "Antenna labels classify means on the ping scale and keep mesh readings complete") {
	const struct { int rssi, snr; const char *rssi_label, *snr_label; } levels[] = {
		{ -70, 40, "excellent", "excellent" },
		{ -71, 39, "good", "good" },
		{ -100, 0, "good", "good" },
		{ -101, -1, "fair", "fair" },
		{ -110, -30, "fair", "fair" },
		{ -111, -31, "weak", "weak" },
		{ -120, -60, "weak", "weak" },
		{ -121, -61, "poor", "poor" },
	};
	for (const auto& level : levels) {
		Fixture f;
		f.packet._snr = level.snr;
		f.stats.advert(f.packet, f.key, level.rssi, 0);
		f.response();
		f.packet.payload[1] = static_cast<uint8_t>(level.snr);
		f.stats.startProbe(42, 0);
		f.stats.discovery(f.packet, level.rssi, 100);
		char reply[162];
		memset(reply, 0xA5, sizeof(reply));
		f.stats.format(0, reply, 161, 200);
		std::string text(reply);
		CHECK(text.find(std::string("(") + level.rssi_label + ") SNR=") != std::string::npos);
		CHECK(text.find(std::string("(") + level.snr_label + ") TX=1 SNR=") != std::string::npos);
		CHECK(text.substr(text.size() - strlen(level.snr_label) - 2) == std::string("(") + level.snr_label + ")");
		CHECK(static_cast<unsigned char>(reply[161]) == 0xA5);
	}
	Fixture f;
	f.packet._snr = 0;
	f.stats.advert(f.packet, f.key, -100, 0);
	f.packet._snr = -1;
	f.stats.advert(f.packet, f.key, -101, 100);
	CHECK(f.text().find("RSSI=-101/-100.50/-100(fair)") != std::string::npos);
	CHECK(f.text().find("(fair) TX=0") != std::string::npos);
}
