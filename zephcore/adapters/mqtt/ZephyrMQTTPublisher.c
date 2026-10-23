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
#include <errno.h>
#include <string.h>
#include <stdio.h>

LOG_MODULE_REGISTER(mqtt_pub, CONFIG_LOG_DEFAULT_LEVEL);
#define PUB_TOPIC_MAX 160
#define PUB_PAYLOAD_MAX 1024
#define PUB_QUEUE_LEN 8
#define PUB_RECONNECT_BIT BIT(0)

struct pub_msg {
	uint16_t payload_len;
	uint8_t topic;
	char payload[PUB_PAYLOAD_MAX];
};
K_MSGQ_DEFINE(s_pub_queue, sizeof(struct pub_msg), PUB_QUEUE_LEN, 4);
static __noinit struct pub_msg s_stage, s_drain;
static const struct ObserverCreds *s_creds;
static char s_client_id[64], s_status_topic[PUB_TOPIC_MAX], s_packets_topic[PUB_TOPIC_MAX];
static void (*s_connect_cb)(void);
static atomic_t s_connected;
static atomic_t s_packets_queued, s_packets_sent, s_packets_dropped, s_packets_failed, s_publish_error;
static atomic_t s_time_offset, s_time_valid;
static K_EVENT_DEFINE(s_pub_events);
static K_SEM_DEFINE(s_started, 0, 1);
static __noinit char s_lwt_offline[128];

struct broker {
	struct mqtt_client client;
	struct sockaddr_in broker_addr;
	uint8_t rx_buf[256], ws_buf[512];
	struct mqtt_topic lwt_topic;
	struct mqtt_utf8 lwt_msg, mqtt_user, mqtt_pass, mqtt_client_id;
	int64_t retry_at, connack_deadline;
	uint32_t backoff_s;
	bool open, connected;
};
static __noinit struct broker s_broker;
/* MQTT calls run serially on one worker; TX data is consumed before return. */
static __noinit uint8_t s_tx_buf[1024];
static __noinit char s_auth_username[68], s_auth_token[640];

struct broker_config {
	const char *host;
	uint16_t port;
	bool tls;
};
static struct broker_config config(void)
{
	return (struct broker_config){ s_creds->mqtt_host, s_creds->mqtt_port, s_creds->mqtt_tls };
}

static void set_connected(bool connected)
{
	s_broker.connected = connected;
	atomic_set(&s_connected, connected);
	zc_wifi_station_session_up(connected);
}

static void mqtt_evt_handler(struct mqtt_client *client, const struct mqtt_evt *evt)
{
	ARG_UNUSED(client);
	if (evt->type == MQTT_EVT_CONNACK) {
		if (evt->result == 0) {
			set_connected(true);
			s_broker.backoff_s = 5;
			LOG_INF("MQTT connected");
			if (s_connect_cb) s_connect_cb();
		} else LOG_WRN("MQTT CONNACK rejected: %d", evt->result);
	} else if (evt->type == MQTT_EVT_DISCONNECT) {
		set_connected(false);
	}
}

static uint16_t s_pkt_id;

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
		.message_id  = ++s_pkt_id,
		.dup_flag    = 0,
		.retain_flag = retain ? 1 : 0,
	};
	return mqtt_publish(client, &p);
}

