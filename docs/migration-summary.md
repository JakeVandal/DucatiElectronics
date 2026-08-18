# Migration Summary — ESP32-S3 → Teensy 4.1

Branch: `teensy41-migration`, based on `ESP32` (not `main` directly — `main`
was several commits behind `ESP32` and missing `RelayControl.cpp`/`.h`, so
the migration started from the branch with the actual current logic, per
the task brief).

## What changed, file by file

| File | Change |
|---|---|
| `lib/pinMap.h` | Full pin renumbering for Teensy 4.1 (Phase 3). Same macro names as the ESP32 version, so no call sites needed renaming — only the numeric values and the peripheral-fixed-pin documentation changed. |
| `src/main.cpp` | Removed `IRAM_ATTR` from 5 ISRs; removed the ESP32-only `#ifndef LED_BUILTIN` fallback block; `Serial1.begin()` and `SPI.begin()` calls dropped their ESP32 GPIO-matrix pin arguments; removed the RFID-activity status LED entirely (`pulseLed()`, `ledOffAtMs`, and both call sites) per your decision below rather than relocating it off pin 13. No other control-flow, timing, or state-machine logic touched. |
| `src/TFTDisplay.cpp` | Removed `IRAM_ATTR` from `touchISR`; `SPI.begin()` and `Wire.begin()` calls dropped their pin arguments for the same reason as `main.cpp`. No drawing/layout logic touched. |
| `src/RelayControl.cpp` / `.h` | **Unchanged.** Contains no ESP32-specific calls (pure `digitalRead`/`digitalWrite`/`millis()` logic) — ported by pin-map substitution alone, no direct edits needed. |
| `platformio.ini` | Retargeted `[env:teensy41]`: `platform = teensy`, `board = teensy41`; removed ESP32 PSRAM/flash-size/partition-table config (no Teensy equivalent); updated `TFT_eSPI` pin build flags to the new pin map; dropped the unused `Adafruit_ILI9341` dependency; pinned `MFRC522` to `>=1.4.11` to avoid a known Teensy-breaking regression in 1.4.10. |
| `esp32_studio.json` | Deleted — ESP32 flash-tool config, no Teensy equivalent needed (PlatformIO drives Teensy flashing directly). |
| `WIRING.md` | Rewritten: pin tables, mermaid diagrams, and design notes updated for Teensy 4.1 (new pin numbers, new power-in guidance via VIN instead of USB, new pin-13/status-LED caution, updated "not 5V tolerant" warning). |
| `doc/ProjectDescription.md` | One-line change: "ESP32-S3 (N16R8) Microcontroller" → "Teensy 4.1 Microcontroller". |
| `docs/esp32-audit.md`, `docs/library-compatibility.md`, `docs/teensy-pinmap.md` | New — Phase 1-3 deliverables, kept as permanent migration records rather than deleted after use. |

## Behavior that may differ due to hardware differences

- **Single-core vs. dual-core timing:** not applicable — the ESP32 branch
  never used dual-core task pinning or FreeRTOS tasks (confirmed in Phase
  1). `loop()` was already a single cooperative sequence; Teensy 4.1's
  single 600MHz core actually gives every iteration of `loop()` more
  headroom than the ESP32-S3's 240MHz core, so if anything this loosens
  timing margins rather than tightening them.
- **ADC resolution/reference:** both platforms are configured for 12-bit,
  3.3V-referenced ADC (`analogReadResolution(12)`, unchanged). Not verified
  bit-for-bit identical: the ESP32-S3 and IMXRT1062 use different ADC
  silicon with different linearity/noise characteristics, so the fuel and
  gear voltage-threshold constants (`GEAR_VOLTAGE_THRESHOLDS` in
  `main.cpp`) may need re-tuning against real sender voltages once
  hardware is wired — same as any sensor recalibration after a board swap,
  not a migration bug.
- **DHT11 timing:** flagged in Phase 2 as a real, independently-reported
  risk (not hypothetical) — the `adafruit/DHT sensor library`'s
  cycle-counting approach has documented issues on Teensy 4.x. **Confirmed
  fine on your hardware** — readings are accurate and stable, so the
  library was kept as-is with no swap to `DHTNEW` needed.
