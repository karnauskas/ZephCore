/* SPDX-License-Identifier: MIT */
#include "ZephyrMQTTPublisher.h"
#include "ZephyrWiFiStation.h"
#include "uplink_creds.h"
#include <helpers/MqttAuth.h>
#include <zephyr/kernel.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#if defined(CONFIG_MBEDTLS_HAVE_TIME_DATE)
#include <zephyr/sys/clock.h>
#endif
#include <errno.h>
#include <string.h>
#include <stdio.h>

LOG_MODULE_REGISTER(mqtt_pub, CONFIG_LOG_DEFAULT_LEVEL);
#define PUB_TOPIC_MAX 256
#define PUB_PAYLOAD_MAX 1024
#define PUB_QUEUE_LEN 8
#define PUB_RECONNECT_BIT BIT(0)
#define BROKERS CONFIG_ZEPHCORE_MQTT_CONNECTIONS
struct pub_msg {
	uint16_t payload_len;
	uint8_t topic;
	char payload[PUB_PAYLOAD_MAX];
};
struct broker {
	struct mqtt_client client;
	struct sockaddr_in broker_addr;
	uint8_t rx_buf[256], ws_buf[512], tx_buf[1024];
	struct mqtt_topic lwt_topic;
	struct mqtt_utf8 lwt_msg, mqtt_user, mqtt_pass;
	struct MqttSlot pending, slot;
	struct MqttEndpoint endpoint;
	char iata[8], pending_iata[8];
	char status_topic[PUB_TOPIC_MAX], packets_topic[PUB_TOPIC_MAX];
	char username[68], token[640], offline[128], client_id[72];
	int sec_tag;
	struct k_msgq queue;
	struct k_event events;
	struct pub_msg drain;
	atomic_t queued, sent, dropped, failed, filtered, error;
	int64_t retry_at, connack_deadline, renew_at;
	uint32_t backoff_s;
	unsigned index;
	bool open, connected, legacy_topics, pending_legacy;
};
static __noinit struct broker s_brokers[BROKERS];
#if defined(CONFIG_ESP_SPIRAM)
#define MQTT_QUEUE_RAM __attribute__((section(".ext_ram.bss")))
#else
#define MQTT_QUEUE_RAM __noinit
#endif
static char s_queue_data[BROKERS][sizeof(struct pub_msg) * PUB_QUEUE_LEN] __aligned(4) MQTT_QUEUE_RAM;
static struct k_thread s_threads[BROKERS];
K_THREAD_STACK_ARRAY_DEFINE(s_stacks, BROKERS, 12288);
static K_MUTEX_DEFINE(s_config_lock);
static __noinit struct pub_msg s_stage;
static const struct ObserverCreds *s_creds;
static char s_device[65], s_status_topic[PUB_TOPIC_MAX], s_packets_topic[PUB_TOPIC_MAX];
static void (*s_connect_cb)(void);
static atomic_t s_connected;
static atomic_t s_packets_queued, s_packets_sent, s_packets_dropped, s_packets_failed, s_publish_error;
static atomic_t s_time_offset, s_time_valid;

static void set_connected(struct broker *b, bool connected)
{
	k_mutex_lock(&s_config_lock, K_FOREVER);
	b->connected = connected;
	if (connected) atomic_or(&s_connected, BIT(b->index));
	else atomic_and(&s_connected, ~BIT(b->index));
	zc_wifi_station_session_up(atomic_get(&s_connected) != 0);
	k_mutex_unlock(&s_config_lock);
}

static void mqtt_evt_handler(struct mqtt_client *client, const struct mqtt_evt *evt)
{
	struct broker *b = CONTAINER_OF(client, struct broker, client);
	if (evt->type == MQTT_EVT_CONNACK) {
		if (evt->result == 0) {
			set_connected(b, true);
			b->backoff_s = 5;
			LOG_INF("MQTT%u connected", b->index + 1);
			if (s_connect_cb) s_connect_cb();
		} else atomic_set(&b->error, evt->result);
	} else if (evt->type == MQTT_EVT_DISCONNECT) set_connected(b, false);
}
static int do_publish(struct mqtt_client *client,
		      const char *topic, const char *payload, int payload_len,
		      bool retain)
{
	struct mqtt_publish_param p = {
		.message = {
			.topic = {
				.topic = {
					.utf8 = (const uint8_t *)topic,
					.size = strlen(topic),
				},
				.qos = MQTT_QOS_0_AT_MOST_ONCE,
			},
			.payload = {
				.data = (uint8_t *)payload,
				.len  = (uint32_t)payload_len,
			},
		},
		.message_id  = 0,
		.dup_flag    = 0,
		.retain_flag = retain ? 1 : 0,
	};
	return mqtt_publish(client, &p);
}

