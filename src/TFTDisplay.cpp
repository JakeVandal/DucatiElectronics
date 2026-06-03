/*
TFT Display Function For displaying GPS data and card information on the TFT display. This function will
be called in the main loop to update the display with the latest GPS data and card information.

Last Updated: 6/02/2026

*/

#include <SPI.h>
#include "../lib/pinMap.h"
#include <Wire.h>
#include "TFTDisplay.h"

// Select display driver. Define USE_ST7796 to use ST7796_t3 driver.
#define USE_ST7796
#ifdef USE_ST7796
#include <ST7796_t3.h>
ST7796_t3 tft = ST7796_t3(TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);
#else
#include <ILI9341_t3n.h>
ILI9341_t3n tft = ILI9341_t3n(TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);
#endif

// Touch IRQ flag and ISR
volatile bool touchIRQ = false;
void IRAM_ATTR touchISR()
{
	touchIRQ = true;
}

// Pages
static int currentPage = 0; // 0 = tachometer, 1 = RFID writer

// Cached values (updated by main)
static int cachedRPM = 0;
static float cachedGPSSpeed = 0.0f; // Mph
static float cachedTempF = 0.0f;
static float cachedFuelLevel = 0.0f; // percent
static int cachedGear = 0;

// RFID write state
static bool writeRequested = false;
static bool writeSuccess = false;

// Layout constants for gauge
static const int TACH_CENTER_X = 160;
static const int TACH_CENTER_Y = 180;
static const int TACH_RADIUS = 120;
static const int MAX_RPM = 11000; // 11 kRPM

// Helper forward declarations
void drawTachometer(int rpm, float speedMph, float tempF, float fuelLevel, int gear);
void drawRFIDWriterPage();
void drawWriteButton(bool pressed);

// Public API ---------------------------------------------------------------
void TFT_begin()
{
	SPI.begin();
	tft.begin();
	tft.setRotation(1);
	pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
	analogWrite(TFT_BACKLIGHT_PIN, 255);
	tft.fillScreen(ILI9341_BLACK);
	// Init I2C1 for touch controller
	Wire1.begin();
	Wire1.setClock(400000);
	// Configure touch interrupt pin
	pinMode(TOUCH_INT_PIN, INPUT_PULLUP);
	attachInterrupt(digitalPinToInterrupt(TOUCH_INT_PIN), touchISR, FALLING);
	// initial draw
	drawTachometer(cachedRPM, cachedGPSSpeed, cachedTempF, cachedFuelLevel, cachedGear);
}

// Update UI values; call frequently from main loop
void TFT_update(int rpm, float gpsSpeedMph, float tempF, float fuelLevel, int gear)
{
	cachedRPM = rpm;
	cachedGPSSpeed = gpsSpeedMph;
	cachedTempF = tempF;
	cachedFuelLevel = fuelLevel;
	cachedGear = gear;
	if (currentPage == 0) {
		drawTachometer(cachedRPM, cachedGPSSpeed, cachedTempF, cachedFuelLevel, cachedGear);
	} else {
		drawRFIDWriterPage();
	}

	// If touch interrupt fired, read touch controller once and dispatch
	if (touchIRQ) {
		touchIRQ = false;
		const uint8_t addr = 0x38; // FT6336U typical address
		Wire1.beginTransmission(addr);
		Wire1.write(0x02);
		if (Wire1.endTransmission() == 0) {
			Wire1.requestFrom(addr, (uint8_t)1);
			if (Wire1.available()) {
				uint8_t touches = Wire1.read();
				if ((touches & 0x0F) > 0) {
					Wire1.beginTransmission(addr);
					Wire1.write(0x03);
					if (Wire1.endTransmission() == 0) {
						Wire1.requestFrom(addr, (uint8_t)4);
						if (Wire1.available() >= 4) {
							uint8_t xh = Wire1.read();
							uint8_t xl = Wire1.read();
							uint8_t yh = Wire1.read();
							uint8_t yl = Wire1.read();
							int rawX = ((xh & 0x0F) << 8) | xl;
							int rawY = ((yh & 0x0F) << 8) | yl;
							// FT6336U is 12-bit. Map to actual screen size.
							int sx = map(rawX, 0, 4095, 0, tft.width());
							int sy = map(rawY, 0, 4095, 0, tft.height());
							TFT_onTouch(sx, sy);
						}
					}
				}
			}
		}
	}
}

// Touch handler / page control functions. Main should call these when touch
// coordinates are available (or map physical swipe to next/prev calls).
void TFT_nextPage()
{
	currentPage = (currentPage + 1) % 2;
	tft.fillScreen(ILI9341_BLACK);
}

void TFT_prevPage()
{
	currentPage = (currentPage - 1 + 2) % 2;
	tft.fillScreen(ILI9341_BLACK);
}

// Call when user presses the on-screen write button or an external trigger
void TFT_requestWrite()
{
	writeRequested = true;
	writeSuccess = false;
}

// Called by main when write result is known
void TFT_setWriteSuccess(bool success)
{
	writeRequested = false;
	writeSuccess = success;
}

// Consume and return whether a write was requested. Main should call this
// and perform the actual RFID write, then call TFT_setWriteSuccess(result).
bool TFT_takeWriteRequest()
{
	if (writeRequested) {
		writeRequested = false;
		return true;
	}
	return false;
}

