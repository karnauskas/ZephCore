#include "test.h"
#include <helpers/PrivacyEngine.h>
#include <helpers/MultiSerialInterface.h>
#include <mesh/Packet.h>
#include <cmath>
#include <cstring>
#include "dispatcher_fixture.h"

namespace {
using Position = PrivacyEngine::Position;
constexpr int64_t DAY = 1735689600; // 2025-01-01 UTC
bool cmd(PrivacyPrefs &p, const char *s)
{
	char reply[256];
	CHECK(PrivacyEngine::command(p, s, reply, sizeof(reply)));
	return !strcmp(reply, "OK");
}
}

TEST(privacy_zones, "UNIT-PRIVACY-001", "Zones suppress true coordinates before fuzzing and include the dateline and poles") {
	PrivacyPrefs p{};
	PrivacyEngine e;
	CHECK(cmd(p, "privacy zone add home 55.70 21.15 500"));
	CHECK(cmd(p, "privacy zone home action suppress"));
	CHECK(cmd(p, "privacy zone home apply all"));
	CHECK(cmd(p, "privacy gps fuzz 1000"));
	CHECK(PrivacyEngine::valid(p));
	Position home{55.7, 21.15, 40};
	CHECK(!e.filter(p, home, 0, DAY, 2));
	CHECK(cmd(p, "privacy zone delete home"));
	CHECK(cmd(p, "privacy zone add date 0 179.999 500"));
	CHECK(e.blocked(p, {0, -179.999, 0}, DAY, 0));
	CHECK(!e.blocked(p, {0, -179.99, 0}, DAY, 0));
	CHECK(cmd(p, "privacy zone add pole 90 0 500"));
	CHECK(e.blocked(p, {90, 170, 0}, DAY, 0));
	CHECK(e.blocked(p, {NAN, 0, 0}, DAY, 0));
}

TEST(privacy_schedule, "UNIT-PRIVACY-002", "Blackouts use configured timezone with inclusive start exclusive end and unknown-clock suppression") {
	PrivacyPrefs p{};
	PrivacyEngine e;
	CHECK(cmd(p, "privacy gps schedule 22:00 07:00"));
	CHECK(!e.blocked(p, {1, 2, 0}, DAY + 19 * 3600 + 59 * 60, 2));
	CHECK(e.blocked(p, {1, 2, 0}, DAY + 20 * 3600, 2));
	CHECK(e.blocked(p, {1, 2, 0}, DAY + 4 * 3600 + 59 * 60, 2));
	CHECK(!e.blocked(p, {1, 2, 0}, DAY + 5 * 3600, 2));
	CHECK(e.blocked(p, {1, 2, 0}, 0, -12));
	CHECK(cmd(p, "privacy gps schedule 08:30 17:00"));
	CHECK(e.blocked(p, {1, 2, 0}, DAY, 14));
	CHECK(!e.blocked(p, {1, 2, 0}, DAY + 5 * 3600, -12));
	CHECK(cmd(p, "privacy gps schedule off"));
	CHECK(!e.blocked(p, {1, 2, 0}, 0, 0));
}

