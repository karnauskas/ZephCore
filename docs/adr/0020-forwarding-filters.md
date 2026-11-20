# ADR 0020 — Forwarding filters

Status: Accepted, 2026-11-20.

## Context

Repeaters had region, hop, loop and wardriving controls but lacked the requested
SaveMeshcore payload, one-byte-path and Public-channel forwarding filters.
The reference is [SaveMeshcore b9b71db0](https://github.com/aigarslv/SaveMeshcore/tree/b9b71db0d949682f59540fa979d15cee83be7fc8),
specifically `ForwardFilter.h`, `ChannelMatch.cpp`, `CliParse.h` and
`MyMesh::handleFilterCommand`. Local payload assignments still leave `0x0C`–`0x0E`
unassigned, with `GRP_DATA=0x06` and `RAW_CUSTOM=0x0F`.

## Decision

Share one forwarding policy between repeaters and off-grid companions on every
board. Preserve reference defaults: `1byte off`, `data on`, `public off`.
Apply the policy only in forwarding hooks, preserving local delivery and replies.
Keep existing remote authentication and expose the same commands and successful
responses through the shared CLI. Only changed settings trigger persistence;
failed saves restore the previous value.

Public matching checks channel hash and a two-byte HMAC over block-aligned
ciphertext using the published Public key, zero-padded to 32 bytes. It does not
decrypt. Reuse the core HMAC backend through `Utils::verifyMAC`. A hash collision
alone passes; a 16-bit MAC collision remains possible. Crypto failure does not
classify a packet as Public.

Persist three fields under `zc` in `prefs.json`, including explicit zeros.
Missing fields keep initialized defaults; frozen binary codecs are unchanged.
Do not port SaveMeshcore's separate preferences file or enforced radio/advert
settings. Rooms and observers stay non-forwarding.

## Consequences

Existing installations without a saved setting now suppress forwarding group
data by default. `filter data off` restores the underlying forwarding behavior.
Three bytes of preferences plus struct alignment are added; filtering uses fixed
stack buffers and no explicit dynamic allocation. MAC work runs only for group
packets with Public's hash when its filter is enabled.

Host tests exercise production filter/CLI/JSON logic and MAC validation with a
deterministic HMAC boundary and independently generated tags. They do not execute
PSA cryptography or hardware. Hardware forwarding tests remain outstanding.

## Validation

The quick host profile passes 203 cases. Built XIAO nRF52840 repeater,
XIAO ESP32-S3 repeater (sysbuild), RAK4631 companion and ProMicro SX1262 room
server. Against `3624f90` with
identical nRF Kconfig settings, the linker reports these byte deltas:

| Build | Flash region | RAM region | ELF data + BSS |
| --- | ---: | ---: | ---: |
| XIAO nRF52840 repeater | +1,028 | +64 | +16 |
| RAK4631 companion | +1,016 | 0 | +16 |

Region deltas include linker alignment; these are not runtime stack peaks.
No hardware execution or radio measurements were performed.
