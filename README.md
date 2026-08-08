A dedicated electronics system for a 1999 Ducati 900ss with RFID-based access key ignition system, GPS speedometer, accelerometer data, and touchscreen display to highlight key data (speed, RPM, gear, temperature, and ). This is currently a work in progress workspace that will be updated in the coming months.

## Fuel Commander (AFR Mapping)

This project now includes a basic fuel commander table for tuning AFR and trim across RPM and throttle load.

- Firmware module: `src/FuelCommander.h` and `src/FuelCommander.cpp`
- Map CSV: `config/fuel_map.csv`
- Visualization/edit tool: `tools/fuel_map_visualizer.py`

### Firmware behavior

- Uses RPM and throttle position as map axes.
- Computes interpolated target AFR and fuel trim percent.
- Exposes serial commands for quick table edits during bench tuning.
- Default map grid is 0-8000 RPM in 200 RPM increments.

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

- Drag a rectangle on the **Target AFR** heatmap to select a section.
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