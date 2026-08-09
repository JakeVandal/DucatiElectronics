#include "PowerCommander.h"

PowerCommander* PowerCommander::activeInstance_ = nullptr;

PowerCommander::PowerCommander(const PowerCommanderConfig& config, FuelCommander& fuelCommander)
  : config_(config),
    fuelCommander_(fuelCommander),
    enabled_(false),
    externalFault_(false),
    internalFault_(false),
    passThroughMode_(true),
    injectorInputHigh_{ false, false },
    injectorInputRiseUs_{ 0, 0 },
    injectorOutputHigh_{ false, false },
    injectorOutputRiseUs_{ 0, 0 },
    injectorOutputStopUs_{ 0, 0 },
    lastBasePulseUs_{ 0, 0 },
    lastCommandedPulseUs_{ 0, 0 },
    measuredAfr_(14.7f),
    widebandValid_(false),
    closedLoopTrimPercent_(0.0f) {
}

void PowerCommander::begin() {
  for (int i = 0; i < kInjectorCount; ++i) {
    pinMode(config_.injectorInputPins[i], INPUT);
    pinMode(config_.injectorOutputPins[i], OUTPUT);
    digitalWrite(config_.injectorOutputPins[i], LOW);
  }

  if (config_.widebandAfrPin >= 0) {
    pinMode(config_.widebandAfrPin, INPUT);
  }

  activeInstance_ = this;
  attachInterrupt(digitalPinToInterrupt(config_.injectorInputPins[0]), injector0EdgeIsr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(config_.injectorInputPins[1]), injector1EdgeIsr, CHANGE);
}

void PowerCommander::update(int rpm, float throttlePercent) {
  (void)rpm;
  (void)throttlePercent;

  if (enabled_ && !externalFault_ && !internalFault_) {
    passThroughMode_ = false;
  } else {
    enablePassThrough();
  }

  if (config_.widebandAfrPin >= 0) {
    measuredAfr_ = readWidebandAfr();
    widebandValid_ = true;
  } else {
    widebandValid_ = false;
  }

  if (enabled_ && !externalFault_ && !internalFault_) {
    updateClosedLoopTrim(fuelCommander_.getCurrentTargetAfr());
  } else {
    closedLoopTrimPercent_ = 0.0f;
  }

  updateOutputState();

  // Guard against stale ISR state if injector input gets stuck high.
  unsigned long nowUs = micros();
  for (int i = 0; i < kInjectorCount; ++i) {
    if (injectorInputHigh_[i]) {
      if (nowUs - injectorInputRiseUs_[i] > (config_.maxInjectorPulseUs * 2UL)) {
        internalFault_ = true;
        enablePassThrough();
      }
    }
  }

}

void PowerCommander::setEnabled(bool enabled) {
  enabled_ = enabled;

  if (!enabled_) {
    enablePassThrough();
  }
}

void PowerCommander::setExternalFault(bool faultActive) {
  externalFault_ = faultActive;
  if (faultActive) {
    enablePassThrough();
  }
}

bool PowerCommander::hasFault() const {
  return externalFault_ || internalFault_;
}

bool PowerCommander::isPassThroughMode() const {
  return passThroughMode_;
}

float PowerCommander::getMeasuredAfr() const {
  return measuredAfr_;
}

float PowerCommander::getClosedLoopTrimPercent() const {
  return closedLoopTrimPercent_;
}

unsigned long PowerCommander::getLastBasePulseUs() const {
  return getLastBasePulseUs(0);
}

unsigned long PowerCommander::getLastCommandedPulseUs() const {
  return getLastCommandedPulseUs(0);
}

unsigned long PowerCommander::getLastBasePulseUs(int injectorIndex) const {
  if (injectorIndex < 0 || injectorIndex >= kInjectorCount) {
    return 0;
  }

  return lastBasePulseUs_[injectorIndex];
}

unsigned long PowerCommander::getLastCommandedPulseUs(int injectorIndex) const {
  if (injectorIndex < 0 || injectorIndex >= kInjectorCount) {
    return 0;
  }

  return lastCommandedPulseUs_[injectorIndex];
}

void IRAM_ATTR PowerCommander::injector0EdgeIsr() {
  if (activeInstance_ != nullptr) {
    activeInstance_->handleInjectorEdge(0);
  }
}

void IRAM_ATTR PowerCommander::injector1EdgeIsr() {
  if (activeInstance_ != nullptr) {
    activeInstance_->handleInjectorEdge(1);
  }
}

