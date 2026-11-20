/*
 * SPDX-License-Identifier: MIT
 * Uplink credentials: WiFi, MQTT broker and location for the observer and the
 * repeater uplink. Read by the WiFi station and the MQTT publisher; loaded,
 * saved and edited by the roles (app/observer_creds.h).
 *
 * The prefix before slots is frozen for legacy obs_creds migration.
 * New settings are persisted in prefs.json. Keep the legacy field sizes/order.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <helpers/MqttConfig.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ObserverCreds {
	char wifi_ssid[64];      /* WiFi network name */
	char wifi_psk[64];       /* WiFi password (empty = open network) */
	char mqtt_host[128];     /* MQTT broker hostname, e.g. "abydos.hu" */
	uint16_t mqtt_port;      /* MQTT broker port, e.g. 443 */
	uint8_t  mqtt_tls;       /* 1 = TLS (no cert verify), 0 = plaintext */
	char mqtt_user[64];      /* Legacy, unused by identity authentication */
	char mqtt_password[64];  /* Legacy, unused by identity authentication */
	char mqtt_iata[8];       /* IATA location code, e.g. "BUD", "BTS", "VIE" */
	uint8_t  _reserved[1];   /* alignment / future use */
	int32_t  lat_e6;          /* latitude  × 1 000 000 (e.g. 47497900 = 47.497900°N) */
	int32_t  lon_e6;          /* longitude × 1 000 000 (e.g. 19040200 = 19.040200°E) */
	struct MqttSlot slots[MQTT_CONFIG_SLOTS];
	uint8_t mqtt_configured;
};
#define OBSERVER_LEGACY_CREDS_SIZE offsetof(struct ObserverCreds, slots)

/* Apply sensible defaults to a freshly-zeroed creds struct. */
static inline void observer_creds_init(struct ObserverCreds *creds)
{
	creds->mqtt_port = 443;
	creds->mqtt_tls  = 1;
	for (unsigned i = 0; i < MQTT_CONFIG_SLOTS; ++i) mqtt_slot_init(&creds->slots[i], "none");
}

#ifdef __cplusplus
}
#endif
