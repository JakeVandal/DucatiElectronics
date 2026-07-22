/*
TFT Display Function For displaying GPS data and card information on the TFT display. This function will
be called in the main loop to update the display with the latest GPS data and card information.

Last Updated: 7/17/2026

*/

#include <SPI.h>
#include "../lib/pinMap.h"
#include <Wire.h>
#include <TFT_eSPI.h>
#include "TFTDisplay.h"

TFT_eSPI tft = TFT_eSPI();

// Touch IRQ flag and ISR
volatile bool touchIRQ = false;
void IRAM_ATTR touchISR() {
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

// Rolling average for fuel level over 60 seconds
static const int FUEL_AVG_SAMPLES = 128;
struct FuelSample { unsigned long ts; float value; };
static FuelSample fuelSamples[FUEL_AVG_SAMPLES];
static int fuelSampleHead = 0;
static int fuelSampleCount = 0;

// Helper forward declarations
void drawTachometer(int rpm, float speedMph, int gear);
void drawFuel(float fuelLevel);
void drawMisc(float tempF);
void drawRFIDWriterPage();
void drawWriteButton(bool pressed);

// Public API ---------------------------------------------------------------
void TFT_begin() {
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
	drawTachometer(cachedRPM, cachedGPSSpeed, cachedGear);
	drawFuel(cachedFuelLevel);
	drawMisc(cachedTempF);
}

// Update UI values; call frequently from main loop
void TFT_update(int rpm, float gpsSpeedMph, float tempF, float fuelLevel, int gear) {
	cachedRPM = rpm;
	cachedGPSSpeed = gpsSpeedMph;
	cachedTempF = tempF;
	cachedFuelLevel = fuelLevel;
	cachedGear = gear;

	if (currentPage == 0) {
		drawTachometer(cachedRPM, cachedGPSSpeed, cachedGear);
		drawFuel(cachedFuelLevel);
		drawMisc(cachedTempF);
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

void TFT_Tach_update(int rpm, float gpsSpeedMph, int gear) {
	cachedRPM = rpm;
	cachedGPSSpeed = gpsSpeedMph;
	cachedGear = gear;

	if (currentPage == 0) {
		drawTachometer(cachedRPM, cachedGPSSpeed, cachedGear);
	} else {
		drawRFIDWriterPage();
	}
}

void TFT_Fuel_update(float fuelLevel) {
	cachedFuelLevel = fuelLevel;
	if (currentPage == 0) {
		drawFuel(cachedFuelLevel);
	}
}

void TFT_Misc_update(float tempF) {
	cachedTempF = tempF;
	if (currentPage == 0) {
		drawMisc(cachedTempF);
	}
}

// Touch handler / page control functions. Main should call these when touch
// coordinates are available (or map physical swipe to next/prev calls).
void TFT_nextPage() {
	currentPage = (currentPage + 1) % 2;
	tft.fillScreen(TFT_BLACK);
}

void TFT_prevPage() {
	currentPage = (currentPage - 1 + 2) % 2;
	tft.fillScreen(TFT_BLACK);
}

// Call when user presses the on-screen write button or an external trigger
void TFT_requestWrite() {
	writeRequested = true;
	writeSuccess = false;
}

// Called by main when write result is known
void TFT_setWriteSuccess(bool success) {
	writeRequested = false;
	writeSuccess = success;
}

// Consume and return whether a write was requested. Main should call this
// and perform the actual RFID write, then call TFT_setWriteSuccess(result).
bool TFT_takeWriteRequest() {
	if (writeRequested) {
		writeRequested = false;
		return true;
	}
	return false;
}

// Optional: handle raw touch coordinates (x,y) if touch controller available
// A simple implementation: tap right half -> next page, left half -> prev page
void TFT_onTouch(int x, int y) {
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
static bool tachLayoutDrawn = false;

void drawTachometerStatic() {
    tft.fillScreen(TFT_BLACK);
    const int displayWidth = tft.width();

    tft.setTextColor(TFT_WHITE);

    // Static labels only — values get drawn separately with background color
    // so they overwrite in place instead of needing a fillScreen.
    tft.setTextSize(2);

    int barX = 10, barY = 10, barW = displayWidth / 3, barH = 20;
    tft.drawRect(barX, barY, barW, barH, TFT_WHITE);

    int rpmBarX = 10, rpmBarY = 220, rpmBarW = max(120, displayWidth - 20), rpmBarH = 80;
    tft.drawRect(rpmBarX, rpmBarY, rpmBarW, rpmBarH, TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(rpmBarX + rpmBarW - 24, rpmBarY + 2);
    tft.setTextColor(TFT_WHITE);

    tachLayoutDrawn = true;
}

void drawTachometer(int rpm, float speedMph, int gear) {
    if (!tachLayoutDrawn) drawTachometerStatic();

    const int displayWidth = tft.width();
	const int displayHeight = tft.height();

    // Setting an explicit background color makes print() overwrite the
    // previous digits automatically instead of blending old/new text.
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.setCursor(displayWidth / 2 - 77, displayHeight / 2 - 40);
	tft.setTextSize(5);
    tft.print(speedMph, 1); 
	tft.setTextSize(2);
	tft.print(" [Mph]"); // trailing spaces clear leftover digits

	tft.setTextSize(4);
	tft.setCursor(15, 45);
	if (gear == 0) { 
		tft.setTextColor(TFT_GREEN);
		tft.print("N");
	} else { 
		tft.setTextColor(TFT_WHITE); 
		tft.print(gear);
	}

    // RPM bar
    int rpmBarX = 10, rpmBarY = 220, rpmBarW = max(120, displayWidth - 20), rpmBarH = 80;
    tft.fillRect(rpmBarX + 1, rpmBarY + 1, rpmBarW - 2, rpmBarH - 2, TFT_BLACK);
    int rpmFillW = constrain((int)((rpmBarW - 2) * (rpm / (float)MAX_RPM)), 0, rpmBarW - 2);
    tft.fillRect(rpmBarX + 1, rpmBarY + 1, rpmFillW, rpmBarH - 2, TFT_RED);

	// Draw tick marks every 1000 RPM
    int tickHeight = rpmBarH / 3;
    int tickStartY = rpmBarY + (rpmBarH - tickHeight) / 2; // center vertically
    for (int rpm_mark = 1000; rpm_mark <= MAX_RPM; rpm_mark += 1000) {
        int tickX = rpmBarX + 1 + (int)((rpmBarW - 2) * (rpm_mark / (float)MAX_RPM));
        tft.drawLine(tickX, tickStartY, tickX, tickStartY + tickHeight, TFT_DARKGREY);
    }

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
	tft.setTextSize(2.5);
    tft.setCursor(rpmBarX, rpmBarY - 25);
    tft.print("RPM: ");
    tft.print(rpm);
    tft.print("    ");
}

void drawFuel(float fuelLevel) {
	const int displayWidth = tft.width();
	const int displayHeight = tft.height();

	// Record sample with timestamp
	unsigned long now = millis();
	fuelSamples[fuelSampleHead] = { now, fuelLevel };
	fuelSampleHead = (fuelSampleHead + 1) % FUEL_AVG_SAMPLES;
	if (fuelSampleCount < FUEL_AVG_SAMPLES) fuelSampleCount++;

	// Compute rolling average over the last 60 seconds
	float sum = 0.0f;
	int count = 0;
	for (int i = 0; i < fuelSampleCount; i++) {
		int idx = (fuelSampleHead - 1 - i + FUEL_AVG_SAMPLES) % FUEL_AVG_SAMPLES;
		if (now - fuelSamples[idx].ts <= 60000UL) {
			sum += fuelSamples[idx].value;
			count++;
		}
	}
	
	float avgFuel = (count > 0) ? sum / count : fuelLevel;

	// Fuel bar — clear just the fill area, don't redraw the border
    int barX = 10, barY = 10, barW = displayWidth / 3, barH = 20;
    tft.fillRect(barX + 1, barY + 1, barW - 2, barH - 2, TFT_BLACK);
    int fillW = constrain((int)((barW - 2) * (avgFuel / 100.0f)), 0, barW - 2);
    tft.fillRect(barX + 1, barY + 1, fillW, barH - 2, TFT_GREEN);

	tft.setTextSize(2);
	tft.setTextColor(TFT_WHITE, TFT_BLACK);
	tft.setCursor(10 + displayWidth / 3 + 7, 12);
    tft.print(avgFuel, 0); tft.print("[%]");
}

void drawMisc(float tempF) {
	const int displayWidth = tft.width();
	const int displayHeight = tft.height();

	// Temperature display
	tft.setTextColor(TFT_WHITE, TFT_BLACK);
	tft.setTextSize(2);
    tft.setCursor(375, 15);
    tft.print(tempF, 1); tft.print("[F]");

	// Perhaps add IMU data? 
}

void drawWriteButton(bool pressed) {
	int bx = 100, by = 140, bw = 200, bh = 60;
	uint16_t color = pressed ? TFT_RED : TFT_BLUE;
	tft.fillRoundRect(bx, by, bw, bh, 8, color);
	tft.setTextColor(TFT_WHITE);
	tft.setTextSize(2);
	tft.setCursor(bx, by);
	tft.print("Write RFID Card");
}

void drawRFIDWriterPage() {
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