void IRAM_ATTR PowerCommander::handleInjectorEdge(int injectorIndex) {
  if (injectorIndex < 0 || injectorIndex >= kInjectorCount) {
    return;
  }

  bool level = digitalRead(config_.injectorInputPins[injectorIndex]) == HIGH;
  unsigned long nowUs = micros();

  if (passThroughMode_) {
    digitalWrite(config_.injectorOutputPins[injectorIndex], level ? HIGH : LOW);
  }

  if (level) {
    injectorInputHigh_[injectorIndex] = true;
    injectorInputRiseUs_[injectorIndex] = nowUs;

    if (!passThroughMode_) {
      injectorOutputHigh_[injectorIndex] = true;
      injectorOutputRiseUs_[injectorIndex] = nowUs;
      // Tentative stop time, updated when falling edge gives base pulse width.
      injectorOutputStopUs_[injectorIndex] = nowUs + config_.minInjectorPulseUs;
      digitalWrite(config_.injectorOutputPins[injectorIndex], HIGH);
    }
  } else {
    if (!injectorInputHigh_[injectorIndex]) {
      return;
    }

    injectorInputHigh_[injectorIndex] = false;
    unsigned long basePulseUs = nowUs - injectorInputRiseUs_[injectorIndex];
    lastBasePulseUs_[injectorIndex] = basePulseUs;

    if (basePulseUs < config_.minInjectorPulseUs || basePulseUs > config_.maxInjectorPulseUs) {
      internalFault_ = true;
      enablePassThrough();
      return;
    }

    float mapScale = fuelCommander_.getCurrentFuelScale();
    float closedLoopScale = 1.0f + (closedLoopTrimPercent_ / 100.0f);
    float totalScale = mapScale * closedLoopScale;

    float correctedPulse = (float)basePulseUs * totalScale;
    correctedPulse = constrain(correctedPulse, (float)config_.minInjectorPulseUs, (float)config_.maxInjectorPulseUs);
    unsigned long correctedPulseUs = (unsigned long)correctedPulse;

    lastCommandedPulseUs_[injectorIndex] = correctedPulseUs;

    if (passThroughMode_) {
      return;
    }

    if (!injectorOutputHigh_[injectorIndex]) {
      injectorOutputHigh_[injectorIndex] = true;
      injectorOutputRiseUs_[injectorIndex] = nowUs;
      injectorOutputStopUs_[injectorIndex] = nowUs + correctedPulseUs;
      digitalWrite(config_.injectorOutputPins[injectorIndex], HIGH);
      return;
    }

    unsigned long elapsedUs = nowUs - injectorOutputRiseUs_[injectorIndex];
    if (correctedPulseUs <= elapsedUs) {
      injectorOutputHigh_[injectorIndex] = false;
      digitalWrite(config_.injectorOutputPins[injectorIndex], LOW);
    } else {
      injectorOutputStopUs_[injectorIndex] = injectorOutputRiseUs_[injectorIndex] + correctedPulseUs;
    }
  }
}

float PowerCommander::readWidebandAfr() const {
  if (config_.widebandAfrPin < 0) {
    return 14.7f;
  }

  int raw = analogRead(config_.widebandAfrPin);
  raw = constrain(raw, config_.widebandMinRaw, config_.widebandMaxRaw);

  if (config_.widebandMaxRaw <= config_.widebandMinRaw) {
    return 14.7f;
  }

  float t = (float)(raw - config_.widebandMinRaw) / (float)(config_.widebandMaxRaw - config_.widebandMinRaw);
  return config_.widebandMinAfr + (t * (config_.widebandMaxAfr - config_.widebandMinAfr));
}

void PowerCommander::updateClosedLoopTrim(float targetAfr) {
  if (!widebandValid_) {
    return;
  }

  float afrErrorLean = measuredAfr_ - targetAfr;
  closedLoopTrimPercent_ += afrErrorLean * config_.closedLoopKp;
  closedLoopTrimPercent_ = constrain(
    closedLoopTrimPercent_,
    -config_.closedLoopMaxTrimPercent,
    config_.closedLoopMaxTrimPercent
  );
}

void PowerCommander::updateOutputState() {
  if (passThroughMode_) {
    return;
  }

  unsigned long nowUs = micros();
  for (int i = 0; i < kInjectorCount; ++i) {
    if (injectorOutputHigh_[i] && (long)(nowUs - injectorOutputStopUs_[i]) >= 0) {
      injectorOutputHigh_[i] = false;
      digitalWrite(config_.injectorOutputPins[i], LOW);
    }
  }
}

void PowerCommander::enablePassThrough() {
  passThroughMode_ = true;
  for (int i = 0; i < kInjectorCount; ++i) {
    injectorOutputHigh_[i] = false;
    digitalWrite(config_.injectorOutputPins[i], digitalRead(config_.injectorInputPins[i]));
  }
}
