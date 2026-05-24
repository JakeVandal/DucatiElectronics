/*
This is the pinmap for the dedicated hardware. It defines the pin numbers for the various components used 
in the project, such as the RFID reader, LEDs, and buttons. This file is included in the main program
 to ensure that all components are correctly connected to the microcontroller.

Last Updated: 5/23/2026
*/ 

// Pin definitions for the RFID reader with SPI communication
#define CS_PIN 10 // Chip Select pin for the RFID reader
#define RST_PIN 9 // Reset pin for the RFID reader
#define MOSI_PIN 11 // Master Out Slave In pin for SPI communication for the RFID reader
#define MISO_PIN 12 // Master In Slave Out pin for SPI communication for the RFID reader
#define SCK_PIN 13 // Serial Clock pin for SPI communication for the RFID reader
#define IRQ_PIN 2 // Interrupt pin for the RFID reader

// Pin definitions for the