static void publish_status(struct mqtt_client *client, bool online)
{
	char json[128];
	int len = snprintf(json, sizeof(json), "{\"status\":\"%s\",\"origin_id\":\"%s\"}",
			online ? "online" : "offline", s_auth_username + 3);
	do_publish(client, s_status_topic, json, len, true);
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
	const struct broker_config c = config();
	mqtt_client_init(&b->client);

	/* Client ID */
	b->mqtt_client_id.utf8 = (const uint8_t *)s_client_id;
	b->mqtt_client_id.size = strlen(s_client_id);
	b->client.client_id    = b->mqtt_client_id;

	/* Broker address (already resolved) */
	b->client.broker = &b->broker_addr;

	/* RX/TX buffers */
	b->client.rx_buf      = b->rx_buf;
	b->client.rx_buf_size = sizeof(b->rx_buf);
	b->client.tx_buf      = s_tx_buf;
	b->client.tx_buf_size = sizeof(s_tx_buf);

	/* Keepalive (matches CONFIG_MQTT_KEEPALIVE=60) */
	b->client.keepalive    = 60;
	b->client.clean_session = 1;

	/* Only the worker uses these credentials; MQTT CONNECT consumes them synchronously. */
	uint32_t epoch = (uint32_t)atomic_get(&s_time_offset) + (uint32_t)(k_uptime_get() / 1000);
	if (!atomic_get(&s_time_valid)) {
		LOG_WRN("MQTT authentication needs SNTP time");
		return false;
	}
	if (!mqtt_auth_credentials(c.host, epoch,
			s_auth_username, sizeof(s_auth_username), s_auth_token, sizeof(s_auth_token))) return false;
	b->mqtt_user = (struct mqtt_utf8){ .utf8 = (const uint8_t *)s_auth_username, .size = strlen(s_auth_username) };
	b->mqtt_pass = (struct mqtt_utf8){ .utf8 = (const uint8_t *)s_auth_token, .size = strlen(s_auth_token) };
	b->client.user_name = &b->mqtt_user;
	b->client.password = &b->mqtt_pass;

	/* LWT — retained "offline" published if connection drops */
	snprintf(s_lwt_offline, sizeof(s_lwt_offline), "{\"status\":\"offline\",\"origin_id\":\"%s\"}",
		 s_auth_username + 3);
	b->lwt_topic.topic.utf8  = (const uint8_t *)s_status_topic;
	b->lwt_topic.topic.size  = strlen(s_status_topic);
	b->lwt_topic.qos         = MQTT_QOS_0_AT_MOST_ONCE;
	b->lwt_msg.utf8          = (const uint8_t *)s_lwt_offline;
	b->lwt_msg.size          = strlen(s_lwt_offline);
	b->client.will_topic     = &b->lwt_topic;
	b->client.will_message   = &b->lwt_msg;
	b->client.will_retain    = 1;

	/* Transport */
	if (c.tls) {
		b->client.transport.type = MQTT_TRANSPORT_SECURE_WEBSOCKET;
		struct mqtt_sec_config *tls = &b->client.transport.tls.config;
		/* No certificate pinning — encrypted but no CA verification.
		 * This avoids cert rotation issues (e.g. Let's Encrypt renewals). */
		tls->peer_verify  = TLS_PEER_VERIFY_NONE;
		tls->cipher_count = 0;
		tls->cipher_list  = NULL;
		tls->sec_tag_count = 0;
		tls->sec_tag_list  = NULL;
		tls->hostname      = c.host;  /* SNI */
	} else {
		b->client.transport.type = MQTT_TRANSPORT_NON_SECURE_WEBSOCKET;
	}

	b->client.transport.websocket.config.host = c.host;
	b->client.transport.websocket.config.url = "/";
	b->client.transport.websocket.config.tmp_buf = b->ws_buf;
	b->client.transport.websocket.config.tmp_buf_len = sizeof(b->ws_buf);
	b->client.transport.websocket.timeout = 5000;
	b->client.evt_cb = mqtt_evt_handler;
	return true;

}


static int get_sock(const struct broker *b)
{
	return b->client.transport.websocket.sock;
}

static void stop_broker(void)
{
	struct broker *b = &s_broker;
	set_connected(false);
	if (b->open) mqtt_abort(&b->client);
	b->open = false;
	b->retry_at = k_uptime_get() + b->backoff_s * 1000;
	b->backoff_s = MIN(b->backoff_s * 2, 300);
}

static void mqtt_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);
	k_sem_take(&s_started, K_FOREVER);
	struct broker *b = &s_broker;
	b->backoff_s = 5;
	for (;;) {
		if (!zc_wifi_station_is_connected()) {
			if (b->open) stop_broker();
			k_event_wait(&g_wifi_events, WIFI_READY_BIT, false, K_FOREVER);
		}
		if (k_event_test(&s_pub_events, PUB_RECONNECT_BIT)) {
			k_event_clear(&s_pub_events, PUB_RECONNECT_BIT);
			if (b->connected) publish_status(&b->client, false);
			stop_broker();
			b->retry_at = 0;
			b->backoff_s = 5;
		}
		if (!b->open) {
			const struct broker_config c = config();
			if (!*c.host) {
				k_event_wait(&s_pub_events, PUB_RECONNECT_BIT, false, K_FOREVER);
				continue;
			}
			int64_t delay = b->retry_at - k_uptime_get();
			if (delay > 0) {
				k_event_wait(&s_pub_events, PUB_RECONNECT_BIT, false, K_MSEC(delay));
				continue;
			}
			LOG_INF("MQTT connecting to %s:%u (WS /, TLS=%u)", c.host, c.port, c.tls);
			int rc = resolve_host(c.host, c.port, &b->broker_addr);
			if (rc == 0) rc = setup_client(b) ? mqtt_connect(&b->client) : -EAGAIN;
			if (rc < 0) {
				LOG_WRN("MQTT WSS/CONNECT failed: %d", rc);
				stop_broker();
				continue;
			}
			LOG_INF("MQTT WebSocket connected; CONNECT sent, waiting for CONNACK");
			b->open = true;
			b->connack_deadline = k_uptime_get() + 5000;
		}
		while (b->connected && k_msgq_get(&s_pub_queue, &s_drain, K_NO_WAIT) == 0) {
			const char *topic = s_drain.topic == MQTT_PUB_TOPIC_PACKETS ? s_packets_topic : s_status_topic;
			int published = do_publish(&b->client, topic, s_drain.payload, s_drain.payload_len, s_drain.topic == MQTT_PUB_TOPIC_STATUS);
			if (s_drain.topic == MQTT_PUB_TOPIC_PACKETS) {
				atomic_inc(published < 0 ? &s_packets_failed : &s_packets_sent);
				if (published < 0) atomic_set(&s_publish_error, published);
			}
			if (published < 0) {
				LOG_WRN("MQTT publish failed: %d", published);
				stop_broker();
			}
		}
		if (!b->open) continue;
		struct zsock_pollfd fd = { .fd = get_sock(b), .events = ZSOCK_POLLIN };
		int rc = zsock_poll(&fd, 1, 100);
		if (rc < 0 || (fd.revents & (ZSOCK_POLLERR | ZSOCK_POLLHUP | ZSOCK_POLLNVAL))) {
			LOG_WRN("MQTT socket poll failed: rc=%d errno=%d events=0x%x", rc, errno, fd.revents);
			stop_broker();
			continue;
		}
		bool was_connected = b->connected;
		if (fd.revents & ZSOCK_POLLIN) {
			int input = mqtt_input(&b->client);
			if ((input < 0 && input != -EAGAIN && input != -EWOULDBLOCK) ||
			    (was_connected && !b->connected)) {
				LOG_WRN("MQTT input failed: %d", input);
				stop_broker();
				continue;
			}
		}
		if (!b->connected) {
			if (k_uptime_get() >= b->connack_deadline) {
				LOG_WRN("MQTT CONNACK timed out");
				stop_broker();
			}
			continue;
		}
		int left = mqtt_keepalive_time_left(&b->client);
		if (left >= 0 && left <= b->client.keepalive * 250) {
			int ping = mqtt_ping(&b->client);
			if (ping < 0 && ping != -EAGAIN) stop_broker();
		}
	}
}
K_THREAD_DEFINE(mqtt_pub_thread, 12288, mqtt_thread_fn, NULL, NULL, NULL, 5, 0, 0);

