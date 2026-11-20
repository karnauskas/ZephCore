# AGENTS.md

Rules for AI-assisted contributions to ZephCore. They apply to human contributors too. Read the section of
`docs/DESIGN.md` (invariants, layer rules) and `docs/ARCHITECTURE.md` (component reference) for the area you
touch before writing code. Decisions are in `docs/adr/`.

## How a change is judged

1. **Find the root cause.** The PR states the cause and the evidence for it (datasheet section, register dump,
   log, trace). If the cause is not known, say so; do not ship a guess.
2. **No band-aids.** Do not add a retry, delay, timeout, re-init, read-back check or watchdog to make a symptom
   go away. Recovery code is accepted only for a named, understood hardware failure, in the layer that owns the
   state, with its bound (`docs/DESIGN.md` §8.7). There is no hardware watchdog and none will be added.
3. **The smallest change that fixes it.** Close what is reachable today, not what might be reachable later. No
   speculative guards, build asserts, new abstractions, options or refactors riding along. One concern per PR.
   If the diff is larger than the bug, expect to be asked to cut it.
4. **Do what upstream MeshCore does** when it already solves the problem, unless ours can be more accurate at no
   compatibility cost (`docs/DESIGN.md` §9).
5. **Comment diet.** A comment says what the code does or the invariant it keeps, in a few lines. Rationale,
   history, incidents, measurements and rejected alternatives go in the PR description and `docs/`, not in code.
   Do not comment the obvious.
6. **Fix it for every board.** A fix or feature that concerns behaviour goes in the common path, for all
   platforms and roles. Do not scope it to the board or SoC family where it was found or tested, and do not
   exclude a platform because the problem cannot happen there today; a later port or Zephyr bump can make it
   real. Untested boards still get the change; the PR says which hardware it ran on.
7. **Gate on the capability, never on the name.** The only reason to gate code is that the hardware is absent,
   and the test for that is a devicetree node or property, or a Kconfig symbol for the feature. Never
   `CONFIG_SOC_*`, `CONFIG_BOARD_*` or a board name as a stand-in for "has this part" or "has this bug". A board
   that lacks the hardware then carries no code for it, and a new board that has it gets the behaviour for free.
8. **A real platform difference is stated as one.** If something truly is specific to one SoC or chip (an
   erratum, a vendor blob, a peripheral that exists nowhere else), it lives in that platform's layer and the PR
   cites the source that makes it specific. "Only seen on X" is not that.

## Requested integration: SaveMeshcore filtering and CLI

