/*
 * SPDX-License-Identifier: MIT
 * ZephyrMQTTPublisher — MQTT client thread for the Observer role.
 *
 * Independent bounded workers wait for WiFi, resolve each broker hostname,
 * connect over TCP/WebSockets with optional TLS, publish LWT and
 * status, then drain pre-serialized JSON publish queues.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ObserverCreds;

/* Messages carry a topic kind; each slot resolves its own topic template. */
enum mqtt_pub_topic {
	MQTT_PUB_TOPIC_STATUS  = 0,
	MQTT_PUB_TOPIC_PACKETS = 1,
};

/*
 * Start the MQTT publisher thread.
 *   creds         — runtime credentials; pointer kept, must remain valid.
 *   client_id     — legacy caller label; wire client IDs use the public key and slot.
 *   status_topic  — topic for retained online/offline status messages.
 *   packets_topic — topic for received LoRa packet JSON messages.
 *
 * Must be called once from main() after wifi_station_start().
 */
void mqtt_publisher_start(const struct ObserverCreds *creds,
			  const char *client_id,
			  const char *status_topic,
			  const char *packets_topic);

/*
 * Zero-copy publish staging (single producer — the mesh main thread ONLY).
 * mqtt_publisher_stage() returns the staging payload buffer (capacity in
 * *size); the caller builds the JSON directly in it, then calls
 * mqtt_publisher_commit() which copies the staged message into the queue.
 * Drops the message (with a warning) if the queue is full.
 * NOT safe to call from any other thread — the staging slot is unlocked.
 */
char *mqtt_publisher_stage(size_t *size);
void mqtt_publisher_commit(enum mqtt_pub_topic topic, int payload_len);
void mqtt_publisher_commit_packet(uint8_t payload_type, int payload_len);
void mqtt_publisher_format_slot(unsigned slot, char *out, size_t size);
void mqtt_publisher_format_connections(char *out, size_t size);

/* True when at least one broker session is active. */
bool mqtt_publisher_is_connected(void);

/* Boot totals for packet-topic messages; sent means local QoS 0 transport success,
 * not broker or collector acknowledgement. Status messages are excluded. */
void mqtt_publisher_format_status(char *out, size_t size);

/*
 * Trigger an MQTT reconnect (e.g. after credential change via CLI).
 * Safe to call from any thread.
 */
void mqtt_publisher_reconnect(void);
void mqtt_publisher_reconnect_slot(unsigned slot);

/* SNTP time anchor used by the worker to mint fresh per-host authentication tokens. */
void mqtt_publisher_set_time(uint32_t epoch);

/*
 * Register a callback invoked once each time MQTT connects (CONNACK received).
 * Called from the MQTT publisher thread — keep it short.
 * Pass NULL to clear.
 */
void mqtt_publisher_set_connect_cb(void (*cb)(void));

#ifdef __cplusplus
}
#endif
