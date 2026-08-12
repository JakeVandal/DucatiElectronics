/* RelayControl.h
 * Public interface for the relay control
 */
#ifndef RELAYCONTROL_H
#define RELAYCONTROL_H

#include <Arduino.h>

// Initialize relay control pins and state
void relayControl_init();

// Update relay control states (call this in main loop)
void relayControl_update();

// Helper functions for button debouncing and press detection
bool isButtonPressed(int pin);
bool buttonPressDetected(int pin);

// State getters - query what the relay control system knows
bool relayControl_getHighBeamActive();
bool relayControl_getLowBeamActive();
bool relayControl_getLeftBlinkerActive();
bool relayControl_getRightBlinkerActive();

#endif // RELAYCONTROL_H