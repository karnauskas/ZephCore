/* SPDX-License-Identifier: MIT */
#include "MqttConfig.h"
#include <helpers/UplinkConfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void mqtt_slot_init(struct MqttSlot *slot, const char *preset)
{
	memset(slot, 0, sizeof(*slot));
	snprintf(slot->preset, sizeof(slot->preset), "%s", preset);
	slot->filter = UINT16_MAX;
}

bool mqtt_slot_endpoint(const struct MqttSlot *slot, struct MqttEndpoint *e)
{
	memset(e, 0, sizeof(*e));
	if (!strcmp(slot->preset, "none")) return false;
	const struct MQTTPresetDef *p = mqtt_preset_find(slot->preset);
	if (!p && strcmp(slot->preset, "custom")) return false;
	const char *url = p ? p->server_url : slot->server;
	e->retain = p ? p->allow_retain : true;
	e->auth = p ? p->auth_type : *slot->audience ? MQTT_AUTH_JWT :
		*slot->username || *slot->password ? MQTT_AUTH_USERPASS : MQTT_AUTH_NONE;
	e->audience = p ? p->jwt_audience : slot->audience;
	e->ca_cert = p ? p->ca_cert : NULL;
	e->username = p && p->userpass_username ? p->userpass_username : slot->username;
	e->password = p && p->userpass_password ? p->userpass_password : slot->password;
	e->lifetime = p ? (p->token_lifetime ? p->token_lifetime : 86400) :
		slot->lifetime ? slot->lifetime : 86400;
	if (e->auth == MQTT_AUTH_JWT && (e->lifetime <= 300 || e->lifetime > 86400)) return false;
	e->keepalive = p && p->keepalive ? p->keepalive : 120;
	e->topic = p && p->topic_style == MQTT_TOPIC_MESHRANK ?
		"meshrank/uplink/{token}/{device}/{type}" : *slot->topic && !p ?
		slot->topic : "meshcore/{iata}/{device}/{type}";
	if (p && p->topic_style == MQTT_TOPIC_MESHRANK && !*slot->token) return false;
	if (e->auth == MQTT_AUTH_USERPASS && (!*e->username || !*e->password)) return false;
	e->port = 1883;
	if (!strncmp(url, "mqtt://", 7)) url += 7;
	else if (!strncmp(url, "mqtts://", 8)) { url += 8; e->tls = true; e->port = 8883; }
	else if (!strncmp(url, "ws://", 5)) { url += 5; e->websocket = true; e->port = 80; }
	else if (!strncmp(url, "wss://", 6)) { url += 6; e->tls = e->websocket = true; e->port = 443; }
	else if (strstr(url, "://")) return false;
	const char *end = url + strcspn(url, ":/");
	size_t len = end - url;
	if (!len || len >= sizeof(e->host)) return false;
	memcpy(e->host, url, len);
	for (size_t i = 0; i < len; ++i) {
		if (!isalnum((unsigned char)e->host[i]) && e->host[i] != '.' && e->host[i] != '-') return false;
	}
	if (*end == ':') {
		char *tail;
		unsigned long port = strtoul(end + 1, &tail, 10);
		if (tail == end + 1 || !port || port > 65535 || (*tail && *tail != '/')) return false;
		e->port = port;
		end = tail;
	} else if (!p && slot->port) e->port = slot->port;
	if (strlen(end) >= sizeof(e->path) || (!e->websocket && *end && strcmp(end, "/"))) return false;
	snprintf(e->path, sizeof(e->path), "%s", *end ? end : "/");
	return true;
}

