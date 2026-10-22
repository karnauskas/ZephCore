# 0011: Match Atviras Tinklas radio defaults

## Status

Accepted, 2026-10-23.

## Context

Atviras Tinklas in Lithuania recommends EU/UK (Narrow) and two-byte path
hashes in its [setup guide](https://atvirastinklas.lt/zinynas/nuo-ko-pradeti).
The [official MeshCore preset API](https://api.meshcore.nz/api/v1/config),
checked 2026-10-23, gives 869.618 MHz, SF8, bandwidth 62.5 kHz and CR8 (4/8).
ZephCore's shared defaults instead selected SF7 and CR5, so a fresh node did
not match the requested network's modulation settings.

## Decision

Change the shared `LoRaConfig` defaults to SF8 and CR8 for every board and role.
Keep 869.618 MHz and 62.5 kHz. Keep the existing 10% duty-cycle default
(`airtime_factor = 9`) and two-byte path hashes (`path_hash_mode = 1`).
Leave TX power under each board's existing limits.

## Consequences

New configurations match the requested network. Existing valid saved radio
settings are retained; operators can choose EU/UK (Narrow) in the companion
app to change those devices. Invalid-radio fallback also uses these defaults.
No preference layout, on-air format, regional option or board-specific override
is added. No code or RAM is added to the packet path.
