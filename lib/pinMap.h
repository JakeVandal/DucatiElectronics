/*
ESP32-S3 (N16R8) Pinmap
Defines pin assignments for all hardware modules using the ESP32-S3 core.
- MFRC522 RFID Reader -> SPI (shares bus with TFT: MOSI 11 / MISO 13 / SCK 12, separate CS)
- GY-521 IMU (MPU6050) -> I2C
- DHT11 Temperature/Humidity -> One-Wire
- GT-U7 GPS Module -> UART1
- TFT Display -> SPI (shares bus with MFRC522)
- TFT Capacitive Touch Controller -> I2C

Last Updated: 8/13/2026

CHANGELOG (fixes from prior revision):
- GPIO9 conflict resolved: MFRC522_RST moved to 38, TOUCH_SCL moved to 39
- GEAR_VOLTAGE_PIN moved from GPIO42 (not ADC-capable) to GPIO9 (valid ADC1_CH8)
- GY521_INT_PIN moved from GPIO3 (JTAG strapping pin) to GPIO40
- GPS_RX/TX moved from GPIO19/20 (native USB D-/D+) to GPIO41/42
- Reserved GPIO26-37 avoided throughout (used internally by Octal PSRAM/Flash on N16R8)
- TOUCH_RST_PIN moved from GPIO22 to GPIO47 (GPIO22-25 don't exist on the ESP32-S3 chip)
- MFRC522 and TFT intentionally share the SPI bus (MOSI/MISO/SCK) with separate
  CS pins -- this is valid SPI usage, not a conflict
- Relay outputs reassigned: GPIO48-51 were invalid (ESP32-S3 max is GPIO48, and 49-51
  do not exist). HIGH_BEAM_RELAY moved to GPIO0 (valid output post-boot strapping),
  LOW_BEAM_RELAY to GPIO19, LEFT_BLINKER_RELAY to GPIO20, RIGHT_BLINKER_RELAY to GPIO48.
  GPIO19/20 are USB D-/D+ but are available as general GPIO when USB CDC is disabled
  (appropriate for a deployed motorcycle unit).
*/

// MFRC522 RFID Reader - SPI (shares bus with TFT)
#define MFRC522_CS_PIN 10      // Chip Select for MFRC522
#define MFRC522_RST_PIN 38     // Reset pin for MFRC522 (moved from 9 - was conflicting with TOUCH_SCL)
#define MFRC522_IRQ_PIN 14     // Interrupt pin for MFRC522
#define MFRC522_MOSI 11        // SPI MOSI pin (shared with TFT)
#define MFRC522_MISO 13        // SPI MISO pin (shared with TFT)
#define MFRC522_SCK 12         // SPI SCK pin (shared with TFT)

// GY-521 IMU (MPU6050) - I2C
#define GY521_SDA 18           // I2C SDA
#define GY521_SCL 17           // I2C SCL
#define GY521_INT_PIN 40       // Interrupt pin for GY-521 (moved from 3 - JTAG strapping pin)

// DHT11 Sensor - One-Wire
#define DHT11_PIN 16           // One-Wire data pin for DHT11

// Tachometer input
#define TACH_PIN 15            // Pulse input for RPM measurement

// Motorcycle analog inputs (must stay on ADC1: GPIO1-10, since ADC2 is unusable while WiFi is active)
#define FUEL_VOLTAGE_PIN 1     // Fuel-level sensor input (ADC1_CH0)
#define GEAR_VOLTAGE_PIN 9     // Gear position sensor input (ADC1_CH8) - moved from 42 (not ADC-capable)

// GT-U7 GPS Module - UART1
#define GPS_RX_PIN 41          // UART1 RX (moved from 19 - shared with native USB D-)
#define GPS_TX_PIN 42          // UART1 TX (moved from 20 - shared with native USB D+)
#define GPS_UART_NUM Serial1   // Serial1 (UART1)

// TFT Display - SPI (shares bus with MFRC522) - unchanged from original
#define TFT_CS_PIN 5           // Chip Select for TFT display
#define TFT_DC_PIN 6           // Data/Command pin
#define TFT_RST_PIN 7          // Reset pin
#define TFT_BACKLIGHT_PIN 21   // Backlight control (PWM capable)
#define TFT_MOSI 11            // SPI MOSI pin (shared with MFRC522)
#define TFT_MISO 13            // SPI MISO pin (shared with MFRC522)
#define TFT_SCK 12             // SPI SCK pin (shared with MFRC522)

// TFT Capacitive Touch - I2C
#define TOUCH_SDA 8            // I2C SDA for touch controller
#define TOUCH_SCL 39           // I2C SCL for touch controller (moved from 9 - conflicted with MFRC522_RST)
#define TOUCH_INT_PIN 4        // Interrupt pin for touch controller
#define TOUCH_RST_PIN -1       // Optional touch reset; set to -1 when not connected

// Ignition Control Pin
#define IGNITION_CONTROL_PIN 2 // Digital output to control ignition relay

// Turn Signal Inputs
#define TURN_SIGNAL_LEFT_PIN 43  // Left turn signal input
#define TURN_SIGNAL_RIGHT_PIN 44 // Right turn signal input
#define HAZARD_PIN 45            // Hazard signal input

// Headlight Inputs
#define HIGH_BEAM_PIN 46         // High beam input
#define LOW_BEAM_PIN 47          // Low beam input

// Headlight Outputs (RELAYS)
// Note: GPIO19/20 are USB D-/D+ but are usable as general GPIO when USB CDC is disabled (deployed unit)
#define HIGH_BEAM_RELAY_PIN 0    // High beam relay control output (GPIO0: valid output after boot)
#define LOW_BEAM_RELAY_PIN 19    // Low beam relay control output (GPIO19: usable when USB CDC disabled)

// Blinker Outputs (RELAYS)
#define LEFT_BLINKER_RELAY_PIN 20  // Left blinker relay control output (GPIO20: usable when USB CDC disabled)
#define RIGHT_BLINKER_RELAY_PIN 48 // Right blinker relay control output
#define ANALOG_SPEED_PIN 3       // Speed sensor input (GPIO interrupt on hall pulse)
