#!/usr/bin/env python3
"""
tools/provision_device.py — Fortico Device Provisioning CLI Tool

Receives a root key (AES-256-GCM) from an approved vault and OTA Ed25519 public key,
device ID, and MQTT credentials, then sends them to ESP32 via Serial interface.
When --modem-port is supplied, it also provisions the public HiveMQ CA directly
into the SIM7600 certificate store before sending the ESP32 NVS payload.

Usage:
    python tools/provision_device.py --port COM6 --device-id DEVICE_ID
        --vehicle-id VEHICLE_ID --mqtt-user USER --ota-pubkey HEX [options]

Root key and MQTT password are requested interactively and are never accepted
as command-line arguments.
"""

import argparse
import sys
import time
from getpass import getpass
from pathlib import Path

from upload_certificate import CERTIFICATE_PATH, upload_certificate

try:
    import serial
except ImportError:
    serial = None

def main():
    parser = argparse.ArgumentParser(description="Fortico ESP32 Provisioning Tool")
    parser.add_argument("--port", required=True, help="Serial port (e.g. COM6 or /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--device-id", required=True, help="Unique hardware device ID")
    parser.add_argument("--vehicle-id", required=True, help="Stable fleet vehicle ID used in MQTT topics")
    parser.add_argument("--mqtt-client-id", help="MQTT Client ID (defaults to --device-id)")
    parser.add_argument("--mqtt-user", required=True, help="MQTT Username")
    parser.add_argument("--ota-pubkey", required=True,
                        help="32-byte Ed25519 public key from the approved release signer")
    parser.add_argument("--dry-run", action="store_true",
                        help="Validate inputs without sending secrets over serial")
    parser.add_argument(
        "--modem-port",
        help="Direct SIM7600 serial port; uploads tools/certificates/hivemq-ca.pem first",
    )
    parser.add_argument(
        "--certificate",
        type=Path,
        default=CERTIFICATE_PATH,
        help="Public CA PEM file for --modem-port",
    )
    parser.add_argument(
        "--replace-certificate",
        action="store_true",
        help="Explicitly replace an existing hivemq-ca.pem on the SIM7600",
    )

    args = parser.parse_args()

    if not sys.stdin.isatty():
        print("Error: interactive secure input is required for provisioning secrets.", file=sys.stderr)
        sys.exit(1)

    root_key = getpass("Device root key (64 hex characters, from approved vault): ")
    mqtt_password = getpass("MQTT password: ")
    ota_pubkey = args.ota_pubkey
    mqtt_client_id = args.mqtt_client_id or args.device_id

    if len(root_key) != 64 or any(c not in "0123456789abcdefABCDEF" for c in root_key):
        print("Error: Root key must be 64 hexadecimal characters (32 bytes).", file=sys.stderr)
        sys.exit(1)
    if len(ota_pubkey) != 64 or any(c not in "0123456789abcdefABCDEF" for c in ota_pubkey):
        print("Error: OTA public key must be 64 hexadecimal characters (32 bytes).", file=sys.stderr)
        sys.exit(1)
    if not mqtt_password:
        print("Error: MQTT password must not be empty.", file=sys.stderr)
        sys.exit(1)
    fields = (
        args.device_id,
        args.vehicle_id,
        mqtt_client_id,
        args.mqtt_user,
        mqtt_password,
    )
    limits = (63, 63, 63, 63, 95)
    if any(
        not value
        or len(value) > limit
        or not value.isascii()
        or any(ord(char) < 0x20 or ord(char) == 0x7F for char in value)
        or any(delimiter in value for delimiter in (":", '"', "\r", "\n"))
        for value, limit in zip(fields, limits)
    ):
        print("Error: Provisioning fields are empty, too long, or contain a reserved delimiter.",
              file=sys.stderr)
        sys.exit(1)

    payload = f"PROV:{root_key}:{ota_pubkey}:{args.device_id}:{args.vehicle_id}:{mqtt_client_id}:{args.mqtt_user}:{mqtt_password}\n"

    print("==================================================")
    print("      Fortico Device Provisioning Summary        ")
    print("==================================================")
    print(f" Port            : {args.port}")
    print(f" Device ID       : {args.device_id}")
    print(f" Vehicle ID      : {args.vehicle_id}")
    print(f" MQTT Client ID  : {mqtt_client_id}")
    print(" Root key        : supplied interactively (not displayed)")
    print(" MQTT password   : supplied interactively (not displayed)")
    print(" OTA public key  : supplied by approved release signer")
    print("==================================================")

    if args.dry_run:
        print("[DRY-RUN] Payload is not displayed because it contains MQTT credentials.")
        sys.exit(0)

    if args.modem_port:
        try:
            upload_certificate(
                args.modem_port,
                args.certificate.resolve(),
                args.replace_certificate,
            )
        except (OSError, RuntimeError) as error:
            print(f"[ERROR] SIM7600 certificate provisioning failed: {error}", file=sys.stderr)
            sys.exit(1)

    if serial is None:
        print("Error: pyserial package is required. Install via 'py -m pip install pyserial'")
        sys.exit(1)

    import base64

    print(f"Connecting to {args.port} at {args.baud} baud...")
    try:
        ser = serial.Serial(args.port, args.baud, timeout=1)
        # Pulse DTR/RTS to automatically trigger ESP32 hardware reset
        try:
            ser.dtr = False
            ser.rts = True
            time.sleep(0.15)
            ser.rts = False
            time.sleep(0.1)
        except Exception:
            pass

        ser.reset_input_buffer()
        ser.reset_output_buffer()

        print("Listening for ESP32 readiness (PROV:READY)... (or press EN/RST button)")
        start_time = time.time()
        got_ready = False
        device_already_provisioned = False

        while time.time() - start_time < 30:
            if ser.in_waiting > 0:
                line = ser.readline().decode("utf-8", errors="replace").strip()
                if line:
                    print(f"ESP32 Log: {line}")
                    if "Device provisioned" in line:
                        device_already_provisioned = True
                    if "PROV:READY" in line or "[PROV] Device unprovisioned" in line or "[PROV] Device provisioned" in line:
                        got_ready = True
                        break
            time.sleep(0.05)

        if not got_ready:
            print("[WARN] No PROV:READY received within timeout, attempting certificate check anyway...")

        # Step 1: Check if certificate is present on modem
        print("[PROV] Checking HiveMQ CA certificate on SIM7600...")
        ser.write(b"PROV:CERT:CHECK\n")
        ser.flush()

        cert_status = None
        check_start = time.time()
        while time.time() - check_start < 15:
            if ser.in_waiting > 0:
                line = ser.readline().decode("utf-8", errors="replace").strip()
                if line:
                    print(f"ESP32 Log: {line}")
                    if "PROV:CERT:PRESENT" in line:
                        cert_status = "PRESENT"
                        break
                    elif "PROV:CERT:ABSENT" in line:
                        cert_status = "ABSENT"
                        break
            time.sleep(0.05)

        if cert_status is None:
            print("[WARN] No response to PROV:CERT:CHECK. Proceeding with certificate transfer...")
            cert_status = "ABSENT"

        # Step 2: Transfer certificate if absent or if --replace-certificate requested
        if cert_status == "ABSENT" or args.replace_certificate:
            cert_file = args.certificate.resolve()
            if not cert_file.is_file():
                print(f"[ERROR] Certificate file not found: {cert_file}", file=sys.stderr)
                ser.close()
                sys.exit(1)

            cert_data = cert_file.read_bytes()
            if not cert_data:
                print(f"[ERROR] Certificate file is empty: {cert_file}", file=sys.stderr)
                ser.close()
                sys.exit(1)

            cert_b64 = base64.b64encode(cert_data).decode("ascii")
            print(f"[PROV] Uploading certificate ({len(cert_data)} bytes decoded, {len(cert_b64)} bytes base64)...")
            cmd = f"PROV:CERT:{len(cert_data)}:{cert_b64}\n"
            ser.write(cmd.encode("utf-8"))
            ser.flush()

            cert_ok = False
            upload_start = time.time()
            while time.time() - upload_start < 25:
                if ser.in_waiting > 0:
                    line = ser.readline().decode("utf-8", errors="replace").strip()
                    if line:
                        print(f"ESP32 Log: {line}")
                        if "PROV:CERT:OK" in line:
                            cert_ok = True
                            print("[PROV] HiveMQ CA certificate installed and verified successfully!")
                            break
                        elif "PROV:CERT:FAIL" in line:
                            print("[ERROR] Certificate installation failed on ESP32/SIM7600.")
                            ser.close()
                            sys.exit(1)
                time.sleep(0.05)

            if not cert_ok:
                print("[ERROR] Timeout waiting for PROV:CERT:OK response.")
                ser.close()
                sys.exit(1)
        else:
            print("[PROV] Certificate already present on SIM7600; transfer skipped.")

        # Step 3: NVS device provisioning
        print("[PROV] Sending NVS provisioning payload...")
        ser.write(payload.encode("utf-8"))
        ser.flush()

        prov_start = time.time()
        last_retry = prov_start
        while time.time() - prov_start < 20:
            if ser.in_waiting > 0:
                line = ser.readline().decode("utf-8", errors="replace").strip()
                if line:
                    print(f"ESP32 Log: {line}")
                    if "PROV:OK" in line:
                        print("\n==================================================")
                        print("SUCCESS: Device provisioned successfully!")
                        print("==================================================")
                        ser.close()
                        sys.exit(0)
                    elif "PROV:FAIL" in line:
                        if device_already_provisioned:
                            print("\n==================================================")
                            print("NOTICE: Device NVS credentials preserved (already provisioned).")
                            if cert_ok or cert_status == "PRESENT":
                                print("SUCCESS: HiveMQ CA certificate is present and verified on SIM7600!")
                            print("==================================================")
                            ser.close()
                            sys.exit(0)
                        else:
                            print("\nFAILURE: Provisioning rejected by ESP32 (already provisioned or invalid payload).")
                            ser.close()
                            sys.exit(1)

            now = time.time()
            if now - last_retry > 2.0:
                print("[PROV] Re-sending NVS provisioning payload...")
                ser.write(payload.encode("utf-8"))
                ser.flush()
                last_retry = now

            time.sleep(0.05)

        print("\nFAILURE: Timeout reached without PROV:OK response.")
        ser.close()
        sys.exit(1)

    except Exception as e:
        print(f"Serial Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
