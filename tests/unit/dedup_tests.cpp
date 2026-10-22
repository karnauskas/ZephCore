#include "test.h"
#include <mesh/SimpleMeshTables.h>

namespace {
mesh::Packet packet(uint16_t id, uint8_t type = PAYLOAD_TYPE_TXT_MSG) {
	mesh::Packet p{};
	p.header = (type << 2) | ROUTE_TYPE_FLOOD;
	p.payload_len = 2;
	p.payload[0] = uint8_t(id);
	p.payload[1] = uint8_t(id >> 8);
	return p;
}
}

TEST(dedup_clear, "UNIT-DEDUP-001", "Repeated marking clears completely without counting duplicate receives") {
	SimpleMeshTables table;
	auto p = packet(1);
	table.markSeen(&p);
	table.markSeen(&p);
	CHECK(table.getNumFloodDups() == 0 && table.getNumDirectDups() == 0);
	table.clear(&p);
	CHECK(!table.wasSeen(&p));
	table.markSeen(&p);
	CHECK(table.wasSeen(&p));
}

TEST(dedup_capacity, "UNIT-DEDUP-002", "Distinct packet FIFO is bounded and repeated marks neither evict nor refresh") {
	SimpleMeshTables table;
	for (unsigned i = 0; i < MAX_PACKET_HASHES; ++i) {
		auto p = packet(i);
		table.markSeen(&p);
	}
	auto oldest = packet(0);
	for (unsigned i = 0; i < MAX_PACKET_HASHES * 2; ++i) table.markSeen(&oldest);
	for (unsigned i = 0; i < MAX_PACKET_HASHES; ++i) {
		auto p = packet(i);
		CHECK(table.wasSeen(&p));
	}
	auto next = packet(MAX_PACKET_HASHES);
	table.markSeen(&next);
	CHECK(!table.wasSeen(&oldest));
	CHECK(table.wasSeen(&next));
	for (unsigned i = 1; i < MAX_PACKET_HASHES; ++i) {
		auto p = packet(i);
		CHECK(table.wasSeen(&p));
	}
}

TEST(dedup_identity, "UNIT-DEDUP-003", "Changed payloads pass while route copies and identical ACKs deduplicate") {
	SimpleMeshTables table;
	auto p = packet(1);
	table.markSeen(&p);
	p.header = (PAYLOAD_TYPE_TXT_MSG << 2) | ROUTE_TYPE_DIRECT;
	p.path_len = 1;
	p.path[0] = 42;
	CHECK(table.wasSeen(&p));
	p.payload[0] = 2;
	CHECK(!table.wasSeen(&p));
	auto ack = packet(1, PAYLOAD_TYPE_ACK);
	CHECK(!table.wasSeen(&ack));
	table.markSeen(&ack);
	CHECK(table.wasSeen(&ack));
	CHECK(table.getNumDirectDups() == 1 && table.getNumFloodDups() == 1);
	auto trace = packet(1, PAYLOAD_TYPE_TRACE);
	trace.header = (PAYLOAD_TYPE_TRACE << 2) | ROUTE_TYPE_DIRECT;
	table.markSeen(&trace);
	++trace.path_len;
	CHECK(!table.wasSeen(&trace));
}

TEST(dedup_reboot, "UNIT-DEDUP-004", "New table accepts pre-reboot packets and resetting stats preserves suppression") {
	auto p = packet(1);
	SimpleMeshTables before;
	before.markSeen(&p);
	CHECK(before.wasSeen(&p));
	before.resetStats();
	CHECK(before.getNumFloodDups() == 0);
	CHECK(before.wasSeen(&p));
	SimpleMeshTables after;
	CHECK(!after.wasSeen(&p));
	after.clear(&p);
	after.markSeen(&p);
	CHECK(after.wasSeen(&p));
}
