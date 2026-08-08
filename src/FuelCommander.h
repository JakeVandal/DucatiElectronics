#ifndef FUEL_COMMANDER_H
#define FUEL_COMMANDER_H

#include <Arduino.h>

class FuelCommander {
public:
  static const int kRpmBins = 41;
  static const int kLoadBins = 6;

  FuelCommander();

  void begin(Stream& serialPort);
  void update(int rpm, float throttlePercent);
  void processSerial();

  float getCurrentTargetAfr() const;
  float getCurrentFuelTrimPercent() const;
  float getCurrentFuelScale() const;

  float getTargetAfrByIndex(int rpmIndex, int loadIndex) const;
  float getFuelTrimByIndex(int rpmIndex, int loadIndex) const;
  bool setTargetAfrByIndex(int rpmIndex, int loadIndex, float value);
  bool setFuelTrimByIndex(int rpmIndex, int loadIndex, float value);

private:
  Stream* serial_;

  int rpmBins_[kRpmBins];
  float loadBins_[kLoadBins];

  float targetAfrMap_[kRpmBins][kLoadBins];
  float fuelTrimPercentMap_[kRpmBins][kLoadBins];

  float currentTargetAfr_;
  float currentFuelTrimPercent_;

  int lowerIndexForRpm(int rpm) const;
  int lowerIndexForLoad(float load) const;
  float bilinearInterpolate(const float table[kRpmBins][kLoadBins], int rpm, float load) const;

  void printHelp();
  void dumpTable(const float table[kRpmBins][kLoadBins], const char* name);
  bool parseSetCommand(const String& line);
  bool parseGetCommand(const String& line);
};

#endif // FUEL_COMMANDER_H
