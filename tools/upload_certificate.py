#!/usr/bin/env python3
"""Provision the public HiveMQ CA certificate into a SIM7600 modem.

The certificate is uploaded directly to the modem certificate store with
AT+CCERTDOWN. This tool never reads or handles MQTT credentials.
"""

import argparse
import sys
import time
from pathlib import Path

try:
    import serial
except ImportError:
    serial = None


BAUDRATE = 115200
CERTIFICATE_NAME = "hivemq-ca.pem"
CERTIFICATE_PATH = Path(__file__).resolve().parent / "certificates" / CERTIFICATE_NAME
SIM7600_CERTIFICATE_MAX_BYTES = 10240


def has_ok(response: bytes) -> bool:
    return b"\nOK" in response or response.startswith(b"OK")


def read_until(ser, expected: bytes, timeout: float) -> bytes:
    deadline = time.monotonic() + timeout
    response = bytearray()

    while time.monotonic() < deadline:
        waiting = ser.in_waiting
        if waiting:
            response.extend(ser.read(waiting))
            if expected in response:
                return bytes(response)
            if b"\r\nERROR\r\n" in response or b"\nERROR\r\n" in response:
                return bytes(response)
        time.sleep(0.01)

    return bytes(response)


def send_command(ser, command: bytes, expected: bytes, timeout: float) -> bytes:
    ser.write(command + b"\r\n")
    ser.flush()
    return read_until(ser, expected, timeout)


def certificate_is_listed(ser) -> tuple[bool, bytes]:
    response = send_command(ser, b"AT+CCERTLIST", b"\nOK", 5.0)
    if b"ERROR" in response or not has_ok(response):
        raise RuntimeError("AT+CCERTLIST did not complete successfully")
    return CERTIFICATE_NAME.encode("ascii") in response, response


def write_all(ser, data: bytes) -> int:
    sent = 0
    view = memoryview(data)
    while sent < len(view):
        written = ser.write(view[sent:])
        if written is None or written <= 0:
            raise RuntimeError("serial write stopped before the certificate was complete")
        sent += written
    ser.flush()
    return sent


def validate_certificate(path: Path) -> bytes:
    if not path.is_file():
        raise RuntimeError(f"certificate file not found: {path}")

    certificate = path.read_bytes()
    if not certificate:
        raise RuntimeError("certificate file is empty")
    if len(certificate) > SIM7600_CERTIFICATE_MAX_BYTES:
        raise RuntimeError(
            f"certificate is {len(certificate)} bytes; SIM7600 CCERTDOWN maximum is "
            f"{SIM7600_CERTIFICATE_MAX_BYTES} bytes"
        )
    return certificate


def upload_certificate(port: str, certificate_path: Path, replace: bool) -> None:
    if serial is None:
        raise RuntimeError("pyserial is required; install it with: py -m pip install pyserial")

    certificate = validate_certificate(certificate_path)
    print("========================================")
    print(" Fortico SIM7600 CA certificate upload")
    print("========================================")
    print(f"Port        : {port}")
    print(f"Certificate : {certificate_path}")
    print(f"Modem name  : {CERTIFICATE_NAME}")
    print(f"Size        : {len(certificate)} bytes")

    with serial.Serial(
        port=port,
        baudrate=BAUDRATE,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=0.1,
        write_timeout=15,
    ) as ser:
        time.sleep(0.5)
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        response = send_command(ser, b"AT", b"\nOK", 3.0)
        if b"ERROR" in response or not has_ok(response):
            raise RuntimeError("SIM7600 did not acknowledge AT")

        listed, response = certificate_is_listed(ser)
        if listed and not replace:
            print("[OK] Certificate already present; no transfer performed.")
            return

        if listed:
            print("[INFO] Replacing the existing certificate by explicit request.")
            response = send_command(
                ser,
                f'AT+CCERTDELE="{CERTIFICATE_NAME}"'.encode("ascii"),
                b"\nOK",
                5.0,
            )
            if b"ERROR" in response or not has_ok(response):
                raise RuntimeError("SIM7600 refused AT+CCERTDELE")

        response = send_command(ser, b"AT+CCERTDOWN=?", b"\nOK", 3.0)
        if b"ERROR" in response or b"+CCERTDOWN:" not in response or not has_ok(response):
            raise RuntimeError("SIM7600 does not support AT+CCERTDOWN")

        command = f'AT+CCERTDOWN="{CERTIFICATE_NAME}",{len(certificate)}'.encode("ascii")
        response = send_command(ser, command, b">", 5.0)
        if b"ERROR" in response or b">" not in response:
            raise RuntimeError("SIM7600 did not accept CCERTDOWN or did not issue the prompt")

        sent = write_all(ser, certificate)
        if sent != len(certificate):
            raise RuntimeError(f"only {sent} of {len(certificate)} certificate bytes were sent")

        response = read_until(ser, b"\nOK", 15.0)
        if b"ERROR" in response or not has_ok(response):
            raise RuntimeError("SIM7600 did not confirm the certificate transfer")

        listed, response = certificate_is_listed(ser)
        if not listed:
            raise RuntimeError(f"{CERTIFICATE_NAME} is absent from AT+CCERTLIST after transfer")
        print("[OK] Certificate uploaded and verified with AT+CCERTLIST.")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Upload the Fortico public HiveMQ CA to a SIM7600 modem"
    )
    parser.add_argument("--port", required=True, help="SIM7600 serial port, for example COM5")
    parser.add_argument(
        "--certificate",
        type=Path,
        default=CERTIFICATE_PATH,
        help="Public CA PEM file (default: tools/certificates/hivemq-ca.pem)",
    )
    parser.add_argument(
        "--replace",
        action="store_true",
        help="Explicitly delete and replace an existing hivemq-ca.pem certificate",
    )
    args = parser.parse_args()

    try:
        upload_certificate(args.port, args.certificate.resolve(), args.replace)
    except (OSError, RuntimeError, serial.SerialException if serial is not None else RuntimeError) as error:
        print(f"[ERROR] {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
