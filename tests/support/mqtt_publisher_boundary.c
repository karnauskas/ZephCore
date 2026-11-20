/* Runs the production worker against deterministic MQTT/socket boundaries. */
#define k_uptime_get mqtt_test_uptime
#include <setjmp.h>
#include <stdlib.h>
#include "../../zephcore/adapters/mqtt/ZephyrMQTTPublisher.c"

#define s_broker s_brokers[0]
#define s_pub_queue s_brokers[0].queue
#define s_pub_events s_brokers[0].events

void test_auth_init(void);
int test_auth_valid(const char *, size_t, const char *, size_t, const char *);
struct k_event g_wifi_events;
static jmp_buf finished;
static int64_t now;
static int phase, polls, sent, bad_status, retained_status, multi, multi_sent;
static bool session_up;
static struct sockaddr_in addr;
static struct zsock_addrinfo resolved;

int64_t k_uptime_get(void) { return now; }
bool zc_wifi_station_is_connected(void) { return true; }
void zc_wifi_station_session_up(bool up) { session_up = up; }
void mqtt_client_init(struct mqtt_client *c) { memset(c, 0, sizeof(*c)); }
int mqtt_connect(struct mqtt_client *c)
{

	const char *host = multi && c == &s_brokers[1].client ? "two" : "one";
	if (!c->user_name || !c->password || !test_auth_valid((const char *)c->user_name->utf8,
		c->user_name->size, (const char *)c->password->utf8, c->password->size, host) ||
	    c->transport.type != MQTT_TRANSPORT_SECURE_WEBSOCKET ||
	    strcmp(c->transport.websocket.config.host, host) || strcmp(c->transport.websocket.config.url, "/") ||
	    !c->transport.websocket.config.tmp_buf || c->transport.websocket.config.tmp_buf_len < 256 ||
	    !strstr((const char *)c->will_message->utf8, "\"origin_id\"")) return -EINVAL;
	c->transport.tcp.sock = c->transport.tls.sock = 0;
	if (multi && c == &s_brokers[0].client) return -ECONNREFUSED;
	c->transport.websocket.sock = c == &s_brokers[1].client ? 3 : 2;
	return 0;
}
int mqtt_abort(struct mqtt_client *c) { ARG_UNUSED(c); return 0; }
int mqtt_input(struct mqtt_client *c)
{
	struct mqtt_evt evt = { MQTT_EVT_CONNACK, 0 };
	c->evt_cb(c, &evt);
	return 0;
}
int mqtt_keepalive_time_left(const struct mqtt_client *c) { ARG_UNUSED(c); return 60000; }
int mqtt_ping(struct mqtt_client *c) { ARG_UNUSED(c); return 0; }
int mqtt_publish(struct mqtt_client *c, const struct mqtt_publish_param *p)
{
	ARG_UNUSED(c);
	if (multi) { if (c == &s_brokers[1].client && strstr((const char *)p->message.topic.topic.utf8, "/packets")) ++multi_sent; return 0; }
	if (p->message.topic.topic.size == 6 && !memcmp(p->message.topic.topic.utf8, "status", 6)) {
		if (!strstr((const char *)p->message.payload.data, "\"origin_id\"") ||
		    !strstr((const char *)p->message.payload.data, "\"stats\"") || !p->retain_flag) bad_status = 1;
		++retained_status;
	}
	if (p->message.topic.topic.size == 7 && !memcmp(p->message.topic.topic.utf8, "packets", 7)) {
		if (phase == 2) return -EIO;
		++sent;
	}
	return 0;
}
int zsock_getaddrinfo(const char *host, const char *port, const struct zsock_addrinfo *hints,
		     struct zsock_addrinfo **out)
{
	ARG_UNUSED(host); ARG_UNUSED(port); ARG_UNUSED(hints);
	resolved.ai_addr = (struct sockaddr *)&addr;
	*out = &resolved;
	return 0;
}
void zsock_freeaddrinfo(struct zsock_addrinfo *r) { ARG_UNUSED(r); }
static void queue_packet(void)
{
	size_t size;
	strcpy(mqtt_publisher_stage(&size), "sample");
	mqtt_publisher_commit(MQTT_PUB_TOPIC_PACKETS, 6);
}
int zsock_poll(struct zsock_pollfd *fds, unsigned count, int timeout)
{
	ARG_UNUSED(timeout);
	if (++polls > 20) longjmp(finished, 2);
	now += 100;
	if (multi) {
		if (fds[0].fd != 3) longjmp(finished, 4);
		fds[0].revents = s_brokers[1].connected ? 0 : ZSOCK_POLLIN;
		if (multi_sent == 8) {
			strcpy(mqtt_publisher_stage(NULL), "sample");
			mqtt_publisher_commit_packet(4, 6);
		} else if (multi_sent == 9) longjmp(finished, 3);
		return 0;
	}
	if (count != 1 || fds[0].fd != 2 || !s_broker.open) longjmp(finished, 2);
	fds[0].revents = s_broker.connected ? 0 : ZSOCK_POLLIN;
	if (phase == 0 && mqtt_publisher_is_connected()) {
		queue_packet(); phase = 1;
	} else if (phase == 1 && sent == 1) {
		phase = 2; queue_packet();
	}
	return 0;
}


