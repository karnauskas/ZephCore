# Digital immune system — fault recognition and recovery proposal

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


## Purpose

A repeater should recognize a documented recurring fault and invoke the
bounded recovery owned by the affected component. It should expose what it
observed, what it attempted, and what the result actually proves. A whole-node
reboot is not a generic recovery step. No hardware watchdog is introduced.

The initial scope is radio recovery already reachable in the current code.
Behaviour shared by roles belongs in the common path, including companion,
room server, observer and KISS where they use that path. Hardware-specific
remedies remain behind the relevant capability in the radio family layer.

## CLI control contract

The proposed feature is opt-in through `set immune on|off|default`, with
`default` resolving to `off`. `get immune` reports the configured flag and
effective state (`off`, `observe`, `active` or `unsupported`), including the
reason it cannot act. `get immune.status` reports supported signatures,
episodes, attempts and outcomes within the caller's reply budget.

The flag controls only the additional recognition/history behaviour introduced
by this proposal. Existing driver recovery, startup rollback and fatal handling
retain their current policy when it is off. Enabling the flag cannot authorize
an unvalidated remedy: observation-only implementations report `observe`.
Disabling prevents new feature-owned attempts, while an already-started
sequence finishes its bounded operation and releases owned state. Toggling
must not reset an episode's attempt budget or erase diagnostic evidence.

Persist the boolean as JSON-only `zc.immune`, defaulting missing or invalid
values to false. Runtime history remains RAM-only. Apply changes on the main
thread without reboot; report save failure rather than claiming persistence.
Use existing role permissions and transport restrictions. The same contract
applies to capable roles; KISS has no persistent settings CLI and remains
unsupported until an explicit interface decision is made. The flags and status commands are implemented in observation mode.

## Existing evidence and limits

| Condition | Current owner and action | What it establishes |
|---|---|---|
| CAD/channel-busy gate prevents sending for more than four seconds | `Dispatcher::checkSend()` calls `LoRaRadio::recoverRxState()`: cancel RX, invalidate cached configuration, restart RX | The send gate exceeded its bound; it does not prove a silicon fault or exclude real channel activity |
| SX126x receive inactivity for ten minutes and at least eight identical noise-floor samples | `LoRaRadio::agcIdleMaintenance()` requests the family's AGC reset and restarts RX | The existing corroborated AGC heuristic matched; a later packet alone cannot prove causation |
| Modem configuration returns `-EINVAL` or `-ENOTSUP` during startup | `LoRaRadio::begin()` permits one persisted, validated, different known-good modem restoration | The driver rejected settings; arbitrary I/O failure is not this signature |
| Three incomplete starts with the same modem candidate | JSON checkpoint restores a validated, different snapshot before the fourth attempt | Startup was interrupted repeatedly; the checkpoint does not identify the failing component |
| Radio remains outside both RX and TX beyond the stall threshold | `Dispatcher::maintenanceLoop()` raises `ERR_EVENT_STARTRX_TIMEOUT` | Diagnostic evidence only; no new recovery is authorized by this proposal |

Startup details and outstanding hardware acceptance are in
[ADR 0014](adr/0014-radio-startup-recovery.md) and
[Milestone 1](MILESTONE_1_RECOVERY.md). Driver-owned parked-RX and RX-latch
bounds remain in those drivers; a higher layer must not duplicate them.

Known false signals must remain excluded: silence on a quiet network,
reception errors that demonstrate RF activity, duty-cycle sleep, a long valid
reception, and unavailable measurements. In particular, the SX127x adapter's former
fixed −80 dBm RSSI fallback cannot be evidence of a frozen receiver.

## ProMicro SX1262 candidate hardware

The `promicro_sx1262` target uses the native SX126x adapter, so its real RSSI
sampling and existing family AGC remedy make it a candidate for the recovery
audit. The SX127x −80 dBm placeholder exclusion does not apply to this path.
Neither a frozen sample nor silence alone establishes an SX1262 fault.

Before fault injection, record the exact module (bare SX1262 or external-PA
variant), supply wiring, measured supply behaviour under TX, modem settings
and firmware hash. A power interruption is not evidence for an AGC remedy.
Exercise the same exclusions and recovery bounds on another supported board;
any behavioural fix belongs in the shared owner or relevant radio capability,
not behind the ProMicro name. No hardware validation is claimed here.

## Recognition contract

Each implemented fault signature must specify:

1. A stable identifier, owning component and source for the failure mechanism.
2. Required capabilities and valid observations, including return codes,
   state transitions and monotonic elapsed time where relevant.
3. Exclusions for normal operation and missing or stale measurements.
4. The one authorized recovery sequence, its operation bounds, and the
   existing state it may modify.
5. The evidence that completes the attempt, what counts as failure, and what
   remains unknown without a controlled hardware experiment.
