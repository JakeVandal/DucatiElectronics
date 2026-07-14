/*
TFT Display Function For displaying GPS data and card information on the TFT display. This function will
be called in the main loop to update the display with the latest GPS data and card information.

Last Updated: 6/02/2026

*/

#include <SPI.h>
#include "../lib/pinMap.h"
#include <Wire.h>
#include <TFT_eSPI.h>
#include "TFTDisplay.h"

TFT_eSPI tft = TFT_eSPI();

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
static const int MAX_RPM = 11000; // 11 kRPM

// Helper forward declarations
void drawTachometer(int rpm, float speedMph, float tempF, float fuelLevel, int gear);
void drawRFIDWriterPage();
void drawWriteButton(bool pressed);

// Public API ---------------------------------------------------------------
void TFT_begin()
{
	pinMode(TFT_CS_PIN, OUTPUT);
	digitalWrite(TFT_CS_PIN, HIGH);
	pinMode(TFT_DC_PIN, OUTPUT);
	digitalWrite(TFT_DC_PIN, HIGH);
	pinMode(TFT_RST_PIN, OUTPUT);
	digitalWrite(TFT_RST_PIN, LOW);
	delay(50);
	digitalWrite(TFT_RST_PIN, HIGH);
	delay(50);

	SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, -1);
	tft.init();
	tft.setRotation(1);
	pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
	analogWrite(TFT_BACKLIGHT_PIN, 255);
	tft.fillScreen(TFT_BLACK);
	// Init I2C for touch controller
	Wire.begin(TOUCH_SDA, TOUCH_SCL);
	Wire.setClock(400000);
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
		Wire.beginTransmission(addr);
		Wire.write(0x02);
		if (Wire.endTransmission() == 0) {
			Wire.requestFrom(addr, (uint8_t)1);
			if (Wire.available()) {
				uint8_t touches = Wire.read();
				if ((touches & 0x0F) > 0) {
					Wire.beginTransmission(addr);
					Wire.write(0x03);
					if (Wire.endTransmission() == 0) {
						Wire.requestFrom(addr, (uint8_t)4);
						if (Wire.available() >= 4) {
							uint8_t xh = Wire.read();
							uint8_t xl = Wire.read();
							uint8_t yh = Wire.read();
							uint8_t yl = Wire.read();
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
	tft.fillScreen(TFT_BLACK);
}

void TFT_prevPage()
{
	currentPage = (currentPage - 1 + 2) % 2;
	tft.fillScreen(TFT_BLACK);
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
	int screenWidth = tft.width();
	if (x > (screenWidth / 2)) {
		TFT_nextPage();
	} else if (x < (screenWidth / 4)) {
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
	tft.fillScreen(TFT_BLACK);
	const int displayWidth = tft.width();
	const int displayHeight = tft.height();

	// Draw title
	tft.setTextColor(TFT_WHITE);
	tft.setTextSize(3);
	tft.setCursor(10, 10);
	tft.print("1999 Ducati 900SS");

	// Draw GPS speed, temperature, fuel, and gear
	tft.setTextSize(2);
	tft.setCursor(10, 40);
	tft.print("Speed: ");
	tft.print(speedMph, 1);
	tft.print(" [mph]");

	tft.setCursor(10, 64);
	tft.print("Temp:  ");
	tft.print(tempF, 1);
	tft.print(" [F]");

	tft.setCursor(10, 88);
	tft.print("Fuel:  ");
	tft.print(fuelLevel, 0);
	tft.print(" [%]");

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
	int barW = max(120, displayWidth - 20);
	int barH = 16;
	tft.drawRect(barX, barY, barW, barH, TFT_WHITE);
	int fillW = constrain((int)(barW * (fuelLevel / 100.0f)), 0, barW);
	tft.fillRect(barX + 1, barY + 1, fillW, barH - 2, TFT_GREEN);
	tft.setTextSize(1);
	tft.setCursor(barX + 4, barY + 2);
	tft.setTextColor(TFT_BLACK);
	tft.print("Fuel Level");
	tft.setTextColor(TFT_WHITE);

	// RPM bar
	int rpmBarX = 10;
	int rpmBarY = 190;
	int rpmBarW = max(120, displayWidth - 20);
	int rpmBarH = 20;
	tft.drawRect(rpmBarX, rpmBarY, rpmBarW, rpmBarH, TFT_WHITE);
	int rpmFillW = constrain((int)(rpmBarW * (rpm / (float)MAX_RPM)), 0, rpmBarW);
	tft.fillRect(rpmBarX + 1, rpmBarY + 1, rpmFillW, rpmBarH - 2, TFT_RED);
	tft.setTextSize(2);
	tft.setCursor(rpmBarX, rpmBarY - 20);
	tft.print("RPM: ");
	tft.print(rpm);
	// small legend showing max
	tft.setTextSize(1);
	tft.setCursor(rpmBarX + 2, rpmBarY + 2);
	tft.setTextColor(TFT_BLACK);
	tft.print("RPM");
	tft.setTextColor(TFT_WHITE);
	tft.setCursor(rpmBarX + rpmBarW - 24, rpmBarY + 2);
	tft.print("11k");
}

void drawWriteButton(bool pressed)
{
	int bx = 60, by = 140, bw = 200, bh = 60;
	uint16_t color = pressed ? 0x7BEF : TFT_BLUE;
	tft.fillRoundRect(bx, by, bw, bh, 8, color);
	tft.setTextColor(TFT_WHITE);
	tft.setTextSize(2);
	tft.setCursor(bx + 30, by + 18);
	tft.print("Write RFID Card");
}

void drawRFIDWriterPage()
{
	tft.fillScreen(TFT_BLACK);
	tft.setTextColor(TFT_WHITE);
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