- **CAN controller:** not applicable — confirmed in Phase 1 that this
  codebase has no CAN bus usage at all, so there's no CAN migration to
  verify.
- **Status LED behavior:** the RFID-activity LED pulse (`LED_BUILTIN` on
  the ESP32 branch) was **removed entirely** rather than relocated, per
  your call — Teensy 4.1's `LED_BUILTIN` is pin 13, which this design
  also uses as shared SPI clock, so a dedicated pin was proposed and then
  dropped instead of used. There's no card-read/write visual indicator on
  Teensy anymore; the onboard orange LED still flickers passively with
  SPI traffic (cosmetic, not driven by firmware). Documented in
  `WIRING.md` design note 1.
- **5V tolerance:** the ESP32-S3 and Teensy 4.1 are both 3.3V-logic parts,
  but Teensy pins have zero 5V tolerance margin (documented, not just
  "don't exceed 3.3V" as informal guidance). The OEM switch inputs
  (turn/hazard/beam sense) already required level-shifting on the ESP32
  branch — that requirement carries forward unchanged, just called out
  more strictly in the updated `WIRING.md`.

## Compile verification — not performed, flagged for manual verification

No PlatformIO or Arduino build toolchain is available in this workspace
(no network-installed `pio`/`arduino-cli`, and installing one would mean
downloading the Teensy ARM toolchain over the network, which wasn't
attempted). **This code has not been compiled.** Instead, every changed
file was manually reviewed line-by-line for:
- Brace/syntax balance (verified: `main.cpp`, `TFTDisplay.cpp`,
  `RelayControl.cpp`, and `pinMap.h` all have matched `{`/`}` counts).
- No remaining ESP32-only symbols (`IRAM_ATTR`, `LED_BUILTIN`, `esp_*`,
  `ledc*`, `xTaskCreate*`, `Preferences`, GPIO-matrix `SPI.begin()`/
  `Serial1.begin()`/`Wire.begin()` argument forms) — confirmed absent via
  `grep` across `src/` and `lib/`.
- Pin-map self-consistency — every `#define ..._PIN` in `pinMap.h` cross-
  checked for accidental duplicate assignment (the only pins used twice,
  11/12/13, are the intentional shared-SPI-bus MOSI/MISO/SCK for the TFT
  and MFRC522, which is correct/expected, not a bug).

**Before flashing real hardware, build this in your own PlatformIO
environment** (`pio run -e teensy41`) and fix any compile errors that
surface — a manual review, however careful, is not a substitute for an
actual compiler pass.

## Open questions — resolved

1. **DHT11 timing on Teensy 4.1** (Phase 2/5) — **confirmed on your
   hardware**: readings are accurate, no misbehavior. Kept
   `adafruit/DHT sensor library` as-is, no swap needed.
2. **`STATUS_LED_PIN` placement** (Phase 3) — **removed altogether**, per
   your call, rather than relocated. `pulseLed()`, its call sites, and the
   pin reservation were deleted from `main.cpp`/`pinMap.h` in a follow-up
   pass; the RFID read/write activity indicator no longer exists on this
   branch. Pin 13 remains solely the shared SPI clock, undriven by
   firmware, and the onboard LED just flickers passively with SPI traffic.
3. **Full pin map hardware cross-check** (Phase 3/5) — **confirmed**
   against the official PJRC "Welcome to Teensy 4.1" pinout card you
   provided. See `docs/teensy-pinmap.md`'s hardware cross-check section
   for the specific labels checked (default SPI 10/11/12/13, `Wire`
   18/19, `Serial1` 0/1, analog ranges, and the pin-13 LED marking). No
   corrections needed.
4. **`Adafruit_ILI9341` dependency removal** (Phase 1/2) — **confirmed
   correct**, already dropped from `platformio.ini`.
5. **Compile verification** (this phase) — **still outstanding, on you**:
   run `pio run -e teensy41` (or open in Arduino IDE with Teensyduino,
   board set to Teensy 4.1) when convenient, and report back any errors.
   Nothing in this migration has been compiler-checked in this workspace.