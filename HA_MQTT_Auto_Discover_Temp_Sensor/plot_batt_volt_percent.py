#!/usr/bin/env python3
"""
Plot sensor.xiao_sensor_xiao_12d7dc_battery (%) and
sensor.xiao_sensor_xiao_12d7dc_battery_voltage (V) over time on the same plot.

Usage:
    python plot_batt_volt_percent.py [path_to_csv] [--mode live|save] [--out output_image_path]

Defaults:
    path_to_csv -> batt_volt_percent.csv
    --mode      -> save
    --out       -> batt_volt_percent_plot.png (only used when --mode save)

Examples:
    python plot_batt_volt_percent.py                          # save to default png
    python plot_batt_volt_percent.py --mode live               # open interactive window
    python plot_batt_volt_percent.py data.csv --mode save --out plot.png
"""

import argparse
import pandas as pd
import matplotlib

BATTERY_ENTITY = "sensor.xiao_sensor_xiao_12d7dc_battery"
VOLTAGE_ENTITY = "sensor.xiao_sensor_xiao_12d7dc_battery_voltage"


def parse_args():
    parser = argparse.ArgumentParser(description="Plot battery % and voltage over time.")
    parser.add_argument("csv_path", nargs="?", default="batt_volt_percent.csv",
                         help="Path to input CSV (default: batt_volt_percent.csv)")
    parser.add_argument("--mode", choices=["live", "save"], default="save",
                         help="'live' opens an interactive window, 'save' writes a PNG (default: save)")
    parser.add_argument("--out", default="batt_volt_percent_plot.png",
                         help="Output image path when --mode save (default: batt_volt_percent_plot.png)")
    return parser.parse_args()


def main():
    args = parse_args()

    # Choose backend before importing pyplot: non-interactive for save, default for live
    if args.mode == "save":
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    # Load data
    df = pd.read_csv(args.csv_path)
    df["last_changed"] = pd.to_datetime(df["last_changed"])
    df["state"] = pd.to_numeric(df["state"], errors="coerce")
    df = df.dropna(subset=["state"])

    battery = df[df["entity_id"] == BATTERY_ENTITY].sort_values("last_changed")
    voltage = df[df["entity_id"] == VOLTAGE_ENTITY].sort_values("last_changed")

    if battery.empty:
        print(f"Warning: no rows found for {BATTERY_ENTITY}")
    if voltage.empty:
        print(f"Warning: no rows found for {VOLTAGE_ENTITY}")

    # Plot with dual y-axes since % and V are on very different scales
    fig, ax1 = plt.subplots(figsize=(14, 6))

    color1 = "tab:blue"
    ax1.set_xlabel("Time")
    ax1.set_ylabel("Battery (%)", color=color1)
    ax1.plot(battery["last_changed"], battery["state"], color=color1, label="Battery (%)", linewidth=1)
    ax1.tick_params(axis="y", labelcolor=color1)
    ax1.set_ylim(0, 105)
    #ax1.grid(True, alpha=0.3)

    ax2 = ax1.twinx()
    color2 = "tab:red"
    ax2.set_ylabel("Battery Voltage (V)", color=color2)
    ax2.plot(voltage["last_changed"], voltage["state"], color=color2, label="Voltage (V)", linewidth=1)
    ax2.tick_params(axis="y", labelcolor=color2)
    #ax2.grid(True, alpha=0.3, linestyle='--')

    fig.suptitle("Xiao 12d7dc Battery: Percentage vs Voltage Over Time")

    # Combined legend
    lines1, labels1 = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax1.legend(lines1 + lines2, labels1 + labels2, loc="upper right")

    fig.autofmt_xdate()
    fig.tight_layout()

    if args.mode == "live":
        plt.show()
    else:
        fig.savefig(args.out, dpi=150)
        print(f"Saved plot to {args.out}")


if __name__ == "__main__":
    main()
