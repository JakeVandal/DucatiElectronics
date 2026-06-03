/* 
This is the main file for the Ducati Electronics project. It initializes the RFID reader, sets up the necessary pins, 
and contains the main loop for reading RFID tags and controlling the LEDs based on the tag's status. The program uses 
the MFRC522 library for interfacing with the RFID reader and SPI communication.

Last Updated: 6/2/2026

Version: 1.0

*/

#include <MFRC522.h>
#include <SPI.h>
#include "../lib/pinMap.h"
#include "piccWriter.h"
#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "TFTDisplay.h"
#include <DHT.h>

// Initialize the RFID reader
MFRC522 mfrc522(MFRC522_CS_PIN, MFRC522_RST_PIN);   // Create MFRC522 instance.
MFRC522::MIFARE_Key key;

// Initialize GPS
TinyGPSPlus gps;
static const uint32_t GPSBaud = 9600;
int LatitudeCurrent = 0;
int LongitudeCurrent = 0;

int TimeCurrent = 0;

float GPSSpeedMph = 0.0f;

// DHT sensor
#define DHTTYPE DHT11
DHT dht(DHT11_PIN, DHTTYPE);

// RPM placeholder (wire your tach input to update this value)
int RPMValue = 0;
float CurrentTempF = 0.0f;

// Interrupt flag for IRQ pin
volatile boolean irqFlag = false;

// Tachometer pulse counting
volatile unsigned long tachPulseCount = 0;

// ISR for tachometer pulse
void IRAM_ATTR tachISR() {
  tachPulseCount++;
}

// ISR for IRQ pin interrupt
void irqHandler() {
  irqFlag = true;
}

// Function to read PICC from MFRC522 and check if it reads 0x23
boolean readPICC() {
  // Check if interrupt flag is set
  if (!irqFlag) {
    return false;
  }

  irqFlag = false;

  // Look for new cards
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return false;
  }

  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial()) {
    return false;
  }

  // Print the UID of the card
  Serial.print("Card UID: ");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();

  // Check if first byte of UID is 0x23
  boolean cardMatch = (mfrc522.uid.uidByte[0] == 0x23);

  // Halt the PICC
  mfrc522.PICC_HaltA();
  // Stop encryption on PCD
  mfrc522.PCD_StopCrypto1();

  return cardMatch;
}



void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial); // Wait for serial port to connect. Needed for native USB

  Serial5.begin(GPSBaud);

  // Initialize SPI communication
  SPI.begin();      // Init SPI bus
  mfrc522.PCD_Init();   // Init MFRC522 card

  // Initialize TFT and sensors
  dht.begin();
  TFT_begin();

  // Set the key for authentication (default key for MIFARE cards)
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  // Attach interrupt handler to IRQ pin
  pinMode(MFRC522_IRQ_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(MFRC522_IRQ_PIN), irqHandler, FALLING);

  // Tachometer input
  pinMode(TACH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TACH_PIN), tachISR, RISING);

  Serial.println("RFID reader initialized. Waiting for a card...");
}

void loop() {
  // Process GPS serial data
  while (Serial5.available() > 0) {
    gps.encode(Serial5.read());
  }

  if (gps.location.isUpdated() && gps.location.isValid()) {
    LatitudeCurrent = gps.location.rawLat().billionths;
    LongitudeCurrent = gps.location.rawLng().billionths;
    TimeCurrent = gps.time.value();
  }
  // Update GPS speed (Mph)
  if (gps.speed.isValid()) {
    GPSSpeedMph = gps.speed.mph();
  }

  // Read DHT11 periodically (simple, non-blocking delay)
  static unsigned long lastDHT = 0;
  if (millis() - lastDHT > 2000) {
    lastDHT = millis();
    float t = dht.readTemperature();
    if (!isnan(t)) {
      // update cached temp
      CurrentTempF = t;
    }
  }

  // Update TFT with RPM, GPS speed, and temp
  // Update RPM once per second based on pulse count. Assumes 1 pulse per rev.
  static unsigned long lastRPMMillis = 0;
  if (millis() - lastRPMMillis >= 1000) {
    noInterrupts();
    unsigned long pulses = tachPulseCount;
    tachPulseCount = 0;
    interrupts();
    RPMValue = pulses * 60; // pulses per second -> RPM
    lastRPMMillis += 1000;
  }

  TFT_update(RPMValue, GPSSpeedMph, CurrentTempF);

  // Handle touch-triggered RFID write requests
  if (TFT_takeWriteRequest()) {
    Serial.println("TFT requested RFID write");
    // Example: write 0x23 to card
    boolean result = writePICC(0x23);
    TFT_setWriteSuccess(result);
  }

  // Call readPICC to check for card (interrupt-driven)
  boolean cardFound = readPICC();

  if (cardFound) {
    Serial.println("Card with 0x23 detected!");
    // Add code here to control LEDs or perform other actions
  }
}
