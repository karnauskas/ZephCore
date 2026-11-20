/* SPDX-License-Identifier: MIT */
#pragma once
#include <helpers/ConfigSerializer.h>
#include <helpers/UplinkConfig.h>

class MqttSlotJson : public ConfigSerializer {
	MqttSlot *_s;
	void structure() override {
		def("preset", _s->preset, sizeof(_s->preset));
		def("server", _s->server, sizeof(_s->server));
		def("username", _s->username, sizeof(_s->username));
		def("password", _s->password, sizeof(_s->password));
		def("token", _s->token, sizeof(_s->token));
		def("topic", _s->topic, sizeof(_s->topic));
		def("audience", _s->audience, sizeof(_s->audience));
		def("port", _s->port); def("filter", _s->filter); def("ttl", _s->lifetime);
	}
public:
	explicit MqttSlotJson(MqttSlot *s) : _s(s) {}
};

class MqttConfigJson : public ConfigSerializer {
	ObserverCreds *_c;
	void structure() override {
		def("configured", _c->mqtt_configured);
		def("ssid", _c->wifi_ssid, sizeof(_c->wifi_ssid));
		def("psk", _c->wifi_psk, sizeof(_c->wifi_psk));
		def("host", _c->mqtt_host, sizeof(_c->mqtt_host));
		def("port", _c->mqtt_port); def("tls", _c->mqtt_tls);
		def("user", _c->mqtt_user, sizeof(_c->mqtt_user));
		def("password", _c->mqtt_password, sizeof(_c->mqtt_password));
		def("iata", _c->mqtt_iata, sizeof(_c->mqtt_iata));
		def("lat", _c->lat_e6); def("lon", _c->lon_e6);
		for (unsigned i = 0; i < MQTT_CONFIG_SLOTS; ++i) {
			char key[8]; snprintf(key, sizeof(key), "mqtt%u", i + 1);
			MqttSlotJson slot(&_c->slots[i]); def(key, slot);
		}
	}
public:
	explicit MqttConfigJson(ObserverCreds *c) : _c(c) {}
};
