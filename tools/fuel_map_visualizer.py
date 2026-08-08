#!/usr/bin/env python3
"""Fuel map editor and visualizer for DucatiElectronics.

CSV format:
  rpm,load,target_afr,trim_percent
"""

from __future__ import annotations

import argparse
import pathlib
from typing import List

import matplotlib.pyplot as plt
import pandas as pd
from matplotlib.widgets import RectangleSelector


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(description="Fuel map visualizer/editor")
  parser.add_argument("--csv", default="config/fuel_map.csv", help="Path to fuel map CSV")
  parser.add_argument(
    "--set",
    nargs=4,
    metavar=("RPM", "LOAD", "AFR", "TRIM"),
    help="Set one table cell by exact RPM and LOAD values"
  )
  parser.add_argument("--write", action="store_true", help="Write modifications back to CSV")
  parser.add_argument("--no-plot", action="store_true", help="Skip plot window")
  parser.add_argument("--export-cpp", action="store_true", help="Print C++ array initializers")
  return parser.parse_args()


def load_map(csv_path: pathlib.Path) -> pd.DataFrame:
  df = pd.read_csv(csv_path)
  required = {"rpm", "load", "target_afr", "trim_percent"}
  missing = required.difference(df.columns)
  if missing:
    raise ValueError(f"Missing columns: {sorted(missing)}")

  return df.sort_values(["rpm", "load"]).reset_index(drop=True)


def apply_set(df: pd.DataFrame, args: List[str]) -> pd.DataFrame:
  rpm = int(args[0])
  load = float(args[1])
  afr = float(args[2])
  trim = float(args[3])

  mask = (df["rpm"] == rpm) & (df["load"] == load)
  if not mask.any():
    raise ValueError(f"No cell found for rpm={rpm}, load={load}")

  df.loc[mask, "target_afr"] = afr
  df.loc[mask, "trim_percent"] = trim
  return df


def pivot(df: pd.DataFrame, column: str) -> pd.DataFrame:
  piv = df.pivot(index="rpm", columns="load", values=column)
  return piv.sort_index().sort_index(axis=1)


