#include "test.h"
#include "ui/observer_display.h"
#include <cstring>

static std::string line(const ObserverDisplayStatus &s, unsigned row)
{
	char out[40];
	observer_display_line(s, row, out, sizeof(out));
	return out;
}

TEST(observer_status, "UNIT-OBSERVER-001", "Observer screen shows link states counts and never heard") {
	ObserverDisplayStatus s{};
	CHECK(line(s, 1) == "WiFi:down MQTT:down");
	CHECK(line(s, 7) == "Last: never");
	s.wifi_ready = true;
	CHECK(line(s, 1) == "WiFi:up MQTT:down");
	s.mqtt_connected = 1;
	CHECK(line(s, 1) == "WiFi:up MQTT:up");
	s.flood = 12; s.direct = 3; s.errors = 2;
	CHECK(line(s, 2) == "RX 15");
	CHECK(line(s, 3) == "Flood 12");
	CHECK(line(s, 4) == "Direct 3");
	CHECK(line(s, 5) == "RX errors 2");
	s.flood = s.direct = UINT32_MAX;
	CHECK(line(s, 2) == "RX 8589934590");
}

TEST(observer_time, "UNIT-OBSERVER-002", "Observer uptime and last heard handle day boundaries and bounded output") {
	ObserverDisplayStatus s{};
	s.heard = true;
	CHECK(line(s, 6) == "Up 0d00:00:00");
	CHECK(line(s, 7) == "Last 0d00:00:00");
	s.uptime_s = 86400 + 3661;
	s.last_heard_age_s = 86400 - 1;
	CHECK(line(s, 6) == "Up 1d01:01:01");
	CHECK(line(s, 7) == "Last 0d23:59:59");
	s.last_heard_age_s++;
	CHECK(line(s, 7) == "Last 1d00:00:00");
	s.uptime_s = UINT64_C(1) << 32;
	CHECK(line(s, 6) == "Up 49710d06:28:16");
	char out[4] = {};
	observer_display_line(s, 7, out, sizeof(out));
	CHECK(std::strcmp(out, "Las") == 0);
	observer_display_line(s, 7, out, 0);
	CHECK(std::strcmp(out, "Las") == 0);
}
