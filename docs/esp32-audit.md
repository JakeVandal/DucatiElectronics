# ESP32 Branch Audit

Source: `ESP32` branch, commit `72a8501` (includes the in-progress
`hazardIndicatorISR`/`analogSpeedISR` brace fix and `WIRING.md` that were
committed as part of starting this migration).

Purpose: complete inventory of the ESP32 branch's logic, dependencies, and
pin usage, to serve as the baseline for the Teensy 4.1 port. No code is
changed in this phase.

## 1. Source files

| File | Purpose |
|---|---|
| [`src/main.cpp`](../src/main.cpp) | Entry point. Owns RFID-gated ignition immobilizer logic, GPS parsing, DHT11 temp read, fuel/gear ADC read + mapping, tach RPM calc from pulse counting, hall-effect speed calc from ISR timestamps, turn-signal/hazard/high-low-beam input handling, and the ignition RPM-timeout watchdog. Drives `TFTDisplay` and `RelayControl` modules. |
| [`src/RelayControl.h`](../src/RelayControl.h) / [`.cpp`](../src/RelayControl.cpp) | Owns the 4 lighting relay outputs (high beam, low beam, left/right blinker). Reads the raw rocker-switch and turn-signal input pins directly (independent of `main.cpp`'s ISR-driven flags), does its own software debounce (`isButtonPressed`/`buttonPressDetected`), and runs the blinker toggle/animation state machine. Exposes getters main.cpp uses for display sync. |
| [`src/TFTDisplay.h`](../src/TFTDisplay.h) / [`.cpp`](../src/TFTDisplay.cpp) | Owns the ST7796 480x320 TFT UI: tachometer/speed/gear page and an RFID-writer page, paging via capacitive touch (FT6336U over I2C, polled on an IRQ pin), turn-signal and beam indicator icons, and a 60s rolling-average fuel gauge. |
| [`lib/pinMap.h`](../lib/pinMap.h) | Centralized `#define` pin map for the ESP32-S3. Already the single source of truth for pins — Phase 3 mirrors this pattern for the Teensy 4.1. |
| [`platformio.ini`](../platformio.ini) | PlatformIO env `esp32s3_n16r8`: board = `esp32-s3-devkitc-1`, PSRAM/flash config, and `TFT_eSPI` build-time pin/driver flags (`ST7796_DRIVER`, `TFT_MOSI`/`MISO`/`SCLK`/`CS`/`DC`/`RST`/`BL`, font/SPI-speed flags). |
| [`WIRING.md`](../WIRING.md) | Human-readable wiring/power reference generated from the pin map (mermaid diagrams + tables), just added this session. Useful cross-reference for Phase 3's pin remap. |
| [`doc/ProjectDescription.md`](../doc/ProjectDescription.md) | High-level feature list, not code. |
| [`esp32_studio.json`](../esp32_studio.json) | ESP32 IDE/flash-tool config (esptool NVS/flash settings, hardcoded Windows build path). ESP32-toolchain-specific; not portable and not needed on Teensy (PlatformIO handles Teensy flashing directly). Note: the file's raw JSON is malformed (trailing duplicate fragment after the final `}`) — pre-existing, unrelated to this migration. |
| `include/README`, `.vscode/*` | Boilerplate/editor config, no hardware logic. |

Not present on `ESP32` but present on `main`: `src/piccWriter.h` (superseded
by the RFID logic now inlined in `main.cpp`) and `.github/agents/Prompt
Engineer.agent.md` (unrelated tooling file) — both irrelevant to this port.

## 2. Libraries / dependencies (`platformio.ini` `lib_deps`)

| Library | Used for |
|---|---|
| `miguelbalboa/MFRC522` | RFID reader driver (`main.cpp`) |
| `mikalhart/TinyGPSPlus` | GPS NMEA parsing (`main.cpp`) |
| `adafruit/Adafruit GFX Library` | Transitive dependency of TFT_eSPI/graphics stack |
| `https://github.com/adafruit/Adafruit_ILI9341.git` | **Appears unused.** No `#include <Adafruit_ILI9341.h>` anywhere in `src/`; the actual display driver in use is `TFT_eSPI` configured for `ST7796_DRIVER`. Flag for removal or confirm intent in Phase 2. |
| `adafruit/DHT sensor library` | `DHT11_PIN` temp/humidity read (`main.cpp`) |
| `https://github.com/Bodmer/TFT_eSPI.git` | TFT display + touch coordinate mapping support (`TFTDisplay.cpp`) |

Implicit (Arduino core, not in `lib_deps`): `SPI.h`, `Wire.h`, core
`Arduino.h`.

## 3. Full pin map (ESP32-S3-N16R8, from `lib/pinMap.h`)

| Function | GPIO | Peripheral | Direction / mode |
|---|---|---|---|
| MFRC522 CS | 10 | SPI (shared bus) | digital out |
| MFRC522 RST | 38 | — | digital out |
| MFRC522 IRQ | 14 | — | digital in, `INPUT_PULLUP`, interrupt `FALLING` |
| SPI MOSI (shared TFT+MFRC522) | 11 | SPI | — |
| SPI MISO (shared TFT+MFRC522) | 13 | SPI | — |
| SPI SCK (shared TFT+MFRC522) | 12 | SPI | — |
| GY-521/MPU6050 SDA | 18 | I2C1 | **defined, unused** — no `Wire1.begin()` call anywhere |
| GY-521/MPU6050 SCL | 17 | I2C1 | **defined, unused** |
| GY-521/MPU6050 INT | 40 | — | **defined, unused** |
| DHT11 data | 16 | 1-Wire | digital in/out (library-managed) |
| Tach pulse | 15 | — | digital in, `INPUT_PULLUP`, interrupt `RISING` |
| Fuel level | 1 | ADC1_CH0 | analog in, 12-bit |
| Gear position | 9 | ADC1_CH8 | analog in, 12-bit, mapped via fixed voltage thresholds |
| GPS RX (MCU) | 41 | UART1 (`Serial1`) | 9600 baud |
| GPS TX (MCU) | 42 | UART1 (`Serial1`) | 9600 baud |
| TFT CS | 5 | SPI | digital out |
| TFT DC | 6 | — | digital out |
| TFT RST | 7 | — | digital out |
| TFT Backlight | 21 | — | `analogWrite` (PWM), fixed at 255 (full brightness) — no dimming logic |
| Touch SDA | 8 | I2C0 (`Wire`) | 400kHz |
| Touch SCL | 39 | I2C0 (`Wire`) | 400kHz |
| Touch INT | 4 | — | digital in, `INPUT_PULLUP`, interrupt `FALLING` (FT6336U touch controller) |
| Touch RST | -1 (n/c) | — | not connected |
| Ignition control (relay) | 2 | — | digital out, RFID-gated immobilizer |
| Turn signal L (input) | 43 | — | digital in, `INPUT_PULLUP`, interrupt `CHANGE`, active-low |
| Turn signal R (input) | 44 | — | digital in, `INPUT_PULLUP`, interrupt `CHANGE`, active-low |
| Hazard (input) | 45 | — | digital in, `INPUT_PULLUP`, interrupt `CHANGE`, active-low |
| High beam sense (input) | 46 | — | digital in, `INPUT_PULLUP`, interrupt `CHANGE`, active-low |
| Low beam sense (input) | 47 | — | digital in, `INPUT_PULLUP`, interrupt `CHANGE`, active-low (note: `relayControl_update()` also polls this pin directly every loop, separate from the ISR-driven `main.cpp` flags — see §5) |
| High beam relay (output) | 0 | — | digital out (boot-strap pin on ESP32-S3, documented caution in `pinMap.h`/`WIRING.md`) |
| Low beam relay (output) | 19 | — | digital out (native USB D-, unused as USB here) |
| Left blinker relay (output) | 20 | — | digital out (native USB D+, unused as USB here) |
| Right blinker relay (output) | 48 | — | digital out (doubles as ESP32-S3-DevKitC-1 onboard RGB LED) |
| Analog speed (Hall sensor) | 3 | — | digital in, interrupt `RISING`, 6 magnets/rev |
| `LED_BUILTIN` | 48 | — | digital out, status LED pulse on RFID read/write |

No true DAC, capacitive-touch-pin (ESP32 "touch" peripheral), or CAN usage
anywhere in this branch.

## 4. ESP32-specific APIs in use

| API / pattern | Where | Notes for Phase 2 |
|---|---|---|
| `IRAM_ATTR` | `main.cpp` (5 ISRs), `TFTDisplay.cpp` (`touchISR`) | ESP-IDF macro placing ISR code in internal RAM. Not defined by Teensyduino — needs to be `#define`d empty or stripped for Teensy. |
| `Serial1.begin(baud, SERIAL_8N1, rxPin, txPin)` | `main.cpp` `setup()` | ESP32 `HardwareSerial::begin()` 4-arg overload lets any two GPIOs be assigned as UART RX/TX via the GPIO matrix. Teensy 4.1 UART pins are fixed per hardware `SerialN` instance (no GPIO-matrix remapping) — GPIO 41/42 have no equivalent meaning on Teensy; the port must move to whichever `SerialN`'s fixed RX/TX pins are used. |
| `SPI.begin(sck, miso, mosi, ss)` | `main.cpp`, `TFTDisplay.cpp` | ESP32 4-arg overload remaps the SPI bus to arbitrary GPIOs via the GPIO matrix. Teensy 4.1's hardware SPI buses (`SPI`, `SPI1`, `SPI2`) each have a small fixed/alternate pin set (`SPI.setMOSI()`/`setMISO()`/`setSCK()` before `begin()`, from a fixed candidate list) — not arbitrary-pin like ESP32. |
| `analogReadResolution(12)` | `main.cpp` `setup()` | Present on both cores, but confirm equivalent behavior — see Phase 5 ADC note. |
| `analogWrite()` on TFT backlight | `TFTDisplay.cpp` | Present on both cores (ESP32 backs it with LEDC, Teensy with FlexPWM/QuadTimer) — should port directly, no ESP32-only call surface here. |
| `default_16MB.csv` partitions, PSRAM flags (`board_build.psram_type`, `-DBOARD_HAS_PSRAM`), `board_upload.flash_size` | `platformio.ini` | ESP32 flash/partition-table concept; no Teensy equivalent/needed (Teensy 4.1 has no user-facing partition table; PSRAM is an optional soldered chip, not configured this way). |
| `esp32_studio.json` | root | ESP32 flash-tool config, drop entirely for Teensy. |

Confirmed **absent** (matching the task's stated no-wireless-migration-gap
scope): `ledcSetup`/`ledcWrite`, any `esp_*` IDF call, `WiFi.h`/`BluetoothSerial.h`,
`xTaskCreatePinnedToCore`/dual-core task pinning, ESP32 ADC calibration
(`esp_adc_cal_*`), touch-pin peripheral (`touchRead`), RTC/deep-sleep calls,
and `Preferences.h`/NVS. This confirms the migration is a pure
hardware-abstraction port with no concurrency-model or persistent-storage
redesign required.

## 5. Audit notes / anomalies to carry forward (not fixed in this phase)

1. **Reserved-but-unused IMU pins.** GY-521/MPU6050 (SDA 18, SCL 17, INT 40)
   are defined in `pinMap.h` and documented in `WIRING.md` but never
   initialized (`Wire1.begin()` is never called) — the accelerometer data
   mentioned in the project description isn't actually wired into firmware
   yet. Carry the reservation forward in the Teensy pin map but don't invent
   behavior for it.
2. **Two independent low-beam/turn-signal input paths.** `main.cpp` reads
   `HIGH_BEAM_PIN`/`LOW_BEAM_PIN`/turn-signal pins via ISR-driven flags for
   display purposes, while `RelayControl.cpp` separately polls the same raw
   pins each loop (with its own debounce) to drive the relay outputs. This
   is existing ESP32-branch behavior, not a migration concern, but Phase 4
   must preserve both independent read paths exactly as-is.
3. **`Adafruit_ILI9341` library dependency looks unused** — see §2. Will
   confirm/flag disposition in Phase 2 rather than silently dropping it.
4. **`RelayControl.cpp`'s `R1`/`R2` (100k/10k divider) constants are defined
   but never referenced anywhere.** Likely leftover from earlier sender
   voltage-divider math that's now handled by the raw ADC path in
   `main.cpp`. Carried forward as-is in Phase 4 (no functional change), flagged
   here for your awareness.
5. **TFT backlight has no dimming logic** — always driven to 255. Noted
   only so Phase 4 doesn't "improve" it; behavior preserved exactly.

## Open questions for you

- **`Adafruit_ILI9341` dependency**: keep it in `lib_deps` for the Teensy
  port (in case it's a soon-to-be-used fallback) or drop it since nothing
  includes it? Defaulting to *drop* in Phase 2 unless you say otherwise.
- **`esp32_studio.json`**: safe to delete outright once the Teensy target
  is in place, since it's ESP32-toolchain-specific and PlatformIO doesn't
  need it for Teensy. Will delete in Phase 4 unless you object.
