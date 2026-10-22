# 0015: Companion location privacy before packet construction

Status: Accepted

## Context

The companion constructs its position in two self-advertisement paths (send and
export) and in GPS telemetry, including responses piggybacked on path returns.
Filtering just advertisements leaves telemetry able to disclose the same position.
Packets can also wait in the outbound queue while a node enters a private zone.

## Decision

A main-thread `PrivacyEngine` filters firmware-owned coordinates before advert
encoding or CayenneLPP encoding/encryption. Settings are companion-only JSON under
`zc.privacy`; existing files default to no restrictions. No legacy prefs bytes or
on-air formats change. Four named circular zones always suppress all firmware-owned
location. There is no per-packet exemption. Zone matching uses raw coordinates,
including the last GPS fix when available, before any fuzzing or precision reduction.

The engine supports boot-stable geodesic fuzzing, meter-grid precision, daily
blackouts using the existing whole-hour `tz.offset`, a movement threshold and a
minimum delay. An eight-snapshot, time-spaced RAM history bounds memory while
retaining eligible delayed samples under frequent GPS fixes. Reboot and policy
changes clear the history. No packets are scheduled by the engine: an existing
advertisement or telemetry request releases an eligible snapshot. An active
blackout with an unset calendar withholds location. Invalid privacy settings also
withhold location until repaired or cleared locally.
When an existing JSON file cannot be read/parsed, the companion's legacy recovery
sets a persisted location hold: legacy bytes have no privacy policy to restore.
`privacy off` clears that hold; missing prefs on a fresh device keep the normal defaults.

Location-bearing packets carry a local-only generation tag. The dispatcher
rechecks the policy immediately before starting TX and drops invalid packets,
including encrypted telemetry and path-return responses. Policy changes and
observed zone entry invalidate prior generations. Chat remains available.

Commands are accepted from USB serial and authenticated BLE NUS, including the
app's run-CLI opcode or v-contact. Authorization uses the frame's actual transport
and an explicit local invocation flag; a zero remote timestamp grants no local
rights. TCP cannot configure privacy merely because a BLE client is connected.
Remote timezone changes are denied while a privacy schedule is active.

## Limits and consequences

- Names, public identity keys and traffic timing remain linkable. This is location
  control, not identity rotation or anonymity. Fuzzing is an obfuscating offset,
  not a cryptographic privacy guarantee; a known true point can reveal the offset.
- User-authored chat and opaque app/raw payloads are allowed and are not inspected
  for coordinates. Other nodes' forwarded packets are unchanged. App-cached exports
  and packets already on air cannot be recalled. Local GPS/UI and local device-info
  replies retain the true position; applications must respect their own privacy rules.
- Zones can only evaluate known coordinates. Without a fresh GPS fix a moving node
  cannot discover entry into a zone; do not treat saved coordinates as live geofencing.
- Fuzz and precision remove altitude. Precision is an approximate geographic grid,
  with longitude scaled at the quantized latitude. Zone radii include a one-meter
  margin for the stored microdegree centers. Both raw and transformed points are checked.
- Movement compares the raw delayed sample with the last location released for packet
  construction/export, shared across paths. A later allocation/TX failure can therefore
  withhold subsequent stationary replies. Privacy takes precedence over freshness.
- Delay is at least the configured duration. Snapshot spacing and request timing can
  make a released position older. Entering a zone restarts the delay outside it.
- Settings and zone coordinates are stored in plaintext `prefs.json`; disabling
  privacy clears its settings but does not securely erase old flash blocks. Existing
  companion response broadcasting remains; connect only trusted local clients.
- RAM is fixed: `PrivacyPrefs` in each `NodePrefs`, the companion's eight snapshots
  and movement state, and one 32-bit local tag per packet-pool slot. No heap is added
  to the packet path. The hardware-independent rules have host tests.
