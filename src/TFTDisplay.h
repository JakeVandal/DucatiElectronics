/* TFTDisplay.h
 * Public interface for the TFT display UI
 */
#ifndef TFTDISPLAY_H
#define TFTDISPLAY_H

#include <Arduino.h>

void TFT_begin();
void TFT_update(int rpm, float gpsSpeedMph, float tempF, float fuelLevel, int gear);
void TFT_onTouch(int x, int y);
void TFT_nextPage();
void TFT_prevPage();
void TFT_requestWrite();
void TFT_setWriteSuccess(bool success);
bool TFT_takeWriteRequest();

#endif // TFTDISPLAY_H
