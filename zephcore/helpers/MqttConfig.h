/* SPDX-License-Identifier: MIT */
#pragma once
#include "MqttPresets.h"
#ifndef CONFIG_ZEPHCORE_MQTT_CONNECTIONS
#define CONFIG_ZEPHCORE_MQTT_CONNECTIONS 2
#endif
#define MQTT_CONFIG_SLOTS 5
struct MqttSlot {
	char preset[32], server[192], username[64], password[96], token[96];
	char topic[160], audience[128];
	uint16_t port, filter;
	uint32_t lifetime;
};
struct MqttEndpoint {
	char host[128], path[128];
	uint16_t port;
	bool tls, websocket, retain;
	enum MQTTAuthType auth;
	const char *audience, *ca_cert, *username, *password, *topic;
	uint32_t lifetime;
	uint16_t keepalive;
};
struct ObserverCreds;
#ifdef __cplusplus
extern "C" {
#endif
bool mqtt_slot_endpoint(const struct MqttSlot *slot, struct MqttEndpoint *endpoint);
bool mqtt_topic(const struct MqttSlot *slot, const struct MqttEndpoint *endpoint,
	const char *iata, const char *device, const char *type, char *out, size_t size);
/* 0: not handled; 1: read/error; 2: settings changed. */
int mqtt_config_command(struct ObserverCreds *creds, const char *command, char *out, size_t size);
void mqtt_slot_init(struct MqttSlot *slot, const char *preset);
void mqtt_migrate_legacy(struct ObserverCreds *creds);
#ifdef __cplusplus
}
#endif
