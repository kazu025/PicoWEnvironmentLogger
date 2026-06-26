#
# 下記のようにログを保存する
# ./logger_viewer.py --port /dev/ttyUSB0 --baud 460800 | tee sensor_log.txt
#
# python plot_sensor_log.py sensor_log.txt

#!/usr/bin/env python3

import re
import argparse
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt


def extract_msg(line: str):
    """
    logger_viewer.py の出力から msg="..." の中身を取り出す
    """
    m = re.search(r'msg="([^"]+)"', line)
    if not m:
        return None
    return m.group(1)


def parse_log_file(path: Path):
    adc_rows = []
    temp_rows = []

    with path.open("r", encoding="utf-8", errors="replace") as f:
        for line in f:
            msg = extract_msg(line)
            if msg is None:
                continue

            parts = msg.split(",")

            if len(parts) == 5 and parts[0] == "ADC":
                try:
                    adc_rows.append({
                        "timestamp_ms": int(parts[1]),
                        "raw": int(parts[2]),
                        "avg": int(parts[3]),
                        "voltage": float(parts[4]),
                    })
                except ValueError:
                    continue

            elif len(parts) == 4 and parts[0] == "TEMP":
                try:
                    temp_rows.append({
                        "timestamp_ms": int(parts[1]),
                        "temperature_c": float(parts[2]),
                        "status": parts[3],
                    })
                except ValueError:
                    continue

    adc_df = pd.DataFrame(adc_rows)
    temp_df = pd.DataFrame(temp_rows)

    return adc_df, temp_df


def add_time_seconds(adc_df: pd.DataFrame, temp_df: pd.DataFrame):
    timestamps = []

    if not adc_df.empty:
        timestamps.append(adc_df["timestamp_ms"].min())

    if not temp_df.empty:
        timestamps.append(temp_df["timestamp_ms"].min())

    if not timestamps:
        return adc_df, temp_df

    t0 = min(timestamps)

    if not adc_df.empty:
        adc_df = adc_df.copy()
        adc_df["time_s"] = (adc_df["timestamp_ms"] - t0) / 1000.0

    if not temp_df.empty:
        temp_df = temp_df.copy()
        temp_df["time_s"] = (temp_df["timestamp_ms"] - t0) / 1000.0

    return adc_df, temp_df


def plot_temperature(temp_df: pd.DataFrame, output: Path):
    if temp_df.empty:
        print("TEMP data not found.")
        return

    ok_df = temp_df[temp_df["status"] == "OK"]

    plt.figure()
    plt.plot(ok_df["time_s"], ok_df["temperature_c"], marker="o")
    plt.xlabel("Time [s]")
    plt.ylabel("Temperature [degC]")
    plt.title("AE-ADT7410 Temperature")
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(output)
    print(f"saved: {output}")


def plot_adc_voltage(adc_df: pd.DataFrame, output: Path):
    if adc_df.empty:
        print("ADC data not found.")
        return

    plt.figure()
    plt.plot(adc_df["time_s"], adc_df["voltage"], marker="o")
    plt.xlabel("Time [s]")
    plt.ylabel("Voltage [V]")
    plt.title("ADC Voltage")
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(output)
    print(f"saved: {output}")


def plot_adc_raw_avg(adc_df: pd.DataFrame, output: Path):
    if adc_df.empty:
        print("ADC data not found.")
        return

    plt.figure()
    plt.plot(adc_df["time_s"], adc_df["raw"], marker="o", label="raw")
    plt.plot(adc_df["time_s"], adc_df["avg"], marker="o", label="avg")
    plt.xlabel("Time [s]")
    plt.ylabel("ADC count")
    plt.title("ADC Raw and Moving Average")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(output)
    print(f"saved: {output}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "logfile",
        nargs="?",
        default="sensor_log.txt",
        help="logger_viewer.py output log file",
    )
    args = parser.parse_args()

    log_path = Path(args.logfile)

    if not log_path.exists():
        print(f"file not found: {log_path}")
        return

    adc_df, temp_df = parse_log_file(log_path)
    adc_df, temp_df = add_time_seconds(adc_df, temp_df)

    print("ADC rows :", len(adc_df))
    print("TEMP rows:", len(temp_df))

    if not adc_df.empty:
        adc_df.to_csv("adc_log.csv", index=False)
        print("saved: adc_log.csv")

    if not temp_df.empty:
        temp_df.to_csv("temp_log.csv", index=False)
        print("saved: temp_log.csv")

    plot_temperature(temp_df, Path("temperature.png"))
    plot_adc_voltage(adc_df, Path("adc_voltage.png"))
    plot_adc_raw_avg(adc_df, Path("adc_raw_avg.png"))


if __name__ == "__main__":
    main()
