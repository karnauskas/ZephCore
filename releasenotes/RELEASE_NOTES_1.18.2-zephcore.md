# ZephCore 1.18.2-zephcore

Firmware updates over Bluetooth for ESP32 companions, and packets the radio never checked are no
longer repeated.

> [!NOTE]
> A normal upgrade keeps your identity, settings, contacts and phone pairing.

---

## Bluetooth updates for ESP32 companions

ESP32 companions can now be updated from a phone over Bluetooth, the way nRF companions already
can. Send `start ota` (or pick DFU / BLE OTA on the device's screen menu): the node opens an
update window for 10 minutes and keeps running. Then, in **nRF Connect Device Manager** (iOS and
Android), connect to the node, pair with its PIN if asked, and upload the `<board>-update.bin`
from the release (choose "Confirm only"). The node restarts on the new firmware by itself and
keeps your identity, settings, contacts and pairing. An update takes about a minute from a phone.

- Use the `-update.bin`, not the `-merged.bin`: the merged file is for flashing over USB.
- The Nordic DFU app used for nRF boards does not work here; use nRF Connect Device Manager.
- The first update to this version still goes over USB or the web flasher. From then on, updates
  can go over Bluetooth.
- `stop ota` closes the window early. Only a phone paired with the node's PIN can upload.
- Available on the Heltec V4, V4.3, V4 R8 and Wireless Tracker V2, LilyGo T3S3, Meshnology W12,
  Station G2, and the XIAO ESP32-S3 and ESP32-C3. The Heltec V3, Heltec Wireless Tracker, LilyGo
  T-Lora C6 and XIAO ESP32-C6 do not have the memory for it next to their WiFi companion, and the
  classic ESP32 boards (T-Beam) cannot boot an update this way; they update over USB as before.

On boards without an over-the-air update, `start ota` used to answer as if it was rebooting into
an update mode and then did nothing; it now says that the build has no OTA. Selecting DFU on the
screen menu of an ESP32 board no longer blanks the screen and leaves the node in an undefined
state.

## Other changes

- **Packets the radio never checked are no longer repeated.** A LoRa header can say a packet
  carries no CRC, and the radio then passes it on without checking it. MeshCore always sends a CRC,
  so such a packet is a misread header or noise; it is now dropped like a packet with a bad CRC, on
  every radio type. It is rare (one in about 48 000 receptions on a busy repeater) and it does not
  explain the damaged packet copies seen on map.meshcore.hu.
- **Companions ignore empty contact slots.** A temporary contact slot that had been cleared could
  still be matched as a sender with no key, so a crafted message could be taken as coming from it.
  Such slots are now skipped everywhere a contact is looked up (from upstream MeshCore).
- **Repeater and room server duplicate counters stop at 65535** in the stats the app shows, instead
  of starting again from 0 on a busy node (from upstream MeshCore).

- **Zephyr updated to a newer main.** The base system moves forward by about 1,100 upstream commits, including Bluetooth host fixes. Nothing changes in how the nodes are used.

## Compact ping replies

Public `!ping` replies now use `Pong @[Alice] -97(good)/-0.25(fair) ABCD,1234`
after the normal sender prefix. RSSI/SNR have short quality hints, hop IDs have
no labels or counts, and direct replies omit IDs.

## Antenna measurement statistics

Companions and repeaters now offer antenna measurements through local and
authorized remote MeshCore CLI sessions. Remote replies use short key prefixes
to fit all readings in one response. Use
`antenna reset`, `antenna probe`, then `antenna` and `antenna <0..7>` to compare
direct neighbours. Up to eight neighbours have RSSI/SNR minimum, mean and maximum
readings, with quality words describing each mean on the same scale as ping
replies. Remote replies use four-byte key prefixes and omit age to fit the labels.
Probe replies also report how the neighbour heard your transmission
(SNR only). Statistics live in RAM and clear on reboot; probes run only on request.

## Runtime TX-power changes

Fixed radio replies stopping after `set tx`, including on the XIAO nRF52840.
A changed power setting needs a full modem configuration; the common radio
adapter now exits RX before that configuration, as required by the driver.
Normal direction-only switches keep their existing fast path.

## Observer MQTT slots and compatible presets

Observers can configure independent MQTT broker slots using `set mqttN.preset`,
`get mqtt.presets`, per-slot packet filters and `get mqttN.diag`. The preset
catalog matches Gessaman observer revision `860011d2`, including transports,
authentication, topic styles and retain policies. Supported slot counts follow
available SRAM/PSRAM resources; query `get mqtt.capacity` on the running build.
Existing single-broker credentials migrate into `prefs.json` and keep their
primary connection. Fresh observers default to the Let's Mesh US/EU presets.
The observer remains listen-only. Hardware multi-session validation is pending.

## Forwarding filters

Repeaters and off-grid companions now support `filter`, `filter 1byte on|off`,
`filter data on|off` and `filter public on|off` through local and authorized remote
CLI. Defaults are `1byte off`, `data on`, `public off`: group data and reserved/raw
payloads are no longer forwarded by default; use `filter data off` to restore
their previous forwarding policy. Public filtering authenticates the channel MAC
without decrypting and preserves unrelated channels sharing its hash byte.
Choices save immediately in `prefs.json` and survive upgrades. Local reception
is unchanged; rooms and observers remain non-forwarding.
