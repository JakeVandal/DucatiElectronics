#include "FuelCommander.h"

FuelCommander::FuelCommander()
  : serial_(nullptr),
    currentTargetAfr_(14.7f),
    currentFuelTrimPercent_(0.0f) {
  const float loadDefaults[kLoadBins] = { 0.05f, 0.20f, 0.40f, 0.60f, 0.80f, 1.00f };

  for (int i = 0; i < kRpmBins; ++i) {
    rpmBins_[i] = i * 200;
  }

  for (int j = 0; j < kLoadBins; ++j) {
    loadBins_[j] = loadDefaults[j];
  }

  for (int i = 0; i < kRpmBins; ++i) {
    float rpmNorm = (float)rpmBins_[i] / 8000.0f;
    for (int j = 0; j < kLoadBins; ++j) {
      float loadNorm = (float)j / (float)(kLoadBins - 1);
      // Base map: stoich/light load, richer at high load and high RPM.
      float baseAfr = 14.7f - (2.0f * loadNorm) - (0.3f * rpmNorm);
      targetAfrMap_[i][j] = constrain(baseAfr, 12.4f, 14.7f);
      fuelTrimPercentMap_[i][j] = 0.0f;
    }
  }
}

void FuelCommander::begin(Stream& serialPort) {
  serial_ = &serialPort;

  if (serial_ != nullptr) {
    serial_->println("FuelCommander ready. Type 'help' for commands.");
  }
}

void FuelCommander::update(int rpm, float throttlePercent) {
  float load = constrain(throttlePercent, 0.0f, 1.0f);
  currentTargetAfr_ = bilinearInterpolate(targetAfrMap_, rpm, load);
  currentFuelTrimPercent_ = bilinearInterpolate(fuelTrimPercentMap_, rpm, load);
}

void FuelCommander::processSerial() {
  if (serial_ == nullptr || !serial_->available()) {
    return;
  }

  String line = serial_->readStringUntil('\n');
  line.trim();
  if (line.length() == 0) {
    return;
  }

  String lowered = line;
  lowered.toLowerCase();

  if (lowered == "help") {
    printHelp();
    return;
  }

  if (lowered == "dump afr") {
    dumpTable(targetAfrMap_, "target_afr");
    return;
  }

  if (lowered == "dump trim") {
    dumpTable(fuelTrimPercentMap_, "fuel_trim_percent");
    return;
  }

  if (parseSetCommand(lowered) || parseGetCommand(lowered)) {
    return;
  }

  serial_->println("Unknown command. Type 'help'.");
}

float FuelCommander::getCurrentTargetAfr() const {
  return currentTargetAfr_;
}

float FuelCommander::getCurrentFuelTrimPercent() const {
  return currentFuelTrimPercent_;
}

float FuelCommander::getCurrentFuelScale() const {
  return 1.0f + (currentFuelTrimPercent_ / 100.0f);
}

float FuelCommander::getTargetAfrByIndex(int rpmIndex, int loadIndex) const {
  if (rpmIndex < 0 || rpmIndex >= kRpmBins || loadIndex < 0 || loadIndex >= kLoadBins) {
    return NAN;
  }

  return targetAfrMap_[rpmIndex][loadIndex];
}

float FuelCommander::getFuelTrimByIndex(int rpmIndex, int loadIndex) const {
  if (rpmIndex < 0 || rpmIndex >= kRpmBins || loadIndex < 0 || loadIndex >= kLoadBins) {
    return NAN;
  }

  return fuelTrimPercentMap_[rpmIndex][loadIndex];
}

bool FuelCommander::setTargetAfrByIndex(int rpmIndex, int loadIndex, float value) {
  if (rpmIndex < 0 || rpmIndex >= kRpmBins || loadIndex < 0 || loadIndex >= kLoadBins) {
    return false;
  }

  targetAfrMap_[rpmIndex][loadIndex] = constrain(value, 10.0f, 16.0f);
  return true;
}

bool FuelCommander::setFuelTrimByIndex(int rpmIndex, int loadIndex, float value) {
  if (rpmIndex < 0 || rpmIndex >= kRpmBins || loadIndex < 0 || loadIndex >= kLoadBins) {
    return false;
  }

  fuelTrimPercentMap_[rpmIndex][loadIndex] = constrain(value, -30.0f, 30.0f);
  return true;
}

int FuelCommander::lowerIndexForRpm(int rpm) const {
  if (rpm <= rpmBins_[0]) {
    return 0;
  }

  for (int i = 0; i < kRpmBins - 1; ++i) {
    if (rpm < rpmBins_[i + 1]) {
      return i;
    }
  }

  return kRpmBins - 2;
}