static int resolve_host(const char *host, uint16_t port,
			struct sockaddr_in *out_addr)
{
	struct zsock_addrinfo hints = {
		.ai_family   = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};
	struct zsock_addrinfo *result;

	char port_str[8];
	snprintf(port_str, sizeof(port_str), "%u", port);

	int rc = zsock_getaddrinfo(host, port_str, &hints, &result);
	if (rc != 0) {
		LOG_ERR("DNS lookup failed for %s: %d", host, rc);
		return -EHOSTUNREACH;
	}

	memcpy(out_addr, result->ai_addr, sizeof(*out_addr));
	zsock_freeaddrinfo(result);
	LOG_INF("Resolved %s → port %u", host, port);
	return 0;
}

static bool setup_client(struct broker *b)
{
	struct MqttEndpoint *e = &b->endpoint;
	if (!mqtt_slot_endpoint(&b->slot, e)) return false;
	mqtt_client_init(&b->client);
	snprintf(b->client_id, sizeof(b->client_id), "%s-%u", s_device, b->index + 1);
	b->client.client_id = (struct mqtt_utf8){ (const uint8_t *)b->client_id, strlen(b->client_id) };
	b->client.broker = &b->broker_addr;
	b->client.rx_buf = b->rx_buf;
	b->client.rx_buf_size = sizeof(b->rx_buf);
	b->client.tx_buf = b->tx_buf;
	b->client.tx_buf_size = sizeof(b->tx_buf);
	b->client.keepalive = e->keepalive;
	b->client.clean_session = 1;
	if (e->auth == MQTT_AUTH_JWT) {
		uint32_t epoch = (uint32_t)atomic_get(&s_time_offset) + (uint32_t)(k_uptime_get() / 1000);
		if (!atomic_get(&s_time_valid)) {
			atomic_set(&b->error, -EAGAIN);
			LOG_WRN("MQTT%u authentication needs SNTP time", b->index + 1);
			return false;
		}
		if (!e->audience ||
		    !mqtt_auth_credentials_lifetime(e->audience, epoch, e->lifetime,
			b->username, sizeof(b->username), b->token, sizeof(b->token))) return false;
		b->renew_at = k_uptime_get() + (int64_t)(e->lifetime - 300) * 1000;
		b->mqtt_user = (struct mqtt_utf8){ (const uint8_t *)b->username, strlen(b->username) };
		b->mqtt_pass = (struct mqtt_utf8){ (const uint8_t *)b->token, strlen(b->token) };
	} else if (e->auth == MQTT_AUTH_USERPASS) {
		const char *user = !strcmp(e->username, "{pubkey}") ? s_device : e->username;
		b->mqtt_user = (struct mqtt_utf8){ (const uint8_t *)user, strlen(user) };
		b->mqtt_pass = (struct mqtt_utf8){ (const uint8_t *)e->password, strlen(e->password) };
	}
	if (e->auth != MQTT_AUTH_NONE) {
		b->client.user_name = &b->mqtt_user;
		b->client.password = &b->mqtt_pass;
	}
	if (b->legacy_topics) {
		snprintf(b->status_topic, sizeof(b->status_topic), "%s", s_status_topic);
		snprintf(b->packets_topic, sizeof(b->packets_topic), "%s", s_packets_topic);
	} else if (!mqtt_topic(&b->slot, e, b->iata, s_device, "status", b->status_topic, sizeof(b->status_topic)) ||
		   !mqtt_topic(&b->slot, e, b->iata, s_device, "packets", b->packets_topic, sizeof(b->packets_topic))) return false;
	snprintf(b->offline, sizeof(b->offline), "{\"status\":\"offline\",\"origin_id\":\"%s\"}", s_device);
	b->lwt_topic = (struct mqtt_topic){ { (const uint8_t *)b->status_topic, strlen(b->status_topic) }, MQTT_QOS_0_AT_MOST_ONCE };
	b->lwt_msg = (struct mqtt_utf8){ (const uint8_t *)b->offline, strlen(b->offline) };
	b->client.will_topic = &b->lwt_topic;
	b->client.will_message = &b->lwt_msg;
	b->client.will_retain = e->retain;
	if (e->tls) {
		b->client.transport.type = e->websocket ? MQTT_TRANSPORT_SECURE_WEBSOCKET : MQTT_TRANSPORT_SECURE;
		struct mqtt_sec_config *tls = &b->client.transport.tls.config;
		tls->peer_verify = e->ca_cert ? TLS_PEER_VERIFY_REQUIRED : TLS_PEER_VERIFY_NONE;
		tls->hostname = e->host;
		if (e->ca_cert) {
			b->sec_tag = 100 + b->index;
			tls_credential_delete(b->sec_tag, TLS_CREDENTIAL_CA_CERTIFICATE);
			int rc = tls_credential_add(b->sec_tag, TLS_CREDENTIAL_CA_CERTIFICATE, e->ca_cert, strlen(e->ca_cert) + 1);
			if (rc) { atomic_set(&b->error, rc); return false; }
			tls->sec_tag_list = &b->sec_tag;
			tls->sec_tag_count = 1;
		}
	} else b->client.transport.type = e->websocket ? MQTT_TRANSPORT_NON_SECURE_WEBSOCKET : MQTT_TRANSPORT_NON_SECURE;
	if (e->websocket) {
		b->client.transport.websocket.config.host = e->host;
		b->client.transport.websocket.config.url = e->path;
		b->client.transport.websocket.config.tmp_buf = b->ws_buf;
		b->client.transport.websocket.config.tmp_buf_len = sizeof(b->ws_buf);
		b->client.transport.websocket.timeout = 5000;
	}
	b->client.evt_cb = mqtt_evt_handler;
	return true;
}

