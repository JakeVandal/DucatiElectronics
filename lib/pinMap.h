/*
Teensy 4.1 Pinmap
Defines pin assignments for all hardware modules, ported from the ESP32-S3
pin map (see docs/esp32-audit.md for the original, docs/teensy-pinmap.md
for the side-by-side remap rationale).

- MFRC522 RFID Reader -> hardware SPI (shares bus with TFT: MOSI 11 / MISO 12 / SCK 13, separate CS)
- GY-521 IMU (MPU6050) -> Wire1 (I2C), reserved/unused, matches ESP32 branch
- DHT11 Temperature/Humidity -> One-Wire
- GT-U7 GPS Module -> Serial1 (hardware UART)
- TFT Display -> hardware SPI (shares bus with MFRC522)
- TFT Capacitive Touch Controller -> Wire (I2C)

IMPORTANT — Teensy 4.1 fixed-pin peripherals (unlike the ESP32-S3, these
are NOT remappable via begin() arguments or a GPIO matrix; the pin numbers
below for SPI/I2C/UART are dictated by the silicon, not chosen):
  - Hardware SPI (`SPI`):     MOSI 11, MISO 12, SCK 13 (fixed)
  - Hardware I2C (`Wire`):    SDA 18, SCL 19 (fixed)
  - Hardware I2C (`Wire1`):   SDA 17, SCL 16 (fixed)
  - Hardware UART (`Serial1`): RX 0, TX 1 (fixed)
Do not reassign these four peripherals' pin numbers without also changing
which peripheral instance (SPI1/SPI2, Wire2, Serial2-8) the code uses.

RESERVED, DO NOT USE for anything else: GPIO 42-47. These pins are shared
by the onboard microSD card slot (native SDIO, 4-bit), SPI2's default
pins, and the optional bottom-pad PSRAM/flash footprints. None of that is
used by this project, but the pins must stay free in case the onboard SD
slot is ever populated.

CHANGELOG (Teensy 4.1 migration from ESP32-S3):
- All SPI/I2C/UART pin numbers changed to Teensy's fixed hardware pins
  (see note above) - these were freely remappable GPIOs on the ESP32-S3
  but are fixed silicon pins here.
- IMU reserved bus moved from a second I2C peripheral at arbitrary GPIOs
  (SDA 18/SCL 17) to Teensy's Wire1 at its fixed pins (SDA 17/SCL 16).
  Still unused/uninitialized by firmware, same as the ESP32 branch.
- Added STATUS_LED_PIN (33), replacing direct use of `LED_BUILTIN`.
  Teensy 4.1's `LED_BUILTIN` is pin 13, which is ALSO this project's SPI
  SCK pin (TFT + MFRC522 share hardware SPI on pins 11/12/13). main.cpp's
  read/write activity LED must not fight the SPI peripheral for control
  of pin 13, so it now targets a dedicated pin instead. Flagged in
  docs/teensy-pinmap.md - this is a genuine hardware constraint, not a
  style choice.
- All other digital I/O (relays, switch inputs, analog senders, DHT11,
  RFID CS/RST/IRQ, TFT CS/DC/RST/backlight) reassigned to free GPIO in
  the 0-41 range, avoiding the reserved 42-54 block above. Exact numbers
  are otherwise arbitrary - functional groupings preserved, no capability
  constraint forced a specific number the way SPI/I2C/UART did.
*/

// MFRC522 RFID Reader - hardware SPI (shares bus with TFT)
#define MFRC522_CS_PIN 9        // Chip Select for MFRC522
#define MFRC522_RST_PIN 4       // Reset pin for MFRC522
#define MFRC522_IRQ_PIN 6       // Interrupt pin for MFRC522
#define MFRC522_MOSI 11         // Hardware SPI MOSI (fixed, shared with TFT)
#define MFRC522_MISO 12         // Hardware SPI MISO (fixed, shared with TFT)
#define MFRC522_SCK 13          // Hardware SPI SCK (fixed, shared with TFT) - also Teensy LED_BUILTIN, see STATUS_LED_PIN note above

// GY-521 IMU (MPU6050) - Wire1 (I2C), reserved, not initialized by firmware
#define GY521_SDA 17            // Wire1 SDA (fixed)
#define GY521_SCL 16            // Wire1 SCL (fixed)
#define GY521_INT_PIN 32        // Interrupt pin for GY-521 (reserved, unused)

// DHT11 Sensor - One-Wire
#define DHT11_PIN 27            // One-Wire data pin for DHT11

// Tachometer input
#define TACH_PIN 20             // Pulse input for RPM measurement

// Motorcycle analog inputs (A0/A1 - true analog-capable pins)
#define FUEL_VOLTAGE_PIN 14     // Fuel-level sensor input (A0)
#define GEAR_VOLTAGE_PIN 15     // Gear position sensor input (A1)

// GT-U7 GPS Module - Serial1 (hardware UART)
#define GPS_RX_PIN 0            // Serial1 RX (fixed) <- GPS TX
#define GPS_TX_PIN 1            // Serial1 TX (fixed) -> GPS RX
#define GPS_UART_NUM Serial1    // Serial1 (hardware UART)

// TFT Display - hardware SPI (shares bus with MFRC522)
#define TFT_CS_PIN 10           // Chip Select for TFT display
#define TFT_DC_PIN 2            // Data/Command pin
#define TFT_RST_PIN 3           // Reset pin
#define TFT_BACKLIGHT_PIN 5     // Backlight control (PWM capable)
#define TFT_MOSI 11             // Hardware SPI MOSI (fixed, shared with MFRC522)
#define TFT_MISO 12             // Hardware SPI MISO (fixed, shared with MFRC522)
#define TFT_SCK 13              // Hardware SPI SCK (fixed, shared with MFRC522)

// TFT Capacitive Touch - Wire (I2C)
#define TOUCH_SDA 18            // Wire SDA (fixed)
#define TOUCH_SCL 19            // Wire SCL (fixed)
#define TOUCH_INT_PIN 7         // Interrupt pin for touch controller
#define TOUCH_RST_PIN -1        // Optional touch reset; set to -1 when not connected (unchanged from ESP32 branch)

// Ignition Control Pin
#define IGNITION_CONTROL_PIN 8  // Digital output to control ignition relay

// Turn Signal Inputs
#define TURN_SIGNAL_LEFT_PIN 22  // Left turn signal input
#define TURN_SIGNAL_RIGHT_PIN 23 // Right turn signal input
#define HAZARD_PIN 24            // Hazard signal input

// Headlight Inputs
#define HIGH_BEAM_PIN 25         // High beam input
#define LOW_BEAM_PIN 26          // Low beam input

// Headlight Outputs (RELAYS)
#define HIGH_BEAM_RELAY_PIN 28    // High beam relay control output
#define LOW_BEAM_RELAY_PIN 29     // Low beam relay control output

// Blinker Outputs (RELAYS)
#define LEFT_BLINKER_RELAY_PIN 30  // Left blinker relay control output
#define RIGHT_BLINKER_RELAY_PIN 31 // Right blinker relay control output
#define ANALOG_SPEED_PIN 21        // Speed sensor input (GPIO interrupt on hall pulse)

// Status LED (RFID read/write activity pulse). NOT Teensy's LED_BUILTIN (13) -
// that pin is this project's SPI SCK, shared by the TFT and MFRC522. See the
// changelog note above.
#define STATUS_LED_PIN 33