TEST(privacy_delay, "UNIT-PRIVACY-003", "Bounded delay retains old snapshots under frequent fixes and clears on zone entry or reboot") {
	PrivacyPrefs p{};
	PrivacyEngine e;
	CHECK(cmd(p, "privacy gps delay 1800"));
	for (uint64_t t = 0; t < 1800000; t += 10000) {
		e.observe(p, {1, 2 + t / 1e9, 3}, t);
		Position pos{1, 2 + t / 1e9, 3};
		CHECK(!e.filter(p, pos, t, DAY, 0));
	}
	Position pos{1, 2.1, 4};
	CHECK(e.filter(p, pos, 1800000, DAY, 0));
	CHECK(pos.lon == 2 && pos.altitude == 3);
	for (uint64_t t = 1810000; t < 10000000; t += 10000) {
		e.observe(p, {1, 2 + t / 1e9, 3}, t);
		pos = {1, 2 + t / 1e9, 3};
		CHECK(e.filter(p, pos, t, DAY, 0));
		CHECK(pos.lon <= 2 + (t - 1800000) / 1e9);
	}
	uint32_t gen = e.generation();
	CHECK(cmd(p, "privacy zone add home 1 2 500"));
	e.observe(p, {1, 2, 0}, 10000000);
	CHECK(e.generation() != gen);
	pos = {2, 3, 4};
	CHECK(!e.filter(p, pos, 10000001, DAY, 0));
	e.reset(123);
	CHECK(!e.filter(p, pos, 12000000, DAY, 0));
	CHECK(cmd(p, "privacy off"));
	CHECK(cmd(p, "privacy gps delay 60"));
	e.reset(0);
	pos = {1, 2, 3};
	CHECK(!e.filter(p, pos, UINT32_MAX - 30000, DAY, 0));
	CHECK(!e.filter(p, pos, 29998, DAY, 0));
	CHECK(e.filter(p, pos, 29999, DAY, 0));
	CHECK(cmd(p, "privacy gps schedule 22:00 07:00"));
	gen = e.generation();
	CHECK(!e.filter(p, pos, 30000, DAY + 22 * 3600, 0));
	CHECK(e.generation() != gen);
	CHECK(!e.filter(p, pos, 30001, DAY + 86400 + 7 * 3600, 0));
}

TEST(privacy_transform, "UNIT-PRIVACY-004", "Fuzz is boot-stable and geodesic precision removes altitude and movement releases only after threshold") {
	PrivacyPrefs p{};
	PrivacyEngine e;
	e.reset(123456);
	CHECK(cmd(p, "privacy gps fuzz 1000"));
	Position raw{55.7, 21.15, 50}, a = raw, b = raw;
	CHECK(e.filter(p, a, 0, DAY, 0));
	CHECK(std::abs(PrivacyEngine::distance(raw, a) - 1000) < 0.01);
	CHECK(a.altitude == 0);
	CHECK(e.filter(p, b, 1, DAY, 0));
	CHECK(a.lat == b.lat && a.lon == b.lon);
	CHECK(cmd(p, "privacy gps precision 1000"));
	a = {89.9999, 179.99999, 42};
	CHECK(e.filter(p, a, 2, DAY, 0));
	CHECK(std::isfinite(a.lat) && a.lat >= -90 && a.lat <= 90);
	CHECK(std::isfinite(a.lon) && a.lon >= -180 && a.lon < 180 && a.altitude == 0);
	CHECK(cmd(p, "privacy off"));
	CHECK(cmd(p, "privacy gps movement 500"));
	e.reset(0);
	a = raw;
	CHECK(e.filter(p, a, 3, DAY, 0));
	a = {55.701, 21.15, 50};
	CHECK(!e.filter(p, a, 4, DAY, 0));
	a = {55.706, 21.15, 50};
	CHECK(e.filter(p, a, 5, DAY, 0));
}

TEST(privacy_commands, "UNIT-PRIVACY-005", "CLI bounds zone names capacity schedules and numeric values without echoing private coordinates") {
	PrivacyPrefs p{};
	char reply[256];
	CHECK(!PrivacyEngine::command(p, "private", reply, sizeof(reply)));
	CHECK(!cmd(p, "privacy zone add home nan 21 500"));
	CHECK(!cmd(p, "privacy zone add home 91 21 500"));
	CHECK(!cmd(p, "privacy zone add home 55 21 0"));
	CHECK(!cmd(p, "privacy zone add toolongzonename 55 21 500"));
	for (const char *s : {"privacy gps fuzz -1", "privacy gps fuzz 100001", "privacy gps delay 86401",
		"privacy gps fuzz 1.5", "privacy gps fuzz 1x", "privacy gps schedule 25:00 07:00",
		"privacy gps schedule 22:00 22:00", "privacy gps fuzz 1 trailing"}) CHECK(!cmd(p, s));
	for (const char *s : {"privacy zone add a 55 21 500", "privacy zone add b 55 21 500",
		"privacy zone add c 55 21 500", "privacy zone add d 55 21 500"}) CHECK(cmd(p, s));
	CHECK(!cmd(p, "privacy zone add e 55 21 500"));
	CHECK(cmd(p, "privacy zone add a 56 22 1000"));
	CHECK(!cmd(p, "privacy zone a action allow"));
	CHECK(!cmd(p, "privacy zone a apply advert"));
	CHECK(PrivacyEngine::command(p, "privacy status", reply, sizeof(reply)));
	CHECK(!strstr(reply, "55") && !strstr(reply, "21"));
	CHECK(PrivacyEngine::command(p, "privacy zone list", reply, sizeof(reply)));
	CHECK(!strcmp(reply, "Zones: a b c d"));
	p.zones[0].radius_m = 100001;
	PrivacyEngine e;
	CHECK(e.blocked(p, {0, 0, 0}, DAY, 0));
}