static int get_sock(const struct broker *b)
{
	if (b->endpoint.websocket) return b->client.transport.websocket.sock;
	return b->endpoint.tls ? b->client.transport.tls.sock : b->client.transport.tcp.sock;
}

static void stop_broker(struct broker *b)
{
	set_connected(b, false);
	if (b->open) mqtt_abort(&b->client);
	b->open = false;
	b->retry_at = k_uptime_get() + b->backoff_s * 1000;
	b->backoff_s = MIN(b->backoff_s * 2, 300);
}

static void apply_config(struct broker *b)
{
	k_mutex_lock(&s_config_lock, K_FOREVER);
	b->slot = b->pending;
	b->legacy_topics = b->pending_legacy;
	memcpy(b->iata, b->pending_iata, sizeof(b->iata));
	k_mutex_unlock(&s_config_lock);
}

static void mqtt_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p2); ARG_UNUSED(p3);
	struct broker *b = &s_brokers[(uintptr_t)p1];
	b->backoff_s = 5;
	apply_config(b);
	for (;;) {
		if (!zc_wifi_station_is_connected()) {
			if (b->open) stop_broker(b);
			k_event_wait(&g_wifi_events, WIFI_READY_BIT, false, K_FOREVER);
		}
		if (k_event_test(&b->events, PUB_RECONNECT_BIT)) {
			k_event_clear(&b->events, PUB_RECONNECT_BIT);
			stop_broker(b);
			apply_config(b);
			k_msgq_purge(&b->queue);
			b->retry_at = 0;
			b->backoff_s = 5;
		}
		if (!b->open) {
			if (!mqtt_slot_endpoint(&b->slot, &b->endpoint)) {
				if (strcmp(b->slot.preset, "none")) atomic_set(&b->error, -EINVAL);
				k_event_wait(&b->events, PUB_RECONNECT_BIT, false, K_FOREVER);
				continue;
			}
			int64_t delay = b->retry_at - k_uptime_get();
			if (delay > 0) {
				k_event_wait(&b->events, PUB_RECONNECT_BIT, false, K_MSEC(delay));
				continue;
			}
			int rc = resolve_host(b->endpoint.host, b->endpoint.port, &b->broker_addr);
			if (!rc) {
				atomic_set(&b->error, 0);
				rc = setup_client(b) ? mqtt_connect(&b->client) :
					atomic_get(&b->error) ? (int32_t)atomic_get(&b->error) : -EINVAL;
			}
			if (rc < 0) {
				atomic_set(&b->error, rc);
				stop_broker(b);
				continue;
			}
			b->open = true;
			b->connack_deadline = k_uptime_get() + 5000;
		}
		while (b->connected && k_msgq_get(&b->queue, &b->drain, K_NO_WAIT) == 0) {
			const char *topic = b->drain.topic == MQTT_PUB_TOPIC_PACKETS ? b->packets_topic : b->status_topic;
			int rc = do_publish(&b->client, topic, b->drain.payload, b->drain.payload_len,
				b->drain.topic == MQTT_PUB_TOPIC_STATUS && b->endpoint.retain);
			if (b->drain.topic == MQTT_PUB_TOPIC_PACKETS) {
				atomic_inc(rc < 0 ? &b->failed : &b->sent);
				atomic_inc(rc < 0 ? &s_packets_failed : &s_packets_sent);
			}
			if (rc < 0) {
				atomic_set(&b->error, rc);
				if (b->drain.topic == MQTT_PUB_TOPIC_PACKETS) atomic_set(&s_publish_error, rc);
				stop_broker(b);
			}
		}
		if (!b->open) continue;
		struct zsock_pollfd fd = { .fd = get_sock(b), .events = ZSOCK_POLLIN };
		int rc = zsock_poll(&fd, 1, 100);
		if (rc < 0 || (fd.revents & (ZSOCK_POLLERR | ZSOCK_POLLHUP | ZSOCK_POLLNVAL))) {
			atomic_set(&b->error, rc < 0 ? -errno : -ECONNRESET);
			stop_broker(b); continue;
		}
		bool was_connected = b->connected;
		if (fd.revents & ZSOCK_POLLIN) {
			int input = mqtt_input(&b->client);
			if ((input < 0 && input != -EAGAIN && input != -EWOULDBLOCK) || (was_connected && !b->connected)) {
				atomic_set(&b->error, input);
				stop_broker(b); continue;
			}
		}
		if (!b->connected) {
			if (k_uptime_get() >= b->connack_deadline) { atomic_set(&b->error, -ETIMEDOUT); stop_broker(b); }
			continue;
		}
		if (b->endpoint.auth == MQTT_AUTH_JWT && strcmp(b->slot.preset, "waev") && k_uptime_get() >= b->renew_at) {
			stop_broker(b); b->retry_at = 0; continue;
		}
		int left = mqtt_keepalive_time_left(&b->client);
		if (left >= 0 && left <= b->client.keepalive * 250) {
			int ping = mqtt_ping(&b->client);
			if (ping < 0 && ping != -EAGAIN) { atomic_set(&b->error, ping); stop_broker(b); }
		}
	}
}

