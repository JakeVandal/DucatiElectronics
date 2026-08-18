/*
    Relay Control File that handles the relay control for the Ducati Electronics project. 
    This file is responsible for managing the relay states, controlling the ignition, 
    and interfacing with other components as needed.

    Note: Ignition relay control is handled by main.cpp via RFID card detection

    Last Updated: 8/11/2026
*/

#include <SPI.h>
#include "../lib/pinMap.h"
#include <Wire.h>
#include "RelayControl.h"

const float R1 = 100000.0; // 100k ohm
const float R2 = 10000.0; // 10k ohm

// State tracking for turn signal toggle control
static bool leftBlinkerActive = false;
static bool rightBlinkerActive = false;
static bool blinkerState = false; // For blinking animation
static unsigned long lastBlinkTime = 0;
static unsigned long blinkInterval = 500; // 500ms on/off for blinkers

// Debouncing
static const unsigned long debounceDelay = 50; // 50ms debounce

// Helper function to handle debouncing
bool isButtonPressed(int pin) {
    static unsigned long lastPressTime[52] = {0}; // Array to track press time per pin
    static bool lastState[52] = {false}; // false = not pressed (active-low inputs)

    if (pin < 0 || pin >= (int)(sizeof(lastPressTime) / sizeof(lastPressTime[0]))) {
        return false;
    }

    bool currentState = digitalRead(pin) == LOW; // true when pressed (active-low)

    if (currentState != lastState[pin]) {
        lastPressTime[pin] = millis();
    }

    lastState[pin] = currentState;

    // Button is considered pressed if LOW for longer than debounce delay
    if (currentState && (millis() - lastPressTime[pin]) > debounceDelay) {
        return true;
    }
    return false;
}

// Helper function to detect a single button press (edge detection)
bool buttonPressDetected(int pin) {
    static bool wasPressed[52] = {false};

    if (pin < 0 || pin >= (int)(sizeof(wasPressed) / sizeof(wasPressed[0]))) {
        return false;
    }

    bool pressed = isButtonPressed(pin);
    bool detected = pressed && !wasPressed[pin];
    wasPressed[pin] = pressed;
    return detected;
}

// Initialize relay control pins
void relayControl_init() {
    // Input pins are already initialized in main.cpp with interrupt handlers
    // This function only initializes the OUTPUT relay pins
    
    pinMode(HIGH_BEAM_RELAY_PIN, OUTPUT);
    digitalWrite(HIGH_BEAM_RELAY_PIN, LOW);

    pinMode(LOW_BEAM_RELAY_PIN, OUTPUT);
    digitalWrite(LOW_BEAM_RELAY_PIN, LOW);

    pinMode(LEFT_BLINKER_RELAY_PIN, OUTPUT);
    digitalWrite(LEFT_BLINKER_RELAY_PIN, LOW);

    pinMode(RIGHT_BLINKER_RELAY_PIN, OUTPUT);
    digitalWrite(RIGHT_BLINKER_RELAY_PIN, LOW);

    // Note: Ignition relay (IGNITION_CONTROL_PIN) is initialized and controlled by main.cpp via RFID

    Serial.println("Relay Control Initialized.");
}

// Update relay control states (call this from main loop)
void relayControl_update() {
    // ===== HEADLIGHT ROCKER SWITCH CONTROL =====
    // Rocker switch: pressing one side activates that beam, deactivates the other (mutually exclusive)
    if (digitalRead(HIGH_BEAM_PIN) == LOW) {   
        // High beam switch activated
        digitalWrite(HIGH_BEAM_RELAY_PIN, HIGH);
        digitalWrite(LOW_BEAM_RELAY_PIN, LOW);  // Ensure low beam is off
    } else if (digitalRead(LOW_BEAM_PIN) == LOW) {
        // Low beam switch activated
        digitalWrite(LOW_BEAM_RELAY_PIN, HIGH);
        digitalWrite(HIGH_BEAM_RELAY_PIN, LOW);  // Ensure high beam is off
    } else {
        // Both switches released - turn off both beams
        digitalWrite(HIGH_BEAM_RELAY_PIN, LOW);
        digitalWrite(LOW_BEAM_RELAY_PIN, LOW);
    }

    // ===== LEFT TURN SIGNAL (BLINKER) CONTROL =====
    // Toggle behavior: first press starts blinking, second press stops blinking
    if (buttonPressDetected(TURN_SIGNAL_LEFT_PIN)) {
        leftBlinkerActive = !leftBlinkerActive; // Toggle state
        blinkerState = false; // Reset blink animation
        lastBlinkTime = millis();
    }

    // ===== RIGHT TURN SIGNAL (BLINKER) CONTROL =====
    // Toggle behavior: first press starts blinking, second press stops blinking
    if (buttonPressDetected(TURN_SIGNAL_RIGHT_PIN)) {
        rightBlinkerActive = !rightBlinkerActive; // Toggle state
        blinkerState = false; // Reset blink animation
        lastBlinkTime = millis();
    }

    // ===== BLINKER ANIMATION =====
    // Create blinking effect (turn on/off at regular intervals) for active blinkers
    if ((millis() - lastBlinkTime) >= blinkInterval) {
        lastBlinkTime = millis();
        blinkerState = !blinkerState; // Toggle blink state
    }

    // Apply blink state to active blinkers
    if (leftBlinkerActive) {
        digitalWrite(LEFT_BLINKER_RELAY_PIN, blinkerState ? HIGH : LOW);
    } else {
        digitalWrite(LEFT_BLINKER_RELAY_PIN, LOW);
    }

    if (rightBlinkerActive) {
        digitalWrite(RIGHT_BLINKER_RELAY_PIN, blinkerState ? HIGH : LOW);
    } else {
        digitalWrite(RIGHT_BLINKER_RELAY_PIN, LOW);
    }
}

// State getter functions - allow main.cpp to query relay control state
bool relayControl_getHighBeamActive() {
    // High beam is active if HIGH_BEAM_PIN is pressed
    return digitalRead(HIGH_BEAM_PIN) == LOW;
}

bool relayControl_getLowBeamActive() {
    // Low beam is active if LOW_BEAM_PIN is pressed
    return digitalRead(LOW_BEAM_PIN) == LOW;
}

bool relayControl_getLeftBlinkerActive() {
    return leftBlinkerActive;
}

bool relayControl_getRightBlinkerActive() {
    return rightBlinkerActive;
}