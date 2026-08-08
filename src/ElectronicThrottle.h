#ifndef ELECTRONIC_THROTTLE_H
#define ELECTRONIC_THROTTLE_H

#include <Arduino.h>

struct ElectronicThrottleConfig {
  int sensor1Pin;
  int sensor2Pin;

  int sensor1MinRaw;
  int sensor1MaxRaw;
  int sensor2MinRaw;
  int sensor2MaxRaw;

  bool sensor2Inverted;
  float filterAlpha;
  float plausibilityMaxDelta;
};

class ElectronicThrottle {
public:
  explicit ElectronicThrottle(const ElectronicThrottleConfig& config);

  void begin();
  void update();

  float getThrottlePercent() const;
  bool hasFault() const;
  int getRawSensor1() const;
  int getRawSensor2() const;

private:
  float normalize(int raw, int minRaw, int maxRaw) const;

  ElectronicThrottleConfig config_;
  int rawSensor1_;
  int rawSensor2_;
  float filteredPercent_;
  bool faultActive_;
};

#endif // ELECTRONIC_THROTTLE_H
