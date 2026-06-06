/*
Teensy 4.1 Pinmap
Defines pin assignments for all hardware modules:
- MFRC522 RFID Reader -> SPI0
- GY-521 IMU (MPU6050) -> I2C0
- DHT11 Temperature/Humidity -> One-Wire
- GT-U7 GPS Module -> UART5
- TFT Display -> SPI1
- TFT Capacitive Touch Controller -> I2C1

Last Updated: 6/2/2026
*/

// MFRC522 RFID Reader - SPI0
#define MFRC522_CS_PIN 10      // Chip Select for MFRC522 (SPI0 CS)
#define MFRC522_RST_PIN 9      // Reset pin for MFRC522
#define MFRC522_IRQ_PIN 2      // Interrupt pin for MFRC522
#define MFRC522_MOSI 11           // SPI0 MOSI pin
#define MFRC522_MISO 12           // SPI0 MISO pin
#define MFRC522_SCK 13            // SPI0 SCK pin

// GY-521 IMU (MPU6050) - I2C0
#define GY521_SDA 18           // I2C0 SDA (Data)
#define GY521_SCL 19           // I2C0 SCL (Clock)
#define GY521_INT_PIN 3        // Interrupt pin for GY-521

// DHT11 Sensor - One-Wire
#define DHT11_PIN 22           // One-Wire data pin for DHT11

// Tachometer input
#define TACH_PIN 23            // Pulse input for RPM measurement (attach to tach sensor)

// Motorcycle analog inputs
#define FUEL_VOLTAGE_PIN A0    // Fuel-level sensor input
#define GEAR_VOLTAGE_PIN A1    // Gear position sensor input

// GT-U7 GPS Module - UART5
#define GPS_RX_PIN 21          // UART5 RX (receives GPS data)
#define GPS_TX_PIN 20          // UART5 TX (not typically used for GPS)
#define GPS_UART_NUM Serial5   // Serial5 (UART5)

// TFT Display - SPI1
#define TFT_CS_PIN 0           // Chip Select for TFT (SPI1 CS)
#define TFT_DC_PIN 6           // Data/Command pin
#define TFT_RST_PIN 7          // Reset pin
#define TFT_BACKLIGHT_PIN 8    // Backlight control (PWM capable)
#define TFT_MOSI 26           // SPI1 MOSI pin
#define TFT_MISO 1            // SPI1 MISO pin
#define TFT_SCK 27            // SPI1 SCK pin

// TFT Capacitive Touch - I2C1
#define TOUCH_SDA 17           // I2C1 SDA (Data)
#define TOUCH_SCL 16           // I2C1 SCL (Clock)
#define TOUCH_INT_PIN 4        // Interrupt pin for touch controller
#define TOUCH_RST_PIN 5        // Reset pin for touch controller (optional)

// Ignition Control Pin
#define IGNITION_CONTROL_PIN 24 // Digital output to control ignition relay