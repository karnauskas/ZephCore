# LilyGo T-LoRa V2.1-1.6

The Arduino MeshCore environment `LilyGo_TLora_V2_1_1_6` uses the existing
Zephyr short board name **`ttgo_lora32`**. Its full build target is
**`ttgo_lora32/esp32/procpu`**; this directory supplies the ZephCore overlay
and configuration for that target. No separate board definition is needed.

ESP32-PICO-D4, 4 MB flash, SX1276 (868/915 MHz), SSD1306 128×64 OLED,
GPIO0 user/BOOT button, GPIO25 activity LED, and GPIO35 battery ADC.
The 433 MHz SX1278 variant has not been validated by this port.

| Peripheral | GPIOs |
|---|---|
| LoRa SPI | SCK 5, MISO 19, MOSI 27, CS 18 |
| LoRa control | RESET 23, DIO0 26, DIO1 33, DIO2 32 |
| OLED I²C | SDA 21, SCL 22; address 0x3c |
| Battery | GPIO35 / ADC1 channel 7, 2:1 divider |
| Console | UART0 TX 1 / RX 3, onboard USB-UART, 115200 baud |

The inherited Zephyr DTS uses GPIO35/34 for DIO1/DIO2. This overlay replaces
those with the manufacturer's GPIO33/32 mapping, freeing GPIO35 for battery
sensing. RESET remains GPIO23 per the manufacturer's pin map; Arduino
MeshCore's PlatformIO variant lists GPIO14, which the manufacturer assigns
to the SD-card clock. SD storage is not enabled by ZephCore.

Sources:

- [LilyGo hardware and pin map](https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series/blob/master/docs/en/t3_v161_sx1276/t3_v161_sx1276_hw.md)
- [Arduino MeshCore variant](https://github.com/meshcore-dev/MeshCore/blob/main/variants/lilygo_tlora_v2_1/platformio.ini)
- [Arduino MeshCore battery conversion](https://github.com/meshcore-dev/MeshCore/blob/main/variants/lilygo_tlora_v2_1/LilyGoTLoraBoard.h)

## Build

```sh
west build -b ttgo_lora32/esp32/procpu zephcore --pristine
west build -b ttgo_lora32/esp32/procpu zephcore --pristine -- -DEXTRA_CONF_FILE=boards/common/repeater.conf
```

Flash through the onboard USB-UART with `west flash`. Hold BOOT while
resetting if the board does not enter the ROM downloader automatically.

The companion is capped at 50 contacts, 8 channels and 32 queued offline
messages to fit classic ESP32 DRAM. Flash uses DIO mode. This board remains
source-only (`zephcore.yml` has no `release:`); builds are compile-checked,
but these changes have not been tested on hardware.
