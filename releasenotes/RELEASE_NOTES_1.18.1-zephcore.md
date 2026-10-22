# ZephCore 1.18.1-zephcore

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