Integrate the packet filters and CLI described in the
[SaveMeshcore README](https://github.com/aigarslv/SaveMeshcore/blob/main/README.md).
Use its documented forwarding semantics, defaults and command responses:

- `filter` reports all filter states; `filter <1byte|data|public>` reports one;
  `filter <1byte|data|public> on|off` changes it through local and authorized remote CLI.
- `1byte` defaults off and blocks forwarding one-byte path-hash floods only after at least one hop.
- `data` defaults on and blocks forwarding payload types `0x06`, `0x0C`–`0x0E` and `0x0F`.
- `public` defaults off and blocks forwarding Public-channel `GRP_TXT` and `GRP_DATA` packets.
  Identify Public using its hash byte and MAC verification with the published channel key;
  preserve hash-colliding channels and avoid decrypting message contents.
- Save changes immediately in `prefs.json`, preserving explicit saved choices across upgrades.
  Apply ZephCore's JSON-only settings invariant when adapting SaveMeshcore's storage.
- Put forwarding logic in the common path across boards, expose commands for roles that forward,
  and cover filter boundaries, channel-hash collisions, persistence and CLI behavior with host tests.
  Update architecture, CLI documentation and release notes with the implementation.
- Recheck upstream payload assignments and command names before porting reserved-type filters.

## Requested integration: EasySkyMesh power saving

Integrate the power-saving features described by
[EasySkyMesh](https://github.com/IoTThinks/EasySkyMesh) and its
[PowerSaving wiki](https://github.com/IoTThinks/EasySkyMesh/wiki/PowerSaving), using the actual
[IoTThinks/MeshCore source differences](https://github.com/IoTThinks/MeshCore/tree/PowerSaving-v18)
as implementation evidence:

- Compare the feature branch with its upstream merge base and ZephCore's existing power management.
  Record the reviewed commit SHAs and relevant files. Reconcile wiki/source differences in profiles,
  defaults and supported radios before porting; the wiki and feature branch can change independently.
- Cover MCU idle/light sleep with interrupt-driven radio wake, supported CPU frequency scaling,
  GPS acquisition/sleep scheduling, and LoRa RX duty-cycle profiles and manual timing.
  Extend the existing Zephyr event loop, PM locks, GPS state machine and radio adapters.
- Integrate the documented `powersaving`, GPS and `get|set radio.rxps` controls and status where
  applicable, including local and authorized remote CLI. Report requested versus effective RX mode,
  support, timing and errors. Persist settings in `prefs.json`; preserve explicit saved choices.
- Use role-appropriate GPS schedules: bounded acquisition followed by sleep, with early sleep after
  a valid fix/time sync and an explicit sync command. Preserve forward-only time and shared power rails.
- Derive RX/sleep windows from SF, bandwidth, preamble and the radio's documented timing constraints.
  Preserve complete-packet reception after preamble detection, radio IRQ wake and RF-switch/LNA state.
  Distinguish guarded profiles from experimental levels with measured packet loss; document those limits.
- Keep policy common across boards and gate hardware operations on capabilities. Preserve USB/serial
  access and hold the required sleep locks during RX/TX, transport activity and peripheral operations.
- Preserve the BLE invariants below when assessing the fork's BLE power tuning. Apply the existing
  ADR and recovery rules to any conflicting proposal, including periodic reboots for radio deafness.
- Add host tests for scheduling, timing calculations, configuration, persistence and CLI behavior.
  Measure battery-side current, wake behavior and packet reception on hardware; report conditions,
  flash/RAM costs and unverified platforms. Update design/architecture, CLI docs and release notes.

## Requested integration: observer CLI, presets and multiple MQTT upstreams

For the observer role, integrate compatible CLI and MQTT functionality from the
[Gessaman observer documentation](https://observer.gessaman.com/docs). Preserve the same preset
catalog, names and configuration values; verify the documentation against its corresponding source
revision before implementation:

- Match supported command syntax, replies and defaults, including `get mqtt.presets` and indexed
  broker configuration such as `set mqtt1.preset <name>`. Preserve MQTT preset endpoints, transports,
  authentication, topic templates and publish policies, and applicable radio preset parameters.
  Record the reviewed revision and test preset parity rather than pinning an assumed preset count.
- Allow configuring multiple simultaneous MQTT upstreams when hardware resources permit. Use
  independent broker slots with their own settings, connection state, packet filters and diagnostics.
  Publish to each enabled slot without one disconnected broker blocking the others or radio reception.
- Determine the active slot limit from available networking, RAM/PSRAM, TLS and socket capacity,
  using capability-based configuration rather than board names. The reference advertises two active
  brokers without PSRAM and five with PSRAM; validate those budgets on ZephCore before adopting them.
  Expose the supported limit and reject configurations beyond it with a clear CLI error.
- Keep queues and per-slot resource use bounded; report flash/RAM costs and hardware-verified limits.
  Persist configuration in `prefs.json` and migrate existing single-broker settings without losing
  credentials or changing explicit user choices.
- Preserve observer listen-only operation, wire/payload compatibility and existing recovery rules.
  Document the compatible command set and any unsupported reference features.
- Add host tests for CLI compatibility, preset values, settings migration, per-slot publishing,
  capacity limits and broker failure isolation. Build representative hardware with and without PSRAM;
  update architecture, CLI documentation and release notes with measured limits and verification.

## Invariants: do not change without an ADR

- Byte compatibility with Arduino MeshCore on the air and on the companion protocol; `prefs.json` format.
- Settings persist in `prefs.json` only. The legacy binary prefs files are frozen: no new fields in
  `PrefsCodec`, no change to its size constants or to the pinned offsets and lengths in its tests.
- The wall clock only moves forward.
- Mesh, GPS state and UI run on the main thread; loops are event-driven, no polling.
- No heap in the packet path. Companion builds are RAM-bound.
- Never write to a peripheral that was not cleanly identified first.
- BLE: no proactive SMP Security Request, keep `BT_LE_ADV_OPT_USE_IDENTITY` (ADR 0004), leave connection
  parameters alone.
- Files ported from MeshCore (listed in `.editorconfig`) keep upstream's text verbatim; every change in them is
  fenced `// ZEPHCORE:`. Do not restyle or de-duplicate them.
- `zephcore/patches/zephyr`: one patch owns each Zephyr file.
- `board.conf` does not repeat what a parent conf or the devicetree already provides.

## Mechanics

- Branch from `dev`, target `dev`, rebase instead of merging.
- After a rebase, re-read the whole diff against `dev`: a branch older than the code around it can undo newer
  work without a conflict.
- Tabs for indentation, except the upstream-ported files.
- Build with `--pristine` when the board or role changes; ESP32 S3/C-series repeaters need `--sysbuild`.
- Host tests: `python3 tests/run.py run --profile quick` (Linux or WSL). A new case needs a
  `tests/catalog.json` entry. A failing pinned test is a finding: do not edit the pinned values to make it pass.
- Logic that runs without hardware (a protocol parser, a state machine, a timing rule, a `// ZEPHCORE:` change
  to upstream behaviour) comes with host tests that pin it.
- PR CI runs the host tests only. Build every board you touched and one you did not, and run each role you
  changed. Releases build from `zephcore.yml` alone, so nothing may depend on a hand-passed fragment. A role or
  fragment no `zephcore.yml` row builds goes into the "compile-only roles" step of `.github/workflows/build.yml`.
- A new role or composition root puts every part the board has into a defined state, used or not: GPS parked,
  LEDs off, display blanked, as `src/server_main_common.cpp` does.
- New board: `zephcore.yml` without `release:`, then `python zephcore/scripts/board_manifest.py check` and
  `... docs`.
- Docs in the same PR: a clause in `docs/ARCHITECTURE.md` when behaviour, an interface or a limit changes;
  `docs/CLI_commands.md` for any CLI change; an ADR when a decision is taken or reversed; the next release's
  `releasenotes/RELEASE_NOTES_<version>-zephcore.md` for anything a user sees.

## PR description

- Cause, fix, and what was deliberately left out.
- Which boards it was built for, and which hardware it actually ran on. Never claim hardware verification that
  was not done.
- Flash and RAM delta when code is added to a common path.
