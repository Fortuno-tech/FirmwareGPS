#!/usr/bin/env python3
"""Add or remove an RFID badge during the ESP32 provisioning window.

Examples (Windows):
    py tools/manage_rfid_badges.py --port COM6 add  FB:C2:1C:07
    py tools/manage_rfid_badges.py --port COM6 remove FB:C2:1C:07

The tool resets the ESP32 by default, waits for PROV:READY, sends the command,
and returns a non-zero exit code if the firmware rejects it.
"""

import argparse
import re
import sys
import time

try:
    import serial
except ImportError:
    serial = None


BAUDRATE = 115200
READY_TIMEOUT_S = 15.0
RESPONSE_TIMEOUT_S = 5.0
UID_PATTERN = re.compile(r"^(?:[0-9A-F]{2}:){3}[0-9A-F]{2}$|^(?:[0-9A-F]{2}:){6}[0-9A-F]{2}$|^(?:[0-9A-F]{2}:){9}[0-9A-F]{2}$")


def read_line(ser):
    raw = ser.readline()
    return raw.decode("utf-8", errors="replace").strip() if raw else ""


def reset_esp32(ser):
    """Use the usual USB-UART control lines; --no-reset supports manual EN."""
    ser.dtr = False
    ser.rts = True
    time.sleep(0.15)
    ser.rts = False
    time.sleep(0.10)


def wait_for_ready(ser):
    deadline = time.monotonic() + READY_TIMEOUT_S
    while time.monotonic() < deadline:
        line = read_line(ser)
        if not line:
            continue
        print(f"ESP32: {line}")
        if line == "PROV:READY":
            return True
    return False


def wait_for_result(ser):
    deadline = time.monotonic() + RESPONSE_TIMEOUT_S
    expected = {
        "PROV:RFID:ADDED",
        "PROV:RFID:ALREADY_PRESENT",
        "PROV:RFID:REMOVED",
        "PROV:RFID:FAIL",
    }
    while time.monotonic() < deadline:
        line = read_line(ser)
        if not line:
            continue
        print(f"ESP32: {line}")
        if line in expected:
            return line
    return None


def main():
    parser = argparse.ArgumentParser(description="Fortico RFID whitelist manager")
    parser.add_argument("--port", required=True, help="ESP32 serial port, e.g. COM6")
    parser.add_argument("--baud", type=int, default=BAUDRATE, help="Serial baud rate")
    parser.add_argument("--no-reset", action="store_true", help="Do not reset; press EN, then run this command")
    parser.add_argument("action", choices=("add", "remove"), help="Whitelist action")
    parser.add_argument("uid", help="RFID UID, e.g. FB:C2:1C:07")
    args = parser.parse_args()

    uid = args.uid.upper()
    if not UID_PATTERN.fullmatch(uid):
        parser.error("UID must contain 4, 7, or 10 hexadecimal bytes, separated by ':'")
    if serial is None:
        print("[ERROR] pyserial is required: py -m pip install pyserial", file=sys.stderr)
        return 2

    command = f"PROV:RFID:{args.action.upper()}:{uid}\n".encode("ascii")
    try:
        with serial.Serial(args.port, args.baud, timeout=0.2, write_timeout=2) as ser:
            if not args.no_reset:
                reset_esp32(ser)
            ser.reset_input_buffer()

            print("Waiting for PROV:READY...")
            if not wait_for_ready(ser):
                print("[ERROR] PROV:READY not received. Check port/cable, or reset the ESP32.", file=sys.stderr)
                return 1

            print(f"Sending: {command.decode().strip()}")
            ser.write(command)
            ser.flush()
            result = wait_for_result(ser)
    except serial.SerialException as error:
        print(f"[ERROR] Serial connection failed: {error}", file=sys.stderr)
        return 2

    if result in ("PROV:RFID:ADDED", "PROV:RFID:ALREADY_PRESENT", "PROV:RFID:REMOVED"):
        print(f"[OK] {result}")
        return 0
    if result == "PROV:RFID:FAIL":
        print("[ERROR] Firmware rejected the RFID command (invalid UID, full list, or provisioning window closed).", file=sys.stderr)
    else:
        print("[ERROR] No RFID response received.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
