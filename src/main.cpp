/* 
This is the main file for the Ducati Electronics project. It initializes the RFID reader, sets up the necessary pins, 
and contains the main loop for reading RFID tags and controlling the LEDs based on the tag's status. The program uses 
the MFRC522 library for interfacing with the RFID reader and SPI communication.

Last Updated: 5/23/2026

Version: 1.0

*/

#include <iostream>
#include <string>
#include <MFRC522.h>
#include <SPI.h>
#include "pinMap.h"
#include "piccWriter.h"
#include <Arduino.h>

// Initialize the RFID reader
MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.
MFRC522::MIFARE_Key key;

// Interrupt flag for IRQ pin
volatile boolean irqFlag = false;

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

  // Initialize SPI communication
  SPI.begin();      // Init SPI bus
  mfrc522.PCD_Init();   // Init MFRC522 card

  // Set the key for authentication (default key for MIFARE cards)
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  // Attach interrupt handler to IRQ pin
  pinMode(IRQ_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(IRQ_PIN), irqHandler, FALLING);

  Serial.println("RFID reader initialized. Waiting for a card...");
}

void loop() {
  // Call readPICC to check for card
  boolean cardFound = readPICC();

  if (cardFound) {
    Serial.println("Card with 0x23 detected!");
    // Add code here to control LEDs or perform other actions
  }
}
