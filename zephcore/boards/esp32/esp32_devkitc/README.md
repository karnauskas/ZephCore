# ESP32-WROOM dev kit + RFM95W (868 MHz)

DIY wiring target for a classic ESP32-WROOM-32 development board with **4 MB
flash**, no PSRAM, and a Micro-USB connector through an onboard USB-to-UART
bridge. Uses Zephyr's `esp32_devkitc/esp32/procpu` board. Micro-USB does not
identify the MCU: this target is for classic ESP32, not ESP32-S2/S3/C3/C6.

## Wiring

Disconnect USB before wiring. Labels below are **ESP32 GPIO numbers**, not
header positions; 30-pin and 38-pin dev kits place them differently. Connect
the bare RFM95W to **3.3 V**, never the dev kit's 5 V/VIN pin. All signals use
3.3 V logic. Connect all module ground pads to the dev kit's ground, keep SPI
wires short, and fit an 868 MHz antenna to ANT before powering the node.

| RFM95W pad | Signal | ESP32 dev kit connection |
|---|---|---|
| 1, 8, 10 | GND | GND |
| 2 | MISO | GPIO19 |
| 3 | MOSI | GPIO23 |
| 4 | SCK | GPIO18 |
| 5 | NSS / CS | GPIO27 |
| 6 | RESET | GPIO14 |
| 13 | 3.3V | 3V3 |
| 14 | DIO0 | GPIO26 |
| 15 | DIO1 | GPIO33 |
| 16 | DIO2 | GPIO32 |
| 9 | ANT | 868 MHz antenna |
| 7, 11, 12 | DIO5, DIO3, DIO4 | Leave unconnected |

Wire all three listed DIO signals for the Zephyr SX127x backend. This radio
has no BUSY connection. No display, GPS, battery divider or external LED is
assumed. UART0 uses the onboard USB-UART bridge (TX GPIO1 / RX GPIO3) at
115200 baud. The BOOT button is GPIO0; EN resets the ESP32.

The pin assignments are defined in [board.overlay](board.overlay). Module pad
numbers follow HopeRF's [RFM95W datasheet, §1.4](https://www.hoperf.com/uploads/RFM95W-V2.0_1695351437.pdf).
The base board and SPI pin definitions follow the project's pinned Zephyr
[DevKitC DTS](https://github.com/zephyrproject-rtos/zephyr/blob/c136c73d7be3305d2c07ee5726d4c6d0d985068b/boards/espressif/esp32_devkitc/esp32_devkitc_procpu.dts)
and [pinctrl](https://github.com/zephyrproject-rtos/zephyr/blob/c136c73d7be3305d2c07ee5726d4c6d0d985068b/boards/espressif/esp32_devkitc/esp32_devkitc-pinctrl.dtsi).

## Build and flash

Set up the [Zephyr workspace and Espressif blobs](../../../../docs/BUILDING.md)
first. Build one role, then flash through Micro-USB:

```sh
# Companion (BLE)
west build -b esp32_devkitc/esp32/procpu zephcore --pristine
west flash

# Repeater (serial CLI)
west build -b esp32_devkitc/esp32/procpu zephcore --pristine -- -DEXTRA_CONF_FILE=boards/common/repeater.conf
west flash
```

If automatic download entry fails, hold BOOT, press and release EN, then
release BOOT once flashing starts. Switching between companion and repeater
erases the previous role's identity and data; export your identity first.

The 868 MHz module designation is its hardware band, not the mesh channel.
Configure frequency, bandwidth, spreading factor and coding rate to match
your local MeshCore network. Default TX power is 17 dBm. The companion uses
the existing classic ESP32 budget of 50 contacts, 8 channels and 32 queued
messages. Storage is 192 KB LittleFS plus a separate 16 KB BLE bond store.

This board is source-only, with no published release image. Firmware builds
and hardware operation still require validation; no RF testing is claimed.
