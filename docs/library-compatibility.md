# Library & Core-API Compatibility — ESP32 → Teensy 4.1

Sources checked: PJRC's `pins_arduino.h`, `HardwareSerial1‑8.cpp`, `SPI.cpp`,
and `WireIMXRT.cpp` (PaulStoffregen/cores, PaulStoffregen/SPI,
PaulStoffregen/Wire on GitHub) for authoritative pin/API behavior, plus
each library's own repo/issue tracker for Teensy 4.x reports.

## 1. `lib_deps` libraries

| Library | ESP32 usage | Teensy 4.1 status | Plan |
|---|---|---|---|
| `miguelbalboa/MFRC522` | RFID card read/write over shared SPI | **Compatible.** Actively lists `teensy` in supported architectures (v1.4.11+); multiple confirmed Teensy 4.1 deployments. One known regression in v1.4.10 (`VersionReg` misread) — pin the dependency to a version ≥1.4.11 (or the current release) rather than an unpinned `miguelbalboa/MFRC522` to avoid pulling a broken version. | Keep, pin version, no code changes needed beyond pin numbers. |
| `mikalhart/TinyGPSPlus` | Pure NMEA sentence parser fed one `char` at a time via `gps.encode()` | **Compatible, no porting risk.** It has zero hardware dependency — it never touches a pin or peripheral directly, only consumes bytes handed to it from whatever serial stream the caller reads. | Keep as-is. |
| `adafruit/Adafruit GFX Library` | Transitive dependency pulled in by the graphics stack | **Compatible.** Core drawing-primitives library, MCU-agnostic (canvas/buffer math only). | Keep. |
| `adafruit/DHT sensor library` | DHT11 temperature/humidity, one-wire bit-banged timing on `DHT11_PIN` | **Compatible but flagged — verify on hardware.** Multiple independent reports (Teensy forum, `adafruit/DHT-sensor-library` issue #134, Teensy 3.5/4.0 threads) describe this library returning bad/no data on Teensy 3.x/4.x, attributed to the library's cycle-counting timing assumptions not holding at Teensy's much higher clock speed (600MHz vs. ESP32-S3's 240MHz). This is a known, longstanding, unresolved community issue, not a one-off report. | Port as-is first (same `dht.begin()`/`readTemperature()` calls, same pin), but **treat first-boot DHT readings as unverified until confirmed on real hardware** — this is flagged as an open item in Phase 5, not silently assumed to work. If it misbehaves, the drop-in fix is Rob Tillaart's `DHTNEW` library (same DHT11 wiring, actively maintained, no reported Teensy 4.x timing issues) — do **not** silently swap libraries now on unconfirmed suspicion. |
| `https://github.com/adafruit/Adafruit_ILI9341.git` | **Unused** — confirmed in Phase 1 audit, no `#include` anywhere in `src/` | N/A | **Drop from `lib_deps`** per the default stated in the Phase 1 audit (no objection raised). |
| `https://github.com/Bodmer/TFT_eSPI.git` | ST7796 480x320 driver + touch coordinate mapping | **Compatible.** Teensyduino has shipped ST7796-driver support since Teensyduino 1.60, and TFT_eSPI has documented, community-confirmed Teensy 4.x support (Teensy 4.x's higher clock and hardware SPI actually give it *better* throughput headroom than ESP32 for this library). **Important behavioral difference, not a compatibility blocker:** on ESP32 the library's declared `TFT_MOSI`/`TFT_MISO`/`TFT_SCLK` build flags get wired to arbitrary GPIOs through the ESP32 GPIO matrix; on Teensy, TFT_eSPI drives the chip's *fixed* hardware SPI pins — the build flags must be set to Teensy's actual SPI0 pins (11/12/13), not just any 3 GPIOs. See `docs/teensy-pinmap.md`. | Keep, update `platformio.ini` `TFT_MOSI/MISO/SCLK` flags to 11/12/13 in Phase 4. |

Implicit core libraries (`SPI.h`, `Wire.h`, `Arduino.h`) are part of Teensyduino
and require no dependency changes — only call-signature changes, covered below.

**No CAN library appears in this codebase** (confirmed in Phase 1 — the
ESP32 branch has no CAN bus usage at all), so there is nothing to migrate
for FlexCAN_T4. Noting for completeness since the task brief mentions it:
Teensy 4.1 has 3 native CAN controllers via `tonton81/FlexCAN_T4` if CAN is
ever added later — out of scope for this port.

## 2. ESP32-specific core APIs → Teensy 4.1 equivalents

| ESP32 call | Where | Teensy 4.1 replacement | Notes |
|---|---|---|---|
| `IRAM_ATTR` on ISR functions | 6 ISRs across `main.cpp`/`TFTDisplay.cpp` | **Remove entirely.** | Teensyduino doesn't define this macro and doesn't need it — the IMXRT1062 has no separate IRAM/flash execution split the way ESP32 does; all code already executes from tightly-coupled memory. Leaving it in would be a compile error, not silently wrong, so this can't slip through untested. |
| `Serial1.begin(GPSBaud, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN)` | `main.cpp` `setup()` | `Serial1.begin(GPSBaud);` | Teensy's `Serial1` object has **fixed** hardware RX/TX pins (0/1 — confirmed from `HardwareSerial1.cpp`), not remappable via `begin()` args like ESP32's GPIO-matrix UART. Since the port already needs to pick concrete Teensy pins for GPS RX/TX in Phase 3, `pins.h`'s `GPS_RX_PIN`/`GPS_TX_PIN` become **documentation constants matching `Serial1`'s fixed pins (0/1)**, not values passed into `begin()`. |
| `SPI.begin(MFRC522_SCK, MFRC522_MISO, MFRC522_MOSI, -1)` (×2, in `main.cpp` and `TFTDisplay.cpp`) | shared SPI bus init | `SPI.begin();` | Same story: Teensy's default hardware `SPI` object pins are fixed (MOSI 11 / MISO 12 / SCK 13, confirmed from `SPI.cpp`). No GPIO-matrix remap exists — `pins.h`'s SPI pin constants become documentation matching those fixed pins rather than `begin()` arguments. **Both `SPI.begin()` calls in the current code are redundant** (same bus, called from both `main.cpp::setup()` and `TFT_begin()`) — harmless (idempotent) on ESP32 and remains harmless on Teensy; not changing this, just noting it's pre-existing, not migration-introduced. |
| `analogReadResolution(12)` | `main.cpp` `setup()` | Same call, unchanged. | Present on Teensy core too; both platforms support up to 12-bit ADC on paper. Phase 5 flags actual linearity/reference verification. |
| `analogWrite(TFT_BACKLIGHT_PIN, 255)` | `TFTDisplay.cpp` `TFT_begin()` | Same call, unchanged, but the target pin **must** be one of Teensy's PWM-capable pins (see `docs/teensy-pinmap.md`) or it silently degrades to a non-PWM digital on/off. | Backlight is always driven to 255 (full on) in current firmware, so even a non-PWM pin would work today by coincidence — but `pins.h` assigns a genuinely PWM-capable pin so the existing "backlight PWM" intent still works if dimming is ever added. |
| `board_build.psram_type`, `-DBOARD_HAS_PSRAM`, `board_build.partitions=default_16MB.csv`, `board_upload.flash_size` | `platformio.ini` | Removed entirely. | ESP32 flash-partition-table / PSRAM-config concept; Teensy 4.1 has no user-facing partition table, and its optional PSRAM (if the physical chip is populated) is auto-detected by Teensyduino, not configured via build flags. |
| `esp32_studio.json` | repo root | Deleted. | ESP32 IDE/flash-tool config; PlatformIO drives Teensy flashing directly with no equivalent file needed. |

## 3. Concurrency model

Confirmed in Phase 1: no `xTaskCreatePinnedToCore`, no dual-core task
pinning, no FreeRTOS task API usage anywhere in the ESP32 branch — `loop()`
is a single cooperative sequence on one core already. Teensy 4.1 is
single-core (600MHz Cortex-M7, no RTOS by default in this codebase), so
**no concurrency redesign is needed**; the existing ISR-flag + polling-loop
pattern (`volatile` flags set in interrupts, read/cleared in `loop()` with
`noInterrupts()`/`interrupts()` guards) ports directly and is in fact the
idiomatic Teensy pattern too.

## 4. Storage

Confirmed in Phase 1: no `Preferences.h`/NVS usage — nothing persists
across reboots in this firmware today (gear thresholds, wheel
circumference, etc. are all compile-time constants). No storage-layer
port needed.

## Open question for you

- **DHT11 timing reliability on Teensy 4.1** (§1): this is a real,
  independently-reported risk, not a hypothetical. I'm porting the
  existing `adafruit/DHT sensor library` call as-is per the "preserve
  logic exactly" ground rule, but you should specifically bench-test the
  temperature reading once hardware is available. If it returns garbage,
  say so and I'll swap in `DHTNEW` — I'm not doing that swap preemptively
  since it's unconfirmed for your specific board/wiring.
