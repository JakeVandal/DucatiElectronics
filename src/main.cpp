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
#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "TFTDisplay.h"
#include <DHT.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 48
#endif

// Initialize the RFID reader
MFRC522 mfrc522(MFRC522_CS_PIN, MFRC522_RST_PIN);   // Create MFRC522 instance.
MFRC522::MIFARE_Key key;
static const byte RFID_BLOCK = 4;

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
bool writeArmed = false;
unsigned long lastReadPrintMs = 0;
unsigned long lastPollMs = 0;
unsigned long ledOffAtMs = 0;
unsigned long lastCardHandledMs = 0;
unsigned long ignitionCardRearmAtMs = 0;

// Tachometer pulse counting
volatile unsigned long tachPulseCount = 0;

// Bike ignition status
volatile boolean bikeIgnitionOn = false;
 
// Ignition start/watchdog timer: if ignition is enabled but engine
// doesn't reach RPM threshold within this timeout, cut ignition.
unsigned long ignitionOnMillis = 0;
bool ignitionTimerActive = false;
const unsigned long IGNITION_RPM_TIMEOUT_MS = 60000; // 60 seconds
const unsigned long RFID_READ_COOLDOWN_MS = 1000;
const unsigned long IGNITION_CARD_REARM_MS = 3000;
const int RPM_THRESHOLD = 300;

// Display update timing
static unsigned long lastTachUpdate = 0;
static unsigned long lastFuelUpdate = 0;
static unsigned long lastMiscUpdate = 0;
const unsigned long TACH_UPDATE_INTERVAL_MS = 300;
const unsigned long FUEL_UPDATE_INTERVAL_MS = 5000; // 5 seconds
const unsigned long MISC_UPDATE_INTERVAL_MS = 3000; // 3 seconds

// ISR for tachometer pulse
void tachISR() {
  tachPulseCount++;
}

void pulseLed(unsigned long ms) {
  digitalWrite(LED_BUILTIN, HIGH);
  ledOffAtMs = millis() + ms;
}

