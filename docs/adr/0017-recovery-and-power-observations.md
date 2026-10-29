# 0017: Opt-in recovery and power observations before automatic policy

## Status

Accepted for the observation foundation, 2026-10-26. Seasonal control and
learned recovery selection remain proposals.

## Context

The requested Digital Immune System and Winter Memory need evidence to
distinguish hardware faults from quiet channels, and poor charging from voltage
variation or an absent battery. Current radio remedies already own their
failure-specific bounds. The common power budget uses current reserve rather
than charging history. No reproducible new fault or validated seasonal trace
has been supplied to authorize additional recovery or advert suppression.

Observer MQTT status also contained hardcoded zero airtime values despite
Dispatcher maintaining packet airtime counters. RSSI/SNR are already published
per packet; SX127x's fixed noise-floor fallback is an adapter limitation.

## Decision

Add independent JSON-only `zc.immune`, `zc.winter` and `zc.winter_auto`
booleans, default off, controlled through the common and observer CLIs. Preserve
the existing role permissions and companion remote-command allowlist. Return
save failures to these callers; runtime changes still apply in RAM. KISS has
no settings CLI and receives no new control interface.

Record counts and the latest local outcome of existing CAD-gate and AGC
remedies in the radio adapter when immune observation is enabled. Later RX or
RX-error counter activity is reported separately and does not prove causation.
Clearing ordinary packet counters must not fabricate this activity. Flags do
not disable or alter existing recovery bounds and introduce no new remedy.

Collect raw first/last/min/max voltage and sample count on existing main-thread
wakes, at most once per 30 seconds, when Winter Memory is enabled. Share the
existing power-budget acquisition; acquire nothing when both features are off.
Readings below the existing 2000 mV exclusion do not enter the summary, but
accepted readings still do not prove a connected battery. USB absence and
unknown USB capability remain distinct. No charge inference or seasonal
prediction is made. Keep the summaries in RAM and provide an explicit reset.

Store `winter.auto` as permission for future validated control, while returning
`action=unsupported` and leaving scheduling unchanged in this implementation.
Automatic policy requires a subsequent evidence-backed decision. This retains
ADR 0013's reserve policy and all compatibility invariants.

Publish the observer's existing dispatcher TX/RX airtime counters in status.
RX airtime describes accumulated estimated packet duration, not channel
occupancy or time spent listening. A listen-only observer has zero TX airtime.
Preserve the MQTT field names and per-packet signal reporting.

## Consequences

Observations reset on reboot, do not write flash per sample and allocate no
packet-path heap. There is no durable seasonal memory yet. The auto permission
has no actuator; CLI replies and documentation state this explicitly.
Fixed storage and code increase common-path memory; matched build measurements
are recorded in the feature documentation. Host tests cover flags, JSON/legacy
compatibility, observation outcomes, counter reset, raw voltage summaries,
sampling cadence and bounded status output. Hardware operation and energy
savings are not claimed by these tests or builds.
