This is the project description for a 1999 Ducati 900ss Electronics Project:

Description:
A dedicated electronics system for a 1999 Ducati 900ss with RFID-based access key ignition system, GPS speedometer, accelerometer data, and touchscreen display to highlight key data (speed, RPM, gear, temperature, and ).

Features:
-> Teensy 4.1 Microcontroller
-> MFRC522 Based Keyless Ignition Access System
    -Read/Write to card/fob via either laptop or through display
-> GT-U7 GPS based speedometer 
-> DHT-11 based temperature and humdity sensing
-> ST7796S based 4" capacitive touch display
    -Displays RPM, speed, temperature, humidity?, lean angle, gear position
    -Second page for programming RFID cards
    -Welcome page on startup
-> RPM through wiring harness
-> GY-521 based accelerometer data

Communications:
MFRC522 -> SPI (SPI0)
GY-521 -> I2C (I2C0)
DHT11 -> One-Wire
GT-U7 -> UART (UART5)
TFT Display -> SPI (SPI1)
TFT Capacitive Touch -> I2C (I2C1)