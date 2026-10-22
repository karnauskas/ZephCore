# 0010: Opt-in adaptive periodic mesh advertisements

## Status

Accepted, 2026-10-22.

## Context

`RepeaterMesh::updateAdvertTimer` and `updateFloodAdvertTimer`, and the matching
room-server methods, schedule fixed configured durations regardless of discovery
or contention. Both roles already receive signature-verified adverts and expose
the mesh contention tracker. The fixed timers are the cause of the unchanged
advertising rate; there is no diagnosed hardware failure behind this feature.

## Decision

Keep fixed scheduling by default. Add JSON-only `zc.adaptive_adv`, controlled by
`advert.adaptive`, for both periodic-advert roles on every board. Use one shared,
allocation-free policy and the existing deadline-driven loops.

Twice the baseline when no verified non-share advert has arrived for 15 minutes
or contention is at least 4; half the baseline for 10 minutes after hearing a new
identity; baseline otherwise. Congestion takes priority. These are initial policy
choices, not thresholds established by field measurements. Quiet is an advert
activity proxy, and the contention estimate measures duplicate retransmissions,
not channel utilization. Schedule decisions use the current signals; a newly
heard identity may advance a pending timer. Existing due timers remain due.

Track the last 16 distinct full public keys in a FIFO, with no persisted history.
An evicted identity may count as new again. Exclude share rebroadcasts, as the
existing time-sync hook does. Preserve zero intervals, manual adverts, companion
behavior, on-air bytes, and frozen binary preference layouts.

## Consequences

Each server role gains a 528-byte policy object on 32-bit targets, plus any
NodePrefs alignment change. No packet-path allocation or polling is added.
A quiet or congested node may wait up to twice the configured interval; a newly
heard identity advances its deadline to at most half the baseline from receipt,
rather than triggering an immediate response. Large configured intervals still
mean slow discovery. Startup uses the quiet interval until an identity arrives.
No retries, radio recovery, new companion protocol settings, or extra tuning
options are introduced. Hardware performance remains to be measured.
