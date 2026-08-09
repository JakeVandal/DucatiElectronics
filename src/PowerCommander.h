#ifndef POWER_COMMANDER_H
#define POWER_COMMANDER_H

#include <Arduino.h>
#include "FuelCommander.h"

struct PowerCommanderConfig {
  static const int kInjectorCount = 2;
  int injectorInputPins[kInjectorCount];
  int injectorOutputPins[kInjectorCount];
  int widebandAfrPin;

  int widebandMinRaw;
  int widebandMaxRaw;
  float widebandMinAfr;
  float widebandMaxAfr;

  float closedLoopKp;
  float closedLoopMaxTrimPercent;

  unsigned long minInjectorPulseUs;
  unsigned long maxInjectorPulseUs;
};

class PowerCommander {
public:
  static const int kInjectorCount = PowerCommanderConfig::kInjectorCount;

  PowerCommander(const PowerCommanderConfig& config, FuelCommander& fuelCommander);

  void begin();
  void update(int rpm, float throttlePercent);

  void setEnabled(bool enabled);
  void setExternalFault(bool faultActive);

  bool hasFault() const;
  bool isPassThroughMode() const;

  float getMeasuredAfr() const;
  float getClosedLoopTrimPercent() const;
  unsigned long getLastBasePulseUs() const;
  unsigned long getLastCommandedPulseUs() const;
  unsigned long getLastBasePulseUs(int injectorIndex) const;
  unsigned long getLastCommandedPulseUs(int injectorIndex) const;

private:
  static void IRAM_ATTR injector0EdgeIsr();
  static void IRAM_ATTR injector1EdgeIsr();
  void IRAM_ATTR handleInjectorEdge(int injectorIndex);

  float readWidebandAfr() const;
  void updateClosedLoopTrim(float targetAfr);
  void updateOutputState();
  void enablePassThrough();

  static PowerCommander* activeInstance_;

  PowerCommanderConfig config_;
  FuelCommander& fuelCommander_;

  volatile bool enabled_;
  volatile bool externalFault_;
  volatile bool internalFault_;
  volatile bool passThroughMode_;

  volatile bool injectorInputHigh_[kInjectorCount];
  volatile unsigned long injectorInputRiseUs_[kInjectorCount];

  volatile bool injectorOutputHigh_[kInjectorCount];
  volatile unsigned long injectorOutputRiseUs_[kInjectorCount];
  volatile unsigned long injectorOutputStopUs_[kInjectorCount];

  volatile unsigned long lastBasePulseUs_[kInjectorCount];
  volatile unsigned long lastCommandedPulseUs_[kInjectorCount];

  float measuredAfr_;
  bool widebandValid_;
  float closedLoopTrimPercent_;
};

#endif // POWER_COMMANDER_H