bool mqtt_topic(const struct MqttSlot *slot, const struct MqttEndpoint *e,
	const char *iata, const char *device, const char *type, char *out, size_t size)
{
	size_t used = 0;
	for (const char *p = e->topic; *p;) {
		const char *value = p;
		size_t len = 1, skip = 1;
		if (*p == '{') {
			const char *close = strchr(p, '}');
			if (!close) return false;
			skip = close - p + 1;
			if (skip == 6 && !memcmp(p, "{iata}", 6)) value = *iata ? iata : "XXX";
			else if (skip == 8 && !memcmp(p, "{device}", 8)) value = device;
			else if (skip == 7 && !memcmp(p, "{token}", 7)) value = slot->token;
			else if (skip == 6 && !memcmp(p, "{type}", 6)) value = type;
			else return false;
			len = strlen(value);
		}
		if (used + len >= size) return false;
		memcpy(out + used, value, len);
		used += len;
		p += skip;
	}
	if (!used) return false;
	out[used] = 0;
	return true;
}

void mqtt_migrate_legacy(struct ObserverCreds *c)
{
	if (c->mqtt_configured) return;
	mqtt_slot_init(&c->slots[0], "none");
	if (*c->mqtt_host) {
		struct MqttSlot *s = &c->slots[0];
		mqtt_slot_init(s, "custom");
		snprintf(s->server, sizeof(s->server), "%s://%s", c->mqtt_tls ? "wss" : "ws", c->mqtt_host);
		s->port = c->mqtt_port;
		s->lifetime = 3600;
		snprintf(s->audience, sizeof(s->audience), "%s", c->mqtt_host);
	}
	c->mqtt_configured = 1;
}

static bool number(const char *text, unsigned max, unsigned *value)
{
	if (!isdigit((unsigned char)*text)) return false;
	char *end;
	unsigned long n = strtoul(text, &end, 10);
	if (*end || n > max) return false;
	*value = n;
	return true;
}

static bool filter_parse(const char *value, uint16_t *mask)
{
	static const char *const names[] = { "req", "response", "txt_msg", "ack", "advert", "grp_txt", "grp_data", "anon_req", "path", "trace", "multipart", "control", "12", "13", "14", "raw_custom" };
	if (!*value || !strcmp(value, "all")) { *mask = UINT16_MAX; return true; }
	if (!strcmp(value, "none")) { *mask = 0; return true; }
	uint16_t result = 0;
	while (*value) {
		size_t len = strcspn(value, ",");
		bool found = false;
		for (unsigned i = 0; i < 16; ++i) {
			char numeric[3];
			snprintf(numeric, sizeof(numeric), "%u", i);
			if ((strlen(names[i]) == len && !memcmp(value, names[i], len)) ||
			    (strlen(numeric) == len && !memcmp(value, numeric, len))) {
				result |= 1u << i;
				found = true;
				break;
			}
		}
		if (!found) return false;
		value += len;
		if (*value && !*++value) return false;
	}
	*mask = result;
	return true;
}

