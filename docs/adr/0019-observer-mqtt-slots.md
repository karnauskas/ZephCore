# ADR 0019 — Resource-bounded observer MQTT slots and presets

Status: accepted (2026-11-20). Supersedes ADR 0016's single-broker, root-WebSocket-only
and identity-auth-only restrictions; its signed-token encoding remains unchanged.

## Context

Observer operators need to publish to multiple collectors using their existing
broker presets and CLI scripts. A single connection and a shared worker cannot
isolate a collector's connection setup from other collectors' publishing.

The compatibility reference is
[agessaman/MeshCore at 860011d2](https://github.com/agessaman/MeshCore/tree/860011d2dc4d01ecc3719f795821e4a755970a53),
specifically `MQTT_IMPLEMENTATION.md`, `MQTTPresets.h`, `CommonCLI_Observer.cpp`
and `MQTTPresetPolicy.h`. The preset data is tested against that revision's
unchanged header, including CA roots, URLs, credentials and retain policy.

## Decision

Use independent bounded queues and workers for each active MQTT slot. DNS,
connection setup, polling, keepalive and existing connection backoff belong to
that slot. MQTT clients and transmit/authentication buffers are not shared.
Changing a slot reconnects only that slot; packet filters apply at enqueue time.
Client IDs include the device public key and slot number to avoid collisions.

Support TCP, TLS, WS and WSS. Presets retain the reference authentication,
WebSocket paths, certificate roots, topics, keepalive and token lifetime.
Custom brokers support username/password, JWT audience and topic templates.
Custom TLS retains the earlier unverified-TLS behavior; presets with a CA verify
the peer, including validity dates after SNTP anchors the libc clock. That clock
is advanced only, preserving the wall-clock invariant. JWT credentials are signed on connection, with renewal before expiry
for presets that enforce it; the reference's `waev` exception is retained.

`CONFIG_ZEPHCORE_MQTT_CONNECTIONS` budgets sockets, WebSocket/TLS contexts,
worker stacks and queues. Observer defaults are five with PSRAM and at least
256 KiB chosen SRAM, two with sufficient SRAM or PSRAM, and one otherwise.
Repeater uplinks default to one. PSRAM builds place TLS heap and queue payload
storage in external RAM; kernel control objects remain internal. No packet-path
heap allocation is introduced. Published limits are compile-time budgets;
simultaneous live TLS handshakes and packet reception still need hardware tests.

Persist network settings under `zc.uplink` in `prefs.json`, bound at runtime to
the role's credentials. The 404-byte `obs_creds` prefix is read only for migration;
an existing primary becomes a custom WebSocket/JWT slot with its one-hour token
lifetime, and saved JSON overrides
the legacy data. New observers select `analyzer-us` and `analyzer-eu`; the remaining
slots are `none`. Preserve all five stored slots across active-slot limits.
Legacy MeshCore binary preference codecs and offsets are unchanged.

## Consequences

The observer remains listen-only, and packet JSON is unchanged. LoRa alert TX,
active neighbour discovery, SNMP, OTA/web-portal commands, owner/email claims and
raw-topic publishing are outside this integration. Existing radio commands keep
their ZephCore behavior. The supported MQTT CLI subset is documented explicitly.

Host tests pin preset parity, endpoint/topic resolution, CLI parsing, filters,
JSON migration, binary-codec stability, per-slot queues, failure isolation and
transport selection. CI compiles observer resource classes with and without
PSRAM and the existing repeater uplink composition.

## Build measurements

Observer builds against HEAD `e29a71d`, using the same toolchain and board configuration.
Values are bytes from the linker region report; DRAM0 is one region, not total RAM.

| Board | Slots | Flash delta | DRAM0 delta | External DRAM delta |
| --- | ---: | ---: | ---: | ---: |
| TTGO LoRa32 | 1 | +7,920 | +3,992 | — |
| Heltec V3 | 2 | +8,624 | +6,552 | — |
| Heltec V4 | 5 | +8,624 | -26,680 | +91,456 |

The V4 moves the TLS heap and queue payloads to PSRAM. TTGO uses 193,496 of
196,608 bytes in DRAM0; its default stays at one connection. These are compile
measurements, not runtime heap/stack peaks. No hardware or live-broker testing
was performed for this change.