void mqtt_publisher_start(const struct ObserverCreds *creds,
			  const char *client_id,
			  const char *status_topic,
			  const char *packets_topic)
{
	memset(&s_stage, 0, sizeof(s_stage));
	memset(&s_drain, 0, sizeof(s_drain));
	memset(&s_broker, 0, sizeof(s_broker));
	s_creds = creds;
	strncpy(s_client_id,     client_id,     sizeof(s_client_id) - 1);
	strncpy(s_status_topic,  status_topic,  sizeof(s_status_topic) - 1);
	strncpy(s_packets_topic, packets_topic, sizeof(s_packets_topic) - 1);
	s_client_id[sizeof(s_client_id) - 1]         = '\0';
	s_status_topic[sizeof(s_status_topic) - 1]   = '\0';
	s_packets_topic[sizeof(s_packets_topic) - 1] = '\0';

	k_sem_give(&s_started);
	LOG_INF("MQTT publisher ready (client_id=%s)", s_client_id);
}

char *mqtt_publisher_stage(size_t *size)
{
	if (size) {
		*size = sizeof(s_stage.payload);
	}
	return s_stage.payload;
}

void mqtt_publisher_commit(enum mqtt_pub_topic topic, int payload_len)
{
	if (payload_len <= 0) {
		return;
	}
	if (payload_len >= (int)sizeof(s_stage.payload)) {
		payload_len = (int)sizeof(s_stage.payload) - 1;
	}
	s_stage.payload[payload_len] = '\0';
	s_stage.payload_len = (uint16_t)payload_len;
	s_stage.topic = (uint8_t)topic;

	if (k_msgq_put(&s_pub_queue, &s_stage, K_NO_WAIT) != 0) {
		if (topic == MQTT_PUB_TOPIC_PACKETS) atomic_inc(&s_packets_dropped);
		LOG_WRN("Publish queue full — message dropped");
	} else if (topic == MQTT_PUB_TOPIC_PACKETS) {
		atomic_inc(&s_packets_queued);
	}
}

bool mqtt_publisher_is_connected(void)
{
	return atomic_get(&s_connected) != 0;
}

void mqtt_publisher_format_status(char *out, size_t size)
{
	snprintf(out, size, "%s queued=%u sent=%u dropped=%u failed=%u last_error=%d",
		mqtt_publisher_is_connected() ? "connected" : "disconnected",
		(unsigned)atomic_get(&s_packets_queued), (unsigned)atomic_get(&s_packets_sent),
		(unsigned)atomic_get(&s_packets_dropped), (unsigned)atomic_get(&s_packets_failed),
		(int32_t)atomic_get(&s_publish_error));
}

void mqtt_publisher_reconnect(void)
{
	k_event_post(&s_pub_events, PUB_RECONNECT_BIT);
}

void mqtt_publisher_set_connect_cb(void (*cb)(void))
{
	s_connect_cb = cb;
}

void mqtt_publisher_set_time(uint32_t epoch)
{
	atomic_set(&s_time_offset, epoch - (uint32_t)(k_uptime_get() / 1000));
	atomic_set(&s_time_valid, 1);
}
