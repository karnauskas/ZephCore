# ADR 0016 — MQTT WebSockets and node identity authentication

Status: accepted (2026-10-23)

## Decision

Observers and repeater uplinks use a single MQTT-over-WebSocket connection to the
root endpoint `/`, normally WSS on port 443. The existing host/port/TLS CLI and
legacy credential byte layout stay intact; static username/password fields are
unused. New connections authenticate with `v1_<UPPERCASE_PUBLIC_KEY>` and a token
signed by a startup copy of the node identity. Claims are `publicKey`, `iat`,
`exp=iat+3600`, and `aud` equal to the configured broker hostname. SNTP time is
required; an epoch/uptime anchor avoids reading mesh state on the worker.

The encoding matches [meshcore-decoder](https://github.com/michaelhart/meshcore-decoder/blob/819242d066547b6c7d1cd7d59829bc413b49c044/src/utils/auth-token.ts):
`alg=Ed25519`, base64url header/payload without padding, and hex signature.
Retained status and LWT include `origin_id`. Tokens and private keys are not logged.

## Consequences

One worker, one TLS/WebSocket session and the eight-message queue remain. There
is no secondary broker configuration, CLI, session or display indicator. Old
`zc.mqtt2` keys are ignored and omitted on the next prefs save. No binary codec
fields change. The display shows MQTT up/down.

Transport-boundary host tests exercise the worker, signed credentials, root
WebSocket path, correct socket polling, status/LWT and session cleanup. Crypto
tests verify signatures, timestamps, expiry, audience and buffer bounds.

The earlier single-broker hardware failure remains unconfirmed: its log shows
successful WiFi/SNTP followed by `-2`. Connection-stage logging and a temporary
WebSocket debug build distinguish upgrade, MQTT CONNACK and socket-read failures.

Removing the second broker saves 1,904 bytes flash and 4,364 bytes static DRAM
on the TTGO observer diagnostic build (2,936 DRAM0 and 1,428 DRAM1), with the same
network logging configuration. The queue, kernel heap and TLS heap are unchanged.