static void snapshot(struct broker *b)
{
	b->pending = s_creds->slots[b->index];
	b->pending_legacy = !s_creds->mqtt_configured;
	if (b->pending_legacy) {
		mqtt_slot_init(&b->pending, "none");
		if (!b->index && *s_creds->mqtt_host) {
			strcpy(b->pending.preset, "custom");
			snprintf(b->pending.server, sizeof(b->pending.server), "%s://%s", s_creds->mqtt_tls ? "wss" : "ws", s_creds->mqtt_host);
			b->pending.port = s_creds->mqtt_port;
			b->pending.lifetime = 3600;
			snprintf(b->pending.audience, sizeof(b->pending.audience), "%s", s_creds->mqtt_host);
		}
	}
	memcpy(b->pending_iata, s_creds->mqtt_iata, sizeof(b->pending_iata));
}

void mqtt_publisher_start(const struct ObserverCreds *creds, const char *client_id,
	const char *status_topic, const char *packets_topic)
{
	s_creds = creds;
	memset(&s_stage, 0, sizeof(s_stage));
	ARG_UNUSED(client_id);
	snprintf(s_status_topic, sizeof(s_status_topic), "%s", status_topic);
	snprintf(s_packets_topic, sizeof(s_packets_topic), "%s", packets_topic);
	mqtt_auth_public_key(s_device);
	memset(s_brokers, 0, sizeof(s_brokers));
	for (unsigned i = 0; i < BROKERS; ++i) {
		struct broker *b = &s_brokers[i];
		b->index = i;
		k_msgq_init(&b->queue, s_queue_data[i], sizeof(struct pub_msg), PUB_QUEUE_LEN);
		k_event_init(&b->events);
		snapshot(b);
		k_thread_create(&s_threads[i], s_stacks[i], K_THREAD_STACK_SIZEOF(s_stacks[i]), mqtt_thread_fn,
			(void *)(uintptr_t)i, NULL, NULL, 5, 0, K_NO_WAIT);
	}
}

char *mqtt_publisher_stage(size_t *size)
{
	if (size) *size = sizeof(s_stage.payload);
	return s_stage.payload;
}

