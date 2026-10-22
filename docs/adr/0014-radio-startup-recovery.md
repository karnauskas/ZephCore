# 0014: Validate radio startup and retain a last-known-good modem configuration

## Status

Accepted, 2026-10-23. User requested last-known-good radio restoration and
software recovery only; the prohibition on hardware watchdogs remains.

## Context

`LoRaRadio::configure()` already records a failed `lora_config()` by clearing
`_config_cached`, but `startReceive()` previously proceeded to RX anyway, and
`begin()` printed `radio started` even when configuration or RX arming failed.
The same adapter's TX path also proceeded after configuration failure. This is
a source-proven error-propagation defect; no new silicon failure is assumed.

SX126x already has named parked-RX, RX-latch and CAD-timeout recovery. A further
reset-on-silence watchdog would confuse a quiet network with failed hardware.
The existing fatal handler reboots on a CPU fault and reports a crash record,
but there was no persisted record of incomplete radio startup or known-good
modem settings to restore after a failing candidate.

## Decision

Fail startup checks on unready driver, failed modem configuration or failed RX
arming. Report success only after RX arms. Gate operations that could write to
an unready peripheral. On TX configuration failure, release the existing TX/PM
ownership without starting a transmission.

Store a JSON-only checkpoint under `zc.radio_good` in the existing `prefs.json`.
Record only frequency, bandwidth, SF, CR and TX power. A four-decimal-normalized
fingerprint matches JSON float precision; its checksum detects accidental
snapshot corruption, not malicious editing. Legacy preference codecs stay frozen.

Persist an attempt before applying the radio config; mark it complete at the
composition root's event-loop entry after the radio startup checks passed.
Promote only that completed boot's modem values. After three incomplete boots
with the same candidate, restore a valid different snapshot before the next
attempt. This counts interruption as well as crash; it does not diagnose which
component caused the incomplete boot. Changed candidates reset the count.
Invalid snapshots are never replaced with invented defaults.

On `-EINVAL`/`-ENOTSUP` from modem configuration, permit one restore/configure
attempt if a durable startup checkpoint and a valid different snapshot exist.
Do not use that path for arbitrary I/O errors, RX arming failure or a missing
chip. Keep the existing named SX126x software recovery mechanisms unchanged.
No proactive reboot, hardware watchdog, extra delay, polling loop or read-back
probe is introduced.

## Consequences

Normal boot incurs two atomic prefs writes (attempt and confirmation); runtime
saves may include checkpoint metadata in the same prefs file. A write failure is
visible and protection is not claimed durable. Companion saves return a bool
so this failure can be observed instead of being discarded.

The checkpoint is 24 bytes per NodePrefs on the tested 32-bit builds, plus save
callback/context and adapter state. No packet-path heap or format change occurs.
KISS does not persist prefs and receives startup checks only. Early crashes
before checkpoint creation, failures of the known-good configuration itself,
valid-but-wrong network settings, live config rollback and firmware-image
rollback remain outside this mechanism. Hardware fault-injection acceptance is
required before declaring the P0 milestone field-verified.
