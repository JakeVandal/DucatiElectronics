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
#include "ElectronicThrottle.h"
#include "ThrottleBodyControl.h"
#include "FuelCommander.h"

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

ElectronicThrottleConfig throttlePedalConfig = {
  THROTTLE_PEDAL_SENSOR1_PIN,
  THROTTLE_PEDAL_SENSOR2_PIN,
  700,
  3500,
  700,
  3500,
  false,
  0.20f,
  0.15f
};

ThrottleBodyControlConfig throttleBodyConfig = {
  THROTTLE_BODY_LEFT_PWM_PIN,
  THROTTLE_BODY_RIGHT_PWM_PIN,
  0,
  1,
  50,
  16,
  1000,
  2000,
  1.50f
};

ElectronicThrottle throttlePedal(throttlePedalConfig);
ThrottleBodyControl throttleBodies(throttleBodyConfig);
FuelCommander fuelCommander;

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

// Display update timing
static unsigned long lastTachUpdate = 0;
static unsigned long lastFuelUpdate = 0;
static unsigned long lastMiscUpdate = 0;
const unsigned long TACH_UPDATE_INTERVAL_MS = 300;
const unsigned long FUEL_UPDATE_INTERVAL_MS = 5000; // 5 seconds
const unsigned long MISC_UPDATE_INTERVAL_MS = 3000; // 3 seconds
static unsigned long lastThrottleUpdate = 0;
static unsigned long lastThrottleFaultLog = 0;
static unsigned long lastFuelCommandLog = 0;

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

boolean writePICC(byte dataToWrite) {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    Serial.println("No card present for write.");
    return false;
  }

  byte blockAddr = 4;
  byte buffer[16] = {0};
  buffer[0] = dataToWrite;

  if (!mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockAddr, &key, &(mfrc522.uid))) {
    Serial.println("RFID authentication failed.");
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    return false;
  }

  MFRC522::StatusCode status = mfrc522.MIFARE_Write(blockAddr, buffer, 16);
  bool success = (status == MFRC522::STATUS_OK);

  Serial.println(success ? "RFID write successful." : "RFID write failed.");
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  return success;
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
    delay(10);
  }

  Serial1.begin(GPSBaud, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  // Initialize analog inputs
  analogReadResolution(12);

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

  throttlePedal.begin();
  throttleBodies.begin();
  fuelCommander.begin(Serial);
  lastThrottleUpdate = millis();

  Serial.println("RFID reader initialized. Waiting for a card...");
}

void loop() {
  unsigned long now = millis();

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

  throttlePedal.update();
  fuelCommander.processSerial();
  bool throttleFault = throttlePedal.hasFault();
  if (throttleFault && (now - lastThrottleFaultLog >= 1000)) {
    lastThrottleFaultLog = now;
    Serial.println("Throttle pedal plausibility fault; forcing throttle bodies closed.");
  }

  unsigned long throttleDt = now - lastThrottleUpdate;
  lastThrottleUpdate = now;

  bool throttleEnabled = bikeIgnitionOn && !throttleFault;
  throttleBodies.update(throttlePedal.getThrottlePercent(), throttleDt, throttleEnabled);

  // Fuel commander computes target AFR and trim from RPM + throttle load.
  fuelCommander.update(RPMValue, throttlePedal.getThrottlePercent());
  if (now - lastFuelCommandLog >= 1000) {
    lastFuelCommandLog = now;
    Serial.print("FuelCmd AFR=");
    Serial.print(fuelCommander.getCurrentTargetAfr(), 3);
    Serial.print(" trim=");
    Serial.print(fuelCommander.getCurrentFuelTrimPercent(), 3);
    Serial.print(" scale=");
    Serial.println(fuelCommander.getCurrentFuelScale(), 3);
  }

  if (millis() - lastTachUpdate >= TACH_UPDATE_INTERVAL_MS) {
    lastTachUpdate = millis();
    TFT_Tach_update(RPMValue, GPSSpeedMph, CurrentGear);
  }

  if (millis() - lastFuelUpdate >= FUEL_UPDATE_INTERVAL_MS) {
    lastFuelUpdate = millis();
    TFT_Fuel_update(CurrentFuelLevel);
  }

  if (millis() - lastMiscUpdate >= MISC_UPDATE_INTERVAL_MS) {
    lastMiscUpdate = millis();
    TFT_Misc_update(CurrentTempF);
  }

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
  else if (cardFound && bikeIgnitionOn) {
    digitalWrite(IGNITION_CONTROL_PIN, LOW); // turn off ignition relay
    bikeIgnitionOn = false;
    Serial.println("Card with 0x23 detected while ignition on; cutting ignition.");
  }
}