int mqtt_config_command(struct ObserverCreds *c, const char *command, char *out, size_t size)
{
	bool set = !strncmp(command, "set ", 4);
	if (!set && strncmp(command, "get ", 4)) return 0;
	const char *key = command + 4;
	if (!set && (!strcmp(key, "mqtt.capacity") || !strcmp(key, "mqtt.slots"))) {
		snprintf(out, size, "> %u", CONFIG_ZEPHCORE_MQTT_CONNECTIONS);
		return 1;
	}
	if (!set && (!strcmp(key, "mqtt.presets") || !strncmp(key, "mqtt.presets ", 13))) {
		unsigned start = 0;
		if (key[12] && !number(key + 13, MQTT_PRESET_COUNT + 1, &start)) {
			snprintf(out, size, "Error: preset list start must be 0-%u", MQTT_PRESET_COUNT + 1); return 1;
		}
		size_t used = 0;
		unsigned i = start;
		if (size) out[0] = 0;
		for (; i < MQTT_PRESET_COUNT + 2; ++i) {
			const char *name = i < MQTT_PRESET_COUNT ? MQTT_PRESETS[i].name :
				i == MQTT_PRESET_COUNT ? "custom" : "none";
			size_t len = strlen(name);
			if (used + len + (i != start) + 18 >= size) break;
			used += snprintf(out + used, size - used, "%s%s", i != start ? "," : "", name);
		}
		if (!used) snprintf(out, size, "Error: list page too small");
		else if (i < MQTT_PRESET_COUNT + 2) snprintf(out + used, size - used, "... next:%u", i);
		return 1;
	}
	if (strncmp(key, "mqtt", 4) || !isdigit((unsigned char)key[4])) return 0;
	unsigned index = key[4] - '1';
	if (key[5] != '.' || index >= CONFIG_ZEPHCORE_MQTT_CONNECTIONS) {
		snprintf(out, size, "Error: MQTT slot range 1-%u", CONFIG_ZEPHCORE_MQTT_CONNECTIONS); return 1;
	}
	const char *field = key + 6, *value = strchr(field, ' ');
	size_t len = value ? (size_t)(value - field) : strlen(field);
	value = value ? value + 1 : "";
	struct MqttSlot *s = &c->slots[index];
	char *dest = NULL;
	size_t capacity = 0;
#define FIELD(name, member) if (len == sizeof(name)-1 && !memcmp(field, name, len)) { dest = s->member; capacity = sizeof(s->member); }
	FIELD("preset", preset)
	else FIELD("server", server)
	else FIELD("username", username)
	else FIELD("password", password)
	else FIELD("token", token)
	else FIELD("topic", topic)
	else FIELD("audience", audience)
#undef FIELD
	if (dest) {
		if (!set) { snprintf(out, size, "> %s", dest); return 1; }
		if (strlen(value) >= capacity) { snprintf(out, size, "Error: value too long"); return 1; }
		if (dest == s->preset && strcmp(value, "none") && strcmp(value, "custom") && !mqtt_preset_find(value)) {
			snprintf(out, size, "Error: unknown preset"); return 1;
		}
		if (dest == s->server) {
			struct MqttSlot candidate = *s;
			strcpy(candidate.preset, "custom"); strcpy(candidate.server, value);
			candidate.username[0] = candidate.password[0] = candidate.audience[0] = 0;
			struct MqttEndpoint endpoint;
			if (!mqtt_slot_endpoint(&candidate, &endpoint)) { snprintf(out, size, "Error: invalid broker URL"); return 1; }
		}
		if (dest == s->topic && *value) {
			struct MqttEndpoint endpoint = { .topic = value };
			char topic[256];
			if (!mqtt_topic(s, &endpoint, "XXX", "DEVICE", "packets", topic, sizeof(topic))) {
				snprintf(out, size, "Error: invalid topic template"); return 1;
			}
		}
		if (dest == s->audience) {
			for (const char *p = value; *p; ++p) if (!isalnum((unsigned char)*p) && *p != '.' && *p != '-') {
				snprintf(out, size, "Error: invalid audience"); return 1;
			}
		}
		strcpy(dest, value);
	} else if (len == 4 && !memcmp(field, "port", 4)) {
		if (!set) { snprintf(out, size, "> %u", s->port); return 1; }
		unsigned port;
		if (!number(value, 65535, &port) || !port) { snprintf(out, size, "Error: port range 1-65535"); return 1; }
		s->port = port;
	} else if (len == 6 && !memcmp(field, "filter", 6)) {
		if (!set) {
			if (!s->filter || s->filter == UINT16_MAX) snprintf(out, size, "> %s", s->filter ? "all" : "none");
			else { size_t used = snprintf(out, size, "> "); bool first = true;
				for (unsigned i = 0; i < 16 && used < size; ++i) if (s->filter & (1u << i)) {
					used += snprintf(out + used, size - used, "%s%u", first ? "" : ",", i); first = false;
				}
			}
			return 1;
		}
		uint16_t mask;
		if (!filter_parse(value, &mask)) { snprintf(out, size, "Error: invalid packet filter"); return 1; }
		s->filter = mask;
	} else return 0;
	c->mqtt_configured = 1;
	snprintf(out, size, "OK");
	return 2;
}
