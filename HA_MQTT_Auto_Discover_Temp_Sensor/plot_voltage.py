#!/usr/bin/env python3
"""Plot LiPo battery voltage vs time from a Home Assistant history CSV.

Usage:
    python plot_voltage.py              # show the plot in a window
    python plot_voltage.py --save       # save to voltage_vs_time.png (no window)
    python plot_voltage.py --save out.png
    python plot_voltage.py --degree 6   # change best-fit polynomial degree
"""

import argparse
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates


def main():
    parser = argparse.ArgumentParser(description="Plot LiPo voltage vs time.")
    parser.add_argument("csv", nargs="?", default="history.csv",
                        help="input CSV (default: history.csv)")
    parser.add_argument("--save", nargs="?", const="voltage_vs_time.png",
                        default=None, metavar="PATH",
                        help="save to PATH instead of showing "
                             "(default file: voltage_vs_time.png)")
    parser.add_argument("--degree", type=int, default=5,
                        help="polynomial degree for the best-fit curve (default: 5)")
    parser.add_argument("--dpi", type=int, default=150, help="DPI when saving")
    args = parser.parse_args()

    # --- Load -----------------------------------------------------------
    df = pd.read_csv(args.csv)
    df["voltage"] = pd.to_numeric(df["state"], errors="coerce")   # drop non-numeric
    df["time"] = pd.to_datetime(df["last_changed"], utc=True)
    df = df.dropna(subset=["voltage"]).sort_values("time").reset_index(drop=True)

    # Elapsed hours since first sample -> numeric x for fitting
    hours = (df["time"] - df["time"].iloc[0]).dt.total_seconds() / 3600.0

    # --- Best-fit curve (polynomial least squares) ----------------------
    coeffs = np.polyfit(hours, df["voltage"], deg=args.degree)
    poly = np.poly1d(coeffs)
    hours_fit = np.linspace(hours.min(), hours.max(), 500)
    volts_fit = poly(hours_fit)
    # Map the smooth hours back to timestamps for the x-axis
    t0 = df["time"].iloc[0]
    time_fit = t0 + pd.to_timedelta(hours_fit, unit="h")

    # R^2 of the fit
    resid = df["voltage"] - poly(hours)
    r2 = 1 - np.sum(resid**2) / np.sum((df["voltage"] - df["voltage"].mean())**2)

    # --- Plot -----------------------------------------------------------
    fig, ax = plt.subplots(figsize=(11, 5.5))

    ax.plot(df["time"], df["voltage"], lw=1.0, color="#2b6cb0",
            alpha=0.8, label="Measured")
    ax.plot(time_fit, volts_fit, lw=2.2, color="red",
            label=f"Best fit (deg {args.degree}, R²={r2:.3f})")

    ax.set_title("LiPo Battery Voltage vs Time")
    ax.set_xlabel("Time (UTC)")
    ax.set_ylabel("Voltage (V)")
    ax.grid(True, alpha=0.3)
    ax.legend(loc="upper right")

    ax.xaxis.set_major_formatter(mdates.DateFormatter("%b %d\n%H:%M"))
    fig.autofmt_xdate()

    # Reference lines for common LiPo landmarks
    for v, label in [(4.20, "full"), (3.70, "low"), (3.30, "empty")]:
        if df["voltage"].min() - 0.1 <= v <= df["voltage"].max() + 0.1:
            ax.axhline(v, color="grey", ls="--", lw=0.7, alpha=0.6)
            ax.text(df["time"].iloc[0], v, f" {v:.2f} V ({label})",
                    va="bottom", ha="left", fontsize=8, color="grey")

    fig.tight_layout()

    # --- Save or show ---------------------------------------------------
    if args.save:
        fig.savefig(args.save, dpi=args.dpi)
        print(f"Saved {args.save}  ({len(df)} points, "
              f"{df['voltage'].min():.2f}-{df['voltage'].max():.2f} V, "
              f"{hours.iloc[-1]:.1f} h, fit R²={r2:.3f})")
    else:
        plt.show()


if __name__ == "__main__":
    main()
