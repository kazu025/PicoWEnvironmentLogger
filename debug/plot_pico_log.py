#  ---------------------------------------------------------
# python3 plot_pico_log.py adc_log.csv
# ---------------------------------------------------------
#!/usr/bin/env python3
import sys
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt


def parse_pico_log(csv_path: Path) -> pd.DataFrame:
    df = pd.read_csv(csv_path)

    # CRC OK の行だけを解析対象にする
    df = df[df["crc_ok"].astype(str) == "1"].copy()

    rows = []
    for _, r in df.iterrows():
        text = str(r.get("payload_text", ""))
        parts = text.split(",")
        if not parts:
            continue

        kind = parts[0]
        base = {
            "pc_time": pd.to_datetime(r["pc_time"], errors="coerce"),
            "seq": int(r["seq"]),
            "kind": kind,
            "payload_text": text,
        }

        try:
            if kind == "ADC" and len(parts) >= 5:
                base.update({
                    "timestamp_ms": int(parts[1]),
                    "adc_raw": int(parts[2]),
                    "adc_avg": int(parts[3]),
                    "adc_voltage": float(parts[4]),
                })
                rows.append(base)

            elif kind == "TEMP" and len(parts) >= 4:
                base.update({
                    "timestamp_ms": int(parts[1]),
                    "temperature_c": float(parts[2]),
                    "status": parts[3],
                })
                rows.append(base)

            elif kind == "ENV" and len(parts) >= 5:
                base.update({
                    "timestamp_ms": int(parts[1]),
                    "bme_temperature_c": float(parts[2]),
                    "humidity_rh": float(parts[3]),
                    "pressure_hpa": float(parts[4]),
                })
                rows.append(base)

        except ValueError:
            # ENV,xxxxx,ERR などはここでスキップ
            pass

    return pd.DataFrame(rows)


def main() -> None:
    if len(sys.argv) < 2:
        print("Usage: python plot_pico_log.py adc_log.csv")
        sys.exit(1)

    csv_path = Path(sys.argv[1])
    out_dir = csv_path.with_suffix("")
    out_dir = out_dir.parent / f"{out_dir.name}_plots"
    out_dir.mkdir(exist_ok=True)

    parsed = parse_pico_log(csv_path)
    parsed.to_csv(out_dir / "parsed_pico_log.csv", index=False)

    adc = parsed[parsed["kind"] == "ADC"].copy()
    temp = parsed[parsed["kind"] == "TEMP"].copy()
    env = parsed[parsed["kind"] == "ENV"].copy()

    if not adc.empty:
        plt.figure()
        plt.plot(adc["pc_time"], adc["adc_voltage"], marker="o")
        plt.title("ADC Voltage")
        plt.xlabel("PC time")
        plt.ylabel("Voltage [V]")
        plt.xticks(rotation=30)
        plt.tight_layout()
        plt.savefig(out_dir / "adc_voltage.png", dpi=150)
        plt.close()

    if not temp.empty or not env.empty:
        plt.figure()
        if not temp.empty:
            plt.plot(temp["pc_time"], temp["temperature_c"], marker="o", label="ADT7410")
        if not env.empty:
            plt.plot(env["pc_time"], env["bme_temperature_c"], marker="o", label="BME280")
        plt.title("Temperature")
        plt.xlabel("PC time")
        plt.ylabel("Temperature [C]")
        plt.xticks(rotation=30)
        plt.legend()
        plt.tight_layout()
        plt.savefig(out_dir / "temperature.png", dpi=150)
        plt.close()

    if not env.empty:
        plt.figure()
        plt.plot(env["pc_time"], env["humidity_rh"], marker="o")
        plt.title("BME280 Humidity")
        plt.xlabel("PC time")
        plt.ylabel("Humidity [%RH]")
        plt.xticks(rotation=30)
        plt.tight_layout()
        plt.savefig(out_dir / "humidity.png", dpi=150)
        plt.close()

        plt.figure()
        plt.plot(env["pc_time"], env["pressure_hpa"], marker="o")
        plt.title("BME280 Pressure")
        plt.xlabel("PC time")
        plt.ylabel("Pressure [hPa]")
        plt.xticks(rotation=30)
        plt.tight_layout()
        plt.savefig(out_dir / "pressure.png", dpi=150)
        plt.close()

    print(f"parsed rows: {len(parsed)}")
    print(f"ADC rows   : {len(adc)}")
    print(f"TEMP rows  : {len(temp)}")
    print(f"ENV rows   : {len(env)}")
    print(f"output dir : {out_dir}")


if __name__ == "__main__":
    main()