// ISR for IRQ pin interrupt
void IRAM_ATTR irqHandler() {
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

void clearRfidIrqFlags() {
  mfrc522.PCD_WriteRegister(MFRC522::ComIrqReg, 0x7F);
}

void enableRfidIrq() {
  mfrc522.PCD_WriteRegister(MFRC522::ComIEnReg, 0xA0);
  clearRfidIrqFlags();
}

bool authBlock(byte block) {
  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(
      MFRC522::PICC_CMD_MF_AUTH_KEY_A,
      block,
      &key,
      &(mfrc522.uid));

  return status == MFRC522::STATUS_OK;
}

bool readBlock(byte block, byte *buffer, byte &size) {
  if (!authBlock(block)) {
    Serial.println("RFID authentication failed.");
    return false;
  }

  MFRC522::StatusCode status = mfrc522.MIFARE_Read(block, buffer, &size);
  if (status != MFRC522::STATUS_OK) {
    Serial.println("RFID read failed.");
    mfrc522.PCD_StopCrypto1();
    return false;
  }

  return true;
}

void printUidAndBlock(byte block, const byte *buffer) {
  Serial.print("UID:");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();

  Serial.print("Block ");
  Serial.print(block);
  Serial.print(" hex: ");
  for (byte i = 0; i < 16; i++) {
    if (buffer[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(buffer[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
  pulseLed(60);
}

bool writeBlockWithByte23(byte block) {
  if (!authBlock(block)) {
    Serial.println("RFID authentication failed.");
    return false;
  }

  byte data[16];
  memset(data, 0x00, sizeof(data));
  data[0] = 0x23;

  MFRC522::StatusCode status = mfrc522.MIFARE_Write(block, data, 16);
  bool success = (status == MFRC522::STATUS_OK);
  Serial.println(success ? "RFID write successful." : "RFID write failed.");

  if (success) {
    pulseLed(120);
  }

  mfrc522.PCD_StopCrypto1();
  return success;
}

// Function to read PICC block data from MFRC522 and check if it contains 0x23.
boolean readPICC() {
  if (millis() - lastCardHandledMs < RFID_READ_COOLDOWN_MS) {
    if (irqFlag) {
      noInterrupts();
      irqFlag = false;
      interrupts();
      clearRfidIrqFlags();
    }
    return false;
  }

  bool triggeredByIrq = irqFlag;
  if (triggeredByIrq) {
    noInterrupts();
    irqFlag = false;
    interrupts();
  }

  if (!triggeredByIrq) {
    if (millis() - lastPollMs < 120) {
      return false;
    }
    lastPollMs = millis();
  }

  if (!mfrc522.PICC_IsNewCardPresent()) {
    return false;
  }

  if (!mfrc522.PICC_ReadCardSerial()) {
    return false;
  }

  bool writeSuccess = false;
  if (writeArmed) {
    writeSuccess = writeBlockWithByte23(RFID_BLOCK);
    writeArmed = false;
    TFT_setWriteSuccess(writeSuccess);
  }

  byte buffer[18];
  byte size = sizeof(buffer);
  bool cardMatch = false;
  if (readBlock(RFID_BLOCK, buffer, size)) {
    cardMatch = (buffer[0] == 0x23);
    lastCardHandledMs = millis();

    if (millis() - lastReadPrintMs >= 1000) {
      printUidAndBlock(RFID_BLOCK, buffer);
      lastReadPrintMs = millis();
    }

    mfrc522.PCD_StopCrypto1();
  }

  mfrc522.PICC_HaltA();
  clearRfidIrqFlags();
  return cardMatch;
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
    delay(10);
  }

  Serial.println("Ducati Electronics System Starting...");

  Serial1.begin(GPSBaud, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  // Initialize analog inputs
  analogReadResolution(12);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(IGNITION_CONTROL_PIN, OUTPUT);
  digitalWrite(IGNITION_CONTROL_PIN, LOW);

  // Initialize shared SPI bus once and keep both chip selects deasserted.
  pinMode(MFRC522_CS_PIN, OUTPUT);
  digitalWrite(MFRC522_CS_PIN, HIGH);
  pinMode(TFT_CS_PIN, OUTPUT);
  digitalWrite(TFT_CS_PIN, HIGH);
  SPI.begin(MFRC522_SCK, MFRC522_MISO, MFRC522_MOSI, -1);

  // Initialize TFT first so the shared SPI bus is stable before the RFID reader starts using it.
  dht.begin();
  TFT_begin();

  mfrc522.PCD_Init();   // Init MFRC522 card
  mfrc522.PCD_AntennaOn();

  // Set the key for authentication (default key for MIFARE cards)
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  // Attach interrupt handler to IRQ pin
  pinMode(MFRC522_IRQ_PIN, INPUT_PULLUP);
  enableRfidIrq();
  attachInterrupt(digitalPinToInterrupt(MFRC522_IRQ_PIN), irqHandler, FALLING);

  // Tachometer input
  pinMode(TACH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TACH_PIN), tachISR, RISING);

  Serial.println("RFID reader initialized. Waiting for a card...");
}

void loop() {
  // Process GPS serial data
  while (Serial1.available() > 0) {
    gps.encode(Serial1.read());
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

  // Update RPM once per 300ms based on pulse count. Assumes 1 pulse per rev.
  static unsigned long lastRPMMillis = 0;
  if (millis() - lastRPMMillis >= 300) {
    noInterrupts();
    unsigned long pulses = tachPulseCount;
    tachPulseCount = 0;
    interrupts();
    RPMValue = pulses * 60; // pulses per second -> RPM
    lastRPMMillis += 300;
  }

  if (millis() - lastTachUpdate >= TACH_UPDATE_INTERVAL_MS) {
    lastTachUpdate = millis();
    TFT_Tach_update(RPMValue, GPSSpeedMph, CurrentGear);
  }

  if (millis() - lastFuelUpdate >= FUEL_UPDATE_INTERVAL_MS) {
    lastFuelUpdate = millis();
    TFT_Fuel_update(33);
  }

  if (millis() - lastMiscUpdate >= MISC_UPDATE_INTERVAL_MS) {
    lastMiscUpdate = millis();
    TFT_Misc_update(CurrentTempF);
  }

  if (ledOffAtMs != 0 && millis() >= ledOffAtMs) {
    digitalWrite(LED_BUILTIN, LOW);
    ledOffAtMs = 0;
  }
  
  // Handle touch-triggered RFID write requests
  if (TFT_takeWriteRequest()) {
    Serial.println("TFT requested RFID write");
    writeArmed = true;
  }

  // Call readPICC to check for card (interrupt-driven)
  boolean cardFound = readPICC();

  if (cardFound) {
    if (millis() < ignitionCardRearmAtMs) {
      cardFound = false;
    } else if (!bikeIgnitionOn) {
      Serial.println("Card with 0x23 detected!");

      digitalWrite(IGNITION_CONTROL_PIN, HIGH); // turn on ignition relay
      bikeIgnitionOn = true;
      ignitionCardRearmAtMs = millis() + IGNITION_CARD_REARM_MS;

      // Start the ignition-watchdog timer: engine must reach RPM threshold
      // within IGNITION_RPM_TIMEOUT_MS or ignition will be cut.
      ignitionOnMillis = millis();
      ignitionTimerActive = true;
      Serial.println("Ignition enabled; awaiting engine start (RPM>=300) for 60s.");
    } else {
      digitalWrite(IGNITION_CONTROL_PIN, LOW); // turn off ignition relay
      bikeIgnitionOn = false;
      ignitionTimerActive = false;
      ignitionCardRearmAtMs = millis() + IGNITION_CARD_REARM_MS;
      Serial.println("Card with 0x23 detected while ignition on; cutting ignition.");
    }
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

}
