# Teensy 4.1 Pin Map

Companion to `docs/esp32-audit.md` §3 — same layout, so the two can be
diffed side by side. Source of truth is [`lib/pinMap.h`](../lib/pinMap.h).

Pin capability facts below were confirmed directly against PJRC's
Teensyduino core source (not just secondhand docs), since getting this
wrong risks real hardware damage on a physical motorcycle build:
`pins_arduino.h`, `HardwareSerial1-8.cpp`, `SPI.cpp`, and `WireIMXRT.cpp`
from the `PaulStoffregen/cores`, `PaulStoffregen/SPI`, and
`PaulStoffregen/Wire` repositories.

## Teensy 4.1 pin capability summary (background)

- 55 digital I/O pins total (0-54). Pins 42-54 are physically on the
  bottom-side pads, not the main header rows.
- Analog-capable (ADC): pins 14-23 (`A0`-`A9`), 24-27 (`A10`-`A13`),
  38-41 (`A14`-`A17`). 18 pins total, 3.3V reference, not 5V-tolerant.
- PWM-capable: pins 0-15, 18, 19, 22-25, 28, 29, 33, 36, 37, 42-47, 51, 54
  (per `pins_arduino.h`'s `digitalPinHasPWM` macro for `ARDUINO_TEENSY41`).
- Every digital pin supports `attachInterrupt` (no ESP32-style subset
  restriction).
- **Fixed, non-remappable peripheral pins** (this is the key difference
  from the ESP32-S3's GPIO-matrix-backed peripherals):
  - Hardware SPI (`SPI`): MOSI 11, MISO 12, SCK 13, default CS 10.
  - Hardware I2C `Wire`: SDA 18, SCL 19. `Wire1`: SDA 17, SCL 16 (alt 44/45).
    `Wire2`: SDA 25, SCL 24.
  - Hardware UART `Serial1`: RX 0, TX 1. (`Serial2` RX7/TX8, `Serial3`
    RX15/TX14, `Serial4` RX16/TX17, `Serial5` RX21/TX20 — listed for
    completeness; only `Serial1` is used by this project.)
  - A second/third hardware SPI bus exists (`SPI1`: MISO1/MOSI26/SCK27/CS0;
    `SPI2`: MISO42/MOSI43/SCK45/CS44) but isn't needed here — one shared
    SPI bus (TFT + MFRC522) is all this design uses, same as the ESP32
    branch.
- **Reserved, do not use:** pins 42-47 are the onboard microSD card's
  native 4-bit SDIO interface; pins 42-54 overlap with `SPI2`'s default
  pins and the optional bottom-pad PSRAM/flash footprints. Nothing in
  this project uses the onboard SD slot or extra PSRAM today, but these
  pins are kept free in `pinMap.h` in case that changes later.

## Pin map (Teensy 4.1, from `lib/pinMap.h`)

| Function | Pin | Peripheral | ESP32-S3 pin (for reference) | Note |
|---|---|---|---|---|
| GPS RX (MCU) | 0 | `Serial1` (fixed) | 41 | ← GPS TX |
| GPS TX (MCU) | 1 | `Serial1` (fixed) | 42 | → GPS RX |
| TFT DC | 2 | — | 6 | digital out |
| TFT RST | 3 | — | 7 | digital out |
| MFRC522 RST | 4 | — | 38 | digital out |
| TFT Backlight | 5 | PWM | 21 | `analogWrite`, fixed at 255 currently |
| MFRC522 IRQ | 6 | — | 14 | digital in, `INPUT_PULLUP`, `FALLING` |
| Touch INT | 7 | — | 4 | digital in, `INPUT_PULLUP`, `FALLING` |
| Ignition control (relay) | 8 | — | 2 | digital out, RFID-gated immobilizer |
| MFRC522 CS | 9 | SPI (shared bus) | 10 | digital out |
| TFT CS | 10 | SPI (shared bus) | 5 | digital out |
| SPI MOSI (shared TFT+MFRC522) | 11 | `SPI` (fixed) | 11 | — |
| SPI MISO (shared TFT+MFRC522) | 12 | `SPI` (fixed) | 13 | — |
| SPI SCK (shared TFT+MFRC522) | 13 | `SPI` (fixed) | 12 | **Also Teensy `LED_BUILTIN`** — see below |
| Fuel level | 14 | `A0` | 1 (ADC1_CH0) | analog in, 12-bit |
| Gear position | 15 | `A1` | 9 (ADC1_CH8) | analog in, 12-bit, mapped via fixed thresholds |
| GY-521/MPU6050 SCL | 16 | `Wire1` (fixed) | 17 | **defined, unused** — matches ESP32 branch |
| GY-521/MPU6050 SDA | 17 | `Wire1` (fixed) | 18 | **defined, unused** |
| Touch SDA | 18 | `Wire` (fixed) | 8 | 400kHz |
| Touch SCL | 19 | `Wire` (fixed) | 39 | 400kHz |
| Tach pulse | 20 | — | 15 | digital in, `INPUT_PULLUP`, `RISING` |
| Analog speed (Hall sensor) | 21 | — | 3 | digital in, interrupt `RISING`, 6 magnets/rev |
| Turn signal L (input) | 22 | — | 43 | digital in, `INPUT_PULLUP`, `CHANGE`, active-low |
| Turn signal R (input) | 23 | — | 44 | digital in, `INPUT_PULLUP`, `CHANGE`, active-low |
| Hazard (input) | 24 | — | 45 | digital in, `INPUT_PULLUP`, `CHANGE`, active-low |
| High beam sense (input) | 25 | — | 46 | digital in, `INPUT_PULLUP`, `CHANGE`, active-low |
| Low beam sense (input) | 26 | — | 47 | digital in, `INPUT_PULLUP`, `CHANGE`, active-low; also polled directly by `RelayControl.cpp` |
| DHT11 data | 27 | 1-Wire | 16 | needs pull-up; see library-compatibility.md DHT risk note |
| High beam relay (output) | 28 | PWM-capable (used as plain digital out) | 0 | digital out |
| Low beam relay (output) | 29 | PWM-capable (used as plain digital out) | 19 | digital out |
| Left blinker relay (output) | 30 | — | 20 | digital out |
| Right blinker relay (output) | 31 | — | 48 | digital out |
| GY-521/MPU6050 INT | 32 | — | 40 | **defined, unused** |
| Touch RST | -1 (n/c) | — | -1 (n/c) | unchanged, not connected |

Pin 33 is intentionally unused (was briefly reserved for a dedicated status
LED, dropped — see design decision 1 below).

## Design decisions / flags for your review

1. **`LED_BUILTIN` / pin 13 conflict — resolved by dropping the status LED
   feature, not by relocating it.** On the ESP32-S3, the status LED
   (`LED_BUILTIN`, pin 48) and the shared SPI bus (pins 11/12/13) were
   unrelated pins. On Teensy 4.1, `LED_BUILTIN` is hardwired to pin 13 —
   which this project also uses as the shared hardware SPI clock (`SPI`
   SCK) for the TFT and MFRC522, so driving it manually from `pulseLed()`
   would fight the SPI peripheral for control of the pin and corrupt
   in-flight SPI transfers. I initially proposed a dedicated
   `STATUS_LED_PIN` (33) instead of reusing `LED_BUILTIN`; you confirmed
   removing the RFID-activity LED feature entirely rather than relocating
   it, so Phase 4 (updated) deleted `pulseLed()`, its call sites, and the
   pin reservation. The onboard LED still flickers passively with SPI
   traffic since it's electrically tied to pin 13, but nothing in
   firmware drives it on purpose anymore.
2. **Reserved IMU bus moved from a bespoke I2C1 to Teensy's `Wire1`.** Same
   as the ESP32 branch, this stays uninitialized (no `Wire1.begin()` call)
   — the reservation is carried forward, not activated.
3. **GPS UART kept as `Serial1`** (matching the ESP32 branch's object name)
   specifically so `main.cpp`'s `Serial1.encode()`/`Serial1.available()`
   call sites don't need to change objects in Phase 4 — only the `begin()`
   call's argument list changes (see `docs/library-compatibility.md` §2).
4. **All other pin numbers are otherwise arbitrary** within the free
   0-41 range (avoiding the fixed/reserved pins above) — no functional
   requirement forced a specific number for the relays, switch inputs,
   DHT11, or RFID/TFT control lines the way SPI/I2C/UART/ADC did.

## Hardware cross-check — confirmed against the official PJRC pinout card

You provided the official "Welcome to Teensy 4.1" pinout card for
verification. It confirms every fixed-peripheral assumption this pin map
relies on: the default hardware SPI's `CS`/`MOSI`/`MISO`/`SCK` labels sit
exactly on pins 10/11/12/13 as used here; `SDA`/`SCL` sit on 18/19 for the
default `Wire` bus; pins 0/1 are labeled `RX1`/`TX1`; the analog range
labels (`A0`-`A9` on 14-23, `A10`-`A13` on 24-27, `A14`-`A17` on 38-41)
match; and pin 13 carries the card's onboard-LED marking, confirming the
SPI-clock/`LED_BUILTIN` conflict that drove the decision above to drop the
status-LED feature rather than relocate it. No corrections needed.
