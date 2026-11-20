#include "test.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
namespace reference {
#define WITH_MQTT_BRIDGE
#define PROGMEM
#include "mqtt_presets_reference.h"
#undef WITH_MQTT_BRIDGE
#undef PROGMEM
}
#include <helpers/MqttConfig.h>
#include <adapters/mqtt/uplink_creds.h>

namespace {
bool equal(const char *a, const char *b) { return a && b ? strcmp(a, b) == 0 : a == b; }
struct Fixture {
	ObserverCreds creds{};
	char out[256];
	Fixture() { observer_creds_init(&creds); creds.mqtt_configured = 1; }
	int cmd(const char *command) { return mqtt_config_command(&creds, command, out, sizeof(out)); }
};
}
TEST(mqtt_presets_parity, "UNIT-MQTT-010", "All broker presets match the pinned reference including transports credentials and CA roots") {
	CHECK(sizeof(reference::MQTT_PRESETS) / sizeof(reference::MQTT_PRESETS[0]) == MQTT_PRESET_COUNT);
	for (unsigned i = 0; i < MQTT_PRESET_COUNT; ++i) {
		const auto& a = reference::MQTT_PRESETS[i];
		const auto *b = mqtt_preset_find(a.name);
		CHECK(b && equal(a.server_url, b->server_url) && equal(a.jwt_audience, b->jwt_audience));
		CHECK(equal(a.ca_cert, b->ca_cert) && equal(a.userpass_username, b->userpass_username) && equal(a.userpass_password, b->userpass_password));
		CHECK(int(a.auth_type) == int(b->auth_type) && int(a.topic_style) == int(b->topic_style));
		CHECK(a.token_lifetime == b->token_lifetime && a.allow_retain == b->allow_retain && a.keepalive == b->keepalive);
		MqttSlot slot; mqtt_slot_init(&slot, a.name);
		strcpy(slot.username, "test"); strcpy(slot.password, "test"); strcpy(slot.token, "test");
		MqttEndpoint endpoint;
		CHECK(mqtt_slot_endpoint(&slot, &endpoint));
	}
}
TEST(mqtt_slot_cli, "UNIT-MQTT-011", "MQTT slot CLI validates presets filters and capacity without changing invalid settings") {
	Fixture f;
	CHECK(f.cmd("set mqtt1.preset analyzer-eu") == 2);
	CHECK(f.cmd("get mqtt1.preset") == 1 && !strcmp(f.out, "> analyzer-eu"));
	CHECK(f.cmd("set mqtt1.preset nonexistent") == 1);
	CHECK(!strcmp(f.creds.slots[0].preset, "analyzer-eu"));
	CHECK(f.cmd("set mqtt2.filter advert,txt_msg,15") == 2);
	CHECK(f.cmd("get mqtt2.filter") == 1 && !strcmp(f.out, "> 2,4,15"));
	uint16_t before = f.creds.slots[1].filter;
	CHECK(f.cmd("set mqtt2.filter 16") == 1 && f.creds.slots[1].filter == before);
	CHECK(f.cmd("set mqtt2.filter advert,") == 1 && f.creds.slots[1].filter == before);
	CHECK(f.cmd("set mqtt2.filter") == 2 && f.creds.slots[1].filter == UINT16_MAX);
	CHECK(f.cmd("set mqtt2.filter none") == 2 && !f.creds.slots[1].filter);
	CHECK(f.cmd("set mqtt9.preset analyzer-us") == 1 && strstr(f.out, "range"));
	CHECK(f.cmd("set mqtt0.preset analyzer-us") == 1);
	CHECK(f.cmd("set mqtt1.port 65536") == 1 && !f.creds.slots[0].port);
	unsigned index = 0, found = 0;
	do {
		char command[64]; snprintf(command, sizeof(command), "get mqtt.presets %u", index);
		CHECK(f.cmd(command) == 1);
		for (unsigned i = 0; i < MQTT_PRESET_COUNT; ++i) {
			std::string token = std::string(",") + MQTT_PRESETS[i].name;
			const char *match = strstr(f.out, token.c_str());
			if (!strncmp(f.out, MQTT_PRESETS[i].name, strlen(MQTT_PRESETS[i].name)) &&
			    (f.out[strlen(MQTT_PRESETS[i].name)] == ',' || f.out[strlen(MQTT_PRESETS[i].name)] == '.' || !f.out[strlen(MQTT_PRESETS[i].name)])) ++found;
			if (match && (!match[token.size()] || match[token.size()] == ',' || match[token.size()] == '.')) ++found;
		}
		const char *next = strstr(f.out, "next:");
		if (!next) break;
		unsigned previous = index;
		index = strtoul(next + 5, nullptr, 10);
		CHECK(index > previous);
	} while (true);
	CHECK(found == MQTT_PRESET_COUNT);
}
TEST(mqtt_routing, "UNIT-MQTT-012", "MQTT presets resolve TCP WebSocket authentication topics and timing independently") {
	Fixture f;
	MqttEndpoint e;
	CHECK(f.cmd("set mqtt1.preset analyzer-us") == 2);
	CHECK(mqtt_slot_endpoint(&f.creds.slots[0], &e));
	CHECK(e.tls && e.websocket && e.port == 443 && e.auth == MQTT_AUTH_JWT && e.retain && e.keepalive == 55);
	CHECK(!strcmp(e.path, "/mqtt") && !strcmp(e.audience, "mqtt-us-v1.letsmesh.net"));
	CHECK(f.cmd("set mqtt2.preset meshrank") == 2);
	CHECK(!mqtt_slot_endpoint(&f.creds.slots[1], &e));
	CHECK(f.cmd("set mqtt2.token test-token") == 2);
	CHECK(mqtt_slot_endpoint(&f.creds.slots[1], &e) && !e.websocket && e.tls && !e.retain && e.auth == MQTT_AUTH_NONE);
	char topic[256];
	CHECK(mqtt_topic(&f.creds.slots[1], &e, "VNO", "DEVICE", "packets", topic, sizeof(topic)));
	CHECK(!strcmp(topic, "meshrank/uplink/test-token/DEVICE/packets"));
	CHECK(!mqtt_topic(&f.creds.slots[1], &e, "VNO", "DEVICE", "packets", topic, 5));
	CHECK(f.cmd("set mqtt1.preset custom") == 2);
	CHECK(f.cmd("set mqtt1.server ws://192.168.1.2:9001/mqtt") == 2);
	CHECK(mqtt_slot_endpoint(&f.creds.slots[0], &e) && e.websocket && !e.tls && e.port == 9001);
	CHECK(f.cmd("set mqtt1.server ftp://invalid") == 1);
	CHECK(f.cmd("set mqtt1.topic bad/{unknown}") == 1);
	CHECK(f.cmd("set mqtt1.audience bad\"host") == 1);
	CHECK(f.cmd("set mqtt1.username user") == 2);
	CHECK(f.cmd("set mqtt1.password pass") == 2);
	CHECK(mqtt_slot_endpoint(&f.creds.slots[0], &e) && e.auth == MQTT_AUTH_USERPASS);
	CHECK(f.cmd("set mqtt1.audience example.org") == 2);
	CHECK(mqtt_slot_endpoint(&f.creds.slots[0], &e) && e.auth == MQTT_AUTH_JWT);
	CHECK(f.cmd("set mqtt1.topic custom/{iata}/{device}/{type}") == 2);
	CHECK(mqtt_slot_endpoint(&f.creds.slots[0], &e));
	CHECK(mqtt_topic(&f.creds.slots[0], &e, "VNO", "DEVICE", "status", topic, sizeof(topic)));
	CHECK(!strcmp(topic, "custom/VNO/DEVICE/status"));
	CHECK(f.cmd("set mqtt1.preset waev") == 2);
	CHECK(mqtt_slot_endpoint(&f.creds.slots[0], &e) && e.lifetime == 3300 && !e.retain);
}
