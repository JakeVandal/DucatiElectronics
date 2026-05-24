#ifndef PICCWRITER_H
#define PICCWRITER_H

#include <Arduino.h>

// Function to get hex input from Serial (e.g., "0x23")
// Returns the byte value entered by the user
byte getHexInputFromSerial();

// Function to write a byte value to the card's first data block
// Parameters:
//   dataToWrite - The byte value to write (e.g., 0x23)
// Returns:
//   true if write is successful, false otherwise
boolean writePICC(byte dataToWrite);

// Function to write via Serial user input
// Prompts user for hex value and writes to card
void writePICCFromSerialInput();

#endif
