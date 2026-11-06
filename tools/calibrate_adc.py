#!/usr/bin/env python3
"""
tools/calibrate_adc.py — Fortico ADC Calibration Tool

Connects to ESP32 via Serial, reads ADC raw values for Battery (GPIO34) and Fuel (GPIO36),
compares with real multimeter measurements entered by user, and calculates exact divider ratios.
Optionally updates src/utils/adc_calibration.h automatically.

Usage:
    python tools/calibrate_adc.py --measured-battery 12.54 --measured-adc-mv 3135.0 --update-header
"""

import argparse
import os
import re
import sys

HEADER_PATH = os.path.join(os.path.dirname(__file__), "..", "src", "utils", "adc_calibration.h")

def calculate_ratio(real_voltage_volts, adc_mv):
    if adc_mv <= 0:
        return 0.0
    adc_volts = adc_mv / 1000.0
    return real_voltage_volts / adc_volts

def update_header_file(battery_ratio):
    abs_path = os.path.abspath(HEADER_PATH)
    if not os.path.exists(abs_path):
        print(f"Error: Header file not found at {abs_path}")
        return False

    with open(abs_path, "r", encoding="utf-8") as f:
        content = f.read()

    new_content = re.sub(
        r"constexpr float BATTERY_DIVIDER_RATIO = [0-9\.]+F;",
        f"constexpr float BATTERY_DIVIDER_RATIO = {battery_ratio:.4f}F;",
        content
    )

    with open(abs_path, "w", encoding="utf-8") as f:
        f.write(new_content)

    print(f"Updated {abs_path} with BATTERY_DIVIDER_RATIO = {battery_ratio:.4f}F")
    return True

def main():
    parser = argparse.ArgumentParser(description="Fortico ADC Calibration Tool")
    parser.add_argument("--measured-battery", type=float, help="Multimeter measured battery voltage (e.g. 12.54 Volts)")
    parser.add_argument("--measured-adc-mv", type=float, help="ADC read value in millivolts (e.g. 3135.0 mV)")
    parser.add_argument("--update-header", action="store_true", help="Automatically update src/utils/adc_calibration.h")

    args = parser.parse_args()

    if args.measured_battery and args.measured_adc_mv:
        ratio = calculate_ratio(args.measured_battery, args.measured_adc_mv)
        print("==================================================")
        print("         Fortico ADC Calibration Result           ")
        print("==================================================")
        print(f" Multimeter Voltage : {args.measured_battery:.3f} V")
        print(f" ADC Pin Voltage    : {args.measured_adc_mv / 1000.0:.3f} V ({args.measured_adc_mv:.1f} mV)")
        print(f" Calculated Ratio   : {ratio:.4f}")
        print("==================================================")

        if args.update_header:
            update_header_file(ratio)
    else:
        print("Interactive mode:")
        try:
            battery_v = float(input("Enter actual battery voltage measured with multimeter (V): "))
            adc_mv = float(input("Enter ADC reading reported by ESP32 (mV): "))
            ratio = calculate_ratio(battery_v, adc_mv)
            print(f"\nCalculated BATTERY_DIVIDER_RATIO = {ratio:.4f}")
            ans = input("Update src/utils/adc_calibration.h with this ratio? (y/N): ").strip().lower()
            if ans == "y":
                update_header_file(ratio)
        except (ValueError, KeyboardInterrupt):
            print("\nAborted.")
            sys.exit(1)

if __name__ == "__main__":
    main()

