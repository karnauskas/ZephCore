# ADR 0021 — Power-saving controls and guarded RX profiles

Status: Accepted, 2026-11-20. Extends ADR 0006 without relaxing its timing bounds.

## Context

The requested reference is [EasySkyMesh](https://github.com/IoTThinks/EasySkyMesh)
and its [PowerSaving wiki](https://github.com/IoTThinks/EasySkyMesh/wiki/PowerSaving).
Reviewed IoTThinks/MeshCore `PowerSaving-v18` at
`3957b6f4547f629bfdd2a54ddb09b9615576ea04`, compared with upstream merge base
`81dbae592ffad40b4212bc0b37e82b38da08f4bc` (upstream main reviewed at
`75b04ef06def54b348400cb1b138afb6749ffd4f`). Relevant source:

- `src/helpers/PowerSaving.cpp`, `ESP32Board.cpp`, `NRF52Board.cpp`:
  idle/light sleep, interrupt wake, CPU scaling and USB locks.
- `src/helpers/sensors/EnvironmentSensorManager.cpp`, `LocationProvider.h`:
  bounded acquisition, early GPS sleep, explicit sync and shared power rails.
- `src/helpers/radiolib/RXPowerSaving.{h,cpp}`, `RXPowerSavingCLI.cpp`,
  `CustomSX1262*`, `CustomLR1110*`, `SX1262RxPowerSaving.h`: profiles, timer
  quantization, arm status and family-specific timer behavior.
- `src/helpers/esp32/SerialBLEInterface.{h,cpp}`: connection/advertising tuning.

The wiki's level 1/5 conservative/balanced mapping differs from the reviewed
source's 3/6 mapping. The source adds guarded level 8 (`max`) and experimental
9/10 (`overdrive`/`riskyWorkingMax`). Its bench results are specific to its
RadioLib wrappers, including a timer workaround. They are not Zephyr hardware
verification. The wiki's ten-minute GPS maxima also differ from ZephCore's
existing two-minute warm/five-minute cold/server windows.
The reviewed repeater source initializes MCU power saving on, despite the wiki's
off default. Companion defaults also differ from ZephCore. This integration keeps
ZephCore's existing MCU defaults and explicit saved choices: optional light sleep
is off for companions and on for capable fresh server builds. The fork's default
companion RXPS is not enabled here. ZephCore additionally supports LR2021 duty RX
through its existing driver; the wiki lists SX1262 and LR1110 only.

## Decision

Retain ZephCore's event-driven PM, radio IRQ wake, peripheral/transport locks,
USB accessibility, GPS rail ownership, and forward-only clock callbacks.
Enable the existing light-sleep composition for room servers on boards declaring
that capability, using the same policy as repeaters. Other builds continue to
use Zephyr's normal idle path. Keep the existing ESP32 80 MHz operating point;
the fork's IDF automatic frequency scaling is not substituted for Zephyr's clock
ownership. BLE security, identity advertising and connection parameters stay
under ADR 0004. No periodic reboot or new recovery loop is introduced.

Add `get/set radio.rxps` across companion, repeater, room and observer CLI.
Use source labels/levels 3/6/8 and its guarded catch ladders, capped by ADR 0006:
`sleep + wake <= (P - 2D - 1) * Tsym`, `R >= (D+1) * Tsym`, and
`2R + S >= (P+14) * Tsym`, including the existing drift margin. Named profiles
use the actual configured preamble; unlike the fork they do not assume 16.
An explicit preamble constrains the assumed sender preamble and cannot increase
it beyond the configured one. Several levels can therefore produce identical
timings. With default D=8, a 16-symbol preamble has no guarded sleep budget:
continuous RX is reported with `-ERANGE`. This also affects SF9–SF12 presets.
RXPS stays off by default for all roles, including companions.

Manual timings are accepted only in the configured 1,000–30,000,000 us range;
violating catch/completion bounds leaves continuous RX. Level 0 with zero
periods retains the old automatic `rxduty` calculation. Periods are quantized
to milliseconds for LR drivers, with an additional RX millisecond covering
kernel and SX126x timer truncation. Kernel timeout conversion rounds down;
native radio timer granularity remains below the submitted values. Report
manual requested periods and submitted periods (`erx`, `eslp`) separately.

An explicit ops-table capability gates the hardware call: SX126x, LR1110 and
LR2021 support it; SX127x does not. Unsupported enable requests are rejected.
Arm errors increment a counter and fall back to continuous RX. Changes defer
past active RX/TX; normal rearm applies the requested configuration. No retry,
reinitialization or watchdog is added. `clear stats` clears the arm-error count.

Reject experimental levels 9/10 and their aliases. Their measured loss claims
cannot be transferred to different drivers. Preserve saved RX enable choices;
new timing fields live only in `prefs.json` under `zc.rxps_*`. Save failures
roll back CLI edits. The existing legacy `rx_dc` switch is reused; frozen binary
codec sizes/offsets do not change.

GPS uses the existing bounded state machine, five validated fixes, early sleep,
standby/full-off rail handling and `gps sync`. Fresh server settings now default
to daily sync; saved intervals, always-on mode and enable choices survive.
Companions retain five-minute standby and two-/five-minute warm/cold windows.
Authorized companion remote CLI now permits these GPS and power controls.

## Consequences and validation

Settings add a 12-byte timing structure to `NodePrefs`, plus struct alignment;
radio status adds fixed counters/flags. No heap allocation or periodic wake is
introduced. A more economical profile is not a promise of packet-loss-free RX:
mixed preambles, interference, oscillator behavior and RF-switch timing require
measurement on the target hardware.

Host tests exercise timing bounds, profile/CLI parsing, JSON persistence,
legacy-codec stability, GPS window selection and production adapter methods
against fake radio boundaries. No battery-side current, wake, RF-switch or
packet-delivery measurements were performed. Runtime savings and packet-loss
rates remain unverified on every board.

The quick host profile passes 208 cases. Firmware builds pass for XIAO nRF52840
repeater, RAK4631 companion, T1000-E companion (LR1110/GPS), Heltec V4 observer
(LR2021), TTGO LoRa32 observer (SX127x), and XIAO ESP32-S3 room server (sysbuild/PM).
The room composition is also included in CI's compile-only roles.

Against `b1acd4f` using the same toolchain, with only the intended daily GPS
default differing in Kconfig, linker byte deltas are:

| Build | Flash region | RAM region | ELF data + BSS |
| --- | ---: | ---: | ---: |
| XIAO nRF52840 repeater | +2,056 | 0 | +32 |
| RAK4631 companion | +2,100 | 0 | +32 |

Linker regions include alignment. These compile measurements do not establish
runtime stack peaks or current consumption; the room-server PM composition's
incremental cost was not measured against a baseline build.