// Optional: handle raw touch coordinates (x,y) if touch controller available
// A simple implementation: tap right half -> next page, left half -> prev page
void TFT_onTouch(int x, int y)
{
	if (x > 240) {
		TFT_nextPage();
	} else if (x < 80) {
		TFT_prevPage();
	} else if (currentPage == 1) {
		// check if tap is inside write button area
		int bx = 60, by = 140, bw = 200, bh = 60;
		if (x >= bx && x <= (bx + bw) && y >= by && y <= (by + bh)) {
			TFT_requestWrite();
		}
	}
}

// Internal drawing functions ------------------------------------------------
void drawTachometer(int rpm, float speedMph, float tempF, float fuelLevel, int gear)
{
	tft.fillScreen(ILI9341_BLACK);

	// Draw title
	tft.setTextColor(ILI9341_WHITE);
	tft.setTextSize(2);
	tft.setCursor(10, 10);
	tft.print("Tachometer");

	// Draw GPS speed, temperature, fuel, and gear
	tft.setTextSize(2);
	tft.setCursor(10, 40);
	tft.print("Speed: ");
	tft.print(speedMph, 1);
	tft.print(" mph");

	tft.setCursor(10, 64);
	tft.print("Temp:  ");
	tft.print(tempF, 1);
	tft.print(" F");

	tft.setCursor(10, 88);
	tft.print("Fuel:  ");
	tft.print(fuelLevel, 0);
	tft.print(" %");

	tft.setCursor(10, 112);
	tft.print("Gear:  ");
	if (gear == 0) {
		tft.print("N");
	} else {
		tft.print(gear);
	}

	// Fuel bar
	int barX = 10;
	int barY = 140;
	int barW = 220;
	int barH = 16;
	tft.drawRect(barX, barY, barW, barH, ILI9341_WHITE);
	int fillW = constrain((int)(barW * (fuelLevel / 100.0f)), 0, barW);
	tft.fillRect(barX + 1, barY + 1, fillW, barH - 2, ILI9341_GREEN);
	tft.setTextSize(1);
	tft.setCursor(barX + 4, barY + 2);
	tft.setTextColor(ILI9341_BLACK);
	tft.print("Fuel Level");
	tft.setTextColor(ILI9341_WHITE);

	// Draw gauge background (tick marks)
	for (int i = 0; i <= 11; i++) {
		float fraction = (float)i / 11.0f;
		int tickR = TACH_RADIUS;
		float angle = (-135.0 + fraction * 270.0) * DEG_TO_RAD;
		int x1 = TACH_CENTER_X + (int)((tickR - 10) * cos(angle));
		int y1 = TACH_CENTER_Y + (int)((tickR - 10) * sin(angle));
		int x2 = TACH_CENTER_X + (int)((tickR) * cos(angle));
		int y2 = TACH_CENTER_Y + (int)((tickR) * sin(angle));
		tft.drawLine(x1, y1, x2, y2, ILI9341_WHITE);
		// label
		int lx = TACH_CENTER_X + (int)((tickR - 28) * cos(angle));
		int ly = TACH_CENTER_Y + (int)((tickR - 28) * sin(angle));
		tft.setTextSize(1);
		tft.setCursor(lx - 6, ly - 4);
		tft.print(i); // label from 0..11 (x1000)
	}

	// Draw needle
	float rpmFrac = constrain((float)rpm / (float)MAX_RPM, 0.0f, 1.0f);
	float needleAngle = (-135.0 + rpmFrac * 270.0) * DEG_TO_RAD;
	int nx = TACH_CENTER_X + (int)((TACH_RADIUS - 30) * cos(needleAngle));
	int ny = TACH_CENTER_Y + (int)((TACH_RADIUS - 30) * sin(needleAngle));
	// Needle base (clear center)
	tft.fillCircle(TACH_CENTER_X, TACH_CENTER_Y, 6, ILI9341_WHITE);
	// Draw needle line
	tft.drawLine(TACH_CENTER_X, TACH_CENTER_Y, nx, ny, ILI9341_RED);

	// Draw rpm text
	tft.setTextSize(2);
	tft.setCursor(10, 100);
	tft.print("RPM: ");
	tft.print(rpm);
	tft.print(" ");

	// small legend showing max
	tft.setTextSize(1);
	tft.setCursor(10, 130);
	tft.print("Max 11k RPM");
}

void drawWriteButton(bool pressed)
{
	int bx = 60, by = 140, bw = 200, bh = 60;
	uint16_t color = pressed ? ILI9341_DARKGREY : ILI9341_BLUE;
	tft.fillRoundRect(bx, by, bw, bh, 8, color);
	tft.setTextColor(ILI9341_WHITE);
	tft.setTextSize(2);
	tft.setCursor(bx + 30, by + 18);
	tft.print("Write RFID Card");
}

void drawRFIDWriterPage()
{
	tft.fillScreen(ILI9341_BLACK);
	tft.setTextColor(ILI9341_WHITE);
	tft.setTextSize(2);
	tft.setCursor(10, 10);
	tft.print("RFID Writer");

	// Button
	drawWriteButton(writeRequested);

	// Show status
	tft.setTextSize(2);
	tft.setCursor(10, 220);
	if (writeRequested) {
		tft.print("Writing...");
	} else if (writeSuccess) {
		tft.print("Write Success!");
	} else {
		tft.print("Ready");
	}
}
