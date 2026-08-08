#ifndef THROTTLE_BODY_CONTROL_H
#define THROTTLE_BODY_CONTROL_H

#include <Arduino.h>

struct ThrottleBodyControlConfig {
  int leftActuatorPin;
  int rightActuatorPin;

  int leftChannel;
  int rightChannel;

  int pwmFrequencyHz;
  int pwmResolutionBits;

  int closedPulseUs;
  int wideOpenPulseUs;

  float maxSlewPerSecond;
};

class ThrottleBodyControl {
public:
  explicit ThrottleBodyControl(const ThrottleBodyControlConfig& config);

  void begin();
  void update(float requestedPercent, unsigned long dtMs, bool enabled);
  void forceClosed();

  float getCommandedPercent() const;

private:
  uint32_t pulseUsToDuty(int pulseUs) const;
  void writePulseUs(int pulseUs);

  ThrottleBodyControlConfig config_;
  float commandedPercent_;
};

#endif // THROTTLE_BODY_CONTROL_H