def render(df: pd.DataFrame) -> None:
  afr = pivot(df, "target_afr")
  trim = pivot(df, "trim_percent")

  fig, axes = plt.subplots(1, 2, figsize=(14, 5), constrained_layout=True)

  im0 = axes[0].imshow(afr.values, aspect="auto", origin="lower", cmap="viridis")
  axes[0].set_title("Target AFR")
  axes[0].set_xlabel("Load")
  axes[0].set_ylabel("RPM")
  axes[0].set_xticks(range(len(afr.columns)), labels=[f"{x:.2f}" for x in afr.columns])
  axes[0].set_yticks(range(len(afr.index)), labels=[str(x) for x in afr.index])
  axes[0].set_xticks([x - 0.5 for x in range(1, len(afr.columns))], minor=True)
  axes[0].set_yticks([y - 0.5 for y in range(1, len(afr.index))], minor=True)
  axes[0].grid(which="minor", color="white", linestyle="-", linewidth=0.5, alpha=0.6)
  axes[0].tick_params(which="minor", bottom=False, left=False)
  fig.colorbar(im0, ax=axes[0], label="AFR")

  im1 = axes[1].imshow(trim.values, aspect="auto", origin="lower", cmap="coolwarm")
  axes[1].set_title("Fuel Trim (%)")
  axes[1].set_xlabel("Load")
  axes[1].set_ylabel("RPM")
  axes[1].set_xticks(range(len(trim.columns)), labels=[f"{x:.2f}" for x in trim.columns])
  axes[1].set_yticks(range(len(trim.index)), labels=[str(x) for x in trim.index])
  axes[1].set_xticks([x - 0.5 for x in range(1, len(trim.columns))], minor=True)
  axes[1].set_yticks([y - 0.5 for y in range(1, len(trim.index))], minor=True)
  axes[1].grid(which="minor", color="white", linestyle="-", linewidth=0.5, alpha=0.6)
  axes[1].tick_params(which="minor", bottom=False, left=False)
  fig.colorbar(im1, ax=axes[1], label="Trim %")

  selection = {
    "r0": 0,
    "r1": len(afr.index) - 1,
    "c0": 0,
    "c1": len(afr.columns) - 1,
  }

  def clamp_index(value: float, high: int) -> int:
    return max(0, min(high, int(round(value))))

  def on_select(eclick, erelease) -> None:
    if eclick.xdata is None or eclick.ydata is None or erelease.xdata is None or erelease.ydata is None:
      return

    c0 = clamp_index(min(eclick.xdata, erelease.xdata), len(afr.columns) - 1)
    c1 = clamp_index(max(eclick.xdata, erelease.xdata), len(afr.columns) - 1)
    r0 = clamp_index(min(eclick.ydata, erelease.ydata), len(afr.index) - 1)
    r1 = clamp_index(max(eclick.ydata, erelease.ydata), len(afr.index) - 1)

    selection["r0"] = r0
    selection["r1"] = r1
    selection["c0"] = c0
    selection["c1"] = c1
    print(f"Selected AFR section: rpm_idx={r0}:{r1}, load_idx={c0}:{c1}")

  selector = RectangleSelector(
    axes[0],
    on_select,
    useblit=True,
    button=[1],
    minspanx=0.5,
    minspany=0.5,
    spancoords="data",
    interactive=True,
  )

  def adjust_selected_afr(delta: float) -> None:
    afr.iloc[
      selection["r0"]:selection["r1"] + 1,
      selection["c0"]:selection["c1"] + 1,
    ] = (
      afr.iloc[
        selection["r0"]:selection["r1"] + 1,
        selection["c0"]:selection["c1"] + 1,
      ] + delta
    ).clip(lower=10.0, upper=16.0)

    im0.set_data(afr.values)
    fig.canvas.draw_idle()

  def on_key(event) -> None:
    if event.key in {"+", "=", "plus"}:
      adjust_selected_afr(+0.1)
      print("Adjusted selected AFR section by +0.1 (leaner).")
    elif event.key in {"-", "minus"}:
      adjust_selected_afr(-0.1)
      print("Adjusted selected AFR section by -0.1 (richer).")

  fig.canvas.mpl_connect("key_press_event", on_key)

  def save_back_to_df() -> None:
    for rpm_idx, rpm_value in enumerate(afr.index):
      for load_idx, load_value in enumerate(afr.columns):
        mask = (df["rpm"] == rpm_value) & (df["load"] == load_value)
        df.loc[mask, "target_afr"] = float(afr.iloc[rpm_idx, load_idx])

  def on_close(_event) -> None:
    save_back_to_df()

  fig.canvas.mpl_connect("close_event", on_close)

  print("Drag on Target AFR plot to select a section.")
  print("Use '+' for +0.1 AFR (leaner), '-' for -0.1 AFR (richer).")
  print("Close the plot window to commit visual edits in memory before --write.")

  plt.show()


def export_cpp(df: pd.DataFrame) -> None:
  afr = pivot(df, "target_afr")
  trim = pivot(df, "trim_percent")

  rpm_bins = list(afr.index)
  load_bins = list(afr.columns)

  print("// Paste into FuelCommander.cpp")
  print("const int rpmDefaults[FuelCommander::kRpmBins] = {", end="")
  print(", ".join(str(v) for v in rpm_bins), end="")
  print("};")

  print("const float loadDefaults[FuelCommander::kLoadBins] = {", end="")
  print(", ".join(f"{v:.2f}f" for v in load_bins), end="")
  print("};")

  print("const float afrDefaults[FuelCommander::kRpmBins][FuelCommander::kLoadBins] = {")
  for _, row in afr.iterrows():
    values = ", ".join(f"{v:.3f}f" for v in row.values)
    print(f"  {{ {values} }},")
  print("};")

  print("const float trimDefaults[FuelCommander::kRpmBins][FuelCommander::kLoadBins] = {")
  for _, row in trim.iterrows():
    values = ", ".join(f"{v:.3f}f" for v in row.values)
    print(f"  {{ {values} }},")
  print("};")


def main() -> None:
  args = parse_args()
  csv_path = pathlib.Path(args.csv)
  df = load_map(csv_path)

  if args.set:
    df = apply_set(df, args.set)
    print("Updated one cell in memory.")

  if not args.no_plot:
    render(df)

  if args.export_cpp:
    export_cpp(df)

  if args.write:
    df.to_csv(csv_path, index=False)
    print(f"Wrote {csv_path}")


if __name__ == "__main__":
  main()
