#include "ElectronicThrottle.h"

ElectronicThrottle::ElectronicThrottle(const ElectronicThrottleConfig& config)
  : config_(config),
    rawSensor1_(0),
    rawSensor2_(0),
    filteredPercent_(0.0f),
    faultActive_(false) {
}

void ElectronicThrottle::begin() {
  pinMode(config_.sensor1Pin, INPUT);

  if (config_.sensor2Pin >= 0) {
    pinMode(config_.sensor2Pin, INPUT);
  }
}

void ElectronicThrottle::update() {
  rawSensor1_ = analogRead(config_.sensor1Pin);
  float percent1 = normalize(rawSensor1_, config_.sensor1MinRaw, config_.sensor1MaxRaw);

  float requestedPercent = percent1;
  faultActive_ = false;

  if (config_.sensor2Pin >= 0) {
    rawSensor2_ = analogRead(config_.sensor2Pin);
    float percent2 = normalize(rawSensor2_, config_.sensor2MinRaw, config_.sensor2MaxRaw);

    if (config_.sensor2Inverted) {
      percent2 = 1.0f - percent2;
    }

    float delta = fabsf(percent1 - percent2);
    if (delta > config_.plausibilityMaxDelta) {
      faultActive_ = true;
      requestedPercent = 0.0f;
    } else {
      requestedPercent = (percent1 + percent2) * 0.5f;
    }
  }

  float alpha = constrain(config_.filterAlpha, 0.01f, 1.0f);
  filteredPercent_ = (alpha * requestedPercent) + ((1.0f - alpha) * filteredPercent_);
}

float ElectronicThrottle::getThrottlePercent() const {
  return filteredPercent_;
}

bool ElectronicThrottle::hasFault() const {
  return faultActive_;
}

int ElectronicThrottle::getRawSensor1() const {
  return rawSensor1_;
}

int ElectronicThrottle::getRawSensor2() const {
  return rawSensor2_;
}

float ElectronicThrottle::normalize(int raw, int minRaw, int maxRaw) const {
  if (maxRaw <= minRaw) {
    return 0.0f;
  }

  float normalized = (float)(raw - minRaw) / (float)(maxRaw - minRaw);
  return constrain(normalized, 0.0f, 1.0f);
}
