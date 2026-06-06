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

// DHT11 Temperature and Humidity Sensor
#define DHTTYPE DHT11
DHT dht(DHT11_PIN, DHTTYPE);

// RPM placeholder (wire your tach input to update this value)
int RPMValue = 0;
float CurrentTempF = 0.0f;
float CurrentFuelLevel = 0.0f;
int CurrentGear = 0;

// Interrupt flag for IRQ pin
volatile boolean irqFlag = false;

// Tachometer pulse counting
volatile unsigned long tachPulseCount = 0;

// Bike ignition status
volatile boolean bikeIgnitionOn = false;
 
// Ignition start/watchdog timer: if ignition is enabled but engine
// doesn't reach RPM threshold within this timeout, cut ignition.
unsigned long ignitionOnMillis = 0;
bool ignitionTimerActive = false;
const unsigned long IGNITION_RPM_TIMEOUT_MS = 60000; // 60 seconds
const int RPM_THRESHOLD = 300;

// ISR for tachometer pulse
void tachISR() {
  tachPulseCount++;
}

// ISR for IRQ pin interrupt
void irqHandler() {
  irqFlag = true;
}

// Gear sensor ADC thresholds (0-4095, 12-bit).
// Adjust these values to match your motorcycle's fixed gear step voltages.
static const int GEAR_VOLTAGE_THRESHOLDS[] = {
  200,  // Neutral / below first gear
  700,  // 1st gear upper threshold
  1200, // 2nd gear upper threshold
  1700, // 3rd gear upper threshold
  2200, // 4th gear upper threshold
  2700, // 5th gear upper threshold
  3200  // 6th gear upper threshold
};

int getGearFromVoltage(int rawValue) {
  if (rawValue < GEAR_VOLTAGE_THRESHOLDS[0]) {
    return 0;
  }
  for (int gear = 1; gear < (int)(sizeof(GEAR_VOLTAGE_THRESHOLDS) / sizeof(GEAR_VOLTAGE_THRESHOLDS[0])); ++gear) {
    if (rawValue < GEAR_VOLTAGE_THRESHOLDS[gear]) {
      return gear;
    }
  }
  return 6;
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

  // Initialize analog inputs
  analogReadResolution(12);

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
  if (millis() - lastDHT > 5000) {
    lastDHT = millis();
    float tempC = dht.readTemperature();
    if (!isnan(tempC)) {
      CurrentTempF = tempC * 9.0f / 5.0f + 32.0f;
    }
  }

  // Read fuel voltage from analog input
  int rawFuel = analogRead(FUEL_VOLTAGE_PIN);
  CurrentFuelLevel = constrain(rawFuel * (100.0f / 4095.0f), 0.0f, 100.0f);

  // Read gear voltage from analog input and map via fixed stepped thresholds
  int rawGear = analogRead(GEAR_VOLTAGE_PIN);
  CurrentGear = getGearFromVoltage(rawGear);

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

  TFT_update(RPMValue, GPSSpeedMph, CurrentTempF, CurrentFuelLevel, CurrentGear);

  // Handle touch-triggered RFID write requests
  if (TFT_takeWriteRequest()) {
    Serial.println("TFT requested RFID write");
    // Example: write 0x23 to card
    boolean result = writePICC(0x23);
    TFT_setWriteSuccess(result);
  }

  // Call readPICC to check for card (interrupt-driven)
  boolean cardFound = readPICC();

  // Bike turn on scenario: if card with 0x23 is detected while ignition is off, enable ignition and start watchdog timer
  if (cardFound && !bikeIgnitionOn) {
    Serial.println("Card with 0x23 detected!");

    digitalWrite(IGNITION_CONTROL_PIN, HIGH); // turn on ignition relay
    bikeIgnitionOn = true;

    // Start the ignition-watchdog timer: engine must reach RPM threshold
    // within IGNITION_RPM_TIMEOUT_MS or ignition will be cut.
    ignitionOnMillis = millis();
    ignitionTimerActive = true;
    Serial.println("Ignition enabled; awaiting engine start (RPM>=300) for 60s.");
  }

  // Safety watchdog: if ignition enabled but RPM stays below threshold
  // for the configured timeout, cut ignition and reset state.
  if (bikeIgnitionOn && ignitionTimerActive) {
    if (RPMValue >= RPM_THRESHOLD) {
      // Engine started, cancel watchdog
      ignitionTimerActive = false;
      Serial.println("Engine started; ignition timer canceled.");
    } else if (millis() - ignitionOnMillis >= IGNITION_RPM_TIMEOUT_MS) {
      Serial.println("Safety: Engine did not start within 60s; cutting ignition.");
      digitalWrite(IGNITION_CONTROL_PIN, LOW);
      bikeIgnitionOn = false;
      ignitionTimerActive = false;
    }
  }

  // Bike turn off scenario: if card is detected while ignition is on, cut ignition immediately
  if (cardFound && bikeIgnitionOn) {
    digitalWrite(IGNITION_CONTROL_PIN, LOW); // turn off ignition relay
    bikeIgnitionOn = false;
    Serial.println("Card with 0x23 detected while ignition on; cutting ignition.");
  }
}
