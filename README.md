A dedicated electronics system for a 1999 Ducati 900ss with RFID-based access key ignition system, GPS speedometer, accelerometer data, and touchscreen display to highlight key data (speed, RPM, gear, temperature, and ). This is currently a work in progress workspace that will be updated in the coming months.

## Fuel Commander (AFR Mapping)

This project now includes a basic fuel commander table for tuning AFR and trim across RPM and throttle load.

- Firmware module: `src/FuelCommander.h` and `src/FuelCommander.cpp`
- Map CSV: `config/fuel_map.csv`
- Visualization/edit tool: `tools/fuel_map_visualizer.py`

### Firmware behavior

- Uses RPM and throttle position as map axes.
- Load is normalized throttle opening from 0.00 (closed) to 1.00 (wide open).
- Computes interpolated target AFR and fuel trim percent.
- Exposes serial commands for quick table edits during bench tuning.
- Default map grid is 0-8000 RPM in 200 RPM increments.
- Default load grid is 0.00-1.00 in 0.05 increments.

Serial commands at 115200 baud:

- `help`
- `dump afr`
- `dump trim`
- `get <rpmIndex> <loadIndex>`
- `set afr <rpmIndex> <loadIndex> <value>`
- `set trim <rpmIndex> <loadIndex> <value>`

Example:

- `set afr 4 5 12.8`
- `set trim 4 5 3.5`

### Python visualizer

Install dependencies:

```bash
pip install pandas matplotlib
```

Run with plotting:

```bash
python tools/fuel_map_visualizer.py --csv config/fuel_map.csv
```

Interactive plot controls:

- Click a single AFR cell to select one cell.
- Drag a rectangle on the **Target AFR** heatmap to select multiple cells.
- Press `+` to increase AFR in the selected section by 0.1 (leaner).
- Press `-` to decrease AFR in the selected section by 0.1 (richer).
- Run with `--write`, make edits, then close the plot window to save updated values.

Update one cell and write it back:

```bash
python tools/fuel_map_visualizer.py --csv config/fuel_map.csv --set 5000 0.80 12.7 2.0 --write
```

Export C++ initializer blocks from CSV:

```bash
python tools/fuel_map_visualizer.py --csv config/fuel_map.csv --export-cpp --no-plot
```

Safety note: validate all AFR targets and trims on a bench or dyno with wideband O2 feedback before road use.

## Power Commander Layer (Injector Path)

The project now includes a prototype injector rewrite layer that sits in the injector signal path.

- Module: `src/PowerCommander.h` and `src/PowerCommander.cpp`
- Integration: `src/main.cpp`
- Pin definitions: `lib/pinMap.h`

### Implemented behavior

- Measures injector pulse width from an injector input signal.
- Applies fuel map scale from `FuelCommander` to injector pulse duration.
- Applies optional closed-loop correction from a wideband AFR analog input.
- Provides pass-through fallback mode on internal or external faults.
- Streams runtime diagnostics over serial (mode, fault, AFR, trims, base/corrected pulse widths).

### Pin defaults

- `INJECTOR1_SIGNAL_IN_PIN` = 45
- `INJECTOR1_SIGNAL_OUT_PIN` = 46
- `INJECTOR2_SIGNAL_IN_PIN` = 47
- `INJECTOR2_SIGNAL_OUT_PIN` = 48
- `WIDEBAND_AFR_PIN` = -1 (disabled by default; set to an ADC1 pin to enable)

### Important prototype note

This layer is designed as a development prototype. Before road use, validate timing and failover behavior with bench instrumentation (scope/logic analyzer) and wideband feedback under controlled conditions.