/*
ESP32-S3 (N16R8) Pinmap
Defines pin assignments for all hardware modules using the ESP32-S3 core.
- MFRC522 RFID Reader -> SPI
- GY-521 IMU (MPU6050) -> I2C
- DHT11 Temperature/Humidity -> One-Wire
- GT-U7 GPS Module -> UART1
- TFT Display -> SPI
- TFT Capacitive Touch Controller -> I2C

Last Updated: 7/12/2026
*/

// MFRC522 RFID Reader - SPI
#define MFRC522_CS_PIN 10      // Chip Select for MFRC522
#define MFRC522_RST_PIN 9      // Reset pin for MFRC522
#define MFRC522_IRQ_PIN 14     // Interrupt pin for MFRC522
#define MFRC522_MOSI 11        // SPI MOSI pin
#define MFRC522_MISO 13        // SPI MISO pin
#define MFRC522_SCK 12         // SPI SCK pin

// GY-521 IMU (MPU6050) - I2C
#define GY521_SDA 18           // I2C SDA
#define GY521_SCL 17           // I2C SCL
#define GY521_INT_PIN 3        // Interrupt pin for GY-521

// DHT11 Sensor - One-Wire
#define DHT11_PIN 16           // One-Wire data pin for DHT11

// Tachometer input
#define TACH_PIN 15            // Pulse input for RPM measurement

// Motorcycle analog inputs
#define FUEL_VOLTAGE_PIN 4     // Fuel-level sensor input (ADC1)
#define GEAR_VOLTAGE_PIN 5     // Gear position sensor input (ADC1)

// GT-U7 GPS Module - UART1
#define GPS_RX_PIN 19          // UART1 RX
#define GPS_TX_PIN 20          // UART1 TX
#define GPS_UART_NUM Serial1   // Serial1 (UART1)

// TFT Display - SPI
#define TFT_CS_PIN 5           // Chip Select for TFT display
#define TFT_DC_PIN 6           // Data/Command pin
#define TFT_RST_PIN 7          // Reset pin
#define TFT_BACKLIGHT_PIN 21   // Backlight control (PWM capable)
#define TFT_MOSI 11            // SPI MOSI pin
#define TFT_MISO 13            // SPI MISO pin
#define TFT_SCK 12             // SPI SCK pin

// TFT Capacitive Touch - I2C
#define TOUCH_SDA 8            // I2C SDA for touch controller
#define TOUCH_SCL 9            // I2C SCL for touch controller
#define TOUCH_INT_PIN 4        // Interrupt pin for touch controller
#define TOUCH_RST_PIN 22       // Reset pin for touch controller (optional)

// Ignition Control Pin
#define IGNITION_CONTROL_PIN 2 // Digital output to control ignition relay