static void commit(enum mqtt_pub_topic topic, int len, unsigned type)
{
	if (len <= 0) return;
	len = MIN(len, (int)sizeof(s_stage.payload) - 1);
	s_stage.payload[len] = 0;
	s_stage.payload_len = len;
	s_stage.topic = topic;
	for (unsigned i = 0; i < BROKERS; ++i) {
		struct broker *b = &s_brokers[i];
		if (s_creds->mqtt_configured && !strcmp(s_creds->slots[i].preset, "none")) continue;
		if (!s_creds->mqtt_configured && (i || !*s_creds->mqtt_host)) continue;
		uint16_t filter = s_creds->mqtt_configured ? s_creds->slots[i].filter : UINT16_MAX;
		if (topic == MQTT_PUB_TOPIC_PACKETS && (filter == 0 || (type < 16 && !(filter & BIT(type))))) {
			atomic_inc(&b->filtered); continue;
		}
		if (k_msgq_put(&b->queue, &s_stage, K_NO_WAIT)) {
			if (topic == MQTT_PUB_TOPIC_PACKETS) { atomic_inc(&b->dropped); atomic_inc(&s_packets_dropped); }
		} else if (topic == MQTT_PUB_TOPIC_PACKETS) { atomic_inc(&b->queued); atomic_inc(&s_packets_queued); }
	}
}
void mqtt_publisher_commit(enum mqtt_pub_topic topic, int len) { commit(topic, len, 255); }
void mqtt_publisher_commit_packet(uint8_t type, int len) { commit(MQTT_PUB_TOPIC_PACKETS, len, type); }
bool mqtt_publisher_is_connected(void) { return atomic_get(&s_connected) != 0; }
void mqtt_publisher_format_status(char *out, size_t size)
{
	snprintf(out, size, "%s queued=%u sent=%u dropped=%u failed=%u last_error=%d",
		mqtt_publisher_is_connected() ? "connected" : "disconnected",
		(unsigned)atomic_get(&s_packets_queued), (unsigned)atomic_get(&s_packets_sent),
		(unsigned)atomic_get(&s_packets_dropped), (unsigned)atomic_get(&s_packets_failed),
		(int32_t)atomic_get(&s_publish_error));
}
void mqtt_publisher_format_slot(unsigned slot, char *out, size_t size)
{
	if (slot >= BROKERS) { snprintf(out, size, "Error: MQTT slot range 1-%u", BROKERS); return; }
	const struct broker *b = &s_brokers[slot];
	snprintf(out, size, "mqtt%u %s queued=%u sent=%u dropped=%u failed=%u filt=%u last_error=%d",
		slot + 1, (atomic_get(&s_connected) & BIT(slot)) ? "connected" : "disconnected",
		(unsigned)atomic_get(&b->queued), (unsigned)atomic_get(&b->sent), (unsigned)atomic_get(&b->dropped),
		(unsigned)atomic_get(&b->failed), (unsigned)atomic_get(&b->filtered), (int32_t)atomic_get(&b->error));
}
void mqtt_publisher_format_connections(char *out, size_t size)
{
	size_t used = snprintf(out, size, "> ");
	for (unsigned i = 0; i < BROKERS && used < size; ++i) {
		const char *state = (atomic_get(&s_connected) & BIT(i)) ? "connected" : "disconnected";
		if (s_creds && s_creds->mqtt_configured && !strcmp(s_creds->slots[i].preset, "none")) state = "off";
		used += snprintf(out + used, size - used, "%smqtt%u:%s", i ? ", " : "", i + 1, state);
	}
}
void mqtt_publisher_reconnect_slot(unsigned slot)
{
	if (!s_creds || slot >= BROKERS) return;
	k_mutex_lock(&s_config_lock, K_FOREVER);
	snapshot(&s_brokers[slot]);
	k_event_post(&s_brokers[slot].events, PUB_RECONNECT_BIT);
	k_mutex_unlock(&s_config_lock);
}
void mqtt_publisher_reconnect(void)
{
	for (unsigned i = 0; i < BROKERS; ++i) mqtt_publisher_reconnect_slot(i);
}
void mqtt_publisher_set_connect_cb(void (*cb)(void)) { s_connect_cb = cb; }
void mqtt_publisher_set_time(uint32_t epoch)
{
#if defined(CONFIG_MBEDTLS_HAVE_TIME_DATE)
	struct timespec now;
	if (!sys_clock_gettime(SYS_CLOCK_REALTIME, &now) && (int64_t)epoch > now.tv_sec) {
		struct timespec trusted = { .tv_sec = epoch };
		sys_clock_settime(SYS_CLOCK_REALTIME, &trusted);
	}
#endif
	atomic_set(&s_time_offset, epoch - (uint32_t)(k_uptime_get() / 1000));
	atomic_set(&s_time_valid, 1);
}