6. The maximum attempts per episode, the rule that ends an episode, and the
   treatment of further recurrences. These must be justified for that fault,
   not supplied by a global retry policy.

An episode starts when the complete signature matches. Repeated observations
of that same unresolved condition belong to the same episode. Defining a new
episode requires fault-specific evidence; merely finishing a reset must not
reset the attempt budget. Existing mechanisms must be audited against this
contract before any claim that they implement the proposal in full.

## Recovery and outcome

The owner evaluates the signature from its existing events or maintenance
deadlines. It executes only the named remedy, then records the result from
existing operation returns and subsequent events. No extra peripheral
read-back, periodic probe, retry, delay or timeout is added just to produce a
health verdict. All main-thread ownership rules remain in force; driver work
continues to hand state to the main thread through existing mechanisms.

Outcome reporting must distinguish:

- **Attempt failed:** an operation required by the remedy failed.
- **Local state restored:** the operations needed to restore the owner's
  state succeeded. For RX this does not establish RF reception.
- **Activity observed:** a subsequent qualifying event occurred, with its
  elapsed time recorded. Received or errored frames can establish receiver
  activity; silence leaves reception unverified.
- **Unverified:** available events cannot establish the required result.

No automatic escalation to another reset, configuration rollback, factory
reset or reboot follows an unverified or failed result. The owner follows its
documented exhaustion behaviour and reports the condition. A later design may
add escalation only for a separately evidenced failure mechanism.

## What “previously worked” means

The first implementation has one reviewed remedy per supported signature.
Recurrence history helps an operator compare episodes; it does not choose or
invent a remedy. A successful driver return is not sufficient evidence to
promote a sequence into a learned recovery rule.

Choosing among multiple remedies is deferred until one real fault has
reproducible evidence that distinguishes those remedies, including their
failure cases and RX cost. No scoring engine, script interpreter, universal
fault manager or persistent learned-policy database is proposed.

## Diagnostics and storage

The smallest useful implementation records the signature, attempt count,
last operation result and monotonic time in the owning component's existing
diagnostics. Add fields only when the selected fault needs them. Storage is
fixed-size, with no packet-path allocation and no per-packet history.

Runtime episode history is RAM-only initially. Existing startup checkpoint
persistence stays unchanged. Any future durable state must use `prefs.json`,
with an explicit write-frequency and power-loss argument; legacy binary prefs
and their pinned layouts remain frozen.

The CLI controls above are the proposed interface. An implementation must pin
exact reply formats and budgets, update architecture and CLI docs, and preserve
companion and over-the-air compatibility. No MQTT schema change is proposed.

## Delivery and acceptance

Start with an audit of one existing recovery, selected using a reproducible
fault report. Capture board, role, firmware hash, modem settings, relevant
driver returns and state transitions before changing code. The −80 noise-floor
placeholder, unexplained zero uptime and battery indications discussed during
this proposal are not demonstrated receiver faults and authorize no recovery.

For that one recovery, implement only missing evidence or episode accounting
needed by the contract. Do not bundle the other recoveries or rewrite their
shared interfaces in anticipation of future work.

Host tests must pin signature matching, normal-operation exclusions,
CLI defaults, JSON flag round-trips, permission checks, save failures,
disabling during an attempt and toggling without replenishing attempt budgets,
unavailable observations, unsigned uptime wrap where used, one episode's
attempt bound, operation failure, recurrence and the distinction between local
state restoration and observed activity. Add cases to `tests/catalog.json`
and run `python3 tests/run.py run --profile quick`.

Build every board touched and an additional board, covering each affected
role and applicable radio capabilities. Report flash/RAM delta against a
matched baseline. Hardware acceptance must reproduce the named fault, show
the intended transition and attempt bound, then exercise a quiet channel,
normal traffic and long receptions without false recovery. Record RX cost and
which hardware actually ran; builds and host tests do not establish it.

An ADR is required before adopting a new cross-component policy or changing
an invariant. Implementation and user-visible diagnostics require the usual
architecture and release-note updates. This proposal alone changes neither
an invariant nor an accepted recovery bound.

## Observation-foundation verification (2026-10-26)

The quick host profile passes 173 cases. Builds pass for ProMicro SX1262
companion and repeater, RAK4631 room server, and T-LoRa V2.1-1.6 observer and
KISS. KISS verifies the shared code compiles without a new settings CLI.
No hardware execution or fault injection was performed.

Against the same-board/role HEAD baseline, the ProMicro repeater grows from
269,244 to 271,000 bytes of flash (+1,756 B) and 84,316 to 84,380 bytes of RAM
(+64 B). This is the combined observation-foundation delta, including flags
and voltage acquisition/status; it is not a per-feature attribution. Other
roles were not paired with baselines. See [ADR 0017](adr/0017-recovery-and-power-observations.md).
