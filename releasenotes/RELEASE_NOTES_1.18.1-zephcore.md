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
