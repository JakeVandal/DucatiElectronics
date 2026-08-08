#include "ThrottleBodyControl.h"

ThrottleBodyControl::ThrottleBodyControl(const ThrottleBodyControlConfig& config)
  : config_(config), commandedPercent_(0.0f) {
}

void ThrottleBodyControl::begin() {
  ledcSetup(config_.leftChannel, config_.pwmFrequencyHz, config_.pwmResolutionBits);
  ledcSetup(config_.rightChannel, config_.pwmFrequencyHz, config_.pwmResolutionBits);

  ledcAttachPin(config_.leftActuatorPin, config_.leftChannel);
  ledcAttachPin(config_.rightActuatorPin, config_.rightChannel);

  forceClosed();
}

void ThrottleBodyControl::update(float requestedPercent, unsigned long dtMs, bool enabled) {
  if (!enabled) {
    forceClosed();
    return;
  }

  requestedPercent = constrain(requestedPercent, 0.0f, 1.0f);

  float maxStep = config_.maxSlewPerSecond * ((float)dtMs / 1000.0f);
  maxStep = constrain(maxStep, 0.0f, 1.0f);

  if (requestedPercent > commandedPercent_) {
    commandedPercent_ = min(commandedPercent_ + maxStep, requestedPercent);
  } else {
    commandedPercent_ = max(commandedPercent_ - maxStep, requestedPercent);
  }

  int pulseUs = (int)(config_.closedPulseUs + (commandedPercent_ * (float)(config_.wideOpenPulseUs - config_.closedPulseUs)));
  writePulseUs(pulseUs);
}

void ThrottleBodyControl::forceClosed() {
  commandedPercent_ = 0.0f;
  writePulseUs(config_.closedPulseUs);
}

float ThrottleBodyControl::getCommandedPercent() const {
  return commandedPercent_;
}

uint32_t ThrottleBodyControl::pulseUsToDuty(int pulseUs) const {
  pulseUs = constrain(pulseUs, config_.closedPulseUs, config_.wideOpenPulseUs);

  const int pwmPeriodUs = 1000000 / config_.pwmFrequencyHz;
  const uint32_t maxDuty = (1UL << config_.pwmResolutionBits) - 1UL;

  return (uint32_t)(((uint64_t)pulseUs * maxDuty) / (uint32_t)pwmPeriodUs);
}

void ThrottleBodyControl::writePulseUs(int pulseUs) {
  uint32_t duty = pulseUsToDuty(pulseUs);
  ledcWrite(config_.leftChannel, duty);
  ledcWrite(config_.rightChannel, duty);
}
