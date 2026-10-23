#include "test.h"
#include <cstring>
extern "C" int test_mqtt_single(void);
TEST(mqtt_worker, "UNIT-MQTT-001", "Single MQTT worker publishes and clears its session after a publish failure") {
	CHECK(test_mqtt_single());
}
#include "MqttAuth.h"
#include <mesh/Identity.h>

static mesh::LocalIdentity auth_identity()
{
	uint8_t seed[32];
	for (unsigned i = 0; i < sizeof(seed); ++i) seed[i] = i;
	mesh::LocalIdentity id;
	id.fromSeed(seed);
	return id;
}
extern "C" void test_auth_init(void) { mqtt_auth_init(auth_identity()); }

static std::string decode_claims(const std::string &token)
{
	const auto first = token.find('.'), last = token.rfind('.');
	std::string out;
	unsigned bits = 0, count = 0;
	const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
	for (size_t i = first + 1; i < last; ++i) {
		auto v = alphabet.find(token[i]);
		if (v == std::string::npos) return "";
		bits = (bits << 6) | v; count += 6;
		if (count >= 8) { count -= 8; out += (char)(bits >> count); }
	}
	return out;
}
extern "C" int test_auth_valid(const char *user, size_t user_len, const char *raw, size_t len, const char *host)
{
	std::string token(raw, len), username(user, user_len);
	const auto last = token.rfind('.');
	if (last == std::string::npos || token.size() - last - 1 != 128 || user_len != 67) return 0;
	uint8_t sig[64], pub[32];
	if (!mesh::Utils::fromHex(sig, sizeof(sig), token.c_str() + last + 1) ||
	    !mesh::Utils::fromHex(pub, sizeof(pub), username.c_str() + 3)) return 0;
	auto id = auth_identity();
	if (username.substr(0, 3) != "v1_" || memcmp(pub, id.pub_key, sizeof(pub))) return 0;
	const auto claims = decode_claims(token);
	if (claims.find("\"aud\":\"" + std::string(host) + "\"") == std::string::npos ||
	    claims.find("\"publicKey\":\"" + username.substr(3) + "\"") == std::string::npos) return 0;
	return id.verify(sig, (const uint8_t *)token.data(), last);
}
TEST(mqtt_auth_token, "UNIT-MQTT-005", "WSS tokens sign the node identity with per-host audience and bounded lifetime") {
	test_auth_init();
	char user[68], token[640], second[640];
	CHECK(mqtt_auth_credentials("one.example", 1700000000, user, sizeof(user), token, sizeof(token)));
	CHECK(std::string(token).find("eyJhbGciOiJFZDI1NTE5IiwidHlwIjoiSldUIn0.") == 0);
	CHECK(test_auth_valid(user, strlen(user), token, strlen(token), "one.example"));
	CHECK(decode_claims(token).find("\"iat\":1700000000,\"exp\":1700003600") != std::string::npos);
	CHECK(mqtt_auth_credentials("two.example", 1700000000, user, sizeof(user), second, sizeof(second)));
	CHECK(strcmp(token, second) != 0);
	CHECK(test_auth_valid(user, strlen(user), second, strlen(second), "two.example"));
	CHECK(!test_auth_valid(user, strlen(user), token, strlen(token), "two.example"));
	CHECK(!mqtt_auth_credentials("one.example", 0, user, sizeof(user), token, sizeof(token)));
	CHECK(!mqtt_auth_credentials("bad\"host", 1700000000, user, sizeof(user), token, sizeof(token)));
	CHECK(!mqtt_auth_credentials("one.example", UINT32_MAX, user, sizeof(user), token, sizeof(token)));
	CHECK(!mqtt_auth_credentials("one.example", 1700000000, user, 67, token, sizeof(token)));
	CHECK(!mqtt_auth_credentials("one.example", 1700000000, user, sizeof(user), token, 10));
	std::string longest(127, 'a');
	CHECK(mqtt_auth_credentials(longest.c_str(), 1700000000, user, sizeof(user), token, sizeof(token)));
	CHECK(test_auth_valid(user, strlen(user), token, strlen(token), longest.c_str()));
}

extern "C" int test_mqtt_stats(void);
TEST(mqtt_stats, "UNIT-MQTT-008", "MQTT packet diagnostics count queue drops send success and errors without counting status") {
	CHECK(test_mqtt_stats());
}

#include <helpers/MeshcoreJson.h>
TEST(mqtt_full_status, "UNIT-MQTT-009", "MQTT full status includes signed noise floor battery voltage and uptime") {
	MeshcoreStatusJson status{};
	status.status = "online"; status.epoch = 1700000000;
	status.origin = "Observer"; status.origin_id = "pubkey";
	status.radio = "869.618,62.5,8,8"; status.model = "test"; status.firmware_version = "test";
	status.battery_mv = 4100; status.uptime_secs = 321; status.noise_floor = -119;
	char json[1024];
	int size = meshcore_build_status_json(json, sizeof(json), &status);
	CHECK(size > 0 && size < (int)sizeof(json));
	CHECK(strstr(json, "\"battery_mv\":4100") != nullptr);
	CHECK(strstr(json, "\"uptime_secs\":321") != nullptr);
	CHECK(strstr(json, "\"noise_floor\":-119") != nullptr);
}