int FuelCommander::lowerIndexForLoad(float load) const {
  if (load <= loadBins_[0]) {
    return 0;
  }

  for (int i = 0; i < kLoadBins - 1; ++i) {
    if (load < loadBins_[i + 1]) {
      return i;
    }
  }

  return kLoadBins - 2;
}

float FuelCommander::bilinearInterpolate(const float table[kRpmBins][kLoadBins], int rpm, float load) const {
  load = constrain(load, 0.0f, 1.0f);

  int i = lowerIndexForRpm(rpm);
  int j = lowerIndexForLoad(load);

  int rpm0 = rpmBins_[i];
  int rpm1 = rpmBins_[i + 1];
  float load0 = loadBins_[j];
  float load1 = loadBins_[j + 1];

  float tr = (rpm1 == rpm0) ? 0.0f : (float)(rpm - rpm0) / (float)(rpm1 - rpm0);
  tr = constrain(tr, 0.0f, 1.0f);

  float tl = (load1 == load0) ? 0.0f : (load - load0) / (load1 - load0);
  tl = constrain(tl, 0.0f, 1.0f);

  float v00 = table[i][j];
  float v10 = table[i + 1][j];
  float v01 = table[i][j + 1];
  float v11 = table[i + 1][j + 1];

  float v0 = v00 + ((v10 - v00) * tr);
  float v1 = v01 + ((v11 - v01) * tr);
  return v0 + ((v1 - v0) * tl);
}

void FuelCommander::printHelp() {
  serial_->println("Commands:");
  serial_->println("  help");
  serial_->println("  dump afr");
  serial_->println("  dump trim");
  serial_->println("  get <rpmIndex> <loadIndex>");
  serial_->println("  set afr <rpmIndex> <loadIndex> <value>");
  serial_->println("  set trim <rpmIndex> <loadIndex> <value>");
}

void FuelCommander::dumpTable(const float table[kRpmBins][kLoadBins], const char* name) {
  serial_->print("table,");
  serial_->println(name);

  serial_->print("rpm/load");
  for (int j = 0; j < kLoadBins; ++j) {
    serial_->print(',');
    serial_->print(loadBins_[j], 3);
  }
  serial_->println();

  for (int i = 0; i < kRpmBins; ++i) {
    serial_->print(rpmBins_[i]);
    for (int j = 0; j < kLoadBins; ++j) {
      serial_->print(',');
      serial_->print(table[i][j], 3);
    }
    serial_->println();
  }
}

bool FuelCommander::parseSetCommand(const String& line) {
  int typeStart = 4;
  if (!line.startsWith("set ")) {
    return false;
  }

  int typeEnd = line.indexOf(' ', typeStart);
  if (typeEnd < 0) {
    serial_->println("Invalid set syntax.");
    return true;
  }

  String setType = line.substring(typeStart, typeEnd);
  String remainder = line.substring(typeEnd + 1);

  int firstSpace = remainder.indexOf(' ');
  int secondSpace = remainder.indexOf(' ', firstSpace + 1);
  if (firstSpace < 0 || secondSpace < 0) {
    serial_->println("Invalid set syntax.");
    return true;
  }

  int rpmIndex = remainder.substring(0, firstSpace).toInt();
  int loadIndex = remainder.substring(firstSpace + 1, secondSpace).toInt();
  float value = remainder.substring(secondSpace + 1).toFloat();

  bool success = false;
  if (setType == "afr") {
    success = setTargetAfrByIndex(rpmIndex, loadIndex, value);
  } else if (setType == "trim") {
    success = setFuelTrimByIndex(rpmIndex, loadIndex, value);
  } else {
    serial_->println("Unknown set type. Use afr or trim.");
    return true;
  }

  if (success) {
    serial_->println("OK");
  } else {
    serial_->println("Index out of range.");
  }

  return true;
}

bool FuelCommander::parseGetCommand(const String& line) {
  if (!line.startsWith("get ")) {
    return false;
  }

  String remainder = line.substring(4);
  int split = remainder.indexOf(' ');
  if (split < 0) {
    serial_->println("Invalid get syntax.");
    return true;
  }

  int rpmIndex = remainder.substring(0, split).toInt();
  int loadIndex = remainder.substring(split + 1).toInt();

  float afr = getTargetAfrByIndex(rpmIndex, loadIndex);
  float trim = getFuelTrimByIndex(rpmIndex, loadIndex);

  if (isnan(afr) || isnan(trim)) {
    serial_->println("Index out of range.");
  } else {
    serial_->print("AFR=");
    serial_->print(afr, 3);
    serial_->print(", trim=");
    serial_->println(trim, 3);
  }

  return true;
}
