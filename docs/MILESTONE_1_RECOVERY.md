# Milestone 1 — Recover from failures (P0)

## Implementation

- SX1262 recovery: retain the existing SX126x parked-RX, RX-latch deadline,
  CAD-timeout REST-to-RX recovery and documented AGC remedy. Each addresses a
  named fault; silence alone is not a failure signal.
- Watchdog: software mechanisms only, as required by DESIGN §8.7. Existing
  TX/CAD/RX bounds and fatal-error reboot remain the supervision mechanisms.
- Radio initialization checks: ready driver, configuration accepted, RX armed;
  success logging and post-start actions require those checks to pass.
- Boot-loop protection: a durable pending-start record and saturated attempts
  identify three incomplete starts of the same modem candidate.
- Safe configuration rollback: restore only a validated, different modem
  snapshot that previously completed startup. No factory reset or identity loss.

The implementation and limits are in [ADR 0014](adr/0014-radio-startup-recovery.md).
The milestone is not field-verified until the hardware checks below pass.

## Host acceptance

- No snapshot before successful startup confirmation.
- Three incomplete starts restore the prior modem settings on the fourth start.
- New candidates restart the count; malformed counts/snapshots cannot authorize
  an unrelated rollback; attempts saturate without inventing a default snapshot.
- Confirmation resets pending/count and updates the snapshot.
- Both JSON roles round-trip the checkpoint; legacy binary bytes stay unchanged.

## Build verification (2026-10-23)

The quick host profile passes 144 cases. Builds pass for
`ttgo_lora32/esp32/procpu` (companion, repeater, observer and KISS),
`rak4631` (room server), and `xiao_nrf52840` (companion and repeater).
No hardware execution or fault injection was performed.

Matched build deltas against the preceding adaptive-power-budget builds:

| Target / role | Flash delta | RAM delta |
|---|---:|---:|
| ttgo_lora32 companion | +1,312 B | +64 B |
| ttgo_lora32 repeater | +1,328 B | +64 B |
| ttgo_lora32 observer | +1,200 B | +40 B |
| ttgo_lora32 KISS | +880 B | +40 B |
| rak4631 room server | +1,232 B | +64 B |

ESP32 IRAM/other DRAM regions are unchanged; its RAM deltas above are in the
binding DRAM segment. The XIAO builds were not paired with a matching baseline,
so no size delta is claimed for them.

## Hardware acceptance still required

1. Run companion and repeater on SX1262 hardware, record firmware hash and board.
2. Inject `lora_config()` rejection and RX-arm failure separately. Confirm no
   failed configuration proceeds into RX/TX and no false startup success log.
3. Confirm one saved-setting restoration on `-EINVAL`/`-ENOTSUP`; inject an I/O
   error and verify it does not trigger a speculative config rollback.
4. Interrupt the same candidate startup three times before event-loop entry.
   Confirm restoration before the fourth attempt and unchanged identity,
   unrelated preferences, contacts and channels.
5. Interrupt checkpoint writes. Confirm atomic prefs remain loadable, a failed
   save is reported, and no unverified backup is promoted.
6. Reproduce the named parked-RX/CAD-timeout faults and verify the existing
   bounded recovery. Validate ordinary long receptions and quiet networks are
   not reset. Read the recovery counters, not only the success log.
7. Repeat successful boot and fault cases on another radio family to verify the
   common adapter path. A build alone is not hardware validation.

No claim is made that software can recover a bootloader/driver-init loop that
prevents the application from reaching its persistent checkpoint, or identify
a valid but wrong network channel without a network-specific health criterion.