void mqtt_test_wait(void)
{
	if (multi) longjmp(finished, 2);
	if (phase == 2 && !s_broker.open) longjmp(finished, 1);
}
static void connected_status(void)
{
	char *payload = mqtt_publisher_stage(NULL);
	strcpy(payload, "{\"origin_id\":\"test\",\"stats\":{\"uptime_secs\":42}}");
	mqtt_publisher_commit(MQTT_PUB_TOPIC_STATUS, strlen(payload));
}
int test_mqtt_single(void)
{
	static struct ObserverCreds primary;
	memset(&primary, 0, sizeof(primary));
	strcpy(primary.mqtt_host, "one");
	primary.mqtt_tls = 1; primary.mqtt_port = 443;
	s_connected = 0; s_pub_queue.count = 0; s_pub_events.bits = 0;
	now = 0; phase = 0; polls = 0; sent = 0; bad_status = 0; retained_status = 0;
	test_auth_init();
	mqtt_publisher_set_time(1700000000);
	mqtt_publisher_start(&primary, "observer", "status", "packets");
	mqtt_publisher_set_connect_cb(connected_status);
	int result = setjmp(finished);
	if (!result) mqtt_thread_fn(NULL, NULL, NULL);
	return result == 1 && !bad_status && retained_status == 1 && sent == 1 && !session_up && !mqtt_publisher_is_connected();
}

int test_mqtt_stats(void)
{
	if (!test_mqtt_single()) return 0;
	char status[160];
	mqtt_publisher_format_status(status, sizeof(status));
	if (strcmp(status, "disconnected queued=2 sent=1 dropped=0 failed=1 last_error=-5")) return 0;
	strcpy(mqtt_publisher_stage(NULL), "status");
	mqtt_publisher_commit(MQTT_PUB_TOPIC_STATUS, 6);
	for (unsigned i = 0; i < 8; ++i) queue_packet();
	mqtt_publisher_format_status(status, sizeof(status));
	if (strcmp(status, "disconnected queued=9 sent=1 dropped=1 failed=1 last_error=-5")) return 0;
	mqtt_publisher_reconnect();
	mqtt_publisher_format_status(status, sizeof(status));
	if (!strstr(status, "queued=9 sent=1 dropped=1 failed=1")) return 0;
	char bounded[8] = {0};
	bounded[7] = 'X';
	mqtt_publisher_format_status(bounded, 7);
	return bounded[6] == 0 && bounded[7] == 'X';
}

