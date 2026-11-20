/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
enum MQTTAuthType { MQTT_AUTH_NONE, MQTT_AUTH_USERPASS, MQTT_AUTH_JWT };
enum MQTTTopicStyle { MQTT_TOPIC_MESHCORE, MQTT_TOPIC_MESHRANK };
struct MQTTPresetDef {
	const char *name, *server_url, *jwt_audience, *ca_cert;
	enum MQTTAuthType auth_type;
	enum MQTTTopicStyle topic_style;
	unsigned long token_lifetime;
	bool allow_retain;
	uint16_t keepalive;
	const char *userpass_username, *userpass_password;
};
#define MQTT_PRESET_COUNT 42
extern const struct MQTTPresetDef MQTT_PRESETS[MQTT_PRESET_COUNT];
const struct MQTTPresetDef *mqtt_preset_find(const char *name);
#ifdef __cplusplus
}
#endif
