# 0012: Add drift and confidence to mesh clock fusion

## Status

Accepted, 2026-10-23.

## Context

`MeshTimeSync::computeConsensus()` already combines signed timestamps from
multiple nodes using Marzullo interval intersection and rejects outliers through
quorum and strict-majority gates. Its status reports offset and interval radius,
but has no rate estimate or explicit confidence level. Replacing that consensus
would duplicate existing policy and change clock corrections without evidence.

## Decision

Keep the consensus and correction policy. Add diagnostic confidence based on
normal quorum, majority agreement, and sample freshness. High requires all
eligible voters to agree with samples no older than the 15-minute evaluation
interval; medium requires normal quorum and strict majority; bootstrap and
insufficient agreement are low; no eligible data is none. These are descriptive
levels, not calibrated probabilities of UTC accuracy.

Retain the first advert timestamp in each tenure. Compute a sender's relative
rate from first/latest timestamps and their uptime difference. Include only
sources whose vote intervals contain the consensus midpoint, and whose baseline
spans at least 24 hours. Require the configured normal quorum of rates, then
use their median. The day-long baseline reduces one-second timestamp
quantization compared with short intervals; it is an initial measurement policy,
not a field-validated error bound. Report relative ppm and contributor count.

Do not feed this rate back into time projection or clock discipline. Delay
variation, accepted sender corrections, and a coherent wrong-time majority
remain limitations. A signature identifies a sender, not a trusted clock.

## Consequences

All existing time-sync roles share the diagnostics and CLI formatter. The
additional persistent storage is four bytes per slot (64 bytes on default
companions, 128 on server roles), with a bounded automatic array of rates during
evaluation. No heap, new polling, new setting or persistent format is added.
Tenure reset, eviction and reboot clear each affected anchor; local clock steps
have no effect because measurement uses sender timestamps and local uptime.
Existing GPS/manual suppression, step caps, rate limits, forward-only behavior
and packet compatibility are unchanged. Hardware rate accuracy remains untested.