int test_mqtt_multiple(void)
{
	static struct ObserverCreds creds;
	memset(&creds, 0, sizeof(creds)); observer_creds_init(&creds);
	creds.mqtt_configured = 1;
	for (unsigned i = 0; i < 2; ++i) {
		mqtt_slot_init(&creds.slots[i], "custom");
		snprintf(creds.slots[i].server, sizeof(creds.slots[i].server), "wss://%s", i ? "two" : "one");
		strcpy(creds.slots[i].audience, i ? "two" : "one");
	}
	multi = 1; multi_sent = 0; polls = 0; now = 0;
	s_connected = 0;
	test_auth_init(); mqtt_publisher_set_time(1700000000);
	mqtt_publisher_start(&creds, "observer", "status", "packets");
	mqtt_publisher_set_connect_cb(NULL);
	creds.slots[0].filter = 0; creds.slots[1].filter = 1u << 4;
	strcpy(mqtt_publisher_stage(NULL), "sample");
	mqtt_publisher_commit_packet(2, 6); mqtt_publisher_commit_packet(4, 6);
	if (s_brokers[0].queue.count || s_brokers[0].filtered != 2 ||
	    s_brokers[1].queue.count != 1 || s_brokers[1].filtered != 1) return 0;
	k_msgq_purge(&s_brokers[1].queue);
	creds.slots[0].filter = UINT16_MAX;
	for (unsigned i = 0; i < 8; ++i) mqtt_publisher_commit_packet(4, 6);
	int result = setjmp(finished);
	if (!result) mqtt_thread_fn(NULL, NULL, NULL);
	if (result != 2 || s_brokers[0].open || s_brokers[0].error != (unsigned)-ECONNREFUSED) return 0;
	result = setjmp(finished);
	if (!result) mqtt_thread_fn((void *)1, NULL, NULL);
	if (result != 3 || multi_sent != 9 || s_brokers[0].dropped != 1 ||
	    s_brokers[1].dropped || !mqtt_publisher_is_connected() || !session_up) return 0;
	mqtt_publisher_reconnect_slot(0);
	return s_brokers[0].events.bits == PUB_RECONNECT_BIT && !s_brokers[1].events.bits &&
		strcmp(s_brokers[0].client_id, s_brokers[1].client_id);
}

int test_mqtt_transports(void)
{
	static struct ObserverCreds creds;
	memset(&creds, 0, sizeof(creds)); observer_creds_init(&creds);
	creds.mqtt_configured = 1;
	test_auth_init(); mqtt_publisher_set_time(1700000000);
	mqtt_publisher_start(&creds, "observer", "status", "packets");
	mqtt_publisher_set_time(1699999999);
	if (mqtt_test_wall_time != 1700000000) return 0;
	mqtt_publisher_set_time(1700000000);
	struct broker *b = &s_brokers[0];
	mqtt_slot_init(&b->pending, "tennmesh"); apply_config(b);
	if (!setup_client(b) || b->client.transport.type != MQTT_TRANSPORT_NON_SECURE ||
	    !b->client.user_name || !b->client.password || b->endpoint.port != 1883) return 0;
	b->client.transport.tcp.sock = 7;
	if (get_sock(b) != 7) return 0;
	mqtt_slot_init(&b->pending, "meshrank"); strcpy(b->pending.token, "test"); apply_config(b);
	if (!setup_client(b) || b->client.transport.type != MQTT_TRANSPORT_SECURE ||
	    b->client.user_name || b->client.password || b->client.will_retain ||
	    b->client.transport.tls.config.peer_verify != TLS_PEER_VERIFY_REQUIRED ||
	    !b->client.transport.tls.config.sec_tag_count) return 0;
	b->client.transport.tls.sock = 9;
	if (get_sock(b) != 9 || !strstr(b->packets_topic, "meshrank/uplink/test/")) return 0;
	mqtt_slot_init(&b->pending, "waev"); apply_config(b);
	if (!setup_client(b) || b->client.transport.type != MQTT_TRANSPORT_SECURE_WEBSOCKET ||
	    b->endpoint.lifetime != 3300 || b->renew_at != 3000000 || b->client.will_retain ||
	    !test_auth_valid((const char*)b->client.user_name->utf8, b->client.user_name->size,
	        (const char*)b->client.password->utf8, b->client.password->size, "mqtt.waev.app")) return 0;
	b->client.transport.websocket.sock = 11;
	return get_sock(b) == 11 && !strcmp(b->client.transport.websocket.config.url, "/mqtt");
}