TEST(privacy_metadata, "UNIT-PRIVACY-006", "Local privacy tags do not change wire bytes or packet hashes") {
	mesh::Packet p;
	p.header = (PAYLOAD_TYPE_ADVERT << PH_TYPE_SHIFT) | ROUTE_TYPE_FLOOD;
	p.payload_len = 2; p.payload[0] = 1; p.payload[1] = 2;
	uint8_t before[256], after[256], h1[MAX_HASH_SIZE], h2[MAX_HASH_SIZE];
	int len = p.writeTo(before);
	p.calculatePacketHash(h1);
	p.privacy_generation = 123456;
	CHECK(p.writeTo(after) == len && !memcmp(before, after, len));
	p.calculatePacketHash(h2);
	CHECK(!memcmp(h1, h2, sizeof(h1)));
}

TEST(privacy_queue, "UNIT-PRIVACY-007", "Transmit policy discards queued location without transmitting or blocking chat and clears reused tags") {
	test_support::Fixture f;
	f.begin();
	auto *p = f.packet();
	p->privacy_generation = 123;
	f.dispatcher.sendPacket(p, 0, 100);
	f.dispatcher.block_private = true;
	f.clock.now += 100;
	f.dispatcher.loop();
	CHECK(f.radio.attempts == 0 && f.pool.getFreeCount() == 32);
	CHECK(f.dispatcher.getTotalAirTime() == 0);
	p = f.packet();
	CHECK(p->privacy_generation == 0);
	f.dispatcher.sendPacket(p, 0);
	f.dispatcher.loop();
	CHECK(f.radio.attempts == 1);
	f.complete();
	CHECK(f.pool.getFreeCount() == 32);
}

TEST(privacy_origin, "UNIT-PRIVACY-008", "Frame origin follows the actual transport when BLE USB and TCP are simultaneously enabled") {
	CHECK(PrivacyEngine::canConfigure(true, true, false));
	CHECK(PrivacyEngine::canConfigure(true, false, true));
	CHECK(!PrivacyEngine::canConfigure(true, false, false));
	CHECK(!PrivacyEngine::canConfigure(false, true, true));
	CHECK(!PrivacyEngine::canConfigure(false, false, true));
	CHECK(!PrivacyEngine::canConfigure(false, true, false));
	CHECK(!PrivacyEngine::canConfigure(false, false, false));
	struct Link : BaseSerialInterface {
		bool pending = false;
		void enable() override {}
		void disable() override {}
		bool isEnabled() const override { return true; }
		bool isConnected() const override { return true; }
		bool isWriteBusy() const override { return false; }
		size_t writeFrame(const uint8_t *, size_t n) override { return n; }
		size_t checkRecvFrame(uint8_t *buf) override {
			if (!pending) return 0;
			pending = false; buf[0] = 1; return 1;
		}
	} ble, usb, tcp;
	MultiSerialInterface serial;
	serial.addInterface(InterfaceType::Bluetooth, &ble);
	serial.addInterface(InterfaceType::USB, &usb);
	serial.addInterface(InterfaceType::WiFi, &tcp);
	serial.enable();
	CHECK(serial.lastReceiveType() == InterfaceType::NONE);
	uint8_t buf[8];
	ble.pending = true;
	CHECK(serial.checkRecvFrame(buf) == 1 && serial.lastReceiveType() == InterfaceType::Bluetooth);
	tcp.pending = true;
	CHECK(serial.checkRecvFrame(buf) == 1 && serial.lastReceiveType() == InterfaceType::WiFi);
	usb.pending = true;
	CHECK(serial.checkRecvFrame(buf) == 1 && serial.lastReceiveType() == InterfaceType::USB);
}
