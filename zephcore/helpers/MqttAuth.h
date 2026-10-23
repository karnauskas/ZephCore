/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
namespace mesh { class LocalIdentity; }
void mqtt_auth_init(const mesh::LocalIdentity &identity);
extern "C" {
#endif
/* MeshCore token: base64url header/payload, hex Ed25519 signature. */
bool mqtt_auth_credentials(const char *audience, uint32_t epoch,
			   char *username, size_t username_size,
			   char *token, size_t token_size);
#ifdef __cplusplus
}
#endif
