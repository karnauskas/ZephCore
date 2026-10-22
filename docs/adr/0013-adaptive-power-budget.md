# 0013: Opt-in adaptive power budget

## Status

Accepted, 2026-10-23.

## Context

Periodic repeater/room adverts and common GPS standby scheduling use configured
intervals without considering reserve. Telemetry has no periodic broadcast
scheduler: `ZephyrSensorManager::querySensors()` reads requested sensors, and
optional UI pages use the same reader. The existing companion `PowerPolicy`
protects against low voltage but does not reduce ongoing work before shutdown.

## Decision

Add one opt-in JSON-only setting, `power.budget`, default off. Bind the shared
policy to live preferences and the board at each composition root. Use the
existing battery curve for reserve. Three samples at least 30 seconds apart
select factor 2 at ≤20% and factor 4 at ≤10%; hysteresis recovers at 15% and 25%.
These are initial policy choices, not measured lifetime or state-of-charge
accuracy guarantees. Detectable external power and readings below 2000 mV
reset to factor 1. Policy sampling rides existing wakes with no new timer.

Scale new local/flood advert and GPS standby schedules, including existing
network-adaptive advert delays. Preserve zero timers, manual adverts, packet
forwarding and essential battery protection. Cap durations to the signed
32-bit deadline horizon; do not rewrite configured intervals. Existing timers
complete before the next scheduling decision, including on charge recovery.
GPS always-on mode remains always on.

Cache successful nonempty environment/power sensor reads for 60 seconds in low
reserve and 300 seconds in critical reserve. This applies consistently to
requested telemetry and optional UI acquisition. Leave permission checks,
channel numbering and protocol replies unchanged. Do not cache read failures or
return an expired reading after a failed fetch. Do not cache battery protection
readings. No new radio transmit-power policy or packet suppression is added.

## Consequences

The sensor cache is fixed at 16 slots × 44 bytes = 704 bytes on the measured
32-bit builds, plus singleton and preference alignment costs. Without sensor
support the cache is absent. No heap or additional polling/wake source is added.
The flag is stored under `zc.power_budget` for both JSON layouts; legacy binary
preferences remain frozen. Unknown external power and uncalibrated battery
curves limit accuracy. Quiet nodes can take longer to confirm transitions, and
in-flight timer durations do not change until rescheduling. Savings and hardware
behavior still need measurement.
