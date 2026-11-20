#include <mesh/Utils.h>
#include <stdexcept>
#include <cstring>

int mac_test_calls;
bool mac_test_fail;

// Python hmac/SHA-256 vectors: Public's zero-padded key, ciphertext 00..0f / 00..1f.
// The PSA primitive is a boundary; production framing, comparison and filter run here.
static int compute_hmac_truncated(const uint8_t *key, size_t key_len,
    const uint8_t *data, size_t data_len, uint8_t *out, size_t out_len)
{
    ++mac_test_calls;
    if (mac_test_fail) return -1;
    const uint8_t expected[32] = {0x8b,0x33,0x87,0xe9,0xc5,0xcd,0xea,0x6a,
        0xc9,0xe5,0xed,0xba,0xa1,0x15,0xcd,0x72};
    if (key_len != 32 || memcmp(key, expected, 32) || out_len != 2 ||
        (data_len != 16 && data_len != 32)) throw std::runtime_error("unexpected HMAC input");
    for (size_t i = 0; i < data_len; ++i)
        if (data[i] != i) throw std::runtime_error("unexpected ciphertext");
    // Filled from independently generated vectors, not firmware output.
    const uint8_t tags[2][2] = {{0x47,0x0c},{0x50,0x30}};
    memcpy(out, tags[data_len == 32], 2);
    return 0;
}

namespace mesh {
#include "mac_validation.inc"
}
