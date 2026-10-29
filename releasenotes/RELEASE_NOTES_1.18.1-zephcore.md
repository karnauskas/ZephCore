# ZephCore 1.18.1-zephcore

## ProMicro USB flashing

ProMicro SX1262 now defaults to UF2 for `west flash`. Enter the bootloader and
mount its UF2 drive before flashing. Existing build directories need a pristine
rebuild to pick up the runner configuration.

## Repeater scope bypass

`set repeat.all on` lets repeaters forward unknown, denied and unscoped floods
without configuring regions. Defaults to off and persists across reboot. General
and advert hop limits, duplicate suppression and loop protection still apply.
Use `set repeat.all off` to restore scope filtering.

## Companion location privacy

USB Serial and authenticated BLE CLI can configure four local privacy zones,
location fuzzing/precision, timezone-based blackout hours, movement thresholds and
delayed location. Zones suppress firmware-generated location across advertisements,
self-advert exports and telemetry; queued location is checked again before TX.
Defaults preserve existing behavior. Chat remains available. Identity keys and
app-authored/raw coordinates are outside this policy. See
[CLI commands](../docs/CLI_commands.md#companion-location-privacy-zephcore-only).

## Adaptive mesh advertisement intervals

Repeaters and room servers can enable `set advert.adaptive on` to advertise
less often during quiet or congested periods, and more often after hearing a
new identity. It defaults to off. Existing interval settings remain the baseline;
disabled timers and manual adverts keep their behavior.

## Duplicate suppression

Repeatedly sending the same packet no longer consumes extra duplicate-cache
slots. Explicitly clearing a packet (for example, importing a shared contact)
now permits it to be processed again after repeated sends. The cache remains
bounded to 160 hashes, uses capacity-only eviction, and clears on reboot.

## Public #test pingbot

Companions and repeaters can enable `set pingbot on` to answer `!ping` in
public `#test`. Replies address the ping sender by name and include last-link
RSSI/SNR, repeater hop count and
recorded hop ID prefixes. No channel slot is needed. The setting is saved,
defaults off, and can be disabled with `set pingbot off`.

## MQTT WebSockets and identity authentication

Observers and repeater uplinks publish to one broker over WebSockets at `/`,
with port 443 as the default. Authentication uses the node identity and a fresh
signed token whose audience is the configured hostname. SNTP time is required;
static usernames/passwords are not used. Existing saved ports can be updated
with `set mqtt.port 443` (or `set uplink.mqtt.port 443` on a repeater).

Fixed CONNACK timeouts when the broker splits the MQTT response across small
WebSocket frames: unread buffered frames now keep the connection readable.

Observer `get mqtt.status` and repeater `get uplink.mqtt.status` now show
packet queue/send/drop/failure totals and the last publish error, making missing
collector traffic diagnosable without verbose packet logs. Observer output also
includes LCD flood/direct RX and radio error counters for comparison. Send totals describe
local QoS 0 writes, not collector acknowledgements.

Fixed observer noise-floor measurements staying at the initial value by running
radio maintenance on its deadlines. Observer status now reads the board battery
voltage. Full online MQTT status is retained, so subscribers connecting later
receive battery, uptime and noise-floor statistics rather than a minimal payload.

## Observer display

The observer LCD also shows noise floor, last received RSSI (dBm), and SNR
(dB). Readings not yet available show `--`; all existing counters remain.

Display-equipped observers now show WiFi and MQTT connection status, received
packet counts (total, flood and direct), receive errors, uptime, and time since
the last parsed packet. The screen stays visible and updates once per second
(30 seconds on e-paper); before the first packet it shows `Last: never`.

## Observer startup

Fixed a boot crash when applying LED preferences on observers with UI support
compiled in. The LED preference hook no longer schedules heartbeat work before
heartbeat initialization.

## LilyGo T-LoRa V2.1-1.6

The source-only `ttgo_lora32/esp32/procpu` target now maps this revision's
LoRa interrupt pins correctly and adds its battery ADC, user button and
activity LED. The existing SSD1306 display is retained. No published firmware
or hardware verification is claimed; see the board README for local builds.

## Lithuania radio defaults

Fresh configurations now use Atviras Tinklas's EU/UK (Narrow) preset:
869.618 MHz, 62.5 kHz bandwidth, SF8 and coding rate 4/8. The defaults remain
10% duty cycle and two-byte path hashes. Existing saved radio settings are
retained; select EU/UK (Narrow) in the companion app to update those devices.

## Mesh clock fusion diagnostics

`get meshtimesync` now reports confidence in agreement between mesh sources
and a median drift estimate relative to local uptime. Drift needs at least
24 hours of observations from a quorum of agreeing nodes. Existing consensus,
outlier rejection and clock-correction limits remain in place; these new fields
are diagnostic and available while synchronization is disabled.

## Adaptive power budget

Enable `set power.budget on` to lengthen periodic mesh advert and GPS standby
intervals as battery reserve falls. Low reserve also reduces repeated sensor
work by allowing telemetry/UI reads to reuse readings for up to one or five
minutes. It defaults to off; forwarding, manual adverts and low-battery
protection keep their behavior. Use `get power.budget` to see the current factor.

## Radio startup recovery

Startup now reports success only after modem configuration and RX arming succeed.
Companion and server roles retain the last modem configuration that completed
startup. Three incomplete boots of the same candidate restore that snapshot;
driver-rejected modem settings permit one startup rollback attempt. Identity
and unrelated preferences are retained. Existing SX126x software recovery stays
in place; there is no hardware watchdog. Hardware fault-injection validation
remains outstanding; see `docs/MILESTONE_1_RECOVERY.md` for scope and acceptance.

## Recovery and voltage observations

New opt-in CLI flags `immune`, `winter` and `winter.auto` persist in prefs.json.
Immune status counts existing CAD/AGC remedies and distinguishes local RX
restoration from later receiver activity. Winter status reports raw voltage
summaries held in RAM; `winter reset` clears them. These are observation tools:
seasonal prediction, learned recovery selection and automatic advert suppression
are not implemented. `winter.auto` explicitly reports action unsupported.

Observer MQTT status now uses accumulated dispatcher airtime instead of
hardcoded zeros. A listen-only observer still reports zero TX airtime. RX
airtime is estimated packet airtime, not channel occupancy or receiver-on time.
Per-packet RSSI/SNR and driver RX errors retain their existing reporting.

## SX127x noise floor

Fixed a startup crash during noise-floor sampling: the loramac-node SX127x
driver now supplies its missing RSSI callback for both SX1272 and SX1276 variants.

SX127x boards now feed the common noise-floor estimator from instantaneous
RSSI through Zephyr's LoRa API, replacing the fixed −80 dBm placeholder.
This applies to every role using the shared radio adapter. Failed API queries
are discarded by the existing burst sampler. Hardware RF accuracy has not been
verified; a busy channel can influence the estimate.
