/* SPDX-License-Identifier: MIT */
#include "MqttAuth.h"
#include <mesh/Identity.h>
#include <stdio.h>
#include <string.h>

static mesh::LocalIdentity s_identity;

void mqtt_auth_init(const mesh::LocalIdentity &identity)
{
	s_identity = identity;
}

static bool base64url(const char *input, size_t len, char *out, size_t size)
{
	static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
	const size_t needed = len / 3 * 4 + (len % 3 ? len % 3 + 1 : 0);
	if (size <= needed) return false;
	unsigned bits = 0, count = 0;
	size_t used = 0;
	for (size_t i = 0; i < len; ++i) {
		bits = (bits << 8) | (uint8_t)input[i];
		count += 8;
		while (count >= 6) {
			count -= 6;
			out[used++] = alphabet[(bits >> count) & 63];
		}
	}
	if (count) out[used++] = alphabet[(bits << (6 - count)) & 63];
	out[used] = '\0';
	return true;
}

bool mqtt_auth_credentials(const char *audience, uint32_t epoch,
			   char *username, size_t username_size, char *token, size_t token_size)
{
	if (epoch < 1700000000u || epoch > UINT32_MAX - 3600 ||
	    username_size < 68 || !*audience || strlen(audience) > 127) return false;
	for (const char *p = audience; *p; ++p) {
		if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
		      (*p >= '0' && *p <= '9') || *p == '.' || *p == '-')) return false;
	}
	static const char hex[] = "0123456789ABCDEF";
	memcpy(username, "v1_", 3);
	for (unsigned i = 0; i < PUB_KEY_SIZE; ++i) {
		username[3 + i * 2] = hex[s_identity.pub_key[i] >> 4];
		username[4 + i * 2] = hex[s_identity.pub_key[i] & 15];
	}
	username[67] = '\0';
	char claims[320];
	int n = snprintf(claims, sizeof(claims),
		"{\"publicKey\":\"%s\",\"iat\":%u,\"exp\":%u,\"aud\":\"%s\"}",
		username + 3, (unsigned)epoch, (unsigned)(epoch + 3600), audience);
	if (n < 0 || n >= (int)sizeof(claims)) return false;
	static const char header[] = "eyJhbGciOiJFZDI1NTE5IiwidHlwIjoiSldUIn0.";
	if (token_size < sizeof(header)) return false;
	memcpy(token, header, sizeof(header) - 1);
	if (!base64url(claims, n, token + sizeof(header) - 1, token_size - sizeof(header) + 1)) return false;
	size_t len = strlen(token);
	if (token_size < len + 130) return false;
	uint8_t sig[SIGNATURE_SIZE];
	s_identity.sign(sig, (const uint8_t *)token, len);
	token[len++] = '.';
	for (unsigned i = 0; i < sizeof(sig); ++i) {
		token[len++] = hex[sig[i] >> 4];
		token[len++] = hex[sig[i] & 15];
	}
	token[len] = '\0';
	return true;
}
