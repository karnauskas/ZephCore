#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define MQTT_QOS_0_AT_MOST_ONCE 0
#define MQTT_TRANSPORT_SECURE 1
#define MQTT_TRANSPORT_SECURE_WEBSOCKET 3
#define MQTT_TRANSPORT_NON_SECURE_WEBSOCKET 2
#define MQTT_TRANSPORT_NON_SECURE 0
#define MQTT_EVT_CONNACK 1
#define MQTT_EVT_DISCONNECT 2
struct mqtt_utf8 { const uint8_t *utf8; size_t size; };
struct mqtt_topic { struct mqtt_utf8 topic; int qos; };
struct mqtt_publish_param {
 struct { struct { struct mqtt_utf8 topic; int qos; } topic; struct { uint8_t *data; uint32_t len; } payload; } message;
 uint16_t message_id; int dup_flag, retain_flag;
};
struct mqtt_evt { int type, result; };
struct mqtt_sec_config { int peer_verify, cipher_count, sec_tag_count; const int *cipher_list, *sec_tag_list; const char *hostname; };
struct mqtt_client {
 struct mqtt_utf8 client_id, *user_name, *password, *will_message;
 struct mqtt_topic *will_topic;
 void *broker;
 uint8_t *rx_buf, *tx_buf;
 size_t rx_buf_size, tx_buf_size;
 int keepalive, clean_session, will_retain;
 struct { int type; struct { int sock; struct mqtt_sec_config config; } tls; struct { int sock; } tcp;
 struct { int sock, timeout; struct { const char *host, *url; uint8_t *tmp_buf; size_t tmp_buf_len; } config; } websocket;
 } transport;
 void (*evt_cb)(struct mqtt_client *, const struct mqtt_evt *);
};
void mqtt_client_init(struct mqtt_client *);
int mqtt_connect(struct mqtt_client *);
int mqtt_abort(struct mqtt_client *);
int mqtt_input(struct mqtt_client *);
int mqtt_keepalive_time_left(const struct mqtt_client *);
int mqtt_ping(struct mqtt_client *);
int mqtt_publish(struct mqtt_client *, const struct mqtt_publish_param *);
