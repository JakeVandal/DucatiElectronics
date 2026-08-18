/* TFTDisplay.h
 * Public interface for the TFT display UI
 */
#ifndef TFTDISPLAY_H
#define TFTDISPLAY_H

#include <Arduino.h>

void TFT_begin();
void TFT_update(int rpm, float gpsSpeedMph, float tempF, float fuelLevel, int gear);
void TFT_handleTouch();
void TFT_Tach_update(int rpm, float gpsSpeedMph, int gear);
void TFT_Fuel_update(float fuelLevel);
void TFT_Misc_update(float tempF);
void TFT_onTouch(int x, int y);
void TFT_nextPage();
void TFT_prevPage();
void TFT_requestWrite();
void TFT_setWriteSuccess(bool success);
bool TFT_takeWriteRequest();
void TFT_drawTurnSignal(bool leftOn, bool rightOn, bool hazardOn);
void TFT_drawLightIndicator(bool highOn, bool lowOn);

#endif // TFTDISPLAY_H
