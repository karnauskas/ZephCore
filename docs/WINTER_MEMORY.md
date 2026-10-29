# Winter Memory — seasonal power history proposal

Status: observation foundation implemented, 2026-10-26. The remaining
behaviour below is proposed; adopting it requires the evidence and ADR
described in this document.

## Implemented foundation

The CLI flags and raw RAM observations are implemented; see
[CLI commands](CLI_commands.md#observation-features-zephcore-only). The design
below describes the larger proposal. Automatic remedy selection, per-episode
policy, seasonal learning and transmission suppression are not implemented.
`winter.auto on` stores permission but reports `action=unsupported`. Voltage
summaries do not establish charging, battery presence, history coverage or a
forecast. Existing recovery and reserve protection continue independently.


## Purpose and current baseline

A repeater can conserve reserve before it becomes critically low when its
own recent history shows sustained poor replenishment. Seasonal history can
provide context once sufficient observations exist; the month alone must
never trigger suppression.

The existing `PowerBudget` confirms low reserve over three samples, applies
interval multipliers of 2 or 4, and uses hysteresis for recovery. It scales
periodic adverts and GPS standby while leaving forwarding and requested
traffic intact. It stores no history and makes no forecast. Battery-curve
reserve is an estimate, not measured available energy.

Winter Memory extends that shared policy rather than creating a second
transmission controller. Shared scheduling behaviour applies to every role
using it. Hardware observations are selected by sensor capabilities, never
board names. Roles with no eligible periodic transmissions gain no transmit
suppression behaviour.

## CLI control contract

Use two independent persistent flags, both default off:

| Proposed command | Behaviour |
|---|---|
| `set winter on\|off\|default` | Enable/disable history collection and evaluation; `default` means `off` |
| `get winter` | Report configured flag and effective state with reason: off, insufficient evidence, observe, conserving or unsupported |
| `set winter.auto on\|off\|default` | Permit scheduling changes when Winter Memory is enabled and validated; `default` means `off` |
| `get winter.auto` | Report permission to act; an enabled flag alone does not establish adequate evidence |
| `get winter.status` | Report bounded coverage/history/confidence, proposed/applied action and suppressed opportunities |
| `winter reset` | Explicitly clear learned summaries and partial windows after an installation change; retain flags and ordinary preferences |

`winter on` with `winter.auto off` is observation mode. Automatic action
requires both flags, adequate evidence and an implementation validated for
that action. Turning Winter Memory off stops its collection and scheduling
influence at the next existing decision; it retains completed history and
invalidates any partial window spanning the disabled period. Turning only
`winter.auto` off keeps observation running. Neither flag changes existing
reserve protection or the separate `power.budget` setting. The implementation
must report those independent states so `winter off` cannot be mistaken for
disabling reserve-based conservation. No catch-up adverts are emitted.

Store booleans under JSON-only `zc.winter` and `zc.winter_auto`; missing or
invalid values resolve to false. Do not modify legacy binary preferences.
Apply changes on the main thread without reboot and report persistence
failures. `winter reset` uses the existing atomic prefs save when durable
history exists and reports success only when that clearing is durable.
Mutations use existing admin permissions; observer retains its USB-only CLI.
Roles without eligible work or observations report unsupported rather than
pretending to conserve. KISS has no persistent settings CLI and is outside
these controls until an explicit interface decision is made.

The commands are implemented in observation mode. Thresholds remain
validated policy constants; this proposal adds no CLI tuning knobs for them.

## Observation quality

| Available observation | Permitted conclusion | Limit |
|---|---|---|
| Validated battery charge/discharge current and voltage | Observed net energy trend over covered intervals | Needs known polarity, units, accuracy and bounded sample gaps |
| Validated fuel-gauge state of charge | Reserve trend and replenishment episodes | Gauge corrections and battery replacement can create discontinuities |
| Battery voltage only | Voltage/reserve trend under comparable conditions | Load, temperature and relaxation can imitate charging or discharge |
| USB/external power present | An external source is detected | Does not prove the battery is connected or gaining energy |
| Missing battery, failed sensor, unknown power source | Insufficient evidence | Must not train the model as depleted reserve or poor charging |

Use measurements already acquired on existing main-thread wakes. Do not add
polling, radio transmissions or sensor reads solely to train the model.
Sparse observations are acceptable: record coverage and report insufficient
history when necessary. Never interpolate energy through a long unobserved
gap or count repeated cached values as independent evidence.

The T-LoRa V2.1-1.6 path currently supplies an ADC voltage without battery
presence or USB detection. Its reading with no battery attached cannot
authorize charging-based suppression. Voltage-only learning requires a
validated battery installation and measured evidence that the chosen trend
survives load and temperature effects; it cannot claim measured charge input.

## ProMicro SX1262 candidate installation

The repository's `promicro_sx1262` target combines an nRF52840 with a native
SX1262 driver. Its devicetree declares battery voltage on P0.31/AIN7 through
a 150 kΩ + 150 kΩ divider. The common board adapter reads nRF52840 USB VBUS
presence. No charger-status or current measurement is declared by this target.
These are source-level capabilities, not confirmation of a particular clone's
wiring or calibration.

This is a useful candidate for voltage-trend observation mode with a known
battery installed. Verify the divider and compare ADC voltage against a meter
before collecting traces. USB VBUS detection identifies that input only: a
solar charger wired directly to the battery need not assert VBUS, and VBUS
presence does not establish net charging. Classify absent VBUS as “USB not
detected,” not “solar charging absent.” Start collection in RAM; durable
seasonal summaries remain subject to the persistence design below.

For a solar trial, record panel/charger wiring, battery chemistry/capacity,
temperature and the exact SX1262 module. This target supports both bare
modules and an external-PA E22-900M30S; their transmit energy costs are not
interchangeable. Measure at the battery input rather than estimating savings
from configured dBm. The existing safe TX default remains unchanged.

Use periodic adverts as the candidate controllable load, and measure their
share against continuous reception, MCU, optional display/GPS and charger
losses. If that share is negligible, advert suppression cannot justify a
seasonal controller. For measured replenishment rather than a voltage proxy,
the installation needs a supported, identified current/fuel-gauge capability
with validated wiring and readings. Do not add a ProMicro-specific policy.

## Proposed learning model

Start with a bounded history of completed observation windows. Each summary
records observation quality, coverage, starting/ending reserve and measured
net energy only where that quantity is available. Derive recent depletion,
replenishment episodes and recovery from those summaries. Raw samples need
not be retained.

The first delivery should run in observation mode: produce a proposed action
and reason without changing scheduling. Select window lengths, minimum
coverage, persistence thresholds and recovery hysteresis from recorded traces
before enabling automatic action. No universal threshold is asserted here.

Only after recent-history prediction is validated should calendar bins be
added. A bounded annual profile can summarize comparable completed windows
by season, with coverage and age. One winter's observations establish that
winter's history, not a proven year-to-year pattern. Compare matched
installation conditions; panel, battery, orientation or charging changes
invalidate comparability and require an explicit history reset.

Elapsed durations and persistence gates use monotonic uptime. Calendar labels
require valid wall time. Unknown time disables seasonal classification but
can retain continuous recent history. Clock corrections must not manufacture
completed windows, count a window twice or attribute unobserved time to a
season. Reboot gaps are missing coverage; persisted wall timestamps alone do
not establish continuous sampling.

## Scheduling contract

Automatic conservation requires valid current reserve, sufficient recent
coverage and persistent evidence of declining replenishment. A seasonal
expectation can strengthen the explanation or forecast, but cannot override
healthy current evidence. Low-reserve protection remains authoritative when
the historical model is unavailable.

The initial eligible transmissions are periodic local and flood adverts.
Suppress an eligible scheduled advert only when the reviewed policy permits
it; preserve its configured baseline interval and disabled state. A skipped
advert schedules the next opportunity using the existing deadline mechanism;
it does not accumulate a backlog to transmit after charging recovers.

The existing default flood-advert cadence is 47 hours. Further suppression
can impair discovery and the freshness of peers' time-sync samples. Before
enabling it, define and validate a maximum effective advert gap, accounting
for the existing network-adaptive and reserve multipliers. A duration clamp
alone does not establish that network service remains adequate.

Manual adverts, forwarded traffic, acknowledgments, requested replies,
administration, battery protection and radio RX remain governed by their
existing rules. No transmit-power reduction, channel change or blanket
radio sleep is part of this feature. Telemetry is request-driven today and
must not be described as an optional periodic broadcast to suppress.

Combine historical conservation and current reserve into one scheduling
decision with an explicit cap; do not silently multiply another independent
factor onto existing factors. Disabling Winter Memory removes its influence
at the next scheduling decision without erasing history. Recovery needs
confirmed improvement with hysteresis; one sunny sample is insufficient.

## Persistence and diagnostics

Persist only bounded summaries and model version in `prefs.json`, with a
documented maximum size and checkpoint frequency. Do not add legacy binary
prefs fields or a separate settings store. Batch checkpoints with existing
atomic saves where possible; define a maximum acceptable loss of recent
history and evaluate flash wear before implementation. Do not write on each
sample or transmission. Invalid or incompatible history falls back to the
existing reserve policy without modifying unrelated settings.

Diagnostics must distinguish reserve estimate, sensor capability, recent
coverage, history age, seasonal confidence, proposed/applied action and its
reason. No battery and unknown charging are explicit states, not zero charge.
Report suppressed opportunities and effective advert gap so an operator can
assess the cost to network service. Exact reply formats and budgets for the
controls above belong in the implementation ADR and interface documentation.
No MQTT schema change is specified.

## Delivery and acceptance

1. Collect traces from an instrumented solar installation and a control node.
   Record battery chemistry/capacity, sensors, temperature, charging input,
   traffic and configuration. Establish whether optional TX is a meaningful
   share of consumption; continuous RX, MCU or WiFi may dominate instead.
2. Replay those traces through a pure, fixed-storage observation model. Compare
   its proposed actions with the existing reserve-only policy on held-out
   traces. Report false conservation, reserve prediction error, missed adverts
   and coverage. Synthetic traces verify logic, not seasonal accuracy.
3. Add opt-in scheduling only after evidence supports thresholds and the
   permitted advert gap. Record energy saved and discovery/time-sync effects
   on hardware. Do not claim longer battery life from fewer adverts alone.

Host tests must cover persistent weak charging, recovery, cloudy days with
CLI defaults, independent flag combinations, JSON round-trips, permission
checks, save failures, history retention on disable and explicit reset,
healthy reserve, low reserve without history, absent battery, stale/cache
samples, sensor errors, polarity/discontinuity handling, reboot gaps, uptime
wrap, calendar changes, corrupt persisted summaries and configuration reset.
Scheduling tests must preserve essential traffic, disabled timers, deadline
bounds, composed limits and absence of catch-up bursts. Register new cases in
`tests/catalog.json` and run the quick host profile.

Build affected roles and boards plus an additional board, including builds
without charging sensors. Report matched flash/RAM deltas, persistence costs
and actual hardware execution. Update architecture, CLI and release notes
with implementation. No controller, new sensor interface or storage layout
is implemented speculatively by this proposal.

## Observation-foundation verification

Builds, host results and the combined ProMicro repeater flash/RAM delta are
recorded in [Digital Immune System](DIGITAL_IMMUNE_SYSTEM.md#observation-foundation-verification-2026-10-26).
No solar installation was measured and no seasonal or energy-saving claim is
made. The observation-stage decision is [ADR 0017](adr/0017-recovery-and-power-observations.md).
