#pragma once
#define TLS_PEER_VERIFY_NONE 0

#include <stddef.h>
#define TLS_PEER_VERIFY_REQUIRED 2
#define TLS_CREDENTIAL_CA_CERTIFICATE 1
static inline int tls_credential_add(int tag, int type, const void *data, size_t len) { (void)tag; (void)type; (void)data; (void)len; return 0; }
static inline int tls_credential_delete(int tag, int type) { (void)tag; (void)type; return 0; }
