#!/usr/bin/env python3
"""
tools/provision_device.py — Fortico Device Provisioning CLI Tool

Generates or receives root key (AES-256-GCM), OTA Ed25519 public key,
device ID, and MQTT credentials, then sends them to ESP32 via Serial interface.

Usage:
    python tools/provision_device.py --port COM6 [options]
"""

import argparse
import secrets
import sys
import time

try:
    import serial
except ImportError:
    serial = None

def generate_key_hex(length=32):
    return secrets.token_hex(length)

def main():
    parser = argparse.ArgumentParser(description="Fortico ESP32 Provisioning Tool")
    parser.add_argument("--port", required=True, help="Serial port (e.g. COM6 or /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--device-id", default="FORTICO-DEV-001", help="Device ID")
    parser.add_argument("--mqtt-client-id", default="FORTICO-DEV-001", help="MQTT Client ID")
    parser.add_argument("--mqtt-user", default="fortico_device", help="MQTT Username")
    parser.add_argument("--mqtt-pass", default="device_secret_token_123", help="MQTT Password")
    parser.add_argument("--root-key", help="32-byte Hex Root Key (auto-generated if omitted)")
    parser.add_argument("--ota-pubkey", help="32-byte Hex Ed25519 Public Key (auto-generated if omitted)")
    parser.add_argument("--dry-run", action="store_true", help="Print payload without sending over serial")

    args = parser.parse_args()

    root_key = args.root_key or generate_key_hex(32)
    ota_pubkey = args.ota_pubkey or generate_key_hex(32)

    if len(root_key) != 64:
        print(f"Error: Root key must be 64 hex characters (32 bytes), got {len(root_key)}")
        sys.exit(1)
    if len(ota_pubkey) != 64:
        print(f"Error: OTA public key must be 64 hex characters (32 bytes), got {len(ota_pubkey)}")
        sys.exit(1)

    payload = f"PROV:{root_key}:{ota_pubkey}:{args.device_id}:{args.mqtt_client_id}:{args.mqtt_user}:{args.mqtt_pass}\n"

    print("==================================================")
    print("      Fortico Device Provisioning Summary        ")
    print("==================================================")
    print(f" Port            : {args.port}")
    print(f" Device ID       : {args.device_id}")
    print(f" MQTT Client ID  : {args.mqtt_client_id}")
    print(f" Root Key (HEX)  : {root_key[:8]}...{root_key[-8:]}")
    print(f" OTA PubKey(HEX) : {ota_pubkey[:8]}...{ota_pubkey[-8:]}")
    print("==================================================")

    if args.dry_run:
        print("[DRY-RUN] Payload format:")
        print(payload.strip())
        sys.exit(0)

    if serial is None:
        print("Error: pyserial package is required. Install via 'pip install pyserial'")
        sys.exit(1)

    print(f"Connecting to {args.port} at {args.baud} baud...")
    try:
        ser = serial.Serial(args.port, args.baud, timeout=1)

        print("Listening for ESP32 readiness (PROV:READY)...")
        start_time = time.time()
        last_send_time = 0
        got_ready = False

        while time.time() - start_time < 20:
            if ser.in_waiting > 0:
                line = ser.readline().decode("utf-8", errors="replace").strip()
                if line:
                    print(f"ESP32 Log: {line}")
                    if "PROV:READY" in line or "[PROV] Device unprovisioned" in line:
                        got_ready = True
                        print("ESP32 is ready! Sending provisioning payload...")
                        ser.write(payload.encode("utf-8"))
                        ser.flush()
                        last_send_time = time.time()
                    elif "PROV:OK" in line:
                        print("\n==================================================")
                        print("SUCCESS: Device provisioned successfully!")
                        print("==================================================")
                        ser.close()
                        sys.exit(0)
                    elif "PROV:FAIL" in line:
                        print("\nFAILURE: Provisioning rejected by ESP32 (already provisioned or invalid payload).")
                        ser.close()
                        sys.exit(1)

            # If 2 seconds passed without PROV:READY, send payload anyway and retry every 1.5s
            now = time.time()
            if not got_ready and (now - start_time > 2.0) and (now - last_send_time > 1.5):
                print("Sending provisioning payload (retry)...")
                ser.write(payload.encode("utf-8"))
                ser.flush()
                last_send_time = now
            elif got_ready and (now - last_send_time > 1.5):
                print("Re-sending payload...")
                ser.write(payload.encode("utf-8"))
                ser.flush()
                last_send_time = now

            time.sleep(0.05)

        print("\nFAILURE: Timeout reached without PROV:OK response.")
        print("Note: Existing NVS provisioning is preserved; do not erase it to retry.")
        ser.close()
        sys.exit(1)

    except Exception as e:
        print(f"Serial Